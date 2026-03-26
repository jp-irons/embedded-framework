#pragma once

#include <string>
#include <vector>
#include "nvs.h"
#include "nvs_flash.h"

namespace credential_store {

struct WifiCredential {
    std::string ssid;
    std::string password;
    int priority = 0;
};

class CredentialStore {
public:
    CredentialStore(const char* nvsNamespace = "wifi_creds");

    bool loadAll(std::vector<WifiCredential>& out);
    bool saveAll(const std::vector<WifiCredential>& entries);

    bool add(const WifiCredential& entry);
    bool erase(const std::string& ssid);
    bool clear();

private:
    const char* ns;
};

} // namespace credential_store