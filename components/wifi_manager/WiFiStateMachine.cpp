#include "wifi_manager/WiFiStateMachine.hpp"

#include "credential_store/CredentialStore.hpp"
#include "logger/Logger.hpp"
#include "wifi_manager/EmbeddedServer.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiInterface.hpp"
#include "wifi_types/WiFiTypes.hpp"

namespace wifi_manager {

using namespace wifi_types;

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
        log.debug("WiFiStateMachine::start() called in state %s", toString(currentState));
        return;
    }

    log.info("Starting WiFiStateMachine");
    transitionTo(WiFiState::STARTING);

    // Initialise WiFi driver
    ctx.wifiInterface->startDriver();

    // Decide provisioning vs runtime
    if (ctx.credentialStore->count() == 0) {
        log.debug("No credentials found - entering provisioning AP mode");
        transitionTo(WiFiState::UNPROVISIONED_AP);
    } else {
        log.debug("Credentials found -> entering runtime STA mode");
        transitionTo(WiFiState::STA_CONNECTING);
    }
}

void WiFiStateMachine::reset() {
    log.debug("reset");
}

void WiFiStateMachine::startRuntime() {
    log.debug("startRuntime");
}

void WiFiStateMachine::onUnexpectedEvent(const char *eventName) {
    log.error("Unexpected event %s in state %s", eventName, toString(currentState));
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
    if (currentState == WiFiState::PROVISIONING_TEST_STA || currentState == WiFiState::STA_CONNECTING) {
        transitionTo(WiFiState::STA_CONNECTED);
    } else {
        onUnexpectedEvent("onStaConnected");
    }
}

void WiFiStateMachine::onStaGotIp(const ip_event_got_ip_t *ip) {
    log.debug("onStaGotIp");
    log.debug("onStaGotIp - Current State %s ", toString(currentState));
    if (currentState == WiFiState::PROVISIONING_TEST_STA || currentState == WiFiState::STA_CONNECTED) {
        transitionTo(WiFiState::GOT_IP);
    } else {
        onUnexpectedEvent("onStaGotIp()");
    }
}

void WiFiStateMachine::onStaDisconnected(WiFiError reason) {
    log.debug("onStaDisconnected due: %s", toString(reason));

    switch (currentState) {

        case WiFiState::PROVISIONING_TEST_STA:
            // Provisioning test failed
            transitionTo(WiFiState::STA_CONNECT_FAILED);
            return;

        case WiFiState::STA_CONNECTING:
            log.warn("Disconnect during STA_CONNECTING — deferring handling");
            defer.runAfter(150, [this]() {
                // After the WiFi driver has settled
                if (currentState == WiFiState::STA_CONNECTING) {
                    transitionTo(WiFiState::STA_DISCONNECTED);
                }
            });
            return;
			
        case WiFiState::STA_CONNECTED:
        case WiFiState::GOT_IP:
            // Lost connection during runtime
            transitionTo(WiFiState::STA_DISCONNECTED);
            return;

        default:
            onUnexpectedEvent("onStaDisconnected()");
            return;
    }
}

// ---------------------------------------------------------
// Provisioning events
// ---------------------------------------------------------
void WiFiStateMachine::onProvisioningRequestReceived() {
    log.debug("onProvisioningRequestReceived not implemented");
    if (currentState == WiFiState::UNPROVISIONED_AP) {
        transitionTo(WiFiState::PROVISIONING);
    } else {
        onUnexpectedEvent("onProvisioningRequestReceived()");
    }
}

void WiFiStateMachine::onProvisioningCredentialsReceived(const WiFiCredential &creds) {
    log.debug("onProvisioningCredentialsReceived");
    ctx.credentialStore->store(creds);
    transitionTo(WiFiState::PROVISIONING_TEST_STA);
}

void WiFiStateMachine::onProvisioningTestResult(bool success) {
    log.debug("onProvisioningTestResult");
}

// ---------------------------------------------------------
// Error handling
// ---------------------------------------------------------
void WiFiStateMachine::onError(WiFiError error) {
    log.error("onError");
}

// ---------------------------------------------------------
// State queries
// ---------------------------------------------------------
WiFiState WiFiStateMachine::getState() const {
    return currentState;
}

