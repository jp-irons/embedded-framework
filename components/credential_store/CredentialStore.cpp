#include "credential_store/CredentialStore.hpp"

#include "common/Result.hpp"
#include "esp_err.h"
#include "logger/Logger.hpp"
#include "nvs.h"

#include <algorithm>
#include <cstring>
#include "device/EspTypeAdapter.hpp"

namespace credential_store {

using namespace common;
using namespace wifi_types;

static logger::Logger log{"CredentialStore"};

CredentialStore::CredentialStore(const char *nvsNamespace)
    : ns(nvsNamespace) {
    log.debug("constructor");
}

// TODO[CredentialStore::count]: Replace loadAll() with header-only count()
size_t CredentialStore::count() const {
    std::vector<WiFiCredential> entries;
    Result r = loadAll(entries);
    if (r != Result::Ok) {
        return 0;
    }
    return entries.size();
}

Result CredentialStore::loadAll(std::vector<WiFiCredential> &out) const {
    log.debug("loadAll");
    nvs_handle_t handle;
	esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
	if (err == ESP_ERR_NVS_NOT_FOUND) {
	    log.debug("CredentialStore: namespace '%s' not found (empty store)", ns);
	    return Result::Ok;
	}
	if (err != ESP_OK) {
	    Result r = device::toResult(err);
	    log.warn("Error '%s' opening namespace", toString(r));
	    return r;
	}
    size_t size = 0;
    err = nvs_get_blob(handle, "entries", nullptr, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // No credentials stored yet — treat as empty store
        log.debug("CredentialStore: no entries found (empty store)");
        nvs_close(handle);
        return Result::Ok; // or a special Empty result if you prefer
    }

    if (err != ESP_OK) {
        Result r = device::toResult(err);
        log.warn("Error '%s' accessing nvs", toString(r));
        nvs_close(handle);
        return r;
    }

    if (size == 0) {
        // Blob exists but is empty — also not an error
        log.debug("CredentialStore: entries blob is empty");
        nvs_close(handle);
        return Result::Ok;
    }

    std::vector<uint8_t> buf(size);
    err = nvs_get_blob(handle, "entries", buf.data(), &size);
    nvs_close(handle);

    if (err != ESP_OK) {
        Result r = device::toResult(err);
        log.warn("Error '%s' reading nvs", toString(r));
        nvs_close(handle);
        return r;
    }

    out.clear();
    const uint8_t *p = buf.data();
    const uint8_t *end = p + size;

    while (p < end) {
        uint8_t ssidLen = *p++;
        uint8_t passLen = *p++;
        int8_t priority = *p++;

        if (p + ssidLen + passLen > end)
            break;

        WiFiCredential c;
        c.ssid.assign((const char *) p, ssidLen);
        p += ssidLen;

        c.password.assign((const char *) p, passLen);
        p += passLen;

        c.priority = priority;

        out.push_back(c);
    }
    return Result::Ok;
}

Result CredentialStore::loadAllSortedByPriority(std::vector<WiFiCredential> &out) const {
    auto res = loadAll(out);
    if (res != Result::Ok) {
        return res;
    }

    std::sort(out.begin(), out.end(), [](auto &a, auto &b) { return a.priority > b.priority; });

    return Result::Ok;
}

Result CredentialStore::saveAll(const std::vector<WiFiCredential> &entries) {
    log.debug("saveAll");
    // Compute size
    size_t size = 0;
    for (auto &e : entries) {
        size += 1 + 1 + 1; // ssidLen, passLen, priority
        size += e.ssid.size();
        size += e.password.size();
    }

    std::vector<uint8_t> buf(size);
    uint8_t *p = buf.data();

    for (auto &e : entries) {
        *p++ = (uint8_t) e.ssid.size();
        *p++ = (uint8_t) e.password.size();
        *p++ = (int8_t) e.priority;

        memcpy(p, e.ssid.data(), e.ssid.size());
        p += e.ssid.size();

        memcpy(p, e.password.data(), e.password.size());
        p += e.password.size();
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        Result r = device::toResult(err);
        log.warn("Error '%s' opening nvs", r);
        return r;
    }

    err = nvs_set_blob(handle, "entries", buf.data(), size);
    if (err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);
    return Result::Ok;
}

Result CredentialStore::add(const WiFiCredential &entry) {
    log.debug("add");
    std::vector<WiFiCredential> entries;
    loadAll(entries);

    // Replace if SSID exists
    for (auto &e : entries) {
        if (e.ssid == entry.ssid) {
            e = entry;
            return saveAll(entries);
        }
    }

    entries.push_back(entry);
    return saveAll(entries);
}

Result CredentialStore::erase(const std::string &ssid) {
    log.debug("erase");
    std::vector<WiFiCredential> entries;
    loadAll(entries);

    entries.erase(
        std::remove_if(entries.begin(), entries.end(), [&](const WiFiCredential &e) { return e.ssid == ssid; }),
        entries.end());

    return saveAll(entries);
}

Result CredentialStore::clear() {
    log.debug("clear");
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return device::toResult(err);

    err = nvs_erase_key(handle, "entries");
    if (err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);
    return Result::Ok;
}

Result CredentialStore::store(const WiFiCredential &cred) {
    log.debug("store");
    std::vector<WiFiCredential> list;
    Result r = loadAll(list);
    if (r != Result::Ok) {
        return r;
    }

    // Update if SSID already exists
    bool updated = false;
    for (auto &existing : list) {
        if (existing.ssid == cred.ssid) {
            existing = cred;
            updated = true;
            break;
        }
    }

    // Otherwise add new
    if (!updated) {
        list.push_back(cred);
    }

    // Sort by priority (lower = higher priority)
    std::sort(list.begin(), list.end(),
              [](const WiFiCredential &a, const WiFiCredential &b) { return a.priority < b.priority; });

    return saveAll(list);
}

std::optional<WiFiCredential> CredentialStore::getByIndex(std::size_t index) const {
    std::vector<WiFiCredential> all;
    if (loadAllSortedByPriority(all)!=Result::Ok) {
        return std::nullopt;
    }

    if (index >= all.size()) {
        return std::nullopt;
    }

    return all[index];
}

} // namespace credential_store