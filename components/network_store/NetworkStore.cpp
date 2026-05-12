#include "network_store/NetworkStore.hpp"

#include "common/Result.hpp"
#include "esp_err.h"
#include "logger/Logger.hpp"
#include "nvs.h"

#include <algorithm>
#include <cstring>
#include "esp_platform/EspTypeAdapter.hpp"

namespace network_store {

using namespace common;

static logger::Logger log{"NetworkStore"};

NetworkStore::NetworkStore(const char *nvsNamespace)
    : ns(nvsNamespace) {
    log.debug("constructor");
}

size_t NetworkStore::count() const {
    std::vector<WiFiNetwork> entries;
    Result r = loadAll(entries);
    if (r != Result::Ok) {
        return 0;
    }
    return entries.size();
}

Result NetworkStore::loadAll(std::vector<WiFiNetwork> &out) const {
    log.debug("loadAll");
    nvs_handle_t handle;
	esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
	if (err == ESP_ERR_NVS_NOT_FOUND) {
	    log.debug("NetworkStore: namespace '%s' not found (empty store)", ns);
	    return Result::Ok;
	}
	if (err != ESP_OK) {
	    Result r = esp_platform::toResult(err);
	    log.warn("Error '%s' opening namespace", toString(r));
	    return r;
	}
    size_t size = 0;
    err = nvs_get_blob(handle, "entries", nullptr, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // No networks stored yet — treat as empty store
        log.debug("NetworkStore: no entries found (empty store)");
        nvs_close(handle);
        return Result::Ok;
    }

    if (err != ESP_OK) {
        Result r = esp_platform::toResult(err);
        log.warn("Error '%s' accessing nvs", toString(r));
        nvs_close(handle);
        return r;
    }

    if (size == 0) {
        // Blob exists but is empty — also not an error
        log.debug("NetworkStore: entries blob is empty");
        nvs_close(handle);
        return Result::Ok;
    }

    std::vector<uint8_t> buf(size);
    err = nvs_get_blob(handle, "entries", buf.data(), &size);
    nvs_close(handle);

    if (err != ESP_OK) {
        Result r = esp_platform::toResult(err);
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

        WiFiNetwork c;
        c.ssid.assign((const char *) p, ssidLen);
        p += ssidLen;

        c.password.assign((const char *) p, passLen);
        p += passLen;

        c.priority = priority;

        out.push_back(c);
    }
    return Result::Ok;
}

Result NetworkStore::loadAllSortedByPriority(std::vector<WiFiNetwork> &out) const {
    auto res = loadAll(out);
    if (res != Result::Ok) {
        return res;
    }

    std::sort(out.begin(), out.end(), [](auto &a, auto &b) { return a.priority < b.priority; });

    return Result::Ok;
}

Result NetworkStore::saveAll(std::vector<WiFiNetwork> entries) {
    log.debug("saveAll");

    // 1. Sort by priority (just in case caller didn't)
    std::sort(entries.begin(), entries.end(),
              [](auto &a, auto &b) { return a.priority < b.priority; });

    // 2. Renumber priorities sequentially
    for (size_t i = 0; i < entries.size(); ++i) {
        entries[i].priority = i;
    }

    // 3. Compute encoded size
    size_t size = 0;
    for (auto &e : entries) {
        size += 1 + 1 + 1; // ssidLen, passLen, priority
        size += e.ssid.size();
        size += e.password.size();
    }

    std::vector<uint8_t> buf(size);
    uint8_t *p = buf.data();

    // 4. Encode entries
    for (auto &e : entries) {
        *p++ = (uint8_t)e.ssid.size();
        *p++ = (uint8_t)e.password.size();
        *p++ = (uint8_t)e.priority;

        memcpy(p, e.ssid.data(), e.ssid.size());
        p += e.ssid.size();

        memcpy(p, e.password.data(), e.password.size());
        p += e.password.size();
    }

    // 5. Write to NVS
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        Result r = esp_platform::toResult(err);
        log.warn("Error '%s' opening nvs", r);
        return r;
    }

    err = nvs_set_blob(handle, "entries", buf.data(), size);
    if (err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);
    return Result::Ok;
}

Result NetworkStore::add(const WiFiNetwork &entry) {
    log.debug("add");
    std::vector<WiFiNetwork> entries;
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

Result NetworkStore::erase(const std::string &ssid) {
    log.debug("erase");
    std::vector<WiFiNetwork> entries;
    loadAll(entries);

    entries.erase(
        std::remove_if(entries.begin(), entries.end(), [&](const WiFiNetwork &e) { return e.ssid == ssid; }),
        entries.end());

    return saveAll(entries);
}

Result NetworkStore::clear() {
    log.debug("clear");
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return esp_platform::toResult(err);

    err = nvs_erase_key(handle, "entries");
    if (err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);
    return Result::Ok;
}

Result NetworkStore::store(const WiFiNetwork &cred) {
    log.debug("store");
    std::vector<WiFiNetwork> list;
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
              [](const WiFiNetwork &a, const WiFiNetwork &b) { return a.priority < b.priority; });

    return saveAll(list);
}

std::optional<WiFiNetwork> NetworkStore::getByIndex(std::size_t index) const {
    std::vector<WiFiNetwork> all;
    if (loadAllSortedByPriority(all)!=Result::Ok) {
        return std::nullopt;
    }

    if (index >= all.size()) {
        return std::nullopt;
    }

    return all[index];
}

Result  NetworkStore::makeFirst(const std::string& ssid) {
	log.debug("makeFirst");
    std::vector<WiFiNetwork> entries;
    loadAllSortedByPriority(entries);

    auto it = std::find_if(entries.begin(), entries.end(),
        [&](const WiFiNetwork& e) { return e.ssid == ssid; });

    if (it == entries.end()) {
        log.error("makeFirst: SSID not found");
        return Result::NotFound;
    }

	log.debug(("makeFirst " + ssid).c_str());

    WiFiNetwork selected = *it;
    entries.erase(it);
    entries.insert(entries.begin(), selected);

    // Reassign priorities
    for (size_t i = 0; i < entries.size(); i++) {
        entries[i].priority = i;
    }

    saveAll(entries);
	return Result::Ok;
}

} // namespace network_store
