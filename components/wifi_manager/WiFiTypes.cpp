#include <string>
#include "include/wifi_manager/WiFiTypes.hpp"

namespace wifi_manager {

std::string toString(WiFiState state) {
    switch (state) {
        case WiFiState::UNINITIALISED:
            return "UNINITIALISED";
        case WiFiState::STARTING:
            return "STARTING";

        // Provisioning path
        case WiFiState::UNPROVISIONED_AP:
            return "UNPROVISIONED_AP";

        case WiFiState::PROVISIONING:
            return "PROVISIONING";
        case WiFiState::PROVISIONING_TEST_STA:
            return "ROVISIONING_TEST_STA";

        // Runtime path
        case WiFiState::STA_CONNECTING:
            return "STA_CONNECTING";
        case WiFiState::STA_CONNECTED:
            return "STA_CONNECTED";
        case WiFiState::GOT_IP:
            return "GOT_IP";

        // Error / fallback
        case WiFiState::STA_DISCONNECTED:
            return "STA_DISCONNECTE";
        case WiFiState::STA_CONNECT_FAILED:
            return "STA_CONNECT_FAILED";
        case WiFiState::FALLBACK_AP:
            return "FALLBACK_AP";

        // Terminal / maintenance
        case WiFiState::STOPPING:
            return "STOPPING";
        case WiFiState::STOPPED:
            return "STOPPED";
    }
    return "UNKNOWN";
}

std::string toString(WiFiError err) {
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

std::string toString(uint8_t reason){
	return toString(translateReason(reason));
}

WiFiError translateReason(uint8_t reason)
{
    switch (reason) {

        // -------------------------
        // Authentication failures
        // -------------------------
        case 2:   // WIFI_REASON_AUTH_EXPIRE
        case 15:  // WIFI_REASON_AUTH_FAIL
            return WiFiError::AUTH_FAILED;

        // -------------------------
        // AP not found
        // -------------------------
        case 201: // WIFI_REASON_NO_AP_FOUND
            return WiFiError::NO_AP_FOUND;

        // -------------------------
        // Timeouts / general failures
        // -------------------------
        case 4:   // WIFI_REASON_ASSOC_EXPIRE
        case 200: // WIFI_REASON_BEACON_TIMEOUT
        case 202: // WIFI_REASON_CONNECTION_FAIL
            return WiFiError::CONNECTION_TIMEOUT;

        // -------------------------
        // Handshake failures
        // -------------------------
        case 204: // WIFI_REASON_HANDSHAKE_TIMEOUT
            return WiFiError::HANDSHAKE_TIMEOUT;

        // -------------------------
        // AP kicked us off (normal)
        // -------------------------
        case 3:   // WIFI_REASON_ASSOC_LEAVE
            return WiFiError::NONE;

        // -------------------------
        // Unknown / unhandled
        // -------------------------
        default:
            return WiFiError::UNKNOWN;
    }
}} // namespace wifi_manager