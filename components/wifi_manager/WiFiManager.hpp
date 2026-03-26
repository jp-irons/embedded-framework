#pragma once

#include "WiFiContext.hpp"
#include "ProvisioningState.hpp"

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

    void transitionTo(ProvisioningState newState);

    // State handlers
    void handleStartingProvisioning();
    void handleProvisioning();
    void handleProvisioningComplete();
    void handleStartingSTA();
    void handleConnectingSTA();
    void handleSTAConnected();
    void handleStartingRuntime();
    void handleRuntime();

    // WiFi actions (stubs for now)
    void startAP();
    void stopAP();
    void startSTA();
    void stopSTA();
    void startRuntimeServer();
    void stopRuntimeServer();
};

// Factory
WiFiManager* create(WiFiContext& ctx);

} // namespace wifi_manager