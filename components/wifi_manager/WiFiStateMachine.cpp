#include "wifi_manager/WiFiStateMachine.hpp"

#include "credential_store/CredentialStore.hpp"
#include "esp_log.h"
#include "include/wifi_manager/WiFiInterface.hpp"
#include "wifi_manager/WiFiContext.hpp"

namespace wifi_manager {

static const char *TAG = "WiFiStateMachine";

// ---------------------------------------------------------
// Constructor
// ---------------------------------------------------------
WiFiStateMachine::WiFiStateMachine(WiFiContext &ctx)
    : ctx(ctx) {
    ESP_LOGD(TAG, "dispatchApi");
}

// ---------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------
void WiFiStateMachine::start() {}

void WiFiStateMachine::reset() {}

void WiFiStateMachine::startRuntime() {}

// ---------------------------------------------------------
// Driver lifecycle events
// ---------------------------------------------------------
void WiFiStateMachine::onDriverStarted() {}

void WiFiStateMachine::onDriverStopped() {}

// ---------------------------------------------------------
// AP events
// ---------------------------------------------------------
void WiFiStateMachine::onApStarted() {}

void WiFiStateMachine::onApStopped() {}

// ---------------------------------------------------------
// STA events
// ---------------------------------------------------------
void WiFiStateMachine::onStaConnecting() {}

void WiFiStateMachine::onStaConnected() {}

void WiFiStateMachine::onStaGotIp(const ip_event_got_ip_t *ip) {}

void WiFiStateMachine::onStaDisconnected(WiFiError reason) {}

// ---------------------------------------------------------
// Provisioning events
// ---------------------------------------------------------
void WiFiStateMachine::onProvisioningRequestReceived() {}

void WiFiStateMachine::onProvisioningCredentialsReceived(const credential_store::WiFiCredential &creds) {}

void WiFiStateMachine::onProvisioningTestResult(bool success) {}

// ---------------------------------------------------------
// Error handling
// ---------------------------------------------------------
void WiFiStateMachine::onError(WiFiError error) {}

// ---------------------------------------------------------
// State queries
// ---------------------------------------------------------
WiFiState WiFiStateMachine::getState() const {
    return currentState;
}

size_t WiFiStateMachine::getCredentialIndex() const {
    return static_cast<size_t>(currentCredentialIndex);
}

std::string WiFiStateMachine::getCurrentSSID() const {
    return currentCredential->ssid; // fill in later
}

credential_store::WiFiCredential WiFiStateMachine::getCredential(size_t index) const {
    // delegate to credential store later
    return {};
}

// ---------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------
void WiFiStateMachine::enterState(WiFiState newState) {
    currentState = newState;
}

void WiFiStateMachine::tryNextCredential() {}

void WiFiStateMachine::startProvisioningAp() {}

void WiFiStateMachine::startProvisioningTestSta() {}

void WiFiStateMachine::startRuntimeSta() {}

} // namespace wifi_manager