#include "wifi_manager/WiFiStateMachine.hpp"

#include "credential_store/CredentialStore.hpp"
#include "logger/Logger.hpp"
#include "wifi_manager/ProvisioningServer.hpp"
#include "wifi_manager/RuntimeServer.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiInterface.hpp"
#include "wifi_manager/WiFiTypes.hpp"

namespace wifi_manager {

static logger::Logger log{"WiFiStateMachine"};

// ---------------------------------------------------------
// Constructor
// ---------------------------------------------------------
WiFiStateMachine::WiFiStateMachine(WiFiContext &ctx)
    : ctx(ctx) {
    log.debug("Constructor");
}

// ---------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------
void WiFiStateMachine::start() {
    log.debug("start");
    if (currentState != WiFiState::UNINITIALISED) {
        log.warn("WiFiStateMachine::start() called in state %s",
                 toString(currentState));
        return;
    }

    log.info("Starting WiFiStateMachine");
    enterState(WiFiState::STARTING);

    // Initialise WiFi driver
    ctx.wifiInterface->startDriver();

    // Decide provisioning vs runtime
    if (ctx.credentialStore->count() == 0) {
        log.debug("No credentials found - entering provisioning AP mode");
        enterState(WiFiState::UNPROVISIONED_AP);
    } else {
        log.debug("Credentials found -> entering runtime STA mode");
        enterState(WiFiState::STA_CONNECTING);
    }
}


void WiFiStateMachine::reset() {
    log.debug("reset");
}

void WiFiStateMachine::startRuntime() {
    log.debug("startRuntime");
}

// ---------------------------------------------------------
// Driver lifecycle events
// ---------------------------------------------------------
void WiFiStateMachine::onDriverStarted() {
    log.debug("onDriverStarted");
}

void WiFiStateMachine::onDriverStopped() {
    log.debug("onDriverStopped");
}

// ---------------------------------------------------------
// AP events
// ---------------------------------------------------------
void WiFiStateMachine::onApStarted() {
    log.debug("onApStarted");
}

void WiFiStateMachine::onApStopped() {
    log.debug("onApStopped");
}

// ---------------------------------------------------------
// STA events
// ---------------------------------------------------------
void WiFiStateMachine::onStaConnecting() {
    log.debug("onStaConnecting");
}

void WiFiStateMachine::onStaConnected() {
    log.debug("onStaConnected");
	if (currentState == WiFiState::PROVISIONING_TEST_STA) {
	    enterState(WiFiState::STA_CONNECTED);
	}
}

void WiFiStateMachine::onStaGotIp(const ip_event_got_ip_t *ip) {
    log.debug("onStaGotIp");
	if (currentState == WiFiState::PROVISIONING_TEST_STA ||
	    currentState == WiFiState::STA_CONNECTED)
	{
	    enterState(WiFiState::GOT_IP);
	}
}

void WiFiStateMachine::onStaDisconnected(WiFiError reason) {
    log.debug("onStaDisconnected");
	if (currentState == WiFiState::PROVISIONING_TEST_STA) {
	    enterState(WiFiState::STA_CONNECT_FAILED);
	}
}

// ---------------------------------------------------------
// Provisioning events
// ---------------------------------------------------------
void WiFiStateMachine::onProvisioningRequestReceived() {
    log.debug("onProvisioningRequestReceived not implemented");
    if (currentState == WiFiState::UNPROVISIONED_AP) {
        enterState(WiFiState::PROVISIONING);
    }
}

void WiFiStateMachine::onProvisioningCredentialsReceived(const credential_store::WiFiCredential &creds) {
    log.debug("onProvisioningCredentialsReceived");
    ctx.credentialStore->store(creds);
    enterState(WiFiState::PROVISIONING_TEST_STA);
}

void WiFiStateMachine::onProvisioningTestResult(bool success) {
    log.debug("onProvisioningTestResult");
}

// ---------------------------------------------------------
// Error handling
// ---------------------------------------------------------
void WiFiStateMachine::onError(WiFiError error) {
    log.debug("onError");
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
	log.debug("enterState");
    if (currentState == newState) {
        return;
    }

    log.debug("State transition: %s -> %s", toString(currentState), toString(newState));

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
    log.debug("tryNextCredential Not Implemented");
}

void WiFiStateMachine::startProvisioningAp()
{
    log.debug("startProvisioningAp");

    ctx.runtimeServer->stop();          // stop runtime server if running
    ctx.wifiInterface->disconnectSta(); // correct name
	log.debug("start AP %s", ctx.apConfig.ssid.c_str());
    ctx.wifiInterface->startAp(ctx.apConfig); // must pass config
    ctx.provisioningServer->start();
}

void WiFiStateMachine::startProvisioningTestSta() {
    log.debug("startProvisioningTestSta Not Implemented");
//    ctx.provisioningServer.stop();
//    ctx.wifiInterface.stopAp();
//    ctx.wifiInterface.configureSta(ctx.credentialStore.getLatest());
//    ctx.wifiInterface.startSta();
}

void WiFiStateMachine::startRuntimeSta() {
    log.debug("startRuntimeSta Not Implemented");
//	ctx.provisioningServer.stop();
//	ctx.wifiInterface->stopAp();

	currentCredentialIndex = 0;
	log.debug("startRuntimeSta Not Implemented");
//	currentCredential = ctx.credentialStore.getCredential(currentIndex);
//
//	ctx.wifiInterface->configureSta(creds);
//	ctx.wifiInterface.startSta();
}

} // namespace wifi_manager