#include "wifi_manager/WiFiInterface.hpp"

#include "common/Result.hpp"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "logger/Logger.hpp"
#include "wifi_manager/ProvisioningServer.hpp"
#include "wifi_manager/RuntimeServer.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"
#include "wifi_manager/WiFiTypes.hpp"

namespace wifi_manager {

static logger::Logger log{"WiFiInterface"};

WiFiInterface::WiFiInterface(WiFiContext &ctx)
    : ctx(ctx) {
    log.debug("constructor");
}

/**
 * Driver init — ONLY WiFi driver + handlers.
 * DO NOT create event loop or netifs here.
 * Those belong in app_main().
 */
void WiFiInterface::startDriver() {
    log.info("startDriver");

    // 1. Create default netifs
    apNetif = esp_netif_create_default_wifi_ap();
    staNetif = esp_netif_create_default_wifi_sta();

    // 2. Initialize Wi-Fi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
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
    wifi_config_t ap_cfg = config.toEspConfig();
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

/**
 * STA MODE
 */
void WiFiInterface::connectSta(const StaConfig &cfg) {
    log.info("Connecting STA to SSID: %s", cfg.ssid.c_str());

    wifi_config_t sta_cfg = cfg.toEspConfig();
    strncpy((char *) sta_cfg.sta.ssid, cfg.ssid.c_str(), sizeof(sta_cfg.sta.ssid));
    strncpy((char *) sta_cfg.sta.password, cfg.password.c_str(), sizeof(sta_cfg.sta.password));

    staActive = true;
    wifi_mode_t mode = computeMode();
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
    currentMode = mode;
}

void WiFiInterface::disconnectSta() {
    log.debug("Disconnecting STA");
    staActive = false;
    wifi_mode_t mode = computeMode();

    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    currentMode = mode;
}

/**
 * Provisioning server control
 */
void WiFiInterface::startProvisioningServer() {
    log.debug("Starting provisioning server");
    ctx.provisioningServer->start();
}

void WiFiInterface::stopProvisioningServer() {
    log.info("Stopping provisioning server");
    ctx.provisioningServer->stop();
}

/**
 * Runtime server control
 */
void WiFiInterface::startRuntimeServer() {
    log.info("Starting runtime server");
    ctx.runtimeServer->start();
}

void WiFiInterface::stopRuntimeServer() {
    log.info("Stopping runtime server");
    ctx.runtimeServer->stop();
}

/**
 * STATIC EVENT HANDLERS
 */
void WiFiInterface::wifiEventHandler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    auto *self = static_cast<WiFiInterface *>(arg);
    self->handleWiFiEvent(base, id, data);
}

void WiFiInterface::ipEventHandler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    auto *self = static_cast<WiFiInterface *>(arg);
    self->handleIPEvent(base, id, data);
}

/**
 * INSTANCE EVENT HANDLERS
 */
void WiFiInterface::handleWiFiEvent(esp_event_base_t base, int32_t id, void *data) {
    log.debug("handleWiFiEvent");
    switch (id) {
        case WIFI_EVENT_STA_START:
            log.debug("WIFI_EVENT_STA_START");
            break;

        case WIFI_EVENT_STA_CONNECTED:
            log.info("STA connected");
            ctx.stateMachine->onStaConnected();
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            auto *event = (wifi_event_sta_disconnected_t *) data;
            log.warn("STA disconnected, reason=%d", event->reason);
            ctx.stateMachine->onStaDisconnected(toWiFiError(event->reason));
            break;
        }

        case WIFI_EVENT_AP_START:
            log.info("AP started");
            break;

        case WIFI_EVENT_AP_STOP:
            log.info("AP stopped");
            break;

        case WIFI_EVENT_AP_STACONNECTED:
            log.info("STA connected");
            break;
        case WIFI_EVENT_STA_STOP:
            log.info("STA stopped");
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            log.info("STA disconnected");
            break;
        case WIFI_EVENT_HOME_CHANNEL_CHANGE:
            log.info("Home channel changed");
            break;

        case WIFI_EVENT_SCAN_DONE:
            log.info("Scan done");
            break;

        default:
            log.debug("Unhandled WiFi event: %ld", id);
            break;
    }
}

void WiFiInterface::handleIPEvent(esp_event_base_t base, int32_t id, void *data) {
    if (id == IP_EVENT_STA_GOT_IP) {
        auto *event = (ip_event_got_ip_t *) data;
        log.info("Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ctx.stateMachine->onStaGotIp(event);
    }
}

common::Result WiFiInterface::scan(std::vector<WiFiAp> &outAps) {
    log.debug("scan");
    if (!driverStarted) {
        log.error("scan() unsupported: driver not started");
        return common::Result::Unsupported;
    }

    const bool initialStaActive = staActive;

    // Ensure STA is enabled
    if (!setStaState(true)) {
        log.error("scan() setStaState true failed");
        return common::Result::InternalError;
    }
	
    wifi_scan_config_t scanConfig = {};
	log.debug("scan starting scan");
    esp_err_t err = esp_wifi_scan_start(&scanConfig, true);
    if (err != ESP_OK) {
        log.error("scan esp_wifi_scan_start returned %s", esp_err_to_name(err));
        setStaState(initialStaActive); // restore before returning
        return common::Result::InternalError;
    }

    // Retrieve AP records
    uint16_t apCount = 0;
	err = esp_wifi_scan_get_ap_num(&apCount);
	if (err != ESP_OK) {
	    log.error("scan() esp_wifi_scan_get_ap_number error %s", esp_err_to_name(err));
	    setStaState(initialStaActive);
	    return common::Result::InternalError;
	}

    std::vector<wifi_ap_record_t> records(apCount);
    err = esp_wifi_scan_get_ap_records(&apCount, records.data());
    if (err != ESP_OK) {
        log.error("scan() esp_wifi_scan_get_ap_records error %s", esp_err_to_name(err));
        setStaState(initialStaActive);
        return common::Result::InternalError;
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

    return common::Result::Ok;
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

bool WiFiInterface::setStaState(bool enable) {
    log.debug("setStaState %d", enable);
    if (staActive == enable) {
        return true; // nothing to do
    }

    staActive = enable;
    wifi_mode_t mode = computeMode();

    esp_err_t err = esp_wifi_set_mode(mode);
    if (err != ESP_OK) {
        log.error("setStaState() esp_wifi_set_mode error %s", esp_err_to_name(err));
        // rollback
        staActive = !enable;
        return false;
    }

    currentMode = mode;
    return true;
}

} // namespace wifi_manager