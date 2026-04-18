#include "wifi_manager/WiFiHelper.hpp"

#include <cstring>          // for strncpy

namespace wifi_manager {
	
using namespace wifi_types;

wifi_config_t makeStaConfig(const WiFiCredential& cred) {
    wifi_config_t cfg = {};
    auto& sta = cfg.sta;

    // SSID + password
    strncpy(reinterpret_cast<char*>(sta.ssid),
            cred.ssid.c_str(),
            sizeof(sta.ssid));

    strncpy(reinterpret_cast<char*>(sta.password),
            cred.password.c_str(),
            sizeof(sta.password));

    // Modern safe defaults
    sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta.pmf_cfg.capable = true;
    sta.pmf_cfg.required = false;

    return cfg;
}

wifi_config_t makeApConfig(const wifi_types::ApConfig& apCfg) {
    wifi_config_t cfg = {};
    auto& ap = cfg.ap;

    strncpy(reinterpret_cast<char*>(ap.ssid),
            apCfg.ssid.c_str(),
            sizeof(ap.ssid));

    strncpy(reinterpret_cast<char*>(ap.password),
            apCfg.password.c_str(),
            sizeof(ap.password));

    ap.ssid_len       = apCfg.ssid.length();
    ap.channel        = apCfg.channel;
    ap.max_connection = apCfg.maxConnections;
    ap.authmode       = toEspAuth(apCfg.auth);
    return cfg;
}

wifi_auth_mode_t toEspAuth(WiFiAuthMode mode) {
    switch (mode) {
        case WiFiAuthMode::Open:
            return WIFI_AUTH_OPEN;
        case WiFiAuthMode::WEP:
            return WIFI_AUTH_WEP;
        case WiFiAuthMode::WPA_PSK:
            return WIFI_AUTH_WPA_PSK;
        case WiFiAuthMode::WPA2_PSK:
            return WIFI_AUTH_WPA2_PSK;
        case WiFiAuthMode::WPA_WPA2_PSK:
            return WIFI_AUTH_WPA_WPA2_PSK;
        case WiFiAuthMode::WPA3_PSK:
            return WIFI_AUTH_WPA3_PSK;
        default:
            return WIFI_AUTH_OPEN;
    }
};

std::string toString(uint8_t reason) {
    return toString(toWiFiError(reason));
}

WiFiError toWiFiError(uint8_t reason) {
    switch (reason) {
		case 1: return WiFiError::UNKNOWN;
        // -------------------------
        // Authentication failures
        // -------------------------
        case 2: // WIFI_REASON_AUTH_EXPIRE
        case 15: // WIFI_REASON_AUTH_FAIL
        case 202: // WIFI_REASON_CONNECTION_FAIL
            return WiFiError::AUTH_FAILED;

        // -------------------------
        // AP not found
        // -------------------------
        case 201: // WIFI_REASON_NO_AP_FOUND
            return WiFiError::NO_AP_FOUND;

        // -------------------------
        // Timeouts / general failures
        // -------------------------
        case 4: // WIFI_REASON_ASSOC_EXPIRE
        case 200: // WIFI_REASON_BEACON_TIMEOUT
        case 203: // WIFI ASSOC_FAIL";
         	return WiFiError::CONNECTION_TIMEOUT;

       // -------------------------
        // Handshake failures
        // -------------------------
		case 14: //4WAY_HANDSHAKE_TIMEOUT
        case 204: // WIFI_REASON_HANDSHAKE_TIMEOUT
            return WiFiError::HANDSHAKE_TIMEOUT;

        // -------------------------
        // AP kicked us off (normal)
        // -------------------------
        case 3: // WIFI_REASON_ASSOC_LEAVE
		case 8: // WIFI_REASON_ASSOC_LEAVE duplicate
            return WiFiError::NONE;

        // -------------------------
        // Unknown / unhandled
        // -------------------------
        default:
            return WiFiError::UNKNOWN;
    }
}

} // namespace wifi_manager