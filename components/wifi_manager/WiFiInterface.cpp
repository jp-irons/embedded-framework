#include "wifi_manager/WiFiInterface.hpp"

#include "common/Result.hpp"
#include "device/EspTypeAdapter.hpp"
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

namespace wifi_manager {

using namespace common;

static logger::Logger log{"WiFiInterface"};

WiFiInterface::WiFiInterface(WiFiContext &ctx)
    : ctx(ctx) {
    log.debug("constructor");
}

void WiFiInterface::startDriver() {
    log.info("startDriver");

    // 1. Create default netifs
    apNetif = esp_netif_create_default_wifi_ap();
    staNetif = esp_netif_create_default_wifi_sta();

    // 2. Initialize Wi-Fi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	// TODO get rid of the ESP_ERROR_CHECKs
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register WiFi event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiInterface::wifiEventHandler,
                                                        this, nullptr));

    // Register IP event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WiFiInterface::ipEventHandler,
                                                        this, nullptr));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start()); // ← place it here
    driverStarted = true;
    currentMode = WIFI_MODE_NULL; // not set until AP or STA is started
}

void WiFiInterface::stopDriver() {
    log.info("Stopping WiFi driver");
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
}

/**
 * AP MODE
 */
void WiFiInterface::startAp(const ApConfig &config) {
    log.info("Starting SoftAP: %s", config.ssid.c_str());
    /*        ***********************   */
    wifi_config_t ap_cfg = wifi_manager::makeApConfig(config);
    bool useOpenAp = false;

    if (config.password.empty()) {
        useOpenAp = true;
    } else if (config.password.length() < 8) {
        log.warn("AP password '%s' is too short (%d chars). Falling back to OPEN AP.", config.password.c_str(),
                 (int) config.password.length());
        useOpenAp = true;
    }

    if (useOpenAp) {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
        ap_cfg.ap.password[0] = '\0'; // ensure empty
    } else {
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char *) ap_cfg.ap.password, config.password.c_str(), sizeof(ap_cfg.ap.password));
    }

    log.info("Starting SoftAP: %s (authmode=%s)", config.ssid.c_str(), useOpenAp ? "OPEN" : "WPA2");
    apActive = true;
    wifi_mode_t mode = computeMode();
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    currentMode = mode;
}

void WiFiInterface::stopAp() {
    log.info("Stopping SoftAP");
    apActive = false;
    wifi_mode_t mode = computeMode();
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    currentMode = mode;
}

wifi_config_t WiFiInterface::makeStaConfig(const credential_store::WiFiCredential &cred) {
    wifi_config_t cfg = {};
    auto &sta = cfg.sta;

    strncpy((char *) sta.ssid, cred.ssid.c_str(), sizeof(sta.ssid));
    strncpy((char *) sta.password, cred.password.c_str(), sizeof(sta.password));

    // These are safe, modern defaults
    sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta.pmf_cfg.capable = true;
    sta.pmf_cfg.required = false;

    return cfg;
}

/**
 * STA MODE
 */
WiFiStatus WiFiInterface::connectSta(const credential_store::WiFiCredential &cred) {
    log.info("Connecting STA to SSID: %s", cred.ssid.c_str());

    wifi_config_t cfg = makeStaConfig(cred);

    staActive = true;
    wifi_mode_t mode = computeMode();

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
        return WiFiStatus::DriverError;

    if (esp_wifi_set_config(WIFI_IF_STA, &cfg) != ESP_OK)
        return WiFiStatus::ConfigError;

    if (esp_wifi_connect() != ESP_OK)
        return WiFiStatus::ConnectError;

    currentMode = mode;
    return WiFiStatus::Ok;
}

void WiFiInterface::disconnectSta() {
    log.debug("Disconnecting STA");
    staActive = false;
    wifi_mode_t mode = computeMode();

    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    currentMode = mode;
}

/**
 * STATIC EVENT HANDLERS
 */
void WiFiInterface::wifiEventHandler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    log.debug("wifiEventHandler()");
    auto *self = static_cast<WiFiInterface *>(arg);
    self->handleWiFiEvent(base, id, data);
}

void WiFiInterface::ipEventHandler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    log.debug("ipEventHandler()");
    auto *self = static_cast<WiFiInterface *>(arg);
    self->handleIPEvent(base, id, data);
}

/**
 * INSTANCE EVENT HANDLERS
 */

