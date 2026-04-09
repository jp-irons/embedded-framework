#pragma once
#include <string_view>
#include <cstdarg>
#include <cstdio>
#include "LogLevel.hpp"
#include "LogSinkRegistry.hpp"

namespace logger {

class Logger {
public:
    explicit Logger(std::string_view tag) : tag_(tag) {}

    void debug(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        writeFormatted(LogLevel::Debug, fmt, args);
        va_end(args);
    }

    void info(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        writeFormatted(LogLevel::Info, fmt, args);
        va_end(args);
    }

    void warn(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        writeFormatted(LogLevel::Warn, fmt, args);
        va_end(args);
    }

    void error(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        writeFormatted(LogLevel::Error, fmt, args);
        va_end(args);
    }

private:
    void writeFormatted(LogLevel level, const char* fmt, va_list args) {
        auto* sink = LogSinkRegistry::sink();
        if (!sink) return;

        LogLevel minLevel = LogSinkRegistry::levelForTag(tag_);
        if (level < minLevel) return;

        char buffer[256];
        int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
        if (len > 0) {
            sink->write(level, tag_, std::string_view(buffer, len));
        }
    }

    std::string_view tag_;
};

} // namespace logger