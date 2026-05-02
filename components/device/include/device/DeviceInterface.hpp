#pragma once
#include "common/Result.hpp"

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

common::Result init();
common::Result clearNvs();
common::Result reboot(); // optional
DeviceInfo info(); // struct you define

} // namespace device
