#pragma once

#include <vector>
#include "credential_store/CredentialStore.hpp"

#include "esp_event_base.h"
#include "wifi_manager/WiFiState.hpp"

namespace wifi_manager {

struct WiFiContext;                 // forward declaration
class ProvisioningStateMachine;     // forward declaration

class WiFiManager {
public:
    explicit WiFiManager(WiFiContext* ctx);

    void start();
    void loop();
    void startSTAWithFallback();
	WiFiState getState() const;
	int getCurrentCredentialIndex() const;
	const std::vector<credential_store::WiFiCredential>& getLoadedCredentials() const;
	const char* getCurrentSSID() const;
	


private:
    static void wifiEventHandler(void* arg,
                                 esp_event_base_t base,
                                 int32_t id,
                                 void* data);

    static void ipEventHandler(void* arg,
                               esp_event_base_t base,
                               int32_t id,
                               void* data);

    void handleWiFiEvent(esp_event_base_t base,
                         int32_t id,
                         void* data);

    void handleIPEvent(esp_event_base_t base,
                       int32_t id,
                       void* data);

    void tryNextCredential();
    void connectTo(const credential_store::WiFiCredential& cred);

    void onSTAConnected();
    void onSTADisconnected(uint8_t reason);

private:
    WiFiContext* ctx;
    std::vector<credential_store::WiFiCredential> credentials;
    size_t currentIndex = 0;
    WiFiState state = WiFiState::UNPROVISIONED_AP;
};

} // namespace wifi_manager