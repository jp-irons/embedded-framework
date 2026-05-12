#include "TemperatureHandler.hpp"

#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "logger/Logger.hpp"

#include <cstdio>

static logger::Logger log{"TemperatureHandler"};

// Sensor lifecycle is owned by EspDeviceInterface::readTemperature().
// Having two owners caused "Already installed" errors from the ESP-IDF driver.
TemperatureHandler::TemperatureHandler(device::DeviceInterface& device)
    : device_(device) {}

TemperatureHandler::~TemperatureHandler() = default;

common::Result TemperatureHandler::handle(http::HttpRequest& /*req*/,
                                           http::HttpResponse& res) {
    float celsius = device_.readTemperature();

    char body[32];
    snprintf(body, sizeof(body), "{\"celsius\":%.1f}", celsius);
    log.debug("temperature %.1f °C", celsius);

    return res.sendJson(body);
}
