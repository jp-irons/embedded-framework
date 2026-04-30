#pragma once

#include <string>

namespace wifi_types {

enum class WiFiAuthMode { 
	Open, WEP, WPA_PSK, WPA2_PSK, WPA_WPA2_PSK, WPA3_PSK, Unknown 
};

struct WiFiAp {
    std::string ssid; // Human-readable SSID
    uint8_t bssid[6]; // MAC address of the AP
    int rssi; // Signal strength (dBm)
    WiFiAuthMode auth; // Security type
    int channel; // Primary channel
};

struct ApConfig {
    std::string ssid;
    std::string password; // empty = open AP
    uint8_t channel = 1;
    uint8_t maxConnections = 4;
    bool hidden = false;
	WiFiAuthMode auth = WiFiAuthMode::WPA2_PSK;
};

struct WiFiCredential {
    std::string ssid;
    std::string password;
    int priority = 0;
};

struct StaIpInfo {
    std::string ip;
    std::string netmask;
    std::string gateway;
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

enum class WiFiStatus {
    Ok,
    DriverError,
    InvalidCredential,
    ConfigError,
    ConnectError
};

struct WiFiStaStatus {
    std::string state;            // stringified WiFiState
    std::string ssid;             // current or last attempted SSID
    std::string lastErrorReason;  // stringified WiFiError
    bool connected = false;       // true if STA is connected
};

struct IpAddress {
    std::string value;   // "192.168.4.1"
    bool valid = false;
};


const char *toString(WiFiError err);

const char *toString(WiFiAuthMode auth);

const char* toString(WiFiStatus status);

} // namespace wifi_types