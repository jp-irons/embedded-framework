#pragma once

#include "WiFiContext.hpp"

namespace wifi_manager {

class RuntimeServer {
public:
    explicit RuntimeServer(WiFiContext& ctx);

    void start();
    void stop();

    void handleStatusRequest();

private:
    WiFiContext& ctx;
};

} // namespace wifi_manager
