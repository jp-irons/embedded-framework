// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

#include "device/ClockInterface.hpp"

namespace esp_platform {

/**
 * ESP-IDF concrete implementation of ClockInterface.
 *
 * Returns esp_timer_get_time() — microseconds since boot.
 * All ESP-IDF types are confined to EspClockInterface.cpp.
 */
class EspClockInterface : public device::ClockInterface {
  public:
    int64_t nowUs() const override;
};

} // namespace esp_platform
