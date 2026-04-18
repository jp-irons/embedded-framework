#pragma once

#include "wifi_types/WiFiTypes.hpp"
#include "esp_event_base.h"
#include "esp_netif_types.h"
#include "esp_wifi_types_generic.h"
#include <vector>

namespace common {
enum class Result;
}

namespace wifi_manager {

struct WiFiContext;
// forward declaration
class WiFiStateMachine;
// forward declaration

class WiFiInterface {
  public:
    explicit WiFiInterface(WiFiContext &ctx);

    void startDriver();
    void stopDriver();

    void startAp(const wifi_types::ApConfig &cfg);
    void stopAp();

	wifi_config_t makeStaConfig(const wifi_types::WiFiCredential& cred);

    wifi_types::WiFiStatus connectSta(const wifi_types::WiFiCredential& cred);
    void disconnectSta();

    void startProvisioningServer();
    void stopProvisioningServer();

    void startRuntimeServer();
    void stopRuntimeServer();

	common::Result scan(std::vector<wifi_types::WiFiAp>& results);
	
	wifi_types::IpAddress getApIp() const;
	wifi_types::IpAddress getStaIp() const;

  private:
    WiFiContext &ctx;

    esp_netif_t *apNetif = nullptr;
    esp_netif_t *staNetif = nullptr;
	
	bool driverStarted = false;

	bool apActive = false;
	bool staActive = false;

	wifi_mode_t currentMode = WIFI_MODE_NULL;

    static void wifiEventHandler(void *arg, esp_event_base_t base, int32_t id, void *data);

    static void ipEventHandler(void *arg, esp_event_base_t base, int32_t id, void *data);

    void handleWiFiEvent(esp_event_base_t base, int32_t id, void *data);

    void handleIPEvent(esp_event_base_t base, int32_t id, void *data);

    void connectTo(const wifi_types::WiFiCredential &cred);

    void onSTAConnected();
    void onSTADisconnected(uint8_t reason);
	static wifi_types::WiFiAuthMode toAuthMode(wifi_auth_mode_t mode);
	wifi_mode_t computeMode() const;
	common::Result setStaState(bool enable);


};

} // namespace wifi_manager
