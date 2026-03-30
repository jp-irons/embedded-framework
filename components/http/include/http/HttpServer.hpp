#pragma once

#include "esp_http_server.h"

namespace http {

class HttpServer {
  public:
    using HandlerFn = esp_err_t (*)(httpd_req_t *req);

    HttpServer();
    ~HttpServer();

    void start();
    void stop();

    void registerHandler(const char *uri, httpd_method_t method, HandlerFn fn);

  private:
    httpd_handle_t server = nullptr;
};
} // namespace http