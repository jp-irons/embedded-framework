#include "device/DeviceConfigStore.hpp"

#include "logger/Logger.hpp"
#include "nvs.h"

namespace device {

static logger::Logger log{"DeviceConfigStore"};

static constexpr const char* NVS_NS             = "device_cfg";
static constexpr const char* NVS_KEY_HOSTNAME   = "hostname_pfx";
static constexpr const char* NVS_KEY_AP_SSID    = "ap_ssid_pfx";

// Static storage
DeviceConfigStore::Config      DeviceConfigStore::s_config_;
std::string                    DeviceConfigStore::s_defaultHostnamePrefix_;
std::string                    DeviceConfigStore::s_defaultApSsidPrefix_;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool nvsReadStr(nvs_handle_t h, const char* key, std::string& out) {
    size_t len = 0;
    if (nvs_get_str(h, key, nullptr, &len) != ESP_OK) return false;
    std::string tmp(len, '\0');
    if (nvs_get_str(h, key, tmp.data(), &len) != ESP_OK) return false;
    // nvs_get_str writes a null terminator into tmp; strip it.
    if (!tmp.empty() && tmp.back() == '\0') tmp.pop_back();
    out = std::move(tmp);
    return true;
}

static bool nvsEraseKey(const char* key) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        log.error("nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_erase_key(h, key);
    // ESP_ERR_NVS_NOT_FOUND is fine — key was already absent
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        log.error("nvsEraseKey('%s') failed: %s", key, esp_err_to_name(err));
        return false;
    }
    return true;
}

static bool nvsWriteStr(const char* key, const std::string& value) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        log.error("nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_str(h, key, value.c_str());
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        log.error("nvsWriteStr('%s') failed: %s", key, esp_err_to_name(err));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void DeviceConfigStore::init(const std::string& defaultHostnamePrefix,
                              const std::string& defaultApSsidPrefix) {
    s_defaultHostnamePrefix_ = defaultHostnamePrefix;
    s_defaultApSsidPrefix_   = defaultApSsidPrefix;
    s_config_.hostnamePrefix = defaultHostnamePrefix;
    s_config_.apSsidPrefix   = defaultApSsidPrefix;
    s_config_.hostnameFromNvs = false;
    s_config_.apSsidFromNvs   = false;

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        // Namespace not yet written — first boot, use app defaults.
        log.info("No device config in NVS — using app defaults ('%s', '%s')",
                 defaultHostnamePrefix.c_str(), defaultApSsidPrefix.c_str());
        return;
    }

    std::string stored;
    if (nvsReadStr(h, NVS_KEY_HOSTNAME, stored)) {
        log.info("hostname prefix override from NVS: '%s' (no MAC suffix)", stored.c_str());
        s_config_.hostnamePrefix  = stored;
        s_config_.hostnameFromNvs = true;
    }
    if (nvsReadStr(h, NVS_KEY_AP_SSID, stored)) {
        log.info("AP SSID prefix override from NVS: '%s' (no MAC suffix)", stored.c_str());
        s_config_.apSsidPrefix  = stored;
        s_config_.apSsidFromNvs = true;
    }

    nvs_close(h);
}

void DeviceConfigStore::setEffectiveNames(const std::string& hostname,
                                           const std::string& apSsid) {
    s_config_.effectiveHostname = hostname;
    s_config_.effectiveApSsid   = apSsid;
}

bool DeviceConfigStore::saveHostnamePrefix(const std::string& prefix) {
    if (!nvsWriteStr(NVS_KEY_HOSTNAME, prefix)) return false;
    s_config_.hostnamePrefix  = prefix;
    s_config_.hostnameFromNvs = true;
    log.info("hostname prefix saved: '%s'", prefix.c_str());
    return true;
}

bool DeviceConfigStore::saveApSsidPrefix(const std::string& prefix) {
    if (!nvsWriteStr(NVS_KEY_AP_SSID, prefix)) return false;
    s_config_.apSsidPrefix  = prefix;
    s_config_.apSsidFromNvs = true;
    log.info("AP SSID prefix saved: '%s'", prefix.c_str());
    return true;
}

bool DeviceConfigStore::clearHostnamePrefix() {
    if (!nvsEraseKey(NVS_KEY_HOSTNAME)) return false;
    s_config_.hostnamePrefix  = s_defaultHostnamePrefix_;
    s_config_.hostnameFromNvs = false;
    log.info("hostname prefix cleared — will revert to app default ('%s') on reboot",
             s_defaultHostnamePrefix_.c_str());
    return true;
}

bool DeviceConfigStore::clearApSsidPrefix() {
    if (!nvsEraseKey(NVS_KEY_AP_SSID)) return false;
    s_config_.apSsidPrefix  = s_defaultApSsidPrefix_;
    s_config_.apSsidFromNvs = false;
    log.info("AP SSID prefix cleared — will revert to app default ('%s') on reboot",
             s_defaultApSsidPrefix_.c_str());
    return true;
}

const DeviceConfigStore::Config& DeviceConfigStore::config() {
    return s_config_;
}

} // namespace device
