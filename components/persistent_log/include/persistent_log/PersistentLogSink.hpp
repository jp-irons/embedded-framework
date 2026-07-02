// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Result.hpp"
#include "logger/LogSink.hpp"
#include "logger/LogLevel.hpp"

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep — must precede queue.h/task.h
#include "freertos/queue.h"
#include "freertos/task.h"

#include <cstddef>
#include <cstdint>
#include <functional>
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
    static constexpr size_t kReadChunkBytes = 512;

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

    /** Callback invoked with successive chunks of file content. Return
     *  Result::Ok to keep going; anything else aborts the read early and
     *  that Result is propagated back out of streamActive(). */
    using ChunkSink = std::function<common::Result(std::string_view)>;

    /** Streams the currently-active log file out via `sink`, one bounded
     *  chunk (kReadChunkBytes) at a time, without ever holding the whole
     *  file in memory. Returns Result::Ok on a clean full read, NotFound
     *  if not mounted / file missing, whatever `sink` returned if it
     *  stopped the read early, or InternalError on a read fault.
     *  May race with an in-progress worker write/rotation (best-effort
     *  snapshot, not a hard guarantee). */
    common::Result streamActive(const ChunkSink& sink) const;

    /** Streams both log files out via `sink`: inactive file first (older
     *  history), then active file (current run) — a complete chronological
     *  view across reboots and rotations, each file read in bounded chunks
     *  with no full-file buffer ever held in memory. Returns Result::Ok if
     *  at least one file was read, NotFound if not mounted or neither file
     *  exists, whatever `sink` returned if it stopped the read early, or
     *  InternalError on a read fault. Same best-effort / no-lock caveat as
     *  streamActive(). */
    common::Result streamAll(const ChunkSink& sink) const;

  private:
    struct LogEntry {
        int64_t timestampMs;
        logger::LogLevel level;
        char tag[kMaxTagLen + 1];
        char message[kMaxMessageLen + 1];
    };

    logger::LogLevel levelForTag(std::string_view tag) const;

    // Worker-task-only state (and streamActive()/streamAll(), best-effort).
    void rotateIfNeeded();
    void persistEntry(const LogEntry& entry);
    void runWorker();
    static void workerTaskTrampoline(void* arg);

    /** Reads kFileNames[fileIdx] in bounded chunks via `sink`. NotFound if
     *  the file can't be opened (e.g. never rotated to yet). */
    common::Result streamFile(int fileIdx, const ChunkSink& sink) const;

    bool mounted_ = false;
    int activeFile_ = 0;        // 0 = log_a.txt, 1 = log_b.txt
    size_t activeBytes_ = 0;

    QueueHandle_t queue_ = nullptr;
    TaskHandle_t workerTask_ = nullptr;

    logger::LogLevel defaultLevel_ = logger::LogLevel::Warn;
    std::unordered_map<std::string, logger::LogLevel> tagLevels_;
};

} // namespace persistent_log
