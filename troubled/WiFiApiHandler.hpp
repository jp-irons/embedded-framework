#pragma once
#include "esp_http_server.h"
#include "ApplicationContext.hpp"
#include <string>

class WiFiApiHandler {
public:
    explicit WiFiApiHandler(ApplicationContext& ctx);

    bool handles(const std::string& path) const;
    esp_err_t handle(httpd_req_t* req);

private:
    ApplicationContext& ctx;

    esp_err_t handleScan(httpd_req_t* req);
};