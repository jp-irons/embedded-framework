#pragma once

#include "common/Result.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace device {

struct DeviceInfo {
    std::string chipModel;
    int         revision;
    std::string mac;
    size_t      flashSize;
    size_t      psramSize;
    size_t      freeHeap;
    size_t      minFreeHeap;
    uint32_t    cpuFreqMhz;
    std::string idfVersion;
    std::string lastReset;
    float       temperature;
    std::string uptime;
    std::string otaPartition;
};

/**
 * Abstract device interface.
 *
 * Only framework types appear on the boundary (Result, DeviceInfo, primitives).
 * No ESP-IDF headers are included here; all ESP-IDF types and includes are
 * confined to EspDeviceInterface.
 *
 * FrameworkContext holds an EspDeviceInterface and exposes it as DeviceInterface&
 * so callers remain portable and unit-testable without real hardware.
 */
class DeviceInterface {
  public:
    virtual ~DeviceInterface() = default;

    virtual common::Result init()            = 0;
    virtual common::Result reboot()          = 0;
    virtual common::Result clearNvs()        = 0;
    virtual DeviceInfo     info()            = 0;
    virtual float          readTemperature() = 0;
};

} // namespace device
