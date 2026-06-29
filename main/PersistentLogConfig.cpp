#include "PersistentLogConfig.hpp"

#include "esp_platform/EspWiFiInterface.hpp"
#include "logger/LogSinkRegistry.hpp"
#include "wifi_manager/WiFiManager.hpp"

// What gets persisted to LittleFS, surviving reboots and connectivity
// outages. Independent of LogSinkRegistry's per-tag levels in LoggingConfig.cpp,
// EXCEPT that the registry level is a shared ceiling applied before any sink
// (UART or persistent) ever sees a message — so raising a tag here to Debug
// only takes effect if the registry ceiling for that tag is also Debug.
//
// To persist a new category later, add the matching pair of calls below.
void configurePersistentLog(persistent_log::PersistentLogSink& sink) {
    using logger::LogLevel;
    using logger::LogSinkRegistry;

    // Example: persist WiFi-stack activity at Debug.
    LogSinkRegistry::setLevelForTag(wifi_manager::WiFiManager::TAG, LogLevel::Debug);
    LogSinkRegistry::setLevelForTag(esp_platform::EspWiFiInterface::TAG, LogLevel::Debug);
    sink.setTagLevel(wifi_manager::WiFiManager::TAG, LogLevel::Debug);
    sink.setTagLevel(esp_platform::EspWiFiInterface::TAG, LogLevel::Debug);
}
