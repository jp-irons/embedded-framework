#pragma once

#include "framework/FrameworkContext.hpp"

class ApplicationContext {
  public:
    ApplicationContext();
    ~ApplicationContext() = default;

    void start();
    void loop(); // optional, if your app_main uses it
    const wifi_manager::ApConfig &getApConfig() const {
        return apConfig;
    }

  private:
    wifi_manager::ApConfig apConfig = {
		.ssid = "ESP32 FW Test", 
		.password = "esp32", 
		.channel = 1, 
		.maxConnections = 4
	};
    framework::FrameworkContext framework;
};