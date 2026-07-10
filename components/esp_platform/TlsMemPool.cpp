// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#include "esp_platform/TlsMemPool.hpp"

#include "logger/Logger.hpp"

extern "C" {
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/semphr.h"
#include "mbedtls/platform.h"
}

#include <cstdint>
#include <cstring>

namespace esp_platform {

static logger::Logger log{TlsMemPool::TAG};

bool TlsMemPool::installed_ = false;

namespace {

// Minimal first-fit free-list allocator over a static arena. Simple by
// design — the arena is small, requests are few and short-lived (one
// handshake's worth at a time), so a first-fit walk is fast enough and easy
// to reason about. Blocks are singly linked in address order, which is all
// coalesce() needs to merge adjacent free runs.
struct BlockHeader {
    size_t       size;  // usable bytes following this header
    bool         free;
    BlockHeader* next;  // next block in address order; nullptr = last
};

constexpr size_t kAlignment          = 8;
constexpr size_t kMinSplitRemainder  = sizeof(BlockHeader) + kAlignment;

alignas(kAlignment) uint8_t arena_[TlsMemPool::kArenaBytes];
BlockHeader*       freeListHead_ = nullptr;
SemaphoreHandle_t  mutex_        = nullptr;

size_t alignUp(size_t n) {
    return (n + (kAlignment - 1)) & ~(kAlignment - 1);
}

bool inArena(const void* ptr) {
    const auto* p = static_cast<const uint8_t*>(ptr);
    return p >= arena_ && p < (arena_ + TlsMemPool::kArenaBytes);
}

// Caller must hold mutex_. Single linear pass — restarts on the same node
// after a merge so a run of 3+ adjacent free blocks collapses fully.
void coalesce() {
    for (BlockHeader* b = freeListHead_; b && b->next; ) {
        auto* afterB = reinterpret_cast<uint8_t*>(b) + sizeof(BlockHeader) + b->size;
        if (b->free && b->next->free && reinterpret_cast<uint8_t*>(b->next) == afterB) {
            b->size += sizeof(BlockHeader) + b->next->size;
            b->next  = b->next->next;
        } else {
            b = b->next;
        }
    }
}

} // namespace

void TlsMemPool::install() {
    if (installed_) {
        return;
    }

    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        log.error("install: mutex create failed — mbedTLS keeps default allocator");
        return;
    }

    freeListHead_       = reinterpret_cast<BlockHeader*>(arena_);
    freeListHead_->size = kArenaBytes - sizeof(BlockHeader);
    freeListHead_->free = true;
    freeListHead_->next = nullptr;

    mbedtls_platform_set_calloc_free(&TlsMemPool::alloc, &TlsMemPool::release);
    installed_ = true;
    log.info("installed — arena=%u B, small-alloc threshold=%u B",
              static_cast<unsigned>(kArenaBytes),
              static_cast<unsigned>(kSmallAllocThreshold));
}

void* TlsMemPool::alloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    if (total == 0) {
        return nullptr;
    }

    if (total >= kSmallAllocThreshold) {
        // Bulk buffer (SSL I/O, X.509 chain, etc.) — same PSRAM offload as
        // today's CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC behaviour. No mutex
        // needed here; heap_caps_calloc is already thread-safe and this
        // path never touches the arena.
        return heap_caps_calloc(1, total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    total = alignUp(total);

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return nullptr;
    }

    void* result = nullptr;
    for (BlockHeader* b = freeListHead_; b; b = b->next) {
        if (!b->free || b->size < total) {
            continue;
        }
        if (b->size >= total + kMinSplitRemainder) {
            auto* remainder = reinterpret_cast<BlockHeader*>(
                reinterpret_cast<uint8_t*>(b) + sizeof(BlockHeader) + total);
            remainder->size = b->size - total - sizeof(BlockHeader);
            remainder->free = true;
            remainder->next = b->next;
            b->next = remainder;
            b->size = total;
        }
        b->free = false;
        result  = reinterpret_cast<uint8_t*>(b) + sizeof(BlockHeader);
        std::memset(result, 0, total);  // calloc semantics
        break;
    }

    xSemaphoreGive(mutex_);

    if (!result) {
        // Arena exhausted/fragmented — should be rare given the headroom
        // margin (see kArenaBytes), but fall back to the general internal
        // heap rather than fail the TLS operation outright. Logged so this
        // is visible if it ever fires; if it does, kArenaBytes needs
        // revisiting rather than living with silent fallbacks.
        log.warn("arena exhausted for %u B request — falling back to general internal heap",
                  static_cast<unsigned>(total));
        result = heap_caps_calloc(1, total, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    return result;
}

void TlsMemPool::release(void* ptr) {
    if (!ptr) {
        return;
    }

    if (!inArena(ptr)) {
        // Either a large PSRAM-backed buffer or a general-heap fallback —
        // both were allocated via heap_caps_*, both free the same way.
        heap_caps_free(ptr);
        return;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return;
    }

    auto* header = reinterpret_cast<BlockHeader*>(static_cast<uint8_t*>(ptr) - sizeof(BlockHeader));
    header->free = true;
    coalesce();

    xSemaphoreGive(mutex_);
}

size_t TlsMemPool::bytesFree() {
    if (!mutex_) {
        return 0;
    }
    size_t total = 0;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    for (BlockHeader* b = freeListHead_; b; b = b->next) {
        if (b->free) total += b->size;
    }
    xSemaphoreGive(mutex_);
    return total;
}

size_t TlsMemPool::bytesUsed() {
    return kArenaBytes - bytesFree();
}

size_t TlsMemPool::largestFreeBlock() {
    if (!mutex_) {
        return 0;
    }
    size_t largest = 0;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    for (BlockHeader* b = freeListHead_; b; b = b->next) {
        if (b->free && b->size > largest) largest = b->size;
    }
    xSemaphoreGive(mutex_);
    return largest;
}

} // namespace esp_platform
