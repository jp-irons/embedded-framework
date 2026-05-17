#pragma once

#include <string>

namespace wifi_manager {

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

/**
 * Controls whether a MAC-derived suffix is appended to a hostname or SSID.
 *   None      — use the prefix string as-is
 *   MacShort  — append last 3 MAC bytes as 6 lowercase hex chars, e.g. "name-a1b2c3"
 *   MacFull   — append all 6 MAC bytes as 12 lowercase hex chars, e.g. "name-0011a1b2c3"
 */
enum class SuffixPolicy { None, MacShort, MacFull };

struct ApConfig {
    std::string ssid;
    std::string password;                        // empty = open AP
    uint8_t     channel        = 1;
    uint8_t     maxConnections = 4;
    bool        hidden         = false;
    WiFiAuthMode auth          = WiFiAuthMode::WPA2_PSK;
    SuffixPolicy ssidSuffix    = SuffixPolicy::None; // MAC suffix appended to ssid at start()
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