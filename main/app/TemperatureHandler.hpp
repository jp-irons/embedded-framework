#pragma once

#include "common/Result.hpp"
#include "http/HttpHandler.hpp"

#include "driver/temperature_sensor.h"

/**
 * Reads the ESP32-S3 internal temperature sensor and returns a JSON response:
 *   {"celsius": 42.5}
 *
 * The sensor is installed and enabled once in the constructor and released in
 * the destructor.  Register via:
 *   fw_.addRoute(http::HttpMethod::Get, "/app/api/temperature", &tempHandler_);
 */
class TemperatureHandler : public http::HttpHandler {
  public:
    TemperatureHandler();
    ~TemperatureHandler();

    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    temperature_sensor_handle_t sensor_ = nullptr;
};
