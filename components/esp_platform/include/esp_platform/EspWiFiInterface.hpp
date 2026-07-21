// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

#include "wifi_manager/WiFiInterface.hpp"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_netif_types.h"
#include "esp_wifi_types_generic.h"

namespace wifi_manager {
struct WiFiContext;
} // namespace wifi_manager

namespace esp_platform {

/**
 * ESP-IDF concrete implementation of wifi_manager::WiFiInterface.
 *
 * All ESP-IDF includes and types are confined to this header and its
 * corresponding .cpp.  Nothing outside the esp_platform component needs to
 * include this file — consumers depend only on WiFiInterface.hpp.
 */
class EspWiFiInterface : public wifi_manager::WiFiInterface {
  public:
  	static constexpr const char* TAG = "EspWiFiInterface";
	
    explicit EspWiFiInterface(wifi_manager::WiFiContext& ctx);

    common::Result startDriver() override;
    common::Result stopDriver()  override;

    common::Result startAp(const wifi_manager::ApConfig& cfg) override;
    common::Result stopAp()                                   override;

    wifi_manager::WiFiStatus connectSta(const network_store::WiFiNetwork& cred) override;
    common::Result           disconnectSta()                                     override;

    common::Result scan(std::vector<wifi_manager::WiFiAp>& results) override;

    wifi_manager::IpAddress getApIp()  const override;
    wifi_manager::IpAddress getStaIp() const override;

  private:
    wifi_manager::WiFiContext& ctx;

    esp_netif_t* apNetif  = nullptr;
    esp_netif_t* staNetif = nullptr;

    // Captured so startDriver() can tell (on a soft-reset re-entry via
    // WiFiManager's driver-reset escalation) whether these are already
    // registered and skip re-registering — they're tied to the default
    // event loop, not the WiFi driver's init state, so they stay valid and
    // correctly registered across a stopDriver()/startDriver() cycle and
    // must NOT be torn down/recreated each time (see stopDriver()'s comment
    // for why — a confirmed use-after-free, coredump 2026-07-22).
    esp_event_handler_instance_t wifiEventHandlerInstance_ = nullptr;
    esp_event_handler_instance_t ipEventHandlerInstance_   = nullptr;

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

    static wifi_manager::WiFiAuthMode toAuthMode(wifi_auth_mode_t mode);
    wifi_mode_t                       computeMode() const;
    common::Result                    setStaState(bool enable);
};

} // namespace esp_platform
