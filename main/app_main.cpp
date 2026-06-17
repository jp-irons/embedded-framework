#include "app/ApplicationContext.hpp"
#include "LoggingConfig.hpp"
#include "logger/Logger.hpp"
#include "ota/OtaManager.hpp"

extern "C" {
#include "freertos/FreeRTOS.h"	// IWYU pragma: keep — must precede task.h
#include "freertos/task.h"
}

using namespace logger;

// TODO display Wi-Fi AP connected to + strength maybe on Device page?

static Logger log{"app_main"};

// ── setupFramework ────────────────────────────────────────────────────────────
// Constructs and configures the FrameworkContext: who this device is, how it
// authenticates, and how it updates.  Returns the configured context to app_main.
//
// ── FrameworkContext constructors ─────────────────────────────────────────────
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
// ── AuthConfig options ────────────────────────────────────────────────────────
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
//   e.g.    framework::FrameworkContext fw{
//                auth::AuthConfig::fromMac("myapp")
//                    .restrictIfDefault()
//                    .requireChangeOnFirstBoot()
//            };
//
static framework::FrameworkContext setupFramework() {
//  framework::FrameworkContext fw{auth::AuthConfig::none()};
    framework::FrameworkContext fw{auth::AuthConfig::withPassword("espframework")};

    // ── Device identity ───────────────────────────────────────────────────
    // By default both setters append the last 3 MAC bytes (MacShort) to the
    // supplied prefix, e.g. "esp-fw-a1b2c3" / "EspFramework-a1b2c3".
    // This ensures uniqueness when multiple units share a location.
    //
    // To suppress the suffix pass wifi_manager::SuffixPolicy::None, or to use
    // all 6 MAC bytes pass wifi_manager::SuffixPolicy::MacFull — both require
    // #include "wifi_manager/WiFiTypes.hpp".
    fw.setHostnameConfig("esp-fw");
    fw.setApSsidConfig("EspFramework");
    fw.setApPassword("espframework");

    // ── Pull-based OTA ────────────────────────────────────────────────────
    // baseUrl            — GitHub Releases download directory for this repo.
    //                      OtaPuller appends "/version.txt" (checked first)
    //                      and "/firmware.bin" (downloaded only if newer).
    // checkIntervalS     — Seconds between background checks; 0 disables the
    //                      periodic task (manual / MQTT-triggered checks still
    //                      work via the firmware UI or checkNow()).
    // autoUpdateEnabled  — Default auto-update state.  true = checks run
    //                      automatically; false = disabled until toggled on.
    //                      When uiSettable=true, a user-persisted NVS value
    //                      overrides this default after the first toggle.
    //                      Has no effect if checkIntervalS is 0 — no
    //                      background task runs to honour the setting.
    // uiSettable         — When true, the firmware UI exposes an enable/disable
    //                      toggle and the POST /firmware/autoUpdate API is
    //                      accepted; the user's choice survives reboots via NVS.
    //                      When false, autoUpdateEnabled is always authoritative
    //                      and the toggle is hidden.
    fw.setOtaPullConfig({
        .baseUrl           = "https://github.com/jp-irons/embedded-framework/releases/latest/download",
        .checkIntervalS    = 3600,
        .autoUpdateEnabled = false,
        .uiSettable        = true,
    });

    return fw;
}

extern "C" void app_main(void) {
    // ── OTA boot guardian ─────────────────────────────────────────────────
    // Must be called before any tasks are started so that, if this image
    // has exceeded its boot-attempt budget, we restart before doing anything.
    ota::OtaManager::checkOnBoot();

    // Logging
    setupLogging();

    log.info("bringing system up");

    framework::FrameworkContext fw = setupFramework();
    app::ApplicationContext appCtx{fw};
    appCtx.start();
    log.info("System initialised");

    // Main loop
    while (true) {
        appCtx.loop();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
