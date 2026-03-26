#pragma once

#include "WiFiContext.hpp"
#include "esp_http_server.h"

namespace wifi_manager {

class ProvisioningServer {
public:
    explicit ProvisioningServer(WiFiContext& ctx);

    bool start();
    void stop();

    // Called by HTTP handlers or internal logic
    void handleCredentials(const char* ssid, const char* password);
	void handleSubmitCredentials(httpd_req_t* req);

private:
    WiFiContext& ctx;
	httpd_handle_t server = nullptr;   // <-- store it here
};

} // namespace wifi_manager
