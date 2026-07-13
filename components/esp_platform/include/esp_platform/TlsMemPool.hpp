// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>

namespace esp_platform {

/**
 * Reserves a fixed static arena in internal (DMA-capable) RAM at link time
 * and installs it as mbedTLS's calloc/free implementation via
 * mbedtls_platform_set_calloc_free().
 *
 * Why: ESP32-S3's hardware MPI/RSA/DS-peripheral crypto accelerators can
 * only operate on internal RAM. CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC already
 * pushes mbedTLS's big TX/RX buffers (16 KB in / 4 KB out, via
 * CONFIG_MBEDTLS_DYNAMIC_BUFFER) to PSRAM, but the small, unavoidable
 * internal-only crypto-scratch working set was still coming from the
 * general internal heap, competing there with WiFi/lwIP/app allocations
 * for hours at a time. Total free internal RAM stayed healthy while the
 * *largest contiguous block* eroded — see EspDeviceInterface::info()'s
 * largestFreeInternalBlock comment on the framework side, and
 * ApplicationContext.cpp's HeapMonitor task on the sound-capture-node side;
 * both independently documented this exact failure mode ("malloc inside a
 * TLS handshake ... not raw exhaustion") before this fix existed.
 *
 * mbedtls_platform_set_calloc_free() intercepts ALL mbedTLS calloc/free
 * calls — there's no separate hook for "just the small ones" — but this
 * class only actually services requests below kSmallAllocThreshold from
 * the static arena. Anything at or above that threshold is forwarded to
 * heap_caps_calloc(..., MALLOC_CAP_SPIRAM), preserving today's PSRAM
 * offload for the bulk buffers. This only takes over the part of the
 * allocation pattern that was actually fragmenting internal RAM.
 *
 * Call install() exactly once, early in EspDeviceInterface::init(), before
 * any TLS activity (HTTPS server start, esp_https_ota, or any TLS client
 * use) can occur.
 */
class TlsMemPool {
  public:
    static constexpr const char* TAG = "TlsMemPool";

    // 48 KiB: the node-172/node-174 field diagnosis put one handshake's
    // internal-only working set at ~40 KiB; +20% headroom covers mbedTLS/
    // IDF version drift without needing to re-measure on every upgrade.
    // Sized for ONE concurrent handshake — esp_http_server (and therefore
    // esp_https_server) runs its accept/handshake/serve loop on a single
    // task, so only one handshake is ever in flight regardless of
    // max_open_sockets (ESP-IDF's httpd APIs are documented as not
    // thread-safe precisely because the server operates in a single task
    // context). A concurrent OTA client-side handshake is the one other
    // realistic consumer of this pool; revisit this constant if the
    // fallback-to-general-heap path in alloc() ever actually fires.
    static constexpr size_t kArenaBytes = 49152;

    // Requests at or above this go to PSRAM, same as today. Set comfortably
    // above the largest single crypto-scratch chunk mbedTLS's MPI/ECC code
    // asks for during a handshake, and comfortably below the 16 KB SSL
    // input buffer, so the big buffers never compete for arena space.
    static constexpr size_t kSmallAllocThreshold = 8192;

    // Idempotent — safe to call more than once, only installs on the first.
    static void install();

    // Diagnostics, meant to be logged alongside HeapMonitor's periodic
    // internal_largest/psram_largest line so the pool's own health is
    // visible without a separate code path.
    static size_t bytesFree();
    static size_t bytesUsed();
    static size_t largestFreeBlock();

    // --- Timing/pattern diagnostics added 2026-07-13, investigating
    // reported UI/firmware-upload sluggishness since this pool took over
    // mbedTLS's small allocations from the OS heap. Two questions:
    // (1) Is the first-fit scan itself slow (allocAvgUs/allocMaxUs,
    //     freeAvgUs/freeMaxUs — time spent holding the mutex doing actual
    //     work), or is it lock contention from a second concurrent TLS user
    //     like an OTA client handshake overlapping a browser session
    //     (allocMutexWaitAvgUs/MaxUs, freeMutexWaitAvgUs/MaxUs — time spent
    //     waiting to acquire the mutex before that work even starts)?
    // (2) Is mbedTLS's alloc/free pattern strictly LIFO (lifoViolations()
    //     staying at 0)? If so, a much cheaper bump/arena allocator
    //     (O(1) alloc, checkpoint-based free, no scan, no coalescing) would
    //     be safe to switch to instead of this first-fit design.
    static uint32_t allocCount();
    static uint32_t allocAvgUs();
    static uint32_t allocMaxUs();
    static uint32_t allocMutexWaitAvgUs();
    static uint32_t allocMutexWaitMaxUs();
    static uint32_t freeCount();
    static uint32_t freeAvgUs();
    static uint32_t freeMaxUs();
    static uint32_t freeMutexWaitAvgUs();
    static uint32_t freeMutexWaitMaxUs();
    static uint32_t lifoViolations();

  private:
    static void* alloc(size_t nmemb, size_t size);
    static void  release(void* ptr);

    static bool installed_;
};

} // namespace esp_platform
