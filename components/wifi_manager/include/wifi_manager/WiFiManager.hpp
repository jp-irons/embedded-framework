#pragma once

#include "wifi_manager/MdnsManager.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"
#include "wifi_manager/WiFiTypes.hpp"

namespace wifi_manager {

class WiFiManager {
  public:
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

    MdnsManager mdns;

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
