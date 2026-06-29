#include "LoggingConfig.hpp"

#include "AppFileTable.hpp"
#include "ApplicationContext.hpp"
#include "PersistentLogConfig.hpp"
#include "auth/AuthApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "esp_platform/EspDeviceInterface.hpp"
#include "esp_platform/EspHttpServer.hpp"
#include "esp_platform/EspWiFiInterface.hpp"
#include "framework/FrameworkContext.hpp"
#include "framework_files/EmbeddedFileHandler.hpp"
#include "framework_files/EmbeddedFileTable.hpp"
#include "logger/CompositeLogSink.hpp"
#include "logger/EspIdfLogSink.hpp"
#include "logger/LogSinkRegistry.hpp"
#include "network_store/NetworkApiHandler.hpp"
#include "network_store/NetworkStore.hpp"
#include "ota/OtaApiHandler.hpp"
#include "persistent_log/PersistentLogSink.hpp"
#include "wifi_manager/EmbeddedServer.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiManager.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"

using namespace logger;

persistent_log::PersistentLogSink& persistentLogSink() {
    static persistent_log::PersistentLogSink sink;
    return sink;
}

void setupLogging() {
    static EspIdfLogSink uartSink;
    static CompositeLogSink composite;

    persistent_log::PersistentLogSink& persistSink = persistentLogSink();
    persistSink.mount();
    composite.addSink(&uartSink);
    composite.addSink(&persistSink);
    LogSinkRegistry::setSink(&composite);

    LogSinkRegistry::setDefaultLevel(LogLevel::Info);
    LogSinkRegistry::setLevelForTag("app_main", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag(app::ApplicationContext::TAG, LogLevel::Debug);
    // API handlers
    LogSinkRegistry::setLevelForTag(auth::AuthApiHandler::TAG, LogLevel::Debug);
    LogSinkRegistry::setLevelForTag(device::DeviceApiHandler::TAG, LogLevel::Debug);
    LogSinkRegistry::setLevelForTag(network_store::NetworkApiHandler::TAG, LogLevel::Debug);
    LogSinkRegistry::setLevelForTag(ota::OtaApiHandler::TAG, LogLevel::Debug);
    LogSinkRegistry::setLevelForTag(wifi_manager::WiFiApiHandler::TAG, LogLevel::Debug);
    // Device tier
    LogSinkRegistry::setLevelForTag(esp_platform::EspDeviceInterface::TAG, LogLevel::Debug);
    // network_store
    LogSinkRegistry::setLevelForTag(network_store::NetworkStore::TAG, LogLevel::Debug);
    // framework
    LogSinkRegistry::setLevelForTag(framework::FrameworkContext::TAG, LogLevel::Debug);
    // http
    LogSinkRegistry::setLevelForTag(esp_platform::EspHttpServer::TAG, LogLevel::Debug);
    // wifi_manager
    LogSinkRegistry::setLevelForTag(wifi_manager::EmbeddedServer::TAG, LogLevel::Debug);
    LogSinkRegistry::setLevelForTag(esp_platform::EspWiFiInterface::TAG, LogLevel::Debug);
    LogSinkRegistry::setLevelForTag(wifi_manager::WiFiManager::TAG, LogLevel::Debug);
    LogSinkRegistry::setLevelForTag(wifi_manager::WiFiStateMachine::TAG, LogLevel::Debug);
    // framework_files
    LogSinkRegistry::setLevelForTag(framework_files::EmbeddedFileHandler::TAG, LogLevel::Debug);
    LogSinkRegistry::setLevelForTag(framework_files::EmbeddedFileTable::TAG, LogLevel::Debug);
    LogSinkRegistry::setLevelForTag(AppFileTable::TAG, LogLevel::Debug);

    configurePersistentLog(persistSink);
}
