// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#include "persistent_log/PersistentLogSink.hpp"

#include "esp_littlefs.h"
#include "esp_timer.h"
#include "logger/Logger.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace persistent_log {

static logger::Logger log{PersistentLogSink::TAG};

namespace {
constexpr const char* kMountPoint     = "/littlefs";
constexpr const char* kPartitionLabel = "assets_fs";
constexpr const char* kFileNames[2] = {"/littlefs/log_a.txt", "/littlefs/log_b.txt"};
constexpr uint32_t kWorkerStackBytes = 4096;
constexpr UBaseType_t kWorkerPriority = tskIDLE_PRIORITY + 1;
} // namespace

bool PersistentLogSink::mount() {
    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = kMountPoint;
    conf.partition_label = kPartitionLabel;
    conf.format_if_mount_failed = true;
    conf.dont_mount = false;
    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        log.warn("LittleFS mount failed (%s) — persistent logging disabled",
                  esp_err_to_name(err));
        mounted_ = false;
        return false;
    }

    // Determine which file was active at the time of the last crash/reboot.
    // The active file was being appended to, so it's the smaller one; the
    // inactive file is either full (kMaxFileBytes) or freshly truncated (0).
    // Always resuming with log_a (the old behaviour) caused a bug: if log_a
    // was full and the previous run was writing to log_b, the first write
    // after reboot would truncate log_b and destroy the pre-failure history.
    struct stat stA{}, stB{};
    size_t sizeA = (stat(kFileNames[0], &stA) == 0) ? static_cast<size_t>(stA.st_size) : 0;
    size_t sizeB = (stat(kFileNames[1], &stB) == 0) ? static_cast<size_t>(stB.st_size) : 0;
    activeFile_  = (sizeA <= sizeB) ? 0 : 1;
    activeBytes_ = (activeFile_ == 0) ? sizeA : sizeB;

    queue_ = xQueueCreate(kQueueDepth, sizeof(LogEntry));
    if (!queue_) {
        log.warn("failed to allocate log queue — persistent logging disabled");
        mounted_ = false;
        return false;
    }

    BaseType_t ok = xTaskCreate(&PersistentLogSink::workerTaskTrampoline, "persist_log",
                                 kWorkerStackBytes, this, kWorkerPriority, &workerTask_);
    if (ok != pdPASS) {
        log.warn("failed to start persist-log worker task — persistent logging disabled");
        vQueueDelete(queue_);
        queue_ = nullptr;
        mounted_ = false;
        return false;
    }

    mounted_ = true;
    log.info("LittleFS mounted at %s, active log %s (%u bytes)",
              kMountPoint, kFileNames[activeFile_], (unsigned)activeBytes_);
    return true;
}

logger::LogLevel PersistentLogSink::levelForTag(std::string_view tag) const {
    auto it = tagLevels_.find(std::string(tag));
    if (it != tagLevels_.end()) return it->second;
    return defaultLevel_;
}

void PersistentLogSink::write(logger::LogLevel level, std::string_view tag,
                               std::string_view message) {
    if (!mounted_ || !queue_) return;
    if (level < levelForTag(tag)) return;

    // Only ever touch a small fixed-size struct here — never the
    // filesystem. The worker task owns all file I/O so that callers
    // (including thin-stack system tasks) never pay for it on their own
    // stack and are never blocked by it.
    LogEntry entry{};
    entry.timestampMs = esp_timer_get_time() / 1000;
    entry.level = level;

    size_t tlen = std::min(tag.size(), kMaxTagLen);
    std::memcpy(entry.tag, tag.data(), tlen);
    entry.tag[tlen] = '\0';

    size_t mlen = std::min(message.size(), kMaxMessageLen);
    std::memcpy(entry.message, message.data(), mlen);
    entry.message[mlen] = '\0';

    // Never block the caller; drop silently if the worker is backed up.
    // (Must not log here — that would re-enter the logging system.)
    xQueueSend(queue_, &entry, 0);
}

void PersistentLogSink::workerTaskTrampoline(void* arg) {
    static_cast<PersistentLogSink*>(arg)->runWorker();
}

void PersistentLogSink::runWorker() {
    LogEntry entry;
    while (true) {
        if (xQueueReceive(queue_, &entry, portMAX_DELAY) == pdTRUE) {
            persistEntry(entry);
        }
    }
}

void PersistentLogSink::rotateIfNeeded() {
    if (activeBytes_ < kMaxFileBytes) return;
    activeFile_ = 1 - activeFile_;
    activeBytes_ = 0;
    FILE* f = fopen(kFileNames[activeFile_], "w");  // truncate
    if (f) fclose(f);
}

