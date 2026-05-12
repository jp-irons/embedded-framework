#include "esp_platform/EspWiFiInterface.hpp"

#include "common/Result.hpp"
#include "esp_platform/EspTypeAdapter.hpp"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "logger/Logger.hpp"
#include "wifi_manager/EmbeddedServer.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiHelper.hpp"
#include "wifi_manager/WiFiManager.hpp"
#include "wifi_manager/WiFiTypes.hpp"

// Logs the failing call and returns Result::InternalError from the enclosing
// function.  Must only be used in functions that return common::Result.
#define WIFI_CHECK(x)                                                        \
    do {                                                                     \
        esp_err_t _err = (x);                                                \
        if (_err != ESP_OK) {                                                \
            log.error(#x " failed: %s", esp_err_to_name(_err));             \
            return Result::InternalError;                                    \
        }                                                                    \
    } while (0)

namespace wifi_manager {

using namespace common;

static logger::Logger log{"WiFiInterface"};

EspWiFiInterface::EspWiFiInterface(WiFiContext& ctx)
    : ctx(ctx) {
    log.debug("constructor");
}

Result EspWiFiInterface::startDriver() {
    log.info("startDriver");

    // 1. Create default netifs
    apNetif  = esp_netif_create_default_wifi_ap();
    staNetif = esp_netif_create_default_wifi_sta();
    if (!apNetif || !staNetif) {
        log.error("startDriver: failed to create default netifs");
        return Result::InternalError;
    }

    // 2. Initialize Wi-Fi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    WIFI_CHECK(esp_wifi_init(&cfg));

    // Register WiFi event handler
    WIFI_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
               &EspWiFiInterface::wifiEventHandler, this, nullptr));

    // Register IP event handler
    WIFI_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
               &EspWiFiInterface::ipEventHandler, this, nullptr));

    WIFI_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    WIFI_CHECK(esp_wifi_start());

    driverStarted = true;
    currentMode   = WIFI_MODE_NULL;
    return Result::Ok;
}

Result EspWiFiInterface::stopDriver() {
    log.info("Stopping WiFi driver");
    WIFI_CHECK(esp_wifi_stop());
    WIFI_CHECK(esp_wifi_deinit());
    driverStarted = false;
    return Result::Ok;
}

// ---------------------------------------------------------------------------
// AP MODE
// ---------------------------------------------------------------------------

Result EspWiFiInterface::startAp(const ApConfig& config) {
    log.info("Starting SoftAP: %s", config.ssid.c_str());

    wifi_config_t ap_cfg  = wifi_manager::makeApConfig(config);
    bool          useOpen = false;

    if (config.password.empty()) {
        useOpen = true;
    } else if (config.password.length() < 8) {
        log.warn("AP password '%s' is too short (%d chars). Falling back to OPEN AP.",
                 config.password.c_str(), (int)config.password.length());
        useOpen = true;
    }

    if (useOpen) {
        ap_cfg.ap.authmode    = WIFI_AUTH_OPEN;
        ap_cfg.ap.password[0] = '\0';
    } else {
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char*)ap_cfg.ap.password, config.password.c_str(),
                sizeof(ap_cfg.ap.password));
    }

    log.info("Starting SoftAP: %s (authmode=%s)", config.ssid.c_str(),
             useOpen ? "OPEN" : "WPA2");

    apActive = true;
    wifi_mode_t mode = computeMode();
    WIFI_CHECK(esp_wifi_set_mode(mode));
    WIFI_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    currentMode = mode;
    return Result::Ok;
}

Result EspWiFiInterface::stopAp() {
    log.info("Stopping SoftAP");
    apActive = false;
    wifi_mode_t mode = computeMode();
    WIFI_CHECK(esp_wifi_set_mode(mode));
    currentMode = mode;
    return Result::Ok;
}

// ---------------------------------------------------------------------------
// STA MODE
// ---------------------------------------------------------------------------

