// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

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

/**
 * Wi-Fi radio power-save mode, applied by the platform layer on driver start
 * and re-applied on every STA reconnect. Mirrors ESP-IDF's wifi_ps_type_t.
 *
 * Unset is the zero value deliberately -- there is no working default here.
 * Different consuming apps have genuinely different right answers (an
 * idle-battery device wants MinModem/MaxModem; a mains-powered device
 * fighting reconnect reliability over a marginal RF link may want None), so
 * a silently-applied default would just be a trap for whichever app didn't
 * think about it. The platform layer rejects Unset at driver-start time
 * instead of falling back to any particular mode -- every consuming app
 * must call FrameworkContext::setWifiPowerSaveMode() explicitly. See the
 * 2026-07-15 WiFi reconnect reliability investigation (sound-capture-node)
 * for the motivating incident.
 */
enum class WiFiPowerSaveMode {
    Unset = 0,
    None,
    MinModem,
    MaxModem,
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

const char* toString(WiFiPowerSaveMode mode);

} // namespace wifi_types