// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

#include "logger/LogSink.hpp"
#include "logger/LogLevel.hpp"

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep — must precede queue.h/task.h
#include "freertos/queue.h"
#include "freertos/task.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace persistent_log {

/**
 * LogSink that persists selected log lines to the assets_fs LittleFS
 * partition, surviving reboots and connectivity outages that would
 * otherwise make the realtime UART log the only record of what happened.
 *
 * Filtering is independent of LogSinkRegistry's per-tag levels (which are a
 * shared ceiling applied upstream of all sinks) — this sink keeps its own
 * tag-level table, defaulting to Warn, so persistence can stay quiet/cheap
 * on flash wear while UART stays verbose, or vice versa.
 *
 * write() is called from arbitrary caller tasks/contexts — including
 * thin-stack system tasks like ESP-IDF's event loop — so it must never
 * perform file I/O directly. Instead it copies the (truncated) tag/message
 * into a fixed-size LogEntry and posts it to a queue; a dedicated worker
 * task with its own generously-sized stack drains the queue and does all
 * fopen/fprintf/LittleFS work. If the queue is full, the entry is dropped
 * silently (never blocks the caller, never logs from within write() — that
 * would re-enter the logging system).
 *
 * The worker writes to one of two rotating files (log_a.txt / log_b.txt).
 * When the active file exceeds kMaxFileBytes, it switches to the other file
 * and truncates it, bounding total on-flash usage to ~2x kMaxFileBytes.
 *
 * If the LittleFS partition fails to mount, write() becomes a no-op — a
 * persistence failure must never block boot or normal operation.
 */
class PersistentLogSink : public logger::LogSink {
  public:
    static constexpr const char* TAG = "PersistentLogSink";
    static constexpr size_t kMaxFileBytes = 700 * 1024;
    static constexpr size_t kMaxTagLen = 32;
    static constexpr size_t kMaxMessageLen = 199;
    static constexpr size_t kQueueDepth = 16;

    /** Mounts the assets_fs partition and starts the worker task. Safe to
     *  call once at startup. Logs a warning and leaves the sink in no-op
     *  mode if the mount fails. */
    bool mount();

    void setDefaultLevel(logger::LogLevel level) { defaultLevel_ = level; }
    void setTagLevel(std::string_view tag, logger::LogLevel level) {
        tagLevels_[std::string(tag)] = level;
    }

    void write(logger::LogLevel level, std::string_view tag,
               std::string_view message) override;

    /** Contents of the currently-active log file (for HTTP retrieval).
     *  Empty string if not mounted. May race with an in-progress worker
     *  write/rotation (best-effort snapshot, not a hard guarantee). */
    std::string readActive() const;

  private:
    struct LogEntry {
        int64_t timestampMs;
        logger::LogLevel level;
        char tag[kMaxTagLen + 1];
        char message[kMaxMessageLen + 1];
    };

    logger::LogLevel levelForTag(std::string_view tag) const;

    // Worker-task-only state (and readActive(), best-effort).
    void rotateIfNeeded();
    void persistEntry(const LogEntry& entry);
    void runWorker();
    static void workerTaskTrampoline(void* arg);

    bool mounted_ = false;
    int activeFile_ = 0;        // 0 = log_a.txt, 1 = log_b.txt
    size_t activeBytes_ = 0;

    QueueHandle_t queue_ = nullptr;
    TaskHandle_t workerTask_ = nullptr;

    logger::LogLevel defaultLevel_ = logger::LogLevel::Warn;
    std::unordered_map<std::string, logger::LogLevel> tagLevels_;
};

} // namespace persistent_log
