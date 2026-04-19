#pragma once

#include "common/Result.hpp"

namespace device {
	
class DeviceService {
public:
    explicit DeviceService();
    virtual ~DeviceService() = default;
	common::Result clearNvs();

private:
	
};
} // namespace