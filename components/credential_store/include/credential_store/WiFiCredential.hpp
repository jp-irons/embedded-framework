#pragma once

#include <string>

namespace credential_store {

struct WiFiCredential {
    std::string ssid;
    std::string password;
    int priority = 0;
};

} // namespace credential_store