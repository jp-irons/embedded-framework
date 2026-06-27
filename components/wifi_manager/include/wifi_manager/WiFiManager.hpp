// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"
#include "wifi_manager/WiFiTypes.hpp"

namespace wifi_manager {

class WiFiManager {
  public:
    static constexpr const char* TAG = "WiFiManager";

    explicit WiFiManager(WiFiContext &ctx);

    void start();
    void loop();

    // State machine integration
    void onStateChanged(WiFiState oldState, WiFiState newState);

    // ESP-IDF event callbacks
    void onConnectSuccess();
    void onConnectFail();
    void onDisconnect();
    void onFatalError();
    void onStaGotIp(const StaIpInfo &info);
    void onApStarted();
    WiFiStaStatus getStaStatus() const;

  private:
    WiFiContext &ctx;
    WiFiStateMachine sm;

    std::size_t currentNetworkIndex = 0;
	std::optional<network_store::WiFiNetwork> currentNetwork;
	int retryCount = 0;
    static constexpr int MAX_RETRIES = 3;
    int driverRetryCount = 0;
    static constexpr int MAX_DRIVER_RETRIES = 3;
    static constexpr uint32_t DRIVER_RETRY_DELAY_MS = 3000;
    std::string lastErrorReason;

    // Watchdog for a single STA connection attempt — covers the case where
    // esp_wifi_connect() never resolves to either WIFI_EVENT_STA_CONNECTED or
    // WIFI_EVENT_STA_DISCONNECTED, leaving the state machine wedged in
    // STA_Connecting indefinitely.
    static constexpr uint32_t STA_CONNECT_TIMEOUT_MS = 20000;
    uint32_t connectAttemptId = 0;
    void onConnectTimeout(uint32_t attemptId);

    // Load networks only at boot
    void loadInitialNetwork();

    // Wi-Fi actions
    void startAP();
    void stopAP();
    void startSTA();
    void stopSTA();

    // Retry logic
    void scheduleRetry();
    void retryDriver();
};

} // namespace wifi_manager
