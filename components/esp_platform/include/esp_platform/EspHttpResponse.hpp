#pragma once

#include "http/HttpResponse.hpp"
#include "esp_http_server.h"

namespace esp_platform {

/**
 * ESP-IDF concrete implementation of http::HttpResponse.
 *
 * Wraps a httpd_req_t* (used for both request and response in ESP-IDF's httpd)
 * and satisfies the HttpResponse interface.
 * All ESP-IDF types are confined to this header and its .cpp.
 * Nothing outside esp_platform needs to include this file — consumers
 * depend only on HttpResponse.hpp.
 */
class EspHttpResponse : public http::HttpResponse {
  public:
    explicit EspHttpResponse(httpd_req_t *r);

    common::Result send(int code, std::string_view body, const char *type) override;
    common::Result send(std::string_view body, const char *type) override;
    common::Result redirect(const char *target) override;
    common::Result send(const unsigned char *data, unsigned int size, const char *type) override;
    common::Result send(std::string_view body) override;
    common::Result sendText(std::string_view body) override;
    common::Result sendJson(std::string_view body) override;
    common::Result sendJson(int code, std::string_view body) override;
    common::Result sendJsonError(int code, std::string_view message) override;
    common::Result sendJsonStatus(std::string_view status) override;
    common::Result sendUnauthorized(const char *realm) override;

  private:
    httpd_req_t *req_;
    void warnErr(esp_err_t err);
};

} // namespace esp_platform
