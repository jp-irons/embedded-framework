// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#include "esp_platform/EspWiFiInterface.hpp"

#include "common/Result.hpp"
#include "esp_platform/EspTypeAdapter.hpp"
#include "esp_platform/WiFiHelper.hpp"
#include "logger/Logger.hpp"
#include "wifi_manager/EmbeddedServer.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiManager.hpp"
#include "wifi_manager/WiFiTypes.hpp"

#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include <cstring>

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

namespace esp_platform {

using namespace common;
using namespace wifi_manager;

static logger::Logger log{EspWiFiInterface::TAG};

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

    // Push hostname to both netifs so the DHCP client advertises it to the
    // router (STA) and the AP interface is consistent.  Must be done before
    // esp_wifi_start() so the DHCP client picks it up on first connect.
    if (!ctx.mdnsHostname.empty()) {
        esp_netif_set_hostname(staNetif, ctx.mdnsHostname.c_str());
        esp_netif_set_hostname(apNetif,  ctx.mdnsHostname.c_str());
        log.debug("netif hostname set to '%s'", ctx.mdnsHostname.c_str());
    }

    // Power-save mode has no working default (see WiFiPowerSaveMode's doc
    // comment in WiFiTypes.hpp) -- fail loud here rather than silently
    // falling back to any particular ESP-IDF mode. The consuming app must
    // call FrameworkContext::setWifiPowerSaveMode() before start().
    if (ctx.psMode == WiFiPowerSaveMode::Unset) {
        log.error("startDriver: WiFiContext::psMode is Unset -- app must call "
                  "FrameworkContext::setWifiPowerSaveMode() before start()");
        return Result::InternalError;
    }

    // 2. Initialize Wi-Fi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    WIFI_CHECK(esp_wifi_init(&cfg));

    // Register WiFi event handler. Instance captured (not nullptr) so
    // stopDriver() can unregister exactly this registration — needed now
    // that stop/start is a real repeatable cycle (WiFiManager's soft
    // driver-reset escalation), not just a one-time boot/shutdown pairing.
    WIFI_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
               &EspWiFiInterface::wifiEventHandler, this, &wifiEventHandlerInstance_));

    // Register IP event handler
    // ANY_ID so we also catch IP_EVENT_STA_LOST_IP — the radio can stay
    // associated (no WIFI_EVENT_STA_DISCONNECTED) while DHCP/the netif's
    // IP silently dies underneath it.
    WIFI_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
               &EspWiFiInterface::ipEventHandler, this, &ipEventHandlerInstance_));

    WIFI_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    WIFI_CHECK(esp_wifi_start());
    WIFI_CHECK(esp_wifi_set_ps(toEspPs(ctx.psMode)));

    driverStarted = true;
    currentMode   = WIFI_MODE_NULL;
    return Result::Ok;
}

Result EspWiFiInterface::stopDriver() {
    log.info("Stopping WiFi driver");
    WIFI_CHECK(esp_wifi_stop());
    WIFI_CHECK(esp_wifi_deinit());

    // Unregister our event handlers before the netifs go away. Previously
    // this function was only ever called once, at real shutdown, so none
    // of the following mattered. It's now also called mid-run as part of
    // WiFiManager's soft driver-reset escalation (retry a stuck node
    // without a full esp_restart() — see project memory,
    // project_bird_wifi_reliability_investigation, 2026-07-21), so
    // startDriver() can genuinely run again afterward and needs a clean
    // slate: no duplicate handlers, no duplicate netifs.
    if (wifiEventHandlerInstance_) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEventHandlerInstance_);
        wifiEventHandlerInstance_ = nullptr;
    }
    if (ipEventHandlerInstance_) {
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ipEventHandlerInstance_);
        ipEventHandlerInstance_ = nullptr;
    }

    // esp_netif_destroy_default_wifi() does the required
    // esp_wifi_clear_default_wifi_driver_and_handlers() + esp_netif_destroy()
    // in the correct order (IDF v4.4+) for netifs created via
    // esp_netif_create_default_wifi_ap()/_sta() in startDriver(). Skipping
    // this and letting a second startDriver() call esp_netif_create_default_wifi_*()
    // again would create duplicate netifs on top of the old ones.
    if (staNetif) {
        esp_netif_destroy_default_wifi(staNetif);
        staNetif = nullptr;
    }
    if (apNetif) {
        esp_netif_destroy_default_wifi(apNetif);
        apNetif = nullptr;
    }

    driverStarted = false;
    return Result::Ok;
}

// ---------------------------------------------------------------------------
// AP MODE
// ---------------------------------------------------------------------------

Result EspWiFiInterface::startAp(const ApConfig& config) {
    log.info("Starting SoftAP: %s", config.ssid.c_str());

    wifi_config_t ap_cfg  = makeApConfig(config);
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
    // Config must be set before the mode change: the driver is already
    // running continuously (see startDriver()), so switching the mode to
    // include AP triggers wifi_softap_start() immediately in the driver's
    // own task. If that fires before a valid AP config is resident, the
    // WiFi blob null-derefs inside ieee80211_hostap_attach (LoadProhibitedCause,
    // excvaddr=0x2c) — root-caused 2026-07-18 from a field panic coredump.
    WIFI_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    WIFI_CHECK(esp_wifi_set_mode(mode));
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

WiFiStatus EspWiFiInterface::connectSta(const network_store::WiFiNetwork& cred) {
    log.info("Connecting STA to SSID: %s", cred.ssid.c_str());

    wifi_config_t cfg = makeStaConfig(cred);

    // Ensure STA is in a clean idle state
    esp_wifi_disconnect();
    esp_wifi_stop();

    if (esp_wifi_start() != ESP_OK) {
        return WiFiStatus::DriverError;
    }
    esp_wifi_set_ps(toEspPs(ctx.psMode));   // Re-apply after stop/start — PS mode can reset to default
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
        case WIFI_EVENT_AP_START:
            ctx.wifiManager->onApStarted();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ctx.wifiManager->onConnectSuccess();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            auto* event = static_cast<wifi_event_sta_disconnected_t*>(data);
            log.warn("STA disconnected: reason=%d ssid='%.*s' rssi=%d",
                     event->reason, event->ssid_len, event->ssid, event->rssi);
            ctx.wifiManager->onDisconnect(toWiFiError(event->reason));
            break;
        }
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            ctx.wifiManager->onConnectFail();
            break;
        default:
            break;
    }
}

void EspWiFiInterface::handleIPEvent(esp_event_base_t base, int32_t id, void* data) {
    log.debug("handleIPEvent");

    switch (id) {
        case IP_EVENT_STA_GOT_IP: {
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
            break;
        }

        case IP_EVENT_STA_LOST_IP:
            // Radio is still associated — no WIFI_EVENT_STA_DISCONNECTED fired —
            // but the netif's IP is gone (e.g. DHCP lease failure on the AP/
            // repeater side). This is WiFiManager's only chance to learn about
            // it; reuse the same retry/fallback chain a real disconnect would.
            log.warn("Lost STA IP (radio still associated) — treating as disconnect");
            if (ctx.wifiManager) {
                ctx.wifiManager->onDisconnect();
            }
            break;

        default:
            break;
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

} // namespace esp_platform
