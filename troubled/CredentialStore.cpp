#include "CredentialStore.hpp"
#include "WiFiManager.hpp"   // for WiFiEntry + WiFiState
#include "esp_log.h"
#include <cstring>
#include <algorithm>   // <-- required for std::remove_if
#include <vector>
#include <string>

static const char* TAG = "CredentialStore";

CredentialStore::CredentialStore(const char* nvsNamespace)
    : ns(nvsNamespace)
{
    ESP_LOGI(TAG, "constructed");
}

bool CredentialStore::begin()
{
    ESP_LOGI(TAG, "Initialising");
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s",
                 ns, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool CredentialStore::saveEntries(const std::vector<wifi_manager::WiFiEntry>& entries)
{
    std::vector<uint8_t> encoded;
    if (!encodeEntries(entries, encoded)) {
        ESP_LOGE(TAG, "Encoding entries failed");
        return false;
    }

    esp_err_t err = nvs_set_blob(handle, "entries", encoded.data(), encoded.size());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save entries: %s", esp_err_to_name(err));
        return false;
    }

    nvs_commit(handle);
    return true;
}

bool CredentialStore::loadEntries(std::vector<wifi_manager::WiFiEntry>& outEntries)
{
    size_t size = 0;
    esp_err_t err = nvs_get_blob(handle, "entries", nullptr, &size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false; // no entries saved yet
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get entries size: %s", esp_err_to_name(err));
        return false;
    }

    std::vector<uint8_t> buf(size);
    err = nvs_get_blob(handle, "entries", buf.data(), &size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load entries blob: %s", esp_err_to_name(err));
        return false;
    }

    return decodeEntries(buf.data(), size, outEntries);
}

bool CredentialStore::saveState(wifi_manager::WiFiState state)
{
    uint32_t v = static_cast<uint32_t>(state);
    esp_err_t err = nvs_set_u32(handle, "state", v);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save state: %s", esp_err_to_name(err));
        return false;
    }
    nvs_commit(handle);
    return true;
}

bool CredentialStore::loadState(wifi_manager::WiFiState& outState)
{
    uint32_t v = 0;
    esp_err_t err = nvs_get_u32(handle, "state", &v);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load state: %s", esp_err_to_name(err));
        return false;
    }

    outState = static_cast<wifi_manager::WiFiState>(v);
    return true;
}

bool CredentialStore::clearAll() {
    std::vector<wifi_manager::WiFiEntry> empty;
    return saveEntries(empty);
}

bool CredentialStore::erase(const std::string& ssid) {
    std::vector<wifi_manager::WiFiEntry> entries;

    // Load current entries
    if (!loadEntries(entries)) {
        return false;
    }

    // Remove matching SSID
    auto before = entries.size();
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
                       [&](const wifi_manager::WiFiEntry& e) { return e.ssid == ssid; }),
        entries.end()
    );

    // If nothing was removed, return false
    if (entries.size() == before) {
        return false;
    }

    // Save updated list
    return saveEntries(entries);
}

bool CredentialStore::encodeEntries(const std::vector<wifi_manager::WiFiEntry>& entries,
                                    std::vector<uint8_t>& out)
{
    // Format:
    // [count:u16]
    // repeated:
    //   [ssidLen:u8][ssid bytes]
    //   [passLen:u8][pass bytes]
    //   [priority:i32]
    //   [bssid:6 bytes]
    //   [bssidLocked:u8]

    size_t total = 2; // count
    for (const auto& e : entries) {
        total += 1 + e.ssid.size();
        total += 1 + e.password.size();
        total += 4;
        total += 6;
        total += 1;
    }

    out.resize(total);
    uint8_t* p = out.data();

    uint16_t count = entries.size();
    memcpy(p, &count, 2); p += 2;

    for (const auto& e : entries) {
        uint8_t ssidLen = e.ssid.size();
        uint8_t passLen = e.password.size();

        *p++ = ssidLen;
        memcpy(p, e.ssid.data(), ssidLen); p += ssidLen;

        *p++ = passLen;
        memcpy(p, e.password.data(), passLen); p += passLen;

        memcpy(p, &e.priority, 4); p += 4;

        memcpy(p, e.bssid, 6); p += 6;

        *p++ = e.bssidLocked ? 1 : 0;
    }

    return true;
}

bool CredentialStore::decodeEntries(const uint8_t* data, size_t len,
                                    std::vector<wifi_manager::WiFiEntry>& out)
{
    const uint8_t* p = data;
    const uint8_t* end = data + len;

    if (len < 2) return false;

    uint16_t count = 0;
    memcpy(&count, p, 2); p += 2;

    out.clear();
    out.reserve(count);

    for (int i = 0; i < count; i++) {
        if (p >= end) return false;

        wifi_manager::WiFiEntry e;

        uint8_t ssidLen = *p++;
        if (p + ssidLen > end) return false;
        e.ssid.assign((const char*)p, ssidLen);
        p += ssidLen;

        uint8_t passLen = *p++;
        if (p + passLen > end) return false;
        e.password.assign((const char*)p, passLen);
        p += passLen;

        if (p + 4 > end) return false;
        memcpy(&e.priority, p, 4); p += 4;

        if (p + 6 > end) return false;
        memcpy(e.bssid, p, 6); p += 6;

        if (p >= end) return false;
        e.bssidLocked = (*p++ != 0);

        out.push_back(e);
    }

    return true;
}