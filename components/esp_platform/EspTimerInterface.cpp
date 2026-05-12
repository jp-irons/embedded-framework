#include "esp_platform/EspTimerInterface.hpp"

namespace esp_platform {

EspTimerInterface::EspTimerInterface() {
    esp_timer_create_args_t args = {
        .callback         = &EspTimerInterface::timerCallback,
        .arg              = this,
        .dispatch_method  = ESP_TIMER_TASK,
        .name             = "esp_timer_iface",
        .skip_unhandled_events = false
    };
    esp_timer_create(&args, &timerHandle);
}

EspTimerInterface::~EspTimerInterface() {
    if (timerHandle) {
        esp_timer_stop(timerHandle);
        esp_timer_delete(timerHandle);
    }
}

void EspTimerInterface::runAfter(uint32_t delayMs, std::function<void()> fn) {
    callback = std::move(fn);
    esp_timer_stop(timerHandle); // safe even if not running
    esp_timer_start_once(timerHandle, delayMs * 1000ULL);
}

void EspTimerInterface::timerCallback(void* arg) {
    auto* self = static_cast<EspTimerInterface*>(arg);
    if (self->callback) {
        self->callback();
    }
}

} // namespace esp_platform
