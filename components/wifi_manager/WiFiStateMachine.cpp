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
    ESP_LOGD(TAG, "Constructor");
}

// ---------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------
void WiFiStateMachine::start() {
    ESP_LOGD(TAG, "start");
}

void WiFiStateMachine::reset() {
    ESP_LOGD(TAG, "reset");
}

void WiFiStateMachine::startRuntime() {
    ESP_LOGD(TAG, "startRuntime");
}

// ---------------------------------------------------------
// Driver lifecycle events
// ---------------------------------------------------------
void WiFiStateMachine::onDriverStarted() {
    ESP_LOGD(TAG, "onDriverStarted");
}

void WiFiStateMachine::onDriverStopped() {
    ESP_LOGD(TAG, "onDriverStopped");
}

// ---------------------------------------------------------
// AP events
// ---------------------------------------------------------
void WiFiStateMachine::onApStarted() {
    ESP_LOGD(TAG, "onApStarted");
}

void WiFiStateMachine::onApStopped() {
    ESP_LOGD(TAG, "onApStopped");
}

// ---------------------------------------------------------
// STA events
// ---------------------------------------------------------
void WiFiStateMachine::onStaConnecting() {
    ESP_LOGD(TAG, "onStaConnecting");
}

void WiFiStateMachine::onStaConnected() {
    ESP_LOGD(TAG, "onStaConnected");
}

void WiFiStateMachine::onStaGotIp(const ip_event_got_ip_t *ip) {
    ESP_LOGD(TAG, "onStaGotIp");
}

void WiFiStateMachine::onStaDisconnected(WiFiError reason) {
    ESP_LOGD(TAG, "onStaDisconnected");
}

// ---------------------------------------------------------
// Provisioning events
// ---------------------------------------------------------
void WiFiStateMachine::onProvisioningRequestReceived() {
    ESP_LOGD(TAG, "onProvisioningRequestReceive");
}

void WiFiStateMachine::onProvisioningCredentialsReceived(const credential_store::WiFiCredential &creds) {
    ESP_LOGD(TAG, "onProvisioningCredentialsReceived");
}

void WiFiStateMachine::onProvisioningTestResult(bool success) {
    ESP_LOGD(TAG, "onProvisioningTestResult");
}

// ---------------------------------------------------------
// Error handling
// ---------------------------------------------------------
void WiFiStateMachine::onError(WiFiError error) {
    ESP_LOGD(TAG, "onError");
}

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