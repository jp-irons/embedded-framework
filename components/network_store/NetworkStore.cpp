#include "network_store/NetworkStore.hpp"

#include "common/Result.hpp"
#include "logger/Logger.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace network_store {

using namespace common;

static logger::Logger log{NetworkStore::TAG};

void NetworkStore::init(KeyValueStore& kvs) {
    kvs_ = &kvs;
    log.debug("init");
}

size_t NetworkStore::count() const {
    std::vector<WiFiNetwork> entries;
    Result r = loadAll(entries);
    if (r != Result::Ok) {
        return 0;
    }
    return entries.size();
}

Result NetworkStore::loadAll(std::vector<WiFiNetwork>& out) const {
    log.debug("loadAll");

    std::vector<uint8_t> buf;
    Result r = kvs_->getBlob(KVS_KEY, buf);
    if (r == Result::NotFound) {
        log.debug("No entries stored (empty store)");
        return Result::Ok;
    }
    if (r != Result::Ok) {
        log.warn("Error '%s' reading store", toString(r));
        return r;
    }
    if (buf.empty()) {
        log.debug("Entries blob is empty");
        return Result::Ok;
    }

    out.clear();
    const uint8_t* p   = buf.data();
    const uint8_t* end = p + buf.size();

    while (p < end) {
        uint8_t ssidLen = *p++;
        uint8_t passLen = *p++;
        int8_t  priority = static_cast<int8_t>(*p++);

        if (p + ssidLen + passLen > end) break;

        WiFiNetwork c;
        c.ssid.assign(reinterpret_cast<const char*>(p), ssidLen);
        p += ssidLen;
        c.password.assign(reinterpret_cast<const char*>(p), passLen);
        p += passLen;
        c.priority = priority;

        out.push_back(c);
    }
    return Result::Ok;
}

Result NetworkStore::loadAllSortedByPriority(std::vector<WiFiNetwork>& out) const {
    Result r = loadAll(out);
    if (r != Result::Ok) return r;
    std::sort(out.begin(), out.end(),
              [](const WiFiNetwork& a, const WiFiNetwork& b) {
                  return a.priority < b.priority;
              });
    return Result::Ok;
}

Result NetworkStore::saveAll(std::vector<WiFiNetwork> entries) {
    log.debug("saveAll");

    // Sort by priority and renumber sequentially
    std::sort(entries.begin(), entries.end(),
              [](const WiFiNetwork& a, const WiFiNetwork& b) {
                  return a.priority < b.priority;
              });
    for (size_t i = 0; i < entries.size(); ++i) {
        entries[i].priority = static_cast<int8_t>(i);
    }

    // Encode
    size_t size = 0;
    for (const auto& e : entries) {
        size += 1 + 1 + 1; // ssidLen, passLen, priority byte
        size += e.ssid.size();
        size += e.password.size();
    }

    std::vector<uint8_t> buf(size);
    uint8_t* p = buf.data();
    for (const auto& e : entries) {
        *p++ = static_cast<uint8_t>(e.ssid.size());
        *p++ = static_cast<uint8_t>(e.password.size());
        *p++ = static_cast<uint8_t>(e.priority);
        memcpy(p, e.ssid.data(),     e.ssid.size());     p += e.ssid.size();
        memcpy(p, e.password.data(), e.password.size()); p += e.password.size();
    }

    return kvs_->setBlob(KVS_KEY, buf.data(), buf.size());
}

Result NetworkStore::add(const WiFiNetwork& entry) {
    log.debug("add");
    std::vector<WiFiNetwork> entries;
    loadAll(entries);

    for (auto& e : entries) {
        if (e.ssid == entry.ssid) {
            e = entry;
            return saveAll(entries);
        }
    }
    entries.push_back(entry);
    return saveAll(entries);
}

Result NetworkStore::erase(const std::string& ssid) {
    log.debug("erase");
    std::vector<WiFiNetwork> entries;
    loadAll(entries);

    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
                       [&](const WiFiNetwork& e) { return e.ssid == ssid; }),
        entries.end());

    return saveAll(entries);
}

Result NetworkStore::clear() {
    log.debug("clear");
    return kvs_->eraseKey(KVS_KEY);
}

Result NetworkStore::store(const WiFiNetwork& cred) {
    log.debug("store");
    std::vector<WiFiNetwork> list;
    Result r = loadAll(list);
    if (r != Result::Ok) return r;

    bool updated = false;
    for (auto& existing : list) {
        if (existing.ssid == cred.ssid) {
            existing = cred;
            updated  = true;
            break;
        }
    }
    if (!updated) list.push_back(cred);

    std::sort(list.begin(), list.end(),
              [](const WiFiNetwork& a, const WiFiNetwork& b) {
                  return a.priority < b.priority;
              });
    return saveAll(list);
}

std::optional<WiFiNetwork> NetworkStore::getByIndex(std::size_t index) const {
    std::vector<WiFiNetwork> all;
    if (loadAllSortedByPriority(all) != Result::Ok) return std::nullopt;
    if (index >= all.size()) return std::nullopt;
    return all[index];
}

Result NetworkStore::makeFirst(const std::string& ssid) {
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

    for (size_t i = 0; i < entries.size(); ++i) {
        entries[i].priority = static_cast<int8_t>(i);
    }

    return saveAll(entries);
}

} // namespace network_store
