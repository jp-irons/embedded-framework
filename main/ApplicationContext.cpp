#include "ApplicationContext.hpp"

#include "http_types/HttpTypes.hpp"
#include "logger/Logger.hpp"

static logger::Logger log{ApplicationContext::TAG};

ApplicationContext::ApplicationContext(framework::FrameworkContext& fw)
    : fw_(fw)
    , appFileTable_()
    , appFileHandler_("", "index.html", appFileTable_)
    , temperatureHandler_(fw.getDevice()) {
    log.debug("constructor");
}

ApplicationContext::~ApplicationContext() {
    log.info("destructor");
}

void ApplicationContext::start() {
    log.debug("start");

    // ── Register app static-file handler ──────────────────────────────────
    // Mounted at "/" so all paths are looked up verbatim in the app file table.
    // Framework routes and the framework file handler are still tried first
    // (or as fallback) — the app handler returns NotFound for anything not in
    // its table, allowing requests to fall through.
    fw_.addFileHandler("/", &appFileHandler_);

    // ── Set the entry point ────────────────────────────────────────────────
    // Visiting the root URL (/) will redirect here.  Remove or change this
    // line to fall back to the framework's own management UI (/framework/ui/).
    fw_.setEntryPoint("/app/ui/");

    // ── Register app API routes ────────────────────────────────────────────
    fw_.addRoute(http::HttpMethod::Get, "/app/api/temperature", &temperatureHandler_);

    // ── Configure pull-based OTA ──────────────────────────────────────────
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
    // uiSettable         — When true, the firmware UI exposes an enable/disable
    //                      toggle and the POST /firmware/autoUpdate API is
    //                      accepted; the user's choice survives reboots via NVS.
    //                      When false, autoUpdateEnabled is always authoritative
    //                      and the toggle is hidden.
    fw_.setOtaPullConfig({
        .baseUrl           = "https://github.com/jp-irons/embedded-framework/releases/latest/download",
        .checkIntervalS    = 3600,
        .autoUpdateEnabled = false,
        .uiSettable        = true,
    });

    // TODO: FrameworkContext should expose configuration for:
    //
    //   Hostname (mDNS / device identity)
    //     - Fixed string               e.g. setHostname("my-van")
    //     - String + MAC suffix        e.g. setHostname("van", HostnameSuffix::Mac)
    //     - Runtime-configurable       persisted to NVS; changeable via UI / API
    //       (follows the same pattern as auto-update: app sets default + uiSettable)
    //
    //   Wi-Fi AP name
    //     - Fixed string               e.g. apConfig.ssid = "VanMonitor"
    //     - String + MAC suffix        e.g. setApSsid("VanMonitor", SsidSuffix::Mac)
    //       (useful when deploying multiple units — avoids SSID collisions)
    //
    //   Both hostname and AP SSID suffix options should share a common
    //   SuffixPolicy enum (None | MacFull | MacShort) to keep the API
    //   consistent across settings.

    // ── Start the framework (WiFi, server, OTA, …) ────────────────────────
    fw_.start();
}

void ApplicationContext::loop() {
    // Optional per-tick work.  The main loop calls this every 50 ms.
}
