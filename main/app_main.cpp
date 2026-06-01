#include "app/ApplicationContext.hpp"
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
	
	// ── FrameworkContext constructors ─────────────────────────────────────
	//
	// Three forms are available:
	//
	//   framework::FrameworkContext fw{};
	//       Default — built-in AP config, rootUri "/framework", password
	//       "esp32admin" with restrictIfDefault() (non-GET calls blocked until
	//       the password is changed via the Security page).
	//
	//   framework::FrameworkContext fw{authConfig};
	//   framework::FrameworkContext fw{authConfig, "/framework", "esp32"};
	//       Auth-only — built-in AP config, custom AuthConfig.  Use this when
	//       you only need to change the auth policy, e.g.:
	//
	//         framework::FrameworkContext fw{auth::AuthConfig::none()};
	//
	//   framework::FrameworkContext fw{apConfig, authConfig};
	//   framework::FrameworkContext fw{apConfig, authConfig, "/framework", "esp32"};
	//       Full — custom AP config and AuthConfig:
	//
	//         wifi_manager::ApConfig apConfig = {
	//             .ssid = "MyDevice", .password = "password",
	//             .channel = 1, .maxConnections = 4};
	//         framework::FrameworkContext fw{apConfig, auth::AuthConfig::fromMac("myapp")};
	//
	// ── AuthConfig options ────────────────────────────────────────────────
	//
	//   auth::AuthConfig::none()
	//       No authentication — all framework API endpoints are openly
	//       accessible.  Use only on trusted networks or during development.
	//       The Security page will display a notice that auth is disabled.
	//
	//   auth::AuthConfig::withPassword("mypass")
	//       Fixed password — same on every device and every boot.
	//
	//   auth::AuthConfig::fromMac("myapp")
	//       Per-device password derived from the MAC address.  Unique per
	//       unit, stable across reboots, no NVS required.
	//
	//   auth::AuthConfig::generated()
	//       Random password generated on first boot and stored in NVS.
	//       Shown in the AP provisioning UI so the operator can record it.
	//
	// Any option (except none()) accepts chained policy flags:
	//   .restrictIfDefault()        — blocks non-GET calls until password changed
	//   .requireChangeOnFirstBoot() — blocks all calls until password changed
	//   e.g. 	framework::FrameworkContext fw{
	//	    		auth::AuthConfig::fromMac("myapp")
	//	        		.restrictIfDefault()
	//	        		.requireChangeOnFirstBoot()
	//			};
	//
	framework::FrameworkContext fw{auth::AuthConfig::none()};
	//	framework::FrameworkContext fw{};

	app::ApplicationContext appCtx{fw};
	appCtx.start();
    log.info("System initialised");

    // Main loop
    while (true) {
        appCtx.loop();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
