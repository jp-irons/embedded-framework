#pragma once
#include "LogSink.hpp"
#include "esp_log.h"
#include "esp_log_level.h"

#include <string>

namespace logger {

class EspIdfLogSink : public LogSink {
  public:
    void write(LogLevel level, std::string_view tag, std::string_view message) override {
        const char *t = tag.data();
        const char *msg = message.data();
        int len = static_cast<int>(message.size());

        switch (level) {
            case LogLevel::Verbose:
                ESP_LOGV(t, "%.*s", len, msg);
                break;

            case LogLevel::Debug:
                ESP_LOGD(t, "%.*s", len, msg);
                break;

            case LogLevel::Info:
                ESP_LOGI(t, "%.*s", len, msg);
                break;

            case LogLevel::Warn:
                ESP_LOGW(t, "%.*s", len, msg);
                break;

            case LogLevel::Error:
                ESP_LOGE(t, "%.*s", len, msg);
                break;
        }
    }

    void onTagLevelChanged(std::string_view tag, LogLevel level) override {
        esp_log_level_t espLevel = ESP_LOG_INFO;

        switch (level) {
            case LogLevel::Error:
                espLevel = ESP_LOG_ERROR;
                break;
            case LogLevel::Warn:
                espLevel = ESP_LOG_WARN;
                break;
            case LogLevel::Info:
                espLevel = ESP_LOG_INFO;
                break;
            case LogLevel::Debug:
                espLevel = ESP_LOG_DEBUG;
                break;
            case LogLevel::Verbose:
                espLevel = ESP_LOG_VERBOSE;
                break;
        }

        // Apply per-tag ceiling inside ESP-IDF
        esp_log_level_set(std::string(tag).c_str(), espLevel);
    }
};

} // namespace logger