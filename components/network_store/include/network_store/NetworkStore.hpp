#pragma once

#include "common/Result.hpp"
#include "network_store/WiFiNetwork.hpp"

#include <optional>
#include <string>
#include <vector>

namespace network_store {

class NetworkStore {
  public:
    explicit NetworkStore(const char *nvsNamespace = "wifi_creds");

    // Number of stored networks (fast path, no allocations)
    std::size_t count() const;

    common::Result loadAll(std::vector<WiFiNetwork> &out) const;

    // Replace entire set atomically
    common::Result saveAll(std::vector<WiFiNetwork> entries);

    // Add a new network (fails if SSID exists)
    common::Result add(const WiFiNetwork &entry);

    // Insert or update by SSID (idempotent)
    common::Result store(const WiFiNetwork &cred);

    common::Result loadAllSortedByPriority(std::vector<WiFiNetwork> &out) const;

    // Remove a network by SSID
    common::Result erase(const std::string &ssid);

    // Remove all networks
    common::Result clear();

    std::optional<WiFiNetwork> getByIndex(std::size_t index) const;

    common::Result makeFirst(const std::string &ssid);

  private:
    const char *ns;
};

} // namespace network_store
