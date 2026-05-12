#include "device/DeferredExecutor.hpp"

#include "esp_timer.h" // TODO: see DeferredExecutor.hpp — pending move to esp_platform

namespace device {

DeferredExecutor::DeferredExecutor() {
    esp_timer_create_args_t args = {.callback = &DeferredExecutor::timerCallback,
                                    .arg = this,
                                    .dispatch_method = ESP_TIMER_TASK,
                                    .name = "deferred_exec",
                                    .skip_unhandled_events = false};

    esp_timer_create(&args, &timerHandle);
}

DeferredExecutor::~DeferredExecutor() {
    if (timerHandle) {
        esp_timer_stop(timerHandle);
        esp_timer_delete(timerHandle);
    }
}

void DeferredExecutor::runAfter(uint32_t delayMs, std::function<void()> fn) {
    callback = std::move(fn);
    esp_timer_stop(timerHandle); // safe even if not running
    esp_timer_start_once(timerHandle, delayMs * 1000ULL);
}

void DeferredExecutor::timerCallback(void *arg) {
    auto *self = static_cast<DeferredExecutor *>(arg);
    if (self->callback) {
        self->callback();
    }
}

} // namespace device
