#pragma once

#include <cstdint>
#include <functional>

namespace device {

/**
 * Abstract one-shot deferred timer.
 *
 * Schedule a callback to fire once after a given delay.
 * Safe to call from tasks, WiFi event handlers, or ISRs depending on
 * the concrete implementation.
 *
 * Concrete implementation: esp_platform::EspTimerInterface.
 */
class TimerInterface {
  public:
    virtual ~TimerInterface() = default;

    /**
     * Schedule fn to run once after delayMs milliseconds.
     * Cancels any previously scheduled callback that has not yet fired.
     */
    virtual void runAfter(uint32_t delayMs, std::function<void()> fn) = 0;
};

} // namespace device
