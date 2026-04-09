#pragma once
#include "LogSink.hpp"
#include "LogLevel.hpp"
#include <string>
#include <string_view>
#include <unordered_map>

namespace logger {

class LogSinkRegistry {
public:
    static void setSink(LogSink* sink) { sink_ = sink; }

    static LogSink* sink() { return sink_; }

    static void setLevelForTag(std::string_view tag, LogLevel level) {
        tagLevels_[std::string(tag)] = level;
		if (sink_) {
		    sink_->onTagLevelChanged(tag, level);
		}

    }

    static LogLevel levelForTag(std::string_view tag) {
        auto it = tagLevels_.find(std::string(tag));
        if (it != tagLevels_.end()) {
            return it->second;
        }
        return defaultLevel_;
    }

    static void setDefaultLevel(LogLevel level) {
        defaultLevel_ = level;
    }

private:
    inline static LogSink* sink_ = nullptr;
    inline static std::unordered_map<std::string, LogLevel> tagLevels_;
    inline static LogLevel defaultLevel_ = LogLevel::Info;
};

} // namespace logger