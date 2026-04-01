#pragma once

#include "credential_store/CredentialStore.hpp"

#include "esp_event_base.h"
#include "WiFiTypes.hpp"

namespace wifi_manager {

struct WiFiContext;                 // forward declaration
class WiFiStateMachine;     // forward declaration

class WiFiInterface {
public:
    explicit WiFiInterface(WiFiContext& ctx);

	void startDriver();
	void stopDriver();

	void startAp(const ApConfig& cfg);
	void stopAp();

	void connectSta(const StaConfig& cfg);
	void disconnectSta();

	void startProvisioningServer();
	void stopProvisioningServer();

	void startRuntimeServer();
	void stopRuntimeServer();
	
private:
    WiFiContext &ctx;
	
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

    void connectTo(const credential_store::WiFiCredential& cred);

    void onSTAConnected();
    void onSTADisconnected(uint8_t reason);

};

} // namespace wifi_manager