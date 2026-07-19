// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once
#include "LogLevel.hpp"

#include <string_view>

namespace logger {

class LogSink {
  public:
    virtual ~LogSink() = default;

    virtual void write(LogLevel level, std::string_view tag,
		std::string_view message) = 0;
    virtual void onTagLevelChanged(std::string_view tag, LogLevel level) {}

    /** Blocks (briefly, best-effort) until any buffered/queued log entries
     *  this sink is holding have actually been persisted, rather than just
     *  accepted. Default no-op — most sinks (e.g. UART) write synchronously
     *  already. Sinks with an async write path (e.g. PersistentLogSink,
     *  which queues entries for a background worker task) should override
     *  this so callers about to do something irreversible (like
     *  esp_restart()) can be sure a just-written log line actually made it
     *  to durable storage first. */
    virtual void flush() {}
};

} // namespace logger