void WiFiInterface::handleWiFiEvent(esp_event_base_t base, int32_t id, void *data) {
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

void WiFiInterface::handleIPEvent(esp_event_base_t base, int32_t id, void *data) {
    log.debug("handleIPEvent");
    if (id != IP_EVENT_STA_GOT_IP) {
        return;
    }

    auto *event = static_cast<ip_event_got_ip_t *>(data);

    // Convert ESP-IDF IPs to strings
    char ipStr[16];
    char maskStr[16];
    char gwStr[16];

    snprintf(ipStr, sizeof(ipStr), IPSTR, IP2STR(&event->ip_info.ip));
    snprintf(maskStr, sizeof(maskStr), IPSTR, IP2STR(&event->ip_info.netmask));
    snprintf(gwStr, sizeof(gwStr), IPSTR, IP2STR(&event->ip_info.gw));

    StaIpInfo info{.ip = ipStr, .netmask = maskStr, .gateway = gwStr};

    log.info("Got IP: %s", ipStr);

    if (ctx.wifiManager) {
        ctx.wifiManager->onStaGotIp(info);
    }
}

Result WiFiInterface::scan(std::vector<WiFiAp> &outAps) {
    log.debug("scan");
    if (!driverStarted) {
        log.error("scan() unsupported: driver not started");
        return Result::Unsupported;
    }

    const bool initialStaActive = staActive;

    // Ensure STA is enabled
    Result r = setStaState(true);
    if (r != Result::Ok) {
        log.error("scan() setStaState true error '%s'", r);
        return Result::InternalError;
    }

    wifi_scan_config_t scanConfig = {};
    log.debug("scan starting scan");
    esp_err_t err = esp_wifi_scan_start(&scanConfig, true);
    if (err != ESP_OK) {
        Result r = device::toResult(err);
        log.error("scan esp_wifi_scan_start returned %s", r);
        setStaState(initialStaActive); // restore before returning
        return r;
    }

    // Retrieve AP records
    uint16_t apCount = 0;
    err = esp_wifi_scan_get_ap_num(&apCount);
    if (err != ESP_OK) {
        Result r = device::toResult(err);
        log.error("scan() esp_wifi_scan_get_ap_number error %s", r);
        setStaState(initialStaActive);
        return r;
    }

    std::vector<wifi_ap_record_t> records(apCount);
    err = esp_wifi_scan_get_ap_records(&apCount, records.data());
    if (err != ESP_OK) {
        Result r = device::toResult(err);
        log.error("scan() esp_wifi_scan_get_ap_records error %s", r);
        setStaState(initialStaActive);
        return r;
    }

    // Convert to domain type
    outAps.clear();
    outAps.reserve(apCount);

    for (const auto &rec : records) {
        WiFiAp ap;
        ap.ssid = reinterpret_cast<const char *>(rec.ssid);
        memcpy(ap.bssid, rec.bssid, 6);
        ap.rssi = rec.rssi;
        ap.channel = rec.primary;
        ap.auth = toAuthMode(rec.authmode);
        outAps.push_back(ap);
    }

    // Restore original STA state
    setStaState(initialStaActive);

    return Result::Ok;
}

WiFiAuthMode WiFiInterface::toAuthMode(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN:
            return WiFiAuthMode::Open;
        case WIFI_AUTH_WEP:
            return WiFiAuthMode::WEP;
        case WIFI_AUTH_WPA_PSK:
            return WiFiAuthMode::WPA_PSK;
        case WIFI_AUTH_WPA2_PSK:
            return WiFiAuthMode::WPA2_PSK;
        case WIFI_AUTH_WPA_WPA2_PSK:
            return WiFiAuthMode::WPA_WPA2_PSK;
        case WIFI_AUTH_WPA3_PSK:
            return WiFiAuthMode::WPA3_PSK;
        default:
            return WiFiAuthMode::Unknown;
    }
}

wifi_mode_t WiFiInterface::computeMode() const {
    if (apActive && staActive)
        return WIFI_MODE_APSTA;
    if (apActive)
        return WIFI_MODE_AP;
    if (staActive)
        return WIFI_MODE_STA;
    return WIFI_MODE_NULL;
}

Result WiFiInterface::setStaState(bool enable) {
    log.debug("setStaState %d", enable);
    if (staActive == enable) {
        return Result::Ok; // nothing to do
    }

    staActive = enable;
    wifi_mode_t mode = computeMode();

    esp_err_t err = esp_wifi_set_mode(mode);
    if (err != ESP_OK) {
        Result r = device::toResult(err);
        log.error("setStaState() esp_wifi_set_mode error %s", r);
        // rollback
        staActive = !enable;
        return r;
    }

    currentMode = mode;
    return Result::Ok;
}

IpAddress WiFiInterface::getApIp() const {
    esp_netif_ip_info_t ip;
    if (!apNetif || esp_netif_get_ip_info(apNetif, &ip) != ESP_OK) {
        return {};
    }

    char buf[32];
    esp_ip4addr_ntoa(&ip.ip, buf, sizeof(buf));
    return {std::string(buf), true};
}

IpAddress WiFiInterface::getStaIp() const {
    esp_netif_ip_info_t ip;
    if (!staNetif || esp_netif_get_ip_info(staNetif, &ip) != ESP_OK) {
        return {};
    }

    char buf[32];
    esp_ip4addr_ntoa(&ip.ip, buf, sizeof(buf));
    return {std::string(buf), true};
}

} // namespace wifi_manager