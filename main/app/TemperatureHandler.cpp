#include "TemperatureHandler.hpp"

#include "device/DeviceInterface.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "logger/Logger.hpp"

#include <cstdio>

static logger::Logger log{"TemperatureHandler"};

// Sensor lifecycle is owned by device::readTemperature() in DeviceInterface.
// Having two owners caused "Already installed" errors from the ESP-IDF driver.
TemperatureHandler::TemperatureHandler() = default;
TemperatureHandler::~TemperatureHandler() = default;

common::Result TemperatureHandler::handle(http::HttpRequest &req, http::HttpResponse &res) {
    float celsius = device::readTemperature();

    char body[32];
    snprintf(body, sizeof(body), "{\"celsius\":%.1f}", celsius);
    log.debug("temperature %.1f °C", celsius);

    return res.sendJson(body);
}
