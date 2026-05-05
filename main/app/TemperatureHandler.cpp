#include "TemperatureHandler.hpp"

#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "logger/Logger.hpp"

#include <cstdio>

static logger::Logger log{"TemperatureHandler"};

TemperatureHandler::TemperatureHandler() {
    // Range covers typical ambient + self-heating for a running ESP32-S3.
    // Adjust the bounds if you need the sensor calibrated for a different range.
    temperature_sensor_config_t config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);

    esp_err_t err = temperature_sensor_install(&config, &sensor_);
    if (err != ESP_OK) {
        log.warn("temperature_sensor_install failed: %s", esp_err_to_name(err));
        sensor_ = nullptr;
        return;
    }

    err = temperature_sensor_enable(sensor_);
    if (err != ESP_OK) {
        log.warn("temperature_sensor_enable failed: %s", esp_err_to_name(err));
        temperature_sensor_uninstall(sensor_);
        sensor_ = nullptr;
    }
}

TemperatureHandler::~TemperatureHandler() {
    if (sensor_) {
        temperature_sensor_disable(sensor_);
        temperature_sensor_uninstall(sensor_);
    }
}

common::Result TemperatureHandler::handle(http::HttpRequest &req, http::HttpResponse &res) {
    if (!sensor_) {
        return res.sendJsonError(503, "Temperature sensor not available");
    }

    float celsius = 0.0f;
    esp_err_t err = temperature_sensor_get_celsius(sensor_, &celsius);
    if (err != ESP_OK) {
        log.warn("temperature_sensor_get_celsius failed: %s", esp_err_to_name(err));
        return res.sendJsonError(500, "Failed to read temperature sensor");
    }

    char body[32];
    snprintf(body, sizeof(body), "{\"celsius\":%.1f}", celsius);
    log.debug("temperature %.1f °C", celsius);

    return res.sendJson(body);
}
