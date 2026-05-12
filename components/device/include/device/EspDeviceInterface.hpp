#pragma once

#include "device/DeviceInterface.hpp"

namespace device {

/**
 * ESP-IDF concrete implementation of DeviceInterface.
 *
 * All ESP-IDF includes and types are confined to EspDeviceInterface.cpp.
 * Nothing outside the device component needs to include this file —
 * consumers depend only on DeviceInterface.hpp.
 */
class EspDeviceInterface : public DeviceInterface {
  public:
    common::Result init()            override;
    common::Result reboot()          override;
    common::Result clearNvs()        override;
    DeviceInfo     info()            override;
    float          readTemperature() override;
};

} // namespace device
