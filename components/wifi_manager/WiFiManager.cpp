#include "wifi_manager/WiFiManager.hpp"

#include "common/Result.hpp"
#include "network_store/NetworkStore.hpp"
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
    loadInitialNetwork();

    Result r = ctx.wifiInterface->startDriver();
    if (r != Result::Ok) {
        log.error("start(): startDriver() failed — triggering fatal error");
        onFatalError();
        return;
    }

    log.debug("start: starting embeddedServer");
    ctx.embeddedServer->start();

    if (currentNetwork.has_value() && !currentNetwork->ssid.empty()) {
        sm.onEvent(WiFiEvent::NetworkProvided);
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
    log.debug("onStateChanged: %s -> %s", WiFiStateMachine::toString(oldState), WiFiStateMachine::toString(newState));

    switch (newState) {

        case WiFiState::AP_Mode:
            retryCount = 0;
            startAP();
            mdns.start(ctx.mdnsHostname);
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

        case WiFiState::DriverFailed:
            driverRetryCount = 0;
            log.warn("Driver failed — scheduling retry in %ums", DRIVER_RETRY_DELAY_MS);
            deferred.runAfter(DRIVER_RETRY_DELAY_MS, [this]() { retryDriver(); });
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

    // Retry the same network first
    if (retryCount < MAX_RETRIES) {
        retryCount++;
        log.info("Retrying network %d (%d/%d tries)",
                 currentNetworkIndex, retryCount, MAX_RETRIES);
        startSTA();
        return;
    }

    // Move to the next network
    retryCount = 0;
    currentNetworkIndex++;
	std::size_t networkListSize = ctx.networkStore->count();

    if (currentNetworkIndex < networkListSize) {
        log.warn("Network failed, trying next (%d/%d networks)",
                 currentNetworkIndex + 1, networkListSize);
        startSTA();
        return;
    }

    // All networks exhausted
    log.error("All networks failed — falling back to AP");
    sm.onEvent(WiFiEvent::ConnectFail);
}

void WiFiManager::onFatalError() {
	log.info("onFatalError");
    lastErrorReason = "DriverError";
    sm.onEvent(WiFiEvent::FatalError);
}

//
// Load networks only at boot
//

void WiFiManager::loadInitialNetwork() {
    log.debug("loadInitialNetwork()");
    if (!ctx.networkStore)
        return;

    std::vector<network_store::WiFiNetwork> all;
    if (Result::Ok == ctx.networkStore->loadAllSortedByPriority(all) && !all.empty()) {
        currentNetwork = all.front();
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
    Result r = ctx.wifiInterface->startAp(ctx.apConfig);
    if (r != Result::Ok) {
        log.error("startAP(): startAp() failed (%s)", common::toString(r));
    }
}

void WiFiManager::stopAP() {
    log.info("Stopping AP mode");
    Result r = ctx.wifiInterface->stopAp();
    if (r != Result::Ok) {
        log.error("stopAP(): stopAp() failed (%s)", common::toString(r));
    }
}

void WiFiManager::startSTA() {
    log.info("Starting STA mode");
    auto credOpt = ctx.networkStore->getByIndex(currentNetworkIndex);

	currentNetwork = ctx.networkStore->getByIndex(currentNetworkIndex);
    if (!credOpt) {
        currentNetwork = std::nullopt;
        sm.onEvent(WiFiEvent::ConnectFail);
        return;
    }

    currentNetwork = *credOpt;   // store it for UI layers
    ctx.wifiInterface->connectSta(*credOpt);
}

void WiFiManager::stopSTA() {
    log.info("Stopping STA mode");
    Result r = ctx.wifiInterface->disconnectSta();
    if (r != Result::Ok) {
        log.error("stopSTA(): disconnectSta() failed (%s)", common::toString(r));
    }
}

//
// Retry logic
//

void WiFiManager::retryDriver()
{
    driverRetryCount++;
    log.info("retryDriver(): attempt %d/%d", driverRetryCount, MAX_DRIVER_RETRIES);

    Result r = ctx.wifiInterface->startDriver();
    if (r == Result::Ok) {
        log.info("retryDriver(): driver recovered");
        ctx.embeddedServer->start();
        if (currentNetwork.has_value() && !currentNetwork->ssid.empty()) {
            sm.onEvent(WiFiEvent::NetworkProvided);
        } else {
            sm.onEvent(WiFiEvent::StartProvisioning);
        }
        return;
    }

    if (driverRetryCount < MAX_DRIVER_RETRIES) {
        log.warn("retryDriver(): failed, next attempt in %ums", DRIVER_RETRY_DELAY_MS);
        deferred.runAfter(DRIVER_RETRY_DELAY_MS, [this]() { retryDriver(); });
    } else {
        log.error("retryDriver(): driver failed after %d attempts — invoking fatal handler",
                  MAX_DRIVER_RETRIES);
        if (ctx.onDriverFatal) {
            ctx.onDriverFatal();
        }
    }
}

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

    // 3. Active SSID — empty in AP/provisioning mode (no network loaded)
    if (currentNetwork.has_value()) {
        st.ssid = currentNetwork->ssid;
    }

    // 4. Last error reason (string)
    st.lastErrorReason = lastErrorReason;

    return st;
}

} // namespace wifi_manager
