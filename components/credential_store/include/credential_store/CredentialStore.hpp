#pragma once

#include <string>
#include <vector>

namespace credential_store {

struct WiFiCredential {
    std::string ssid;
    std::string password;
    int priority = 0;
};

class CredentialStore {
public:
    CredentialStore(const char* nvsNamespace = "wifi_creds");

    bool loadAll(std::vector<WiFiCredential>& out);
    bool saveAll(const std::vector<WiFiCredential>& entries);

    bool add(const WiFiCredential& entry);
    bool erase(const std::string& ssid);
    bool clear();

private:
    const char* ns;
};

} // namespace credential_store