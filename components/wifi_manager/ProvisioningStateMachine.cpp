#include "wifi_manager/ProvisioningStateMachine.hpp"

#include "wifi_manager/WiFiManager.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/ProvisioningServer.hpp"
#include "wifi_manager/RuntimeServer.hpp"
#include "credential_store/CredentialStore.hpp"

#include "esp_log.h"

namespace wifi_manager {

static const char* TAG = "ProvSM";

ProvisioningStateMachine::ProvisioningStateMachine(
    WiFiManager& wifi,
    WiFiContext& ctx,
    credential_store::CredentialStore& store)
    : wifi(wifi), ctx(ctx), store(store)
{
}

// -----------------------------------------------------------------------------
// Start provisioning (AP + provisioning UI)
// -----------------------------------------------------------------------------

void ProvisioningStateMachine::startProvisioning()
{
    transitionTo(ProvisioningState::StartingProvisioning);

    ctx.provisioningServer->start();

    transitionTo(ProvisioningState::Provisioning);
}

// -----------------------------------------------------------------------------
// Credentials received
// -----------------------------------------------------------------------------

void ProvisioningStateMachine::credentialsReceived(
    const std::string& ssid,
    const std::string& password)
{
    // Package credential
    credential_store::WiFiCredential cred;
    cred.ssid        = ssid;
    cred.password    = password;
    cred.priority    = 0;

    // Always store the credential (update or insert)
    if (!store.store(cred)) {
        ESP_LOGE(TAG, "Failed to store credential");
        return;
    }

    // Only trigger STA test if we are in provisioning mode
    if (currentState == ProvisioningState::Provisioning) {
        transitionTo(ProvisioningState::StartingSTA);
        wifi.startSTAWithFallback();
        transitionTo(ProvisioningState::ConnectingSTA);
    } else {
        ESP_LOGI(TAG, "Credential stored in state %d — no STA restart",
                 static_cast<int>(currentState));
    }
}
// -----------------------------------------------------------------------------
// WiFiManager reports STA success
// -----------------------------------------------------------------------------

void ProvisioningStateMachine::wifiConnected()
{
    if (currentState != ProvisioningState::ConnectingSTA) {
        ESP_LOGW(TAG, "Ignoring wifiConnected in state %d",
                 static_cast<int>(currentState));
        return;
    }

    transitionTo(ProvisioningState::StaConnected);

    // Stop AP + provisioning UI
    ctx.provisioningServer->stop();

    transitionTo(ProvisioningState::ProvisioningComplete);

    transitionTo(ProvisioningState::StartingRuntime);

    ctx.runtimeServer->start();

    transitionTo(ProvisioningState::Runtime);
}

// -----------------------------------------------------------------------------
// WiFiManager reports STA failure (fallback exhausted)
// -----------------------------------------------------------------------------

void ProvisioningStateMachine::wifiConnectionFailed(ProvisioningError err)
{
    if (currentState != ProvisioningState::ConnectingSTA &&
        currentState != ProvisioningState::StartingSTA)
    {
        ESP_LOGW(TAG, "Ignoring wifiConnectionFailed in state %d",
                 static_cast<int>(currentState));
        return;
    }

    ESP_LOGW(TAG, "STA test failed: %d", static_cast<int>(err));

    transitionTo(ProvisioningState::StaTestFailed);

    startProvisioning();
}

// -----------------------------------------------------------------------------
// Explicit runtime start
// -----------------------------------------------------------------------------

void ProvisioningStateMachine::startRuntime()
{
    transitionTo(ProvisioningState::StartingRuntime);
    ctx.runtimeServer->start();
    transitionTo(ProvisioningState::Runtime);
}

// -----------------------------------------------------------------------------
// Factory reset
// -----------------------------------------------------------------------------

void ProvisioningStateMachine::reset()
{
    store.clear();
    startProvisioning();
}

// -----------------------------------------------------------------------------
// Transition helper
// -----------------------------------------------------------------------------

void ProvisioningStateMachine::transitionTo(ProvisioningState newState)
{
    ESP_LOGI(TAG, "Transition: %d → %d",
             static_cast<int>(currentState),
             static_cast<int>(newState));

    currentState = newState;
}

} // namespace wifi_manager