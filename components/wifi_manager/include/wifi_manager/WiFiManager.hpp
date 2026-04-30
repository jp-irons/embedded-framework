#pragma once

#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"
#include "device/DeferredExecutor.hpp"
#include "wifi_manager/WiFiTypes.hpp"

namespace wifi_manager {

class WiFiManager {
public:
    explicit WiFiManager(WiFiContext& ctx);

    void start();
    void loop();

	// State machine integration
	void onStateChanged(WiFiState oldState, WiFiState newState);

    // ESP-IDF event callbacks
    void onConnectSuccess();
    void onConnectFail();
    void onDisconnect();
    void onFatalError();
	void onStaGotIp(const StaIpInfo& info);
	WiFiStaStatus getStaStatus() const;

private:
    WiFiContext& ctx;
    WiFiStateMachine sm;

    int retryCount = 0;
    static constexpr int MAX_RETRIES = 3;
	std::string lastErrorReason;
	
    device::DeferredExecutor deferred;

    // Load credentials only at boot
    void loadInitialCredential();

    // Wi-Fi actions
    void startAP();
    void stopAP();
    void startSTA();
    void stopSTA();

    // Retry logic
    void scheduleRetry();
};

} // namespace wifi_manager
