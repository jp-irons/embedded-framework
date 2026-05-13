#pragma once

#include "common/KeyValueStore.hpp"
#include "common/Result.hpp"
#include "network_store/WiFiNetwork.hpp"

#include <optional>
#include <string>
#include <vector>

namespace network_store {

class NetworkStore {
  public:
    static constexpr const char* TAG = "NetworkStore";

    NetworkStore() = default;

    /**
     * Bind the store to its backing KeyValueStore.
     * Must be called before any other method.
     */
    void init(common::KeyValueStore& kvs);

    // Number of stored networks (fast path, no allocations)
    std::size_t count() const;

    common::Result loadAll(std::vector<WiFiNetwork>& out) const;

    // Replace entire set atomically
    common::Result saveAll(std::vector<WiFiNetwork> entries);

    // Add a new network (replaces if SSID already exists)
    common::Result add(const WiFiNetwork& entry);

    // Insert or update by SSID (idempotent)
    common::Result store(const WiFiNetwork& cred);

    common::Result loadAllSortedByPriority(std::vector<WiFiNetwork>& out) const;

    // Remove a network by SSID
    common::Result erase(const std::string& ssid);

    // Remove all networks
    common::Result clear();

    std::optional<WiFiNetwork> getByIndex(std::size_t index) const;

    common::Result makeFirst(const std::string& ssid);

  private:
    common::KeyValueStore* kvs_ = nullptr;

    static constexpr const char* KVS_KEY = "entries";
};

} // namespace network_store
