#pragma once
#include "common/Result.hpp"

#include <string>

namespace device {

struct DeviceInfo {
    std::string chipModel;
    int revision;
    std::string mac;
    size_t flashSize;
    size_t freeHeap;
};

common::Result init();
common::Result clearNvs();
common::Result reboot(); // optional
DeviceInfo info(); // struct you define

} // namespace device
