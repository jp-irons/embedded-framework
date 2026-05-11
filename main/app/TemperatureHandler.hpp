#pragma once

#include "common/Result.hpp"
#include "http/HttpHandler.hpp"

/**
 * Reads the ESP32-S3 internal temperature sensor and returns a JSON response:
 *   {"celsius": 42.5}
 *
 * Sensor lifecycle is managed by device::readTemperature() (DeviceInterface),
 * which is the single owner of the hardware handle.  Register via:
 *   fw_.addRoute(http::HttpMethod::Get, "/app/api/temperature", &tempHandler_);
 */
class TemperatureHandler : public http::HttpHandler {
  public:
    TemperatureHandler();
    ~TemperatureHandler();

    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;
};
