#include "ApplicationContext.hpp"

#include "http_types/HttpTypes.hpp"
#include "logger/Logger.hpp"
#include "ota/OtaPuller.hpp"

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
    fw_.setOtaPullConfig({
        .baseUrl        = "https://github.com/jp-irons/embedded-framework/releases/latest/download",
        .checkIntervalS = 3600,
    });

    // ── Start the framework (WiFi, server, OTA, …) ────────────────────────
    fw_.start();
}

void ApplicationContext::loop() {
    // Optional per-tick work.  The main loop calls this every 50 ms.
}
