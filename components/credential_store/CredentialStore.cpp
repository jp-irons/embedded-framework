#include "CredentialStore.hpp"
#include "esp_log.h"
#include <cstring>
#include <algorithm>

namespace credential_store {

static const char* TAG = "CredentialStore";

CredentialStore::CredentialStore(const char* nvsNamespace)
    : ns(nvsNamespace)
{
}

bool CredentialStore::loadAll(std::vector<WifiCredential>& out) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No credentials found");
        return false;
    }

    size_t size = 0;
    err = nvs_get_blob(handle, "entries", nullptr, &size);
    if (err != ESP_OK || size == 0) {
        nvs_close(handle);
        return false;
    }

    std::vector<uint8_t> buf(size);
    err = nvs_get_blob(handle, "entries", buf.data(), &size);
    nvs_close(handle);

    if (err != ESP_OK) {
        return false;
    }

    out.clear();
    const uint8_t* p = buf.data();
    const uint8_t* end = p + size;

    while (p < end) {
        uint8_t ssidLen = *p++;
        uint8_t passLen = *p++;
        int8_t priority = *p++;

        if (p + ssidLen + passLen > end) break;

        WifiCredential c;
        c.ssid.assign((const char*)p, ssidLen);
        p += ssidLen;

        c.password.assign((const char*)p, passLen);
        p += passLen;

        c.priority = priority;

        out.push_back(c);
    }

    return true;
}

bool CredentialStore::saveAll(const std::vector<WifiCredential>& entries) {
    // Compute size
    size_t size = 0;
    for (auto& e : entries) {
        size += 1 + 1 + 1; // ssidLen, passLen, priority
        size += e.ssid.size();
        size += e.password.size();
    }

    std::vector<uint8_t> buf(size);
    uint8_t* p = buf.data();

    for (auto& e : entries) {
        *p++ = (uint8_t)e.ssid.size();
        *p++ = (uint8_t)e.password.size();
        *p++ = (int8_t)e.priority;

        memcpy(p, e.ssid.data(), e.ssid.size());
        p += e.ssid.size();

        memcpy(p, e.password.data(), e.password.size());
        p += e.password.size();
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    err = nvs_set_blob(handle, "entries", buf.data(), size);
    if (err == ESP_OK) err = nvs_commit(handle);

    nvs_close(handle);
    return err == ESP_OK;
}

bool CredentialStore::add(const WifiCredential& entry) {
    std::vector<WifiCredential> entries;
    loadAll(entries);

    // Replace if SSID exists
    for (auto& e : entries) {
        if (e.ssid == entry.ssid) {
            e = entry;
            return saveAll(entries);
        }
    }

    entries.push_back(entry);
    return saveAll(entries);
}

bool CredentialStore::erase(const std::string& ssid) {
    std::vector<WifiCredential> entries;
    loadAll(entries);

    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
                       [&](const WifiCredential& e) { return e.ssid == ssid; }),
        entries.end()
    );

    return saveAll(entries);
}

bool CredentialStore::clear() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    err = nvs_erase_key(handle, "entries");
    if (err == ESP_OK) err = nvs_commit(handle);

    nvs_close(handle);
    return err == ESP_OK;
}

} // namespace credential_store