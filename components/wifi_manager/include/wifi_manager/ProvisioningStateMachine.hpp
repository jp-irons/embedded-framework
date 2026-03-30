#pragma once

#include "wifi_manager/ProvisioningState.hpp"
#include "wifi_manager/ProvisioningError.hpp"
#include "wifi_manager/WiFiManager.hpp"
#include "credential_store/CredentialStore.hpp"

namespace wifi_manager {

class ProvisioningStateMachine {
public:
    ProvisioningStateMachine(WiFiManager& wifi,
                             credential_store::CredentialStore& store);

    ProvisioningState state() const;
    ProvisioningError error() const;

    void start();
    void submitCredentials(const std::string& ssid,
                           const std::string& password);
    void complete();
    void fail(ProvisioningError err);

private:
    WiFiManager& wifi;
    credential_store::CredentialStore& store;

    ProvisioningState currentState = ProvisioningState::Idle;
    ProvisioningError lastError = ProvisioningError::None;
};

} // namespace wifi_manager