wifi_config_t EspWiFiInterface::makeStaConfig(const network_store::WiFiNetwork& cred) {
    wifi_config_t cfg = {};
    auto&         sta = cfg.sta;

    strncpy((char*)sta.ssid,     cred.ssid.c_str(),     sizeof(sta.ssid));
    strncpy((char*)sta.password, cred.password.c_str(), sizeof(sta.password));

    sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta.pmf_cfg.capable    = true;
    sta.pmf_cfg.required   = false;

    return cfg;
}

WiFiStatus EspWiFiInterface::connectSta(const network_store::WiFiNetwork& cred) {
    log.info("Connecting STA to SSID: %s", cred.ssid.c_str());

    wifi_config_t cfg = makeStaConfig(cred);

    // Ensure STA is in a clean idle state
    esp_wifi_disconnect();
    esp_wifi_stop();

    if (esp_wifi_start() != ESP_OK) {
        return WiFiStatus::DriverError;
    }
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        return WiFiStatus::DriverError;
    }
    if (esp_wifi_set_config(WIFI_IF_STA, &cfg) != ESP_OK) {
        return WiFiStatus::ConfigError;
    }
    if (esp_wifi_connect() != ESP_OK) {
        return WiFiStatus::ConnectError;
    }

    staActive   = true;
    currentMode = computeMode();
    return WiFiStatus::Ok;
}

Result EspWiFiInterface::disconnectSta() {
    log.debug("Disconnecting STA");
    staActive = false;
    wifi_mode_t mode = computeMode();
    WIFI_CHECK(esp_wifi_set_mode(mode));
    currentMode = mode;
    return Result::Ok;
}

// ---------------------------------------------------------------------------
// STATIC EVENT HANDLERS
// ---------------------------------------------------------------------------

void EspWiFiInterface::wifiEventHandler(void* arg, esp_event_base_t base,
                                         int32_t id, void* data) {
    log.debug("wifiEventHandler()");
    static_cast<EspWiFiInterface*>(arg)->handleWiFiEvent(base, id, data);
}

void EspWiFiInterface::ipEventHandler(void* arg, esp_event_base_t base,
                                       int32_t id, void* data) {
    log.debug("ipEventHandler()");
    static_cast<EspWiFiInterface*>(arg)->handleIPEvent(base, id, data);
}

// ---------------------------------------------------------------------------
// INSTANCE EVENT HANDLERS
// ---------------------------------------------------------------------------

void EspWiFiInterface::handleWiFiEvent(esp_event_base_t base, int32_t id, void* data) {
    log.debug("handleWiFiEvent");
    if (!ctx.wifiManager)
        return;

    switch (id) {
        case WIFI_EVENT_STA_CONNECTED:
            ctx.wifiManager->onConnectSuccess();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ctx.wifiManager->onDisconnect();
            break;
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            ctx.wifiManager->onConnectFail();
            break;
        case IP_EVENT_STA_GOT_IP:
            ctx.wifiManager->onConnectSuccess();
            break;
        default:
            break;
    }
}

void EspWiFiInterface::handleIPEvent(esp_event_base_t base, int32_t id, void* data) {
    log.debug("handleIPEvent");
    if (id != IP_EVENT_STA_GOT_IP)
        return;

    auto* event = static_cast<ip_event_got_ip_t*>(data);

    char ipStr[16], maskStr[16], gwStr[16];
    snprintf(ipStr,   sizeof(ipStr),   IPSTR, IP2STR(&event->ip_info.ip));
    snprintf(maskStr, sizeof(maskStr), IPSTR, IP2STR(&event->ip_info.netmask));
    snprintf(gwStr,   sizeof(gwStr),   IPSTR, IP2STR(&event->ip_info.gw));

    StaIpInfo info{.ip = ipStr, .netmask = maskStr, .gateway = gwStr};
    log.info("Got IP: %s", ipStr);

    if (ctx.wifiManager) {
        ctx.wifiManager->onStaGotIp(info);
    }
}

// ---------------------------------------------------------------------------
// SCAN
// ---------------------------------------------------------------------------

