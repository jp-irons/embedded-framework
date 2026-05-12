#pragma once

#include <string>

namespace network_store {

struct WiFiNetwork {
    std::string ssid;
    std::string password;
    int priority = 0;
};

} // namespace network_store
