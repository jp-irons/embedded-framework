#pragma once

#include "esp_http_server.h"
#include "wifi_manager/WiFiContext.hpp"

namespace wifi_manager {

struct WiFiContext;

class RuntimeServer {
  public:
    explicit RuntimeServer(WiFiContext &ctx);

    bool start(); // start HTTP server
    void stop(); // stop HTTP server

  private:
    WiFiContext &ctx; // non-owning shared state
    httpd_handle_t server; // HTTP server instance

    bool registerHandlers();

    // Static HTTP handlers
    static esp_err_t handleRoot(httpd_req_t *req);
    static esp_err_t handleInfo(httpd_req_t *req);

    // Helper to extract instance pointer
    static RuntimeServer *fromReq(httpd_req_t *req);
};

} // namespace wifi_manager