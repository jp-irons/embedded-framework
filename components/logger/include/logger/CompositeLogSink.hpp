// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

#include "logger/LogSink.hpp"

#include <array>

namespace logger {

/**
 * Fans a single log line out to multiple sinks.
 *
 * Sink slots are nullable — addSink() can be skipped for sinks that aren't
 * configured (e.g. persistent logging disabled), and write()/onTagLevelChanged()
 * silently skip null slots.
 */
class CompositeLogSink : public LogSink {
  public:
    static constexpr size_t kMaxSinks = 4;

    /** Register a sink. No-op if the slot table is full or sink is null. */
    void addSink(LogSink* sink) {
        if (!sink) return;
        for (auto& slot : sinks_) {
            if (slot == nullptr) {
                slot = sink;
                return;
            }
        }
    }

    void write(LogLevel level, std::string_view tag, std::string_view message) override {
        for (auto* sink : sinks_) {
            if (sink) sink->write(level, tag, message);
        }
    }

    void onTagLevelChanged(std::string_view tag, LogLevel level) override {
        for (auto* sink : sinks_) {
            if (sink) sink->onTagLevelChanged(tag, level);
        }
    }

  private:
    std::array<LogSink*, kMaxSinks> sinks_{};
};

} // namespace logger
