#pragma once

#include "network_store/WiFiNetwork.hpp"
#include "wifi_manager/WiFiTypes.hpp"
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

    common::Result startDriver();
    common::Result stopDriver();

    common::Result startAp(const wifi_manager::ApConfig &cfg);
    common::Result stopAp();

	wifi_config_t makeStaConfig(const network_store::WiFiNetwork& cred);

    WiFiStatus connectSta(const network_store::WiFiNetwork& cred);
    common::Result disconnectSta();

	common::Result scan(std::vector<WiFiAp>& results);
	
	IpAddress getApIp() const;
	IpAddress getStaIp() const;

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

    void connectTo(const network_store::WiFiNetwork &cred);

    void onSTAConnected();
    void onSTADisconnected(uint8_t reason);
	static WiFiAuthMode toAuthMode(wifi_auth_mode_t mode);
	wifi_mode_t computeMode() const;
	common::Result setStaState(bool enable);


};

} // namespace wifi_manager
