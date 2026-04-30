#include "wifi_types/WiFiTypes.hpp"

namespace wifi_types {

const char *toString(WiFiAuthMode auth) {
    switch (auth) {
        case WiFiAuthMode::Open:
            return "Open";
        case WiFiAuthMode::Unknown:
            return "Unknown";
        case WiFiAuthMode::WEP:
            return "WEP";
        case WiFiAuthMode::WPA_PSK:
            return "WPA_PSK";
        case WiFiAuthMode::WPA2_PSK:
            return "WPA2_PSK";
        case WiFiAuthMode::WPA3_PSK:
            return "WPA3_PSK";
        case WiFiAuthMode::WPA_WPA2_PSK:
            return "WPA_WPA2_PSK";
    }
    return "UNKNOWN";
}

const char *toString(WiFiError err) {
    switch (err) {
        case WiFiError::NONE:
            return "NONE";

        // STA failures
        case WiFiError::AUTH_FAILED:
            return "AUTH_FAILED";
        case WiFiError::NO_AP_FOUND:
            return "NO_AP_FOUND";
        case WiFiError::CONNECTION_TIMEOUT:
            return "CONNECTION_TIMEOUT";
        case WiFiError::HANDSHAKE_TIMEOUT:
            return "HANDSHAKE_TIMEOUT";

        // Provisioning failures
        case WiFiError::INVALID_CREDENTIALS:
            return "INVALID_CREDENTIALS";
        case WiFiError::PROVISIONING_TIMEOUT:
            return "PROVISIONING_TIMEOUT";

        // Driver/system failures
        case WiFiError::DRIVER_INIT_FAILED:
            return "DRIVER_INIT_FAILED";
        case WiFiError::DRIVER_START_FAILED:
            return "DRIVER_START_FAILED";
        case WiFiError::UNKNOWN:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

} // namespace wifi_manager