#pragma once

#include <string>

namespace wifi_manager {

struct ApConfig {
    std::string ssid;
    std::string password; // empty = open AP
    uint8_t channel = 1;
    uint8_t maxConnections = 4;
    bool hidden = false;
};

struct StaConfig {
    std::string ssid;
    std::string password;
};

enum class WiFiState {
    UNINITIALISED, // Before WiFiInterface::start()
    STARTING, // WiFi driver is being initialised

    // Provisioning path
    UNPROVISIONED_AP, // SoftAP running for provisioning
    PROVISIONING, // Handling provisioning requests
    PROVISIONING_TEST_STA, // Testing STA credentials

    // Runtime path
    STA_CONNECTING, // Attempting STA connection
    STA_CONNECTED, // Connected, waiting for IP
    GOT_IP, // Fully online

    // Error / fallback
    STA_DISCONNECTED, // Lost connection
    STA_CONNECT_FAILED, // Credentials invalid or AP unreachable
    FALLBACK_AP, // Returning to AP mode after failure

    // Terminal / maintenance
    STOPPING,
    STOPPED
};

enum class WiFiError {
    NONE,

    // STA failures
    AUTH_FAILED,
    NO_AP_FOUND,
    CONNECTION_TIMEOUT,
    HANDSHAKE_TIMEOUT,

    // Provisioning failures
    INVALID_CREDENTIALS,
    PROVISIONING_TIMEOUT,

    // Driver/system failures
    DRIVER_INIT_FAILED,
    DRIVER_START_FAILED,
    UNKNOWN
};

std::string toString(WiFiState state);

std::string toString(WiFiError err);

WiFiError translateReason(uint8_t reason);


} // namespace wifi_manager