#pragma once

#include "device/DeviceInterface.hpp"

namespace esp_platform {

/**
 * ESP-IDF concrete implementation of device::DeviceInterface.
 *
 * All ESP-IDF includes and types are confined to EspDeviceInterface.cpp.
 * Nothing outside the esp_platform component needs to include this file —
 * consumers depend only on DeviceInterface.hpp.
 */
class EspDeviceInterface : public device::DeviceInterface {
  public:
  	static constexpr const char* TAG = "EspDeviceInterface";
    common::Result     init()            override;
    common::Result     reboot()          override;
    common::Result     clearNvs()        override;
    device::DeviceInfo info()            override;
    float              readTemperature() override;
};

} // namespace esp_platform
