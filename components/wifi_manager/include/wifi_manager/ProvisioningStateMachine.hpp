#pragma once

#include "wifi_manager/ProvisioningState.hpp"
#include "wifi_manager/ProvisioningError.hpp"
#include <string>

namespace credential_store {
class CredentialStore;
}

namespace wifi_manager {

class WiFiManager;     // forward declaration
struct WiFiContext;    // forward declaration

class ProvisioningStateMachine {
public:
    ProvisioningStateMachine(WiFiManager& wifi,
                             credential_store::CredentialStore& store);

    ProvisioningState state() const { return currentState; }

    void startProvisioning();
    void credentialsReceived(const std::string& ssid,
                             const std::string& password);
    void wifiConnected();
    void wifiConnectionFailed(ProvisioningError);
    void startRuntime();
    void reset();

private:
    void transitionTo(ProvisioningState newState);

private:
    WiFiManager& wifi;
    credential_store::CredentialStore& store;
    ProvisioningState currentState = ProvisioningState::Idle;
};

} // namespace wifi_manager