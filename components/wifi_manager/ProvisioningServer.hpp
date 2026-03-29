#pragma once

#include "esp_http_server.h"
#include "WiFiContext.hpp"

namespace wifi_manager {

class ProvisioningServer {
public:
    explicit ProvisioningServer(WiFiContext* ctx);

    // Explicit lifecycle
    bool start();   // start HTTP server
    void stop();    // stop HTTP server

private:
    WiFiContext* ctx;          // non-owning shared state
    httpd_handle_t server;     // HTTP server instance

    bool registerHandlers();

    // Static HTTP handlers (C-style)
    static esp_err_t handleRoot(httpd_req_t* req);
    static esp_err_t handleSubmit(httpd_req_t* req);
    static esp_err_t handleStatus(httpd_req_t* req);
    static esp_err_t handleScan(httpd_req_t* req);

    // Helper to extract instance pointer
    static ProvisioningServer* fromReq(httpd_req_t* req);
};

} // namespace wifi_manager