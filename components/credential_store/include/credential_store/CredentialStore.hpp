#pragma once
#include "common/Result.hpp"
#include "wifi_types/WiFiTypes.hpp"
#include <optional>
#include <vector>
#include <string>

namespace credential_store {

class CredentialStore {
public:
    explicit CredentialStore(const char* nvsNamespace = "wifi_creds");

    // Number of stored credentials (fast path, no allocations)
    std::size_t count() const;

    common::Result loadAll(std::vector<wifi_types::WiFiCredential>& out) const;

    // Replace entire set atomically
	common::Result saveAll(std::vector<wifi_types::WiFiCredential> entries);

    // Add a new credential (fails if SSID exists)
    common::Result add(const wifi_types::WiFiCredential& entry);

    // Insert or update by SSID (idempotent)
    common::Result store(const wifi_types::WiFiCredential& cred);

	common::Result loadAllSortedByPriority(std::vector<wifi_types::WiFiCredential>& out) const;

    // Remove a credential by SSID
    common::Result erase(const std::string& ssid);

    // Remove all credentials
    common::Result clear();
	
	std::optional<wifi_types::WiFiCredential> getByIndex(std::size_t index) const;
	
	common::Result makeFirst(const std::string& ssid);

private:
    const char* ns;
};

} // namespace credential_store