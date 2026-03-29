#pragma once

#include "esp_event.h"
#include "WiFiContext.hpp"

namespace wifi_manager {

class WiFiManager {
public:
    explicit WiFiManager(WiFiContext* ctx);

    void start();   // entry point from app_main
    void loop();    // optional heartbeat only (no logic)

private:
    WiFiContext* ctx;

    void transitionTo(WiFiState s);

    void startAp();
    void stopAp();

    void startStaWithCurrent();
    void stopSta();
    void loadCredentials();
    void tryNextCredential();

    static void wifiEventHandler(void* arg,
                                 esp_event_base_t base,
                                 int32_t id,
                                 void* data);

    static void ipEventHandler(void* arg,
                               esp_event_base_t base,
                               int32_t id,
                               void* data);
};

// Factory (raw pointer, owned by ApplicationContext)
WiFiManager* create(WiFiContext& ctx);

} // namespace wifi_manager