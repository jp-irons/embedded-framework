// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#include "wifi_manager/WiFiManager.hpp"

#include "common/Result.hpp"
#include "network_store/NetworkStore.hpp"
#include "logger/Logger.hpp"
#include "wifi_manager/EmbeddedServer.hpp"
#include "wifi_manager/MdnsInterface.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiInterface.hpp"

namespace wifi_manager {

static logger::Logger log{WiFiManager::TAG};

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

void WiFiManager::loop() {}

//
// ESP-IDF event handlers
//

void WiFiManager::onStateChanged(WiFiState oldState, WiFiState newState) {
    log.debug("onStateChanged: %s -> %s", WiFiStateMachine::toString(oldState), WiFiStateMachine::toString(newState));

    switch (newState) {

        case WiFiState::AP_Mode:
            retryCount = 0;
            // Also reset the network index here, not just on STA_Connected —
            // otherwise a node that exhausts retries and advances past the
            // end of its network list stays wedged on that invalid index
            // forever. Every future retry (including HubRegistrar's
            // app-level self-heal) then fails instantly back to AP_Mode
            // without ever attempting the real network again. Found
            // 2026-07-12 investigating soundcapture174's sustained offline
            // state — this is the other half of the 2026-07-03 fix above.
            currentNetworkIndex = 0;
            startAP();
            // mDNS is started in onApStarted() once WIFI_EVENT_AP_START fires,
            // ensuring the AP netif is live before we announce.
            if (ctx.embeddedServer)
                ctx.embeddedServer->startProvisioningMode();
            break;

        case WiFiState::STA_Connecting:
			startSTA();
            break;

        case WiFiState::STA_Connected:
            retryCount = 0;
            // A stable connection means whatever index we're on is known-good
            // right now, but currentNetworkIndex has no other reset path —
            // onDisconnect() only ever increments it. Left un-reset, a node
            // that once had to advance past network 0 (e.g. during a
            // transient outage, or after its stored network list changed
            // size/order) stays pinned on that higher index for the rest of
            // the boot. The next disconnect then retries that same index; if
            // it's no longer valid the manager falls straight to AP mode
            // with no real connection attempt and no way back to STA short
            // of a reboot. Resetting here means a fresh disconnect always
            // starts its retry/advance logic from the current network list's
            // actual first entry. Found 2026-07-03 — soundcapture160 got
            // stuck exactly this way after a routine reconnect.
            currentNetworkIndex = 0;
            break;
        case WiFiState::Idle:
            stopAP();
            stopSTA();
            if (ctx.mdnsInterface) ctx.mdnsInterface->stop();
            break;

        case WiFiState::DriverFailed:
            driverRetryCount = 0;
            log.warn("Driver failed — scheduling retry in %ums", DRIVER_RETRY_DELAY_MS);
            ctx.timer->runAfter(DRIVER_RETRY_DELAY_MS, [this]() { retryDriver(); });
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

void WiFiManager::onDisconnect(WiFiError reason) {
    // Stop mDNS — IP is no longer valid
    if (ctx.mdnsInterface) ctx.mdnsInterface->stop();

    // startSTA() is called directly below (not via sm.onEvent(Disconnect))
    // so the retry/next-network/exhausted branching here can run regardless
    // of which state we were actually in (STA_Connected, or already
    // STA_Connecting via onConnectTimeout()). Correct the reported state to
    // STA_Connecting so getStaStatus() doesn't keep reporting a stale
    // "STA_Connected" for the whole outage — this was the bug found
    // 2026-06-30 investigating node 170's overnight drop.
    sm.markState(WiFiState::STA_Connecting);

    // Retry the same network first
    if (retryCount < MAX_RETRIES) {
        retryCount++;
        uint32_t delayMs = retryDelayMsFor(reason, retryCount);
        if (delayMs > 0) {
            log.info("Retrying network %d (%d/%d tries) in %ums (reason=%s)",
                     currentNetworkIndex, retryCount, MAX_RETRIES,
                     static_cast<unsigned>(delayMs), toString(reason));
            ctx.timer->runAfter(delayMs, [this]() { startSTA(); });
        } else {
            log.info("Retrying network %d (%d/%d tries)",
                     currentNetworkIndex, retryCount, MAX_RETRIES);
            startSTA();
        }
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

void WiFiManager::onConnectTimeout(uint32_t attemptId) {
    if (attemptId != connectAttemptId) {
        return;   // a newer attempt has already superseded this one
    }
    if (sm.getState() != WiFiState::STA_Connecting) {
        return;   // already resolved (connected, failed, or otherwise moved on)
    }

    log.warn("STA connect attempt timed out after %ums with no driver event — forcing retry",
              STA_CONNECT_TIMEOUT_MS);

    // Reuse the existing disconnect-handling chain: retry the same network,
    // then advance through the network list, then fall back to AP mode.
    onDisconnect();
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

    // Arm a per-attempt watchdog. If neither onConnectSuccess() nor
    // onDisconnect()/onConnectFail() fires before this expires, the driver
    // never resolved the connect attempt one way or the other — force it.
    uint32_t attempt = ++connectAttemptId;
    ctx.timer->runAfter(STA_CONNECT_TIMEOUT_MS, [this, attempt]() {
        onConnectTimeout(attempt);
    });
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
        ctx.timer->runAfter(DRIVER_RETRY_DELAY_MS, [this]() { retryDriver(); });
    } else {
        log.error("retryDriver(): driver failed after %d attempts — invoking fatal handler",
                  MAX_DRIVER_RETRIES);
        if (ctx.onDriverFatal) {
            ctx.onDriverFatal();
        }
    }
}

uint32_t WiFiManager::retryDelayMsFor(WiFiError reason, int retryAttempt) {
    if (reason != WiFiError::HANDSHAKE_TIMEOUT) {
        return 0;
    }
    // Growing delay across the 3 same-network retries — see this method's
    // doc comment in the header for why. retryAttempt is 1-based (the
    // attempt about to be made), so this is 2s / 5s / 10s.
    switch (retryAttempt) {
        case 1:  return 2000;
        case 2:  return 5000;
        default: return 10000;  // retryAttempt >= 3
    }
}

void WiFiManager::onApStarted() {
    log.info("onApStarted() — AP netif is live");

    // Start mDNS now that the AP interface is confirmed up.
    if (ctx.mdnsInterface) {
        ctx.mdnsInterface->start(ctx.mdnsHostname);
        log.info("Device reachable at https://%s.local", ctx.mdnsHostname.c_str());

        // Re-announce after 2 s to recover from any dropped initial multicast.
        log.debug("mDNS re-announce scheduled in 2000ms");
        ctx.timer->runAfter(2000, [this]() {
            log.debug("mDNS re-announce timer fired");
            if (ctx.mdnsInterface) ctx.mdnsInterface->reannounce();
        });
    }
}

void WiFiManager::onStaGotIp(const StaIpInfo &info) {
    log.info("STA got IP: %s (mask %s, gw %s)", info.ip.c_str(), info.netmask.c_str(), info.gateway.c_str());

    // Advertise via mDNS now that we have a confirmed IP.
    if (ctx.mdnsInterface) {
        ctx.mdnsInterface->start(ctx.mdnsHostname);
        log.info("Device reachable at https://%s.local", ctx.mdnsHostname.c_str());

        // Re-announce after 2 s to recover from any dropped initial multicast.
        log.debug("mDNS re-announce scheduled in 2000ms");
        ctx.timer->runAfter(2000, [this]() {
            log.debug("mDNS re-announce timer fired");
            if (ctx.mdnsInterface) ctx.mdnsInterface->reannounce();
        });
    }

    // Feed the state machine
    sm.onEvent(WiFiEvent::ConnectSuccess);

    // Start runtime server or other post-connect actions
    if (ctx.embeddedServer) {
        ctx.embeddedServer->startRuntimeMode();
    }
}

void WiFiManager::forceReconnect() {
    log.warn("forceReconnect(): app-level self-heal — forcing STA disconnect/reconnect");
    onDisconnect();
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
