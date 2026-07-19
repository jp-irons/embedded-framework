// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#include "esp_platform/WiFiHelper.hpp"

#include <cstring>          // for strncpy

namespace esp_platform {

using namespace wifi_manager;

wifi_config_t makeStaConfig(const network_store::WiFiNetwork& cred) {
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

wifi_config_t makeApConfig(const ApConfig& apCfg) {
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
}

wifi_ps_type_t toEspPs(WiFiPowerSaveMode mode) {
    switch (mode) {
        case WiFiPowerSaveMode::None:
            return WIFI_PS_NONE;
        case WiFiPowerSaveMode::MinModem:
            return WIFI_PS_MIN_MODEM;
        case WiFiPowerSaveMode::MaxModem:
            return WIFI_PS_MAX_MODEM;
        case WiFiPowerSaveMode::Unset:
            return WIFI_PS_NONE;
    }
    return WIFI_PS_NONE;
}

std::string toString(uint8_t reason) {
    return toString(toWiFiError(reason));
}

WiFiError toWiFiError(uint8_t reason) {
    switch (reason) {
        case 1: return WiFiError::UNKNOWN;
        // -------------------------
        // Authentication failures
        // -------------------------
        // CORRECTED 2026-07-19: this case previously listed 15 (actually
        // WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT per IEEE 802.11/ESP-IDF's
        // wifi_err_reason_t) alongside AUTH_EXPIRE — a real bug, not a
        // relabeling. Reason 15 is exactly what every affected node in the
        // 2026-07-19 fleet outage logged ("STA disconnected: reason=15"),
        // so under the old mapping every one of those disconnects was being
        // silently classified as AUTH_FAILED, and any reason-specific logic
        // (e.g. handshake-timeout backoff in WiFiManager::onDisconnect())
        // would never have actually fired for the failure it was built for.
        // Reason 14 (WIFI_REASON_MIC_FAILURE — a WPA integrity-check
        // failure, not a timeout) is grouped here instead, as the closer
        // analogue of the two.
        case 2:  // WIFI_REASON_AUTH_EXPIRE
        case 14: // WIFI_REASON_MIC_FAILURE
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
        case 203: // WIFI ASSOC_FAIL
            return WiFiError::CONNECTION_TIMEOUT;

        // -------------------------
        // Handshake failures
        // -------------------------
        case 15: // WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT — see note above
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

} // namespace esp_platform
