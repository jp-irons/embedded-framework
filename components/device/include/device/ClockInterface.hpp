#pragma once

#include <cstdint>

namespace device {

/**
 * Abstract monotonic clock.
 *
 * Returns microseconds since an arbitrary boot-time epoch.
 * Used for session idle-timeout tracking.
 * Concrete implementation: esp_platform::EspClockInterface.
 */
class ClockInterface {
  public:
    virtual ~ClockInterface() = default;

    /**
     * Return the current monotonic time in microseconds.
     * The epoch is unspecified but stable across a single boot.
     */
    virtual int64_t nowUs() const = 0;
};

} // namespace device
