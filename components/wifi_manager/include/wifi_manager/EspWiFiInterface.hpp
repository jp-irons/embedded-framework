#pragma once

#include "wifi_manager/WiFiInterface.hpp"
#include "esp_event_base.h"
#include "esp_netif_types.h"
#include "esp_wifi_types_generic.h"

namespace wifi_manager {

struct WiFiContext;

/**
 * ESP-IDF concrete implementation of WiFiInterface.
 *
 * All ESP-IDF includes and types are confined to this header and its
 * corresponding .cpp.  Nothing outside the wifi_manager component needs to
 * include this file — consumers depend only on WiFiInterface.hpp.
 */
class EspWiFiInterface : public WiFiInterface {
  public:
    explicit EspWiFiInterface(WiFiContext& ctx);

    common::Result startDriver() override;
    common::Result stopDriver()  override;

    common::Result startAp(const ApConfig& cfg) override;
    common::Result stopAp()                      override;

    WiFiStatus     connectSta(const network_store::WiFiNetwork& cred) override;
    common::Result disconnectSta()                                     override;

    common::Result scan(std::vector<WiFiAp>& results) override;

    IpAddress getApIp()  const override;
    IpAddress getStaIp() const override;

  private:
    WiFiContext& ctx;

    esp_netif_t* apNetif  = nullptr;
    esp_netif_t* staNetif = nullptr;

    bool driverStarted = false;
    bool apActive      = false;
    bool staActive     = false;

    wifi_mode_t currentMode = WIFI_MODE_NULL;

    static void wifiEventHandler(void* arg, esp_event_base_t base, int32_t id, void* data);
    static void ipEventHandler  (void* arg, esp_event_base_t base, int32_t id, void* data);

    void handleWiFiEvent(esp_event_base_t base, int32_t id, void* data);
    void handleIPEvent  (esp_event_base_t base, int32_t id, void* data);

    void onSTAConnected();
    void onSTADisconnected(uint8_t reason);

    static WiFiAuthMode toAuthMode(wifi_auth_mode_t mode);
    wifi_mode_t         computeMode() const;
    common::Result      setStaState(bool enable);
    wifi_config_t       makeStaConfig(const network_store::WiFiNetwork& cred);
};

} // namespace wifi_manager
