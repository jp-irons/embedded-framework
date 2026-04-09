#include "ApplicationContext.hpp"

#include "logger/Logger.hpp"

static logger::Logger log{"ApplicationContext"};

ApplicationContext::ApplicationContext()
    : framework(apConfig) {
    log.debug("constructor");
}

void ApplicationContext::start() {
    log.debug("start");
    framework.start();
}

void ApplicationContext::loop() {
    // Optional: forward to WiFiManager or Framework loop if needed
}