Result EspWiFiInterface::scan(std::vector<WiFiAp>& outAps) {
    log.debug("scan");
    if (!driverStarted) {
        log.error("scan() unsupported: driver not started");
        return Result::Unsupported;
    }

    const bool initialStaActive = staActive;

    Result r = setStaState(true);
    if (r != Result::Ok) {
        log.error("scan() setStaState true error");
        return Result::InternalError;
    }

    wifi_scan_config_t scanConfig = {};
    log.debug("scan starting scan");
    esp_err_t err = esp_wifi_scan_start(&scanConfig, true);
    if (err != ESP_OK) {
        r = esp_platform::toResult(err);
        log.error("scan esp_wifi_scan_start returned error");
        setStaState(initialStaActive);
        return r;
    }

    uint16_t apCount = 0;
    err = esp_wifi_scan_get_ap_num(&apCount);
    if (err != ESP_OK) {
        r = esp_platform::toResult(err);
        log.error("scan() esp_wifi_scan_get_ap_num error");
        setStaState(initialStaActive);
        return r;
    }

    std::vector<wifi_ap_record_t> records(apCount);
    err = esp_wifi_scan_get_ap_records(&apCount, records.data());
    if (err != ESP_OK) {
        r = esp_platform::toResult(err);
        log.error("scan() esp_wifi_scan_get_ap_records error");
        setStaState(initialStaActive);
        return r;
    }

    outAps.clear();
    outAps.reserve(apCount);
    for (const auto& rec : records) {
        WiFiAp ap;
        ap.ssid    = reinterpret_cast<const char*>(rec.ssid);
        memcpy(ap.bssid, rec.bssid, 6);
        ap.rssi    = rec.rssi;
        ap.channel = rec.primary;
        ap.auth    = toAuthMode(rec.authmode);
        outAps.push_back(ap);
    }

    setStaState(initialStaActive);
    return Result::Ok;
}

// ---------------------------------------------------------------------------
// IP QUERIES
// ---------------------------------------------------------------------------

IpAddress EspWiFiInterface::getApIp() const {
    esp_netif_ip_info_t ip;
    if (!apNetif || esp_netif_get_ip_info(apNetif, &ip) != ESP_OK)
        return {};
    char buf[32];
    esp_ip4addr_ntoa(&ip.ip, buf, sizeof(buf));
    return {std::string(buf), true};
}

IpAddress EspWiFiInterface::getStaIp() const {
    esp_netif_ip_info_t ip;
    if (!staNetif || esp_netif_get_ip_info(staNetif, &ip) != ESP_OK)
        return {};
    char buf[32];
    esp_ip4addr_ntoa(&ip.ip, buf, sizeof(buf));
    return {std::string(buf), true};
}

// ---------------------------------------------------------------------------
// PRIVATE HELPERS
// ---------------------------------------------------------------------------

WiFiAuthMode EspWiFiInterface::toAuthMode(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN:         return WiFiAuthMode::Open;
        case WIFI_AUTH_WEP:          return WiFiAuthMode::WEP;
        case WIFI_AUTH_WPA_PSK:      return WiFiAuthMode::WPA_PSK;
        case WIFI_AUTH_WPA2_PSK:     return WiFiAuthMode::WPA2_PSK;
        case WIFI_AUTH_WPA_WPA2_PSK: return WiFiAuthMode::WPA_WPA2_PSK;
        case WIFI_AUTH_WPA3_PSK:     return WiFiAuthMode::WPA3_PSK;
        default:                     return WiFiAuthMode::Unknown;
    }
}

wifi_mode_t EspWiFiInterface::computeMode() const {
    if (apActive && staActive) return WIFI_MODE_APSTA;
    if (apActive)              return WIFI_MODE_AP;
    if (staActive)             return WIFI_MODE_STA;
    return WIFI_MODE_NULL;
}

Result EspWiFiInterface::setStaState(bool enable) {
    log.debug("setStaState %d", enable);
    if (staActive == enable)
        return Result::Ok;

    staActive = enable;
    wifi_mode_t mode = computeMode();

    esp_err_t err = esp_wifi_set_mode(mode);
    if (err != ESP_OK) {
        Result r = esp_platform::toResult(err);
        log.error("setStaState() esp_wifi_set_mode error");
        staActive = !enable; // rollback
        return r;
    }

    currentMode = mode;
    return Result::Ok;
}

} // namespace wifi_manager
