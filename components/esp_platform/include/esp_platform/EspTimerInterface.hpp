#pragma once

#include "device/TimerInterface.hpp"
#include "esp_timer.h"

namespace esp_platform {

/**
 * ESP-IDF concrete implementation of TimerInterface.
 *
 * Uses esp_timer for one-shot deferred callbacks.
 * All ESP-IDF types are confined to this header and its .cpp.
 */
class EspTimerInterface : public device::TimerInterface {
  public:
    EspTimerInterface();
    ~EspTimerInterface() override;

    void runAfter(uint32_t delayMs, std::function<void()> fn) override;

  private:
    static void timerCallback(void* arg);

    esp_timer_handle_t    timerHandle{};
    std::function<void()> callback;
};

} // namespace esp_platform
