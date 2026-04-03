#include "wifi_manager/WiFiInterface.hpp"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "wifi_manager/ProvisioningServer.hpp"
#include "wifi_manager/RuntimeServer.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"
#include "wifi_manager/WiFiTypes.hpp"

namespace wifi_manager {

static const char *TAG = "WiFiInterface";

WiFiInterface::WiFiInterface(WiFiContext &ctx)
    : ctx(ctx) {
    ESP_LOGD(TAG, "constructor");
}

/**
 * Driver init — ONLY WiFi driver + handlers.
 * DO NOT create event loop or netifs here.
 * Those belong in app_main().
 */
void WiFiInterface::startDriver() {
    ESP_LOGI(TAG, "Initialising WiFi driver");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register WiFi event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiInterface::wifiEventHandler,
                                                        this, nullptr));

    // Register IP event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WiFiInterface::ipEventHandler,
                                                        this, nullptr));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
}

void WiFiInterface::stopDriver() {
    ESP_LOGI(TAG, "Stopping WiFi driver");
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
}

/**
 * AP MODE
 */
void WiFiInterface::startAp(const ApConfig &config) {
    ESP_LOGI(TAG, "Starting SoftAP: %s", config.ssid.c_str());
    /*        ***********************   */
    wifi_config_t ap_cfg = {};
    strncpy((char *) ap_cfg.ap.ssid, config.ssid.c_str(), sizeof(ap_cfg.ap.ssid));

    bool useOpenAp = false;

    if (config.password.empty()) {
        useOpenAp = true;
    } else if (config.password.length() < 8) {
        ESP_LOGW(TAG, "AP password '%s' is too short (%d chars). Falling back to OPEN AP.", config.password.c_str(),
                 (int) config.password.length());
        useOpenAp = true;
    }

	if (useOpenAp) {
	    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
	    ap_cfg.ap.password[0] = '\0';  // ensure empty
	} else {
	    ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
	    strncpy((char*)ap_cfg.ap.password, config.password.c_str(), sizeof(ap_cfg.ap.password));
	}

	ap_cfg.ap.channel = config.channel;
	ap_cfg.ap.max_connection = config.maxConnections;

	ESP_LOGI(TAG, "Starting SoftAP: %s (authmode=%s)",
	    config.ssid.c_str(),
	    useOpenAp ? "OPEN" : "WPA2"
	);

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
	ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFiInterface::stopAp() {
    ESP_LOGI(TAG, "Stopping SoftAP");
    ESP_ERROR_CHECK(esp_wifi_stop());
}

/**
 * STA MODE
 */
void WiFiInterface::connectSta(const StaConfig &cfg) {
    ESP_LOGI(TAG, "Connecting STA to SSID: %s", cfg.ssid.c_str());

    wifi_config_t sta_cfg = {};
    strncpy((char *) sta_cfg.sta.ssid, cfg.ssid.c_str(), sizeof(sta_cfg.sta.ssid));
    strncpy((char *) sta_cfg.sta.password, cfg.password.c_str(), sizeof(sta_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void WiFiInterface::disconnectSta() {
    ESP_LOGD(TAG, "Disconnecting STA");
    esp_wifi_disconnect();
}

/**
 * Provisioning server control
 */
void WiFiInterface::startProvisioningServer() {
    ESP_LOGD(TAG, "Starting provisioning server");
    ctx.provisioningServer->start();
}

void WiFiInterface::stopProvisioningServer() {
    ESP_LOGI(TAG, "Stopping provisioning server");
    ctx.provisioningServer->stop();
}

/**
 * Runtime server control
 */
void WiFiInterface::startRuntimeServer() {
    ESP_LOGI(TAG, "Starting runtime server");
    ctx.runtimeServer->start();
}

void WiFiInterface::stopRuntimeServer() {
    ESP_LOGI(TAG, "Stopping runtime server");
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
    switch (id) {

        case WIFI_EVENT_STA_START:
            ESP_LOGD(TAG, "WIFI_EVENT_STA_START");
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "STA connected");
            ctx.stateMachine->onStaConnected();
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            auto *event = (wifi_event_sta_disconnected_t *) data;
            ESP_LOGW(TAG, "STA disconnected, reason=%d", event->reason);
            ctx.stateMachine->onStaDisconnected(toWiFiError(event->reason));
            break;
        }

        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "AP started");
            break;

        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(TAG, "AP stopped");
            break;

        default:
            ESP_LOGD(TAG, "Unhandled WiFi event: %ld", id);
            break;
    }
}

void WiFiInterface::handleIPEvent(esp_event_base_t base, int32_t id, void *data) {
    if (id == IP_EVENT_STA_GOT_IP) {
        auto *event = (ip_event_got_ip_t *) data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ctx.stateMachine->onStaGotIp(event);
    }
}

} // namespace wifi_manager