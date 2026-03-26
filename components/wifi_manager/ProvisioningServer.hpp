#pragma once

#include "WiFiContext.hpp"
#include "esp_http_server.h"

namespace wifi_manager {

class ProvisioningServer {
public:
    explicit ProvisioningServer(WiFiContext& ctx);

    void start();
    void stop();

    // Called by HTTP handlers or internal logic
    void handleCredentials(const char* ssid, const char* password);
	void handleSubmitCredentials(httpd_req_t* req);

private:
    WiFiContext& ctx;
};

} // namespace wifi_manager
