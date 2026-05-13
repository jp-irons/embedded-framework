#include "ApplicationContext.hpp"
#include "LoggingConfig.hpp"
#include "logger/Logger.hpp"
#include "ota/OtaManager.hpp"

extern "C" {
#include "freertos/FreeRTOS.h"	// IWYU pragma: keep — must precede task.h
#include "freertos/task.h"
}

using namespace logger;

static Logger log{"app_main"};

extern "C" void app_main(void) {
    // ── OTA boot guardian ─────────────────────────────────────────────────
    // Must be called before any tasks are started so that, if this image
    // has exceeded its boot-attempt budget, we restart before doing anything.
    ota::OtaManager::checkOnBoot();

    // Logging
    setupLogging();

    // Create the application context (owns everything)
    log.info("bringing system up");
    log.debug("creating fw context");
	
	// Default constructor uses built-in AP config and rootUri "/framework".
	// API endpoints are at  <rootUri>/api/*  e.g. /framework/api/wifi
	// UI assets are at      <rootUri>/ui/*     e.g. /framework/ui/index.html
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
