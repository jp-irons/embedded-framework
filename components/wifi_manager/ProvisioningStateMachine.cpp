#include "wifi_manager/WiFiManager.hpp"
#include "wifi_manager/ProvisioningStateMachine.hpp"
#include "credential_store/CredentialStore.hpp"
#include "credential_store/CredentialStore.hpp"

namespace wifi_manager {

ProvisioningStateMachine::ProvisioningStateMachine(WiFiManager &wifi, credential_store::CredentialStore &store)
    : wifi(wifi)
    , store(store) {}

// ---------------------------------------------------------
// High-level provisioning flow
// ---------------------------------------------------------

void ProvisioningStateMachine::startProvisioning() {
    if (currentState != ProvisioningState::Idle)
        return;

    transitionTo(ProvisioningState::StartingProvisioning);

    // ProvisioningServer will start AP mode here
    transitionTo(ProvisioningState::Provisioning);
}

void ProvisioningStateMachine::credentialsReceived(const std::string &ssid, const std::string &password) {
    if (currentState != ProvisioningState::Provisioning)
        return;

    credential_store::WiFiCredential cred;
    cred.ssid = ssid;
    cred.password = password;
    cred.priority = 0; // or whatever default you want

    store.store(cred);

    transitionTo(ProvisioningState::ProvisioningComplete);

    transitionTo(ProvisioningState::StartingSTA);

    // Multi-SSID fallback is handled inside WiFiManager
    wifi.startSTAWithFallback();

    transitionTo(ProvisioningState::ConnectingSTA);
}

void ProvisioningStateMachine::wifiConnected() {
    if (currentState != ProvisioningState::ConnectingSTA)
        return;

    transitionTo(ProvisioningState::STAConnected);
}

void ProvisioningStateMachine::wifiConnectionFailed(ProvisioningError) {
    if (currentState != ProvisioningState::ConnectingSTA)
        return;

    // Return to provisioning mode (AP running)
    transitionTo(ProvisioningState::Provisioning);
}

void ProvisioningStateMachine::startRuntime() {
    if (currentState != ProvisioningState::STAConnected)
        return;

    transitionTo(ProvisioningState::StartingRuntime);

    // FrameworkContext will start RuntimeServer here
    transitionTo(ProvisioningState::Runtime);
}

void ProvisioningStateMachine::reset() {
    transitionTo(ProvisioningState::Idle);
}

// ---------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------

void ProvisioningStateMachine::transitionTo(ProvisioningState newState) {
    currentState = newState;
}

} // namespace wifi_manager