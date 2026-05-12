#include "esp_platform/EspClockInterface.hpp"

#include "esp_timer.h"

namespace esp_platform {

int64_t EspClockInterface::nowUs() const {
    return esp_timer_get_time();
}

} // namespace esp_platform
