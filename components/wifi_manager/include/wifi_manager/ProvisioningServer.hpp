#pragma once

#include "esp_http_server.h"

namespace wifi_manager {
	
struct WiFiContext;

class ProvisioningServer {
  public:
    explicit ProvisioningServer(WiFiContext *ctx);

    // Explicit lifecycle
    bool start(); // start HTTP server
    void stop(); // stop HTTP server

  private:
    WiFiContext *ctx; // non-owning shared state
    httpd_handle_t server; // HTTP server instance

    bool registerHandlers();

    // Static HTTP handlers (C-style)
    static esp_err_t dispatchApi(httpd_req_t *req);
    static esp_err_t handleStaticFile(httpd_req_t *req);

    // Helper to extract instance pointer
    static ProvisioningServer *fromReq(httpd_req_t *req);
};

} // namespace wifi_manager