wifi_types::WiFiStaStatus WiFiStateMachine::getStaStatus() const {
    wifi_types::WiFiStaStatus s;

    s.state = wifi_types::toString(currentState);
    s.ssid = "Unknown";
	// TODO sort out lastError
//	s.ssid = getCurrentSSID();
//    s.lastErrorReason = wifi_types::toString(lastError);
    s.connected = (currentState == wifi_types::WiFiState::GOT_IP);

    return s;
}

size_t WiFiStateMachine::getCredentialIndex() const {
    return static_cast<size_t>(currentCredentialIndex);
}

std::string WiFiStateMachine::getCurrentSSID() const {
    return currentCredential->ssid; // fill in later
}

WiFiCredential WiFiStateMachine::getCredential(size_t index) const {
    // delegate to credential store later
    return {};
}

// ---------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------
void WiFiStateMachine::transitionTo(WiFiState newState) {
    log.debug("transitionTo");
    if (currentState == newState) {
        return;
    }
    log.debug("State transition: %s -> %s", toString(currentState), toString(newState));
    currentState = newState;
    enterState(newState);
}

void WiFiStateMachine::enterState(WiFiState newState) {
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
            ctx.embeddedServer->start();
            break;
        case WiFiState::STA_DISCONNECTED:
            tryNextCredential();
            break;
        case WiFiState::STA_CONNECT_FAILED:
            transitionTo(WiFiState::FALLBACK_AP);
            break;
        case WiFiState::FALLBACK_AP:
            startProvisioningAp();
            break;
        default:
            break;
    }
}

void WiFiStateMachine::tryNextCredential() {
    if (retryCount < MAX_RETRIES) {
        retryCount++;
        log.warn("Retry %d/%d for credential %d",
                 retryCount, MAX_RETRIES, currentCredentialIndex);

        defer.runAfter(500, [this]() {
            transitionTo(WiFiState::STA_CONNECTING);
        });
        return;
    }

    // Retries exhausted for this credential
    log.warn("Credential %d failed after %d retries",
             currentCredentialIndex, MAX_RETRIES);

    retryCount = 0;
    currentCredentialIndex++;

    const int total = ctx.credentialStore->count();

    if (currentCredentialIndex < total) {
        log.warn("Trying next credential %d", currentCredentialIndex);

        defer.runAfter(200, [this]() {
            transitionTo(WiFiState::STA_CONNECTING);
        });
    } else {
        log.error("All credentials failed — falling back to AP");
        currentCredentialIndex = 0;  // reset for next cycle
        transitionTo(WiFiState::FALLBACK_AP);
    }
}

void WiFiStateMachine::startProvisioningAp() {
    log.debug("startProvisioningAp");

    ctx.embeddedServer->stop(); // stop runtime server if running
    ctx.wifiInterface->disconnectSta(); // correct name
    log.debug("start AP %s", ctx.apConfig.ssid.c_str());
    ctx.wifiInterface->startAp(ctx.apConfig); // must pass config
    ctx.embeddedServer->start();
}

void WiFiStateMachine::startProvisioningTestSta() {
    log.warn("startProvisioningTestSta Not Implemented");
    //    ctx.provisioningServer.stop();
    //    ctx.wifiInterface.stopAp();
    //    ctx.wifiInterface.configureSta(ctx.credentialStore.getLatest());
    //    ctx.wifiInterface.startSta();
}

void WiFiStateMachine::startRuntimeSta() {
    log.debug("startRuntimeSta");

    // 1. Stop provisioning server and AP
    ctx.embeddedServer->stop();
    ctx.wifiInterface->stopAp();

    // 2. Select the current credential
    auto creds = ctx.credentialStore->getByIndex(currentCredentialIndex);
    if (!creds) {
        error = WiFiError::INVALID_CREDENTIALS;
        log.error("No credentials available for runtime STA");
        transitionTo(WiFiState::STA_CONNECT_FAILED);
        return;
    }

    // 3. Configure STA with selected credentials
    ctx.wifiInterface->connectSta(*creds);

    // 5. State machine now waits for:
    //    - WIFI_EVENT_STA_CONNECTED
    //    - IP_EVENT_STA_GOT_IP
}

} // namespace wifi_manager