#pragma once

#include <string>
#include "wifi_manager/ProvisioningState.hpp"
#include "wifi_manager/ProvisioningError.hpp"

namespace credential_store {
class CredentialStore;
}

namespace wifi_manager {

class WiFiManager;
struct WiFiContext;

class ProvisioningStateMachine {
public:
    ProvisioningStateMachine(WiFiManager& wifi,
                             WiFiContext& ctx,
                             credential_store::CredentialStore& store);

    ProvisioningState state() const { return currentState; }

    // External triggers
    void startProvisioning();
    void credentialsReceived(const std::string& ssid,
                             const std::string& password);
    void wifiConnected();
    void wifiConnectionFailed(ProvisioningError err);
    void startRuntime();
    void reset();

private:
    void transitionTo(ProvisioningState newState);

private:
    WiFiManager& wifi;
    WiFiContext& ctx;
    credential_store::CredentialStore& store;

    ProvisioningState currentState = ProvisioningState::Idle;
};

} // namespace wifi_manager