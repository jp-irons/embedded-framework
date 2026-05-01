#include "wifi_manager/WiFiManager.hpp"

#include "common/Result.hpp"
#include "credential_store/CredentialStore.hpp"
#include "logger/Logger.hpp"
#include "wifi_manager/EmbeddedServer.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiInterface.hpp"

namespace wifi_manager {

static const char *TAG = "WiFiManager";

static logger::Logger log{TAG};

using namespace common;

WiFiManager::WiFiManager(WiFiContext &ctx)
    : ctx(ctx)
    , sm() {
    sm.setListener([this](WiFiState oldState, WiFiState newState) { this->onStateChanged(oldState, newState); });
}

void WiFiManager::start() {
    log.debug("start()");
    loadInitialCredential();
	log.debug("start embeddedServer");
	ctx.wifiInterface->startDriver();
	ctx.embeddedServer->start();

    if (!currentCredential->ssid.empty()) {
        sm.onEvent(WiFiEvent::CredentialsProvided);
    } else {
        sm.onEvent(WiFiEvent::StartProvisioning);
    }
}

void WiFiManager::loop() {
    // DeferredExecutor uses esp_timer callbacks, so nothing needed here.
}

//
// ESP-IDF event handlers
//

void WiFiManager::onStateChanged(WiFiState oldState, WiFiState newState) {
    log.debug("onStateChanged: %s → %s", WiFiStateMachine::toString(oldState), WiFiStateMachine::toString(newState));

    switch (newState) {

        case WiFiState::AP_Mode:
            retryCount = 0;
            startAP();
            if (ctx.embeddedServer)
                ctx.embeddedServer->startProvisioningMode();
            break;

        case WiFiState::STA_Connecting:
			startSTA();
            break;

        case WiFiState::STA_Connected:
            retryCount = 0;
            break;
        case WiFiState::Idle:
            stopAP();
            stopSTA();
            mdns.stop();
            break;

        default:
            break;
    }
}

void WiFiManager::onConnectSuccess() {
	log.info("onConnectSuccess()");
    sm.onEvent(WiFiEvent::ConnectSuccess);
}

void WiFiManager::onConnectFail() {
	log.info("onConnectFail()");
    lastErrorReason = "ConnectError";
    sm.onEvent(WiFiEvent::ConnectFail);
}

void WiFiManager::onDisconnect() {
    // Stop mDNS — IP is no longer valid
    mdns.stop();

    // Retry the same credential first
    if (retryCount < MAX_RETRIES) {
        retryCount++;
        log.info("Retrying credential %d (%d/%d tries)",
                 currentCredentialIndex, retryCount, MAX_RETRIES);
        startSTA();
        return;
    }

    // Move to the next credential
    retryCount = 0;
    currentCredentialIndex++;
	std::size_t credentialListSize = ctx.credentialStore->count();

    if (currentCredentialIndex < credentialListSize) {
        log.warn("Credential failed, trying next (%d/%d credentials)",
                 currentCredentialIndex + 1, credentialListSize);
        startSTA();
        return;
    }

    // All credentials exhausted
    log.error("All credentials failed — falling back to AP");
    sm.onEvent(WiFiEvent::ConnectFail);
}

void WiFiManager::onFatalError() {
	log.info("onFatalError");
    lastErrorReason = "DriverError";
    sm.onEvent(WiFiEvent::FatalError);
}

//
// Load credentials only at boot
//

void WiFiManager::loadInitialCredential() {
    log.debug("loadInitialCredential()");
    if (!ctx.credentialStore)
        return;

    std::vector<credential_store::WiFiCredential> all;
    if (Result::Ok == ctx.credentialStore->loadAllSortedByPriority(all) && !all.empty()) {
        currentCredential = all.front();
    }
}

//
// State transition handler
//

//
// Wi-Fi actions
//

void WiFiManager::startAP() {
    log.info("Starting AP mode");
	ctx.wifiInterface->startAp(ctx.apConfig);
}

void WiFiManager::stopAP() {
    log.info("Stopping AP mode");
}

void WiFiManager::startSTA() {
    log.info("Starting STA mode");
    auto credOpt = ctx.credentialStore->getByIndex(currentCredentialIndex);

	currentCredential = ctx.credentialStore->getByIndex(currentCredentialIndex);
    if (!credOpt) {
        currentCredential = std::nullopt;
        sm.onEvent(WiFiEvent::ConnectFail);
        return;
    }

    currentCredential = *credOpt;   // store it for UI layers
    ctx.wifiInterface->connectSta(*credOpt);
}

void WiFiManager::stopSTA() {
    log.info("Stopping STA mode");
    ctx.wifiInterface->disconnectSta();
}

//
// Retry logic
//

void WiFiManager::scheduleRetry() {
    if (retryCount >= MAX_RETRIES) {
        log.warn("STA retries exhausted — falling back to AP");
        sm.onEvent(WiFiEvent::ConnectFail);
        return;
    }

    retryCount++;

    deferred.runAfter(500, [this]() {
        log.info("Retrying STA connection (%d/%d)", retryCount, MAX_RETRIES);
        startSTA();
    });
}

void WiFiManager::onStaGotIp(const StaIpInfo &info) {
    log.info("STA got IP: %s (mask %s, gw %s)", info.ip.c_str(), info.netmask.c_str(), info.gateway.c_str());

    // Advertise via mDNS so the device is reachable by hostname regardless of IP
    mdns.start(ctx.mdnsHostname);
    log.info("Device reachable at https://%s.local", ctx.mdnsHostname.c_str());

    // Feed the state machine
    sm.onEvent(WiFiEvent::ConnectSuccess);

    // Start runtime server or other post-connect actions
    if (ctx.embeddedServer) {
        ctx.embeddedServer->startRuntimeMode();
    }
}

WiFiStaStatus WiFiManager::getStaStatus() const {
    WiFiStaStatus st;

    // 1. State (stringified)
    st.state = sm.toString(sm.getState());

    // 2. Connected?
    st.connected = (sm.getState() == WiFiState::STA_Connected);

    // 3. Active SSID
    st.ssid = currentCredential->ssid;

    // 4. Last error reason (string)
    st.lastErrorReason = lastErrorReason;

    return st;
}

} // namespace wifi_manager
