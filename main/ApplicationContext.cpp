#include "ApplicationContext.hpp"

#include "http/HttpTypes.hpp"
#include "logger/Logger.hpp"

static logger::Logger log{"ApplicationContext"};

ApplicationContext::ApplicationContext(framework::FrameworkContext &fw)
    : fw_(fw)
    , appFileTable_()
    , appFileHandler_("/app/ui", "index.html", appFileTable_) {
    log.debug("constructor");
}

ApplicationContext::~ApplicationContext() {
    log.info("destructor");
}

void ApplicationContext::start() {
    log.debug("start");

    // ── Register app static-file handler ──────────────────────────────────
    // Requests to /app/ui/* are served from main/files/ embedded at build time.
    fw_.addFileHandler("/app/ui/", &appFileHandler_);

    // ── Set the entry point ────────────────────────────────────────────────
    // Visiting the root URL (/) will redirect here.  Remove or change this
    // line to fall back to the framework's own management UI (/framework/ui/).
    fw_.setEntryPoint("/app/ui/");

    // ── Register app API routes ────────────────────────────────────────────
    fw_.addRoute(http::HttpMethod::Get, "/app/api/temperature", &temperatureHandler_);

    // ── Start the framework (WiFi, server, OTA, …) ────────────────────────
    fw_.start();
}

void ApplicationContext::loop() {
    // Optional per-tick work.  The main loop calls this every 50 ms.
}
