#include "ApplicationContext.hpp"
#include "logger/EspIdfLogSink.hpp"
#include "logger/LogSinkRegistry.hpp"
#include "logger/Logger.hpp"
#include "ota/OtaManager.hpp"

extern "C" {
//#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

using namespace logger;

extern "C" void setupLogging();
static Logger log{"app_main"};

extern "C" void app_main(void) {
    // ── OTA boot guardian ─────────────────────────────────────────────────
    // Must be called before any tasks are started so that, if this image
    // has exceeded its boot-attempt budget, we restart before doing anything.
    ota::OtaManager::checkOnBoot();

    // Logging
    setupLogging();
	Logger log{"app_main"};

    // Create the application context (owns everything)
    log.info("bringing system up");
    log.debug("creating fw context");
	
	// Default constructor uses built-in AP config and rootUri "/framework".
	// API endpoints are at  <rootUri>/api/*  e.g. /framework/api/wifi
	// UI assets are at      <rootUri>/ui/*   e.g. /framework/ui/index.html
	//
	// To customise, pass an ApConfig and optional rootUri:
	//
	//	wifi_manager::ApConfig apConfig = {
	//	    .ssid = "ESP32 FW Test", .password = "password", .channel = 1, .maxConnections = 4};
	//
	// framework::FrameworkContext fw{apConfig, auth::AuthConfig::withPassword("mypass")};
	//
	framework::FrameworkContext fw{};

	ApplicationContext app{fw};
	app.start();
    log.info("System initialised");

    // Main loop
    while (true) {
        app.loop();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

extern "C" void setupLogging() {
	
	// TODO refactor logging TAG settings.
    static EspIdfLogSink uartSink;
    LogSinkRegistry::setSink(&uartSink);

    // --- 2. Configure filtering ---
    LogSinkRegistry::setDefaultLevel(LogLevel::Info);
    LogSinkRegistry::setLevelForTag("app_main", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("ApplicationContext", LogLevel::Debug);
    // core_api API Handlers
	LogSinkRegistry::setLevelForTag("AuthApiHandler", LogLevel::Debug);
	LogSinkRegistry::setLevelForTag("CredentialApiHandler", LogLevel::Debug);
	LogSinkRegistry::setLevelForTag("DeviceApiHandler", LogLevel::Debug);
	LogSinkRegistry::setLevelForTag("OtaApiHandler", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("WiFiApiHandler", LogLevel::Debug);
	// Device tier
	LogSinkRegistry::setLevelForTag("DeviceInterface", LogLevel::Debug);
    // credential_store
    LogSinkRegistry::setLevelForTag("CredentialStore", LogLevel::Debug);
    // framework
    LogSinkRegistry::setLevelForTag("FrameworkContext", LogLevel::Debug);
    // http
    LogSinkRegistry::setLevelForTag("HttpServer", LogLevel::Debug);
    // wifi_manager
    LogSinkRegistry::setLevelForTag("EmbeddedServer", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("WiFiInterface", LogLevel::Debug);
	LogSinkRegistry::setLevelForTag("WiFiManager", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("WiFiStateMachine", LogLevel::Debug);
    // embedded_files
    LogSinkRegistry::setLevelForTag("EmbeddedFileHandler", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("EmbeddedFileTable",   LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("AppFileTable",        LogLevel::Debug);
	
	log = Logger("app_main");
}
