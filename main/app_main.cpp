#include "ApplicationContext.hpp"
#include "logger/EspIdfLogSink.hpp"
#include "logger/LogSinkRegistry.hpp"
#include "logger/Logger.hpp"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

using namespace logger;

extern "C" void setupLogging();
static Logger log{"app_main"};

extern "C" void app_main(void) {
    // Logging
    setupLogging();
	Logger log{"app_main"};

    // Create the application context (owns everything)
    log.info("bringing system up");
    log.debug("creating fw context");
	
	// default constructor with default apConfig and rootUri
	// can also take 
	//
	//	wifi_manager::ApConfig apConfig = {
	//	    .ssid = "ESP32 FW Test", .password = "password", .channel = 1, .maxConnections = 4};
	//	std::string rootUri = "/framework/api";
	//
	// framework::FrameworkContext fw{apConfig, rootUri};
	//
	framework::FrameworkContext fw{};

	ApplicationContext app{fw};
	app.start();
    app.start();
    log.info("System initialised");

    // Main loop
    while (true) {
        app.loop();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

extern "C" void setupLogging() {
    static EspIdfLogSink uartSink;
    LogSinkRegistry::setSink(&uartSink);

    // --- 2. Configure filtering ---
    LogSinkRegistry::setDefaultLevel(LogLevel::Info);
    LogSinkRegistry::setLevelForTag("app_main", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("ProvisioningServer", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("app_main", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("ApplicationContext", LogLevel::Debug);
    // core_api API Handlers
    LogSinkRegistry::setLevelForTag("CredentialApiHandler", LogLevel::Debug);
	LogSinkRegistry::setLevelForTag("DeviceApiHandler", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("WiFiApiHandler", LogLevel::Debug);
    // credential_store
    LogSinkRegistry::setLevelForTag("CredentialStore", LogLevel::Debug);
    // framework
    LogSinkRegistry::setLevelForTag("FrameworkContext", LogLevel::Debug);
    // http
    LogSinkRegistry::setLevelForTag("HttpServer", LogLevel::Debug);
    // wifi_manager
    LogSinkRegistry::setLevelForTag("EmbeddedServer", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("WiFiInterface", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("WiFiStateMachine", LogLevel::Debug);
    // static_assets
    LogSinkRegistry::setLevelForTag("ContentType", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("EmbeddedAssetTable", LogLevel::Debug);
    LogSinkRegistry::setLevelForTag("StaticFileHandler", LogLevel::Debug);
	
	log = Logger("app_main");
}
