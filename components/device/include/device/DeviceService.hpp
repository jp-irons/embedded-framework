#pragma once

#include "common/Result.hpp"

namespace device {
	
common::Result init();

class DeviceService {
public:
    explicit DeviceService();
    virtual ~DeviceService() = default;
	common::Result clearNvs();

private:
	
};
} // namespace