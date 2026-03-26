#pragma once

#include "WiFiContext.hpp"

namespace wifi_manager {

class WiFiManager {
public:
    explicit WiFiManager(WiFiContext& ctx);

    void startProvisioning();
    void onProvisioningComplete();
    void onProvisioningFailed();

    void onSTAConnected();
    void onSTADisconnected();

    void loop();

private:
    WiFiContext& ctx;
	WiFiState state;

    void transitionTo(WiFiState newState);
	
	void connectSTAWithStoredCredentials();

};

// Factory
WiFiManager* create(WiFiContext& ctx);

} // namespace wifi_manager