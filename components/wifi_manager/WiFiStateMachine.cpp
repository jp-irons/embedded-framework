#include "wifi_manager/WiFiStateMachine.hpp"
#include "credential_store/CredentialStore.hpp"
#include "wifi_manager/ProvisioningServer.hpp"
#include "wifi_manager/RuntimeServer.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiInterface.hpp"
#include "wifi_manager/WiFiTypes.hpp"
#include "esp_log.h"

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
    if (currentState != WiFiState::UNINITIALISED) {
        ESP_LOGW(TAG, "WiFiStateMachine::start() called in state %s",
                 toString(currentState));
        return;
    }

    ESP_LOGI(TAG, "Starting WiFiStateMachine");
    enterState(WiFiState::STARTING);

    // Initialise WiFi driver
    ctx.wifiInterface->startDriver();

    // Decide provisioning vs runtime
    if (ctx.credentialStore->count() == 0) {
        ESP_LOGD(TAG, "No credentials found - entering provisioning AP mode");
        enterState(WiFiState::UNPROVISIONED_AP);
    } else {
        ESP_LOGD(TAG, "Credentials found -> entering runtime STA mode");
        enterState(WiFiState::STA_CONNECTING);
    }
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
	if (currentState == WiFiState::PROVISIONING_TEST_STA) {
	    enterState(WiFiState::STA_CONNECTED);
	}
}

void WiFiStateMachine::onStaGotIp(const ip_event_got_ip_t *ip) {
    ESP_LOGD(TAG, "onStaGotIp");
	if (currentState == WiFiState::PROVISIONING_TEST_STA ||
	    currentState == WiFiState::STA_CONNECTED)
	{
	    enterState(WiFiState::GOT_IP);
	}
}

void WiFiStateMachine::onStaDisconnected(WiFiError reason) {
    ESP_LOGD(TAG, "onStaDisconnected");
	if (currentState == WiFiState::PROVISIONING_TEST_STA) {
	    enterState(WiFiState::STA_CONNECT_FAILED);
	}
}

// ---------------------------------------------------------
// Provisioning events
// ---------------------------------------------------------
void WiFiStateMachine::onProvisioningRequestReceived() {
    ESP_LOGD(TAG, "onProvisioningRequestReceived not implemented");
    if (currentState == WiFiState::UNPROVISIONED_AP) {
        enterState(WiFiState::PROVISIONING);
    }
}

void WiFiStateMachine::onProvisioningCredentialsReceived(const credential_store::WiFiCredential &creds) {
    ESP_LOGD(TAG, "onProvisioningCredentialsReceived");
    ctx.credentialStore->store(creds);
    enterState(WiFiState::PROVISIONING_TEST_STA);
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
	ESP_LOGD(TAG, "enterState");
    if (currentState == newState) {
        return;
    }

    ESP_LOGD(TAG, "State transition: %s -> %s", toString(currentState), toString(newState));

    currentState = newState;

    switch (newState) {

        case WiFiState::UNPROVISIONED_AP:
            startProvisioningAp();
            break;

        case WiFiState::PROVISIONING:
            // no entry action; triggered by provisioning request
            break;

        case WiFiState::PROVISIONING_TEST_STA:
            startProvisioningTestSta();
            break;

        case WiFiState::STA_CONNECTING:
            startRuntimeSta();
            break;

        case WiFiState::STA_CONNECTED:
            // wait for IP
            break;

        case WiFiState::GOT_IP:
            ctx.runtimeServer->start();
            break;

        case WiFiState::STA_DISCONNECTED:
            tryNextCredential();
            break;

        case WiFiState::STA_CONNECT_FAILED:
            enterState(WiFiState::FALLBACK_AP);
            break;

        case WiFiState::FALLBACK_AP:
            startProvisioningAp();
            break;

        default:
            break;
    }
}

void WiFiStateMachine::tryNextCredential() {
    ESP_LOGD(TAG, "tryNextCredential Not Implemented");
}

void WiFiStateMachine::startProvisioningAp()
{
    ESP_LOGD(TAG, "startProvisioningAp");

    ctx.runtimeServer->stop();          // stop runtime server if running
    ctx.wifiInterface->disconnectSta(); // correct name
	ESP_LOGD(TAG, "start AP %s", ctx.apConfig.ssid.c_str());
    ctx.wifiInterface->startAp(ctx.apConfig); // must pass config
    ctx.provisioningServer->start();
}

void WiFiStateMachine::startProvisioningTestSta() {
    ESP_LOGD(TAG, "startProvisioningTestSta Not Implemented");
//    ctx.provisioningServer.stop();
//    ctx.wifiInterface.stopAp();
//    ctx.wifiInterface.configureSta(ctx.credentialStore.getLatest());
//    ctx.wifiInterface.startSta();
}

void WiFiStateMachine::startRuntimeSta() {
    ESP_LOGD(TAG, "startRuntimeSta Not Implemented");
//	ctx.provisioningServer.stop();
//	ctx.wifiInterface->stopAp();

	currentCredentialIndex = 0;
	ESP_LOGD(TAG, "startRuntimeSta Not Implemented");
//	currentCredential = ctx.credentialStore.getCredential(currentIndex);
//
//	ctx.wifiInterface->configureSta(creds);
//	ctx.wifiInterface.startSta();
}

} // namespace wifi_manager