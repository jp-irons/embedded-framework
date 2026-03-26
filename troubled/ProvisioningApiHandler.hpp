#pragma once

#include "esp_http_server.h"
#include <string>

class ApplicationContext; 
class WifiManager;

class ProvisioningApiHandler {
public:
    explicit ProvisioningApiHandler(ApplicationContext& ctx);

    // Returns true if this handler should process the given path
    bool handles(const std::string& path) const;

    // Dispatches to the correct endpoint
    esp_err_t handle(httpd_req_t* req);

private:
    ApplicationContext& ctx;

    // Endpoint handlers
    esp_err_t handleStatus(httpd_req_t* req);
};