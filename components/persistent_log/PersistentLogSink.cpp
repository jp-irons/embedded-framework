// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#include "persistent_log/PersistentLogSink.hpp"

#include "esp_littlefs.h"
#include "esp_timer.h"
#include "logger/Logger.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
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

    struct stat st{};
    activeFile_ = 0;
    activeBytes_ = (stat(kFileNames[0], &st) == 0) ? static_cast<size_t>(st.st_size) : 0;

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

std::string PersistentLogSink::readActive() const {
    if (!mounted_) return {};
    FILE* f = fopen(kFileNames[activeFile_], "r");
    if (!f) return {};
    std::ostringstream out;
    char buf[256];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.write(buf, n);
    fclose(f);
    return out.str();
}

} // namespace persistent_log