void PersistentLogSink::persistEntry(const LogEntry& entry) {
    rotateIfNeeded();

    FILE* f = fopen(kFileNames[activeFile_], "a");
    if (!f) return;
    int written = fprintf(f, "[%lld] [%s] %s\n",
                           static_cast<long long>(entry.timestampMs),
                           entry.tag, entry.message);
    fclose(f);
    if (written > 0) activeBytes_ += static_cast<size_t>(written);
}

common::Result PersistentLogSink::streamFile(int fileIdx, const ChunkSink& sink) const {
    FILE* f = fopen(kFileNames[fileIdx], "r");
    if (!f) return common::Result::NotFound;

    char buf[kReadChunkBytes];
    common::Result result = common::Result::Ok;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        result = sink(std::string_view(buf, n));
        if (result != common::Result::Ok) break;
    }
    if (ferror(f)) result = common::Result::InternalError;
    fclose(f);
    return result;
}

common::Result PersistentLogSink::streamFileTail(int fileIdx, size_t maxBytes,
                                                  const ChunkSink& sink) const {
    FILE* f = fopen(kFileNames[fileIdx], "r");
    if (!f) return common::Result::NotFound;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return common::Result::InternalError;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return common::Result::InternalError;
    }

    long start = (static_cast<size_t>(size) > maxBytes)
                     ? (size - static_cast<long>(maxBytes))
                     : 0;
    if (fseek(f, start, SEEK_SET) != 0) {
        fclose(f);
        return common::Result::InternalError;
    }

    char buf[kReadChunkBytes];
    common::Result result = common::Result::Ok;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        result = sink(std::string_view(buf, n));
        if (result != common::Result::Ok) break;
    }
    if (ferror(f)) result = common::Result::InternalError;
    fclose(f);
    return result;
}

void PersistentLogSink::flush() {
    if (!mounted_ || !queue_) return;

    for (uint32_t waited = 0; waited < kFlushTimeoutMs; waited += kFlushPollIntervalMs) {
        if (uxQueueMessagesWaiting(queue_) == 0) return;
        vTaskDelay(pdMS_TO_TICKS(kFlushPollIntervalMs));
    }
    // Timed out — best-effort only, nothing more useful to do from here
    // (see the flush() doc comment). Deliberately not logging: we're
    // already in the path of a caller trying to guarantee a log line
    // survives, logging a failure here would just queue another entry
    // behind the ones that didn't drain in time.
}

common::Result PersistentLogSink::streamActive(const ChunkSink& sink) const {
    if (!mounted_) return common::Result::NotFound;
    return streamFile(activeFile_, sink);
}

common::Result PersistentLogSink::streamRecent(size_t maxBytes, const ChunkSink& sink) const {
    if (!mounted_) return common::Result::NotFound;

    struct stat st{};
    size_t activeSize = (stat(kFileNames[activeFile_], &st) == 0)
                             ? static_cast<size_t>(st.st_size)
                             : 0;

    // If the active file alone already covers the requested budget, its own
    // tail is the whole answer.
    if (activeSize >= maxBytes) {
        return streamFileTail(activeFile_, maxBytes, sink);
    }

    // Otherwise pull the remainder from the tail of the inactive (older)
    // file first — so the combined output stays in chronological order —
    // then the whole active file. A missing/short inactive file just means
    // less history exists than was asked for; that's fine, not an error.
    size_t remaining = maxBytes - activeSize;
    const int inactiveFile = 1 - activeFile_;
    common::Result r = streamFileTail(inactiveFile, remaining, sink);
    if (r != common::Result::Ok && r != common::Result::NotFound) return r;

    return streamFile(activeFile_, sink);
}

common::Result PersistentLogSink::streamAll(const ChunkSink& sink) const {
    if (!mounted_) return common::Result::NotFound;

    // Inactive file first (older history), then active file (current run),
    // giving a complete chronological view across reboots and rotations.
    // A missing individual file (e.g. never rotated to yet) is tolerated —
    // only a genuine read fault on a file that does exist is an error.
    const int inactiveFile = 1 - activeFile_;
    bool anyFound = false;
    for (int idx : {inactiveFile, activeFile_}) {
        common::Result r = streamFile(idx, sink);
        if (r == common::Result::NotFound) continue;   // file doesn't exist yet
        anyFound = true;
        if (r != common::Result::Ok) return r;          // real error or early stop
    }
    return anyFound ? common::Result::Ok : common::Result::NotFound;
}

} // namespace persistent_log
