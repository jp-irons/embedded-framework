#include "ApplicationContext.hpp"

#include "esp_log.h"

static const char *TAG = "ApplicationContext";

ApplicationContext::ApplicationContext()
    : framework(apConfig) {
    ESP_LOGD(TAG, "constructor");
}

void ApplicationContext::start() {
    ESP_LOGD(TAG, "start");
    framework.start();
}

void ApplicationContext::loop() {
    // Optional: forward to WiFiManager loop if needed
    // framework.loop();  // if you add one later
}