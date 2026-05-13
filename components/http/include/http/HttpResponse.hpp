#pragma once

#include "common/Result.hpp"

#include <string_view>

namespace http {

/**
 * Abstract interface for an outgoing HTTP response.
 *
 * No ESP-IDF types appear here.  The concrete implementation (EspHttpResponse)
 * lives in the esp_platform component and is constructed inside EspHttpServer.
 *
 * Status strings follow the ESP-IDF httpd convention, e.g.:
 *   "200 Ok", "301 Moved Permanently", "302 Found",
 *   "400 Bad Request", "403 Forbidden", "404 Not Found",
 *   "405 Method Not Allowed", "500 Internal Server Error", "501 Not Implemented"
 */
class HttpResponse {
  public:
    virtual ~HttpResponse() = default;

    virtual common::Result send(int code, std::string_view body, const char *type) = 0;

    virtual common::Result send(std::string_view body, const char *type) = 0;

    virtual common::Result redirect(const char *target) = 0;

    virtual common::Result send(const unsigned char *data, unsigned int size, const char *type) = 0;

    virtual common::Result send(std::string_view body) = 0;

    virtual common::Result sendText(std::string_view body) = 0;

    virtual common::Result sendJson(std::string_view body) = 0;

    virtual common::Result sendJson(int code, std::string_view body) = 0;

    virtual common::Result sendJsonError(int code, std::string_view message) = 0;

    virtual common::Result sendJsonStatus(std::string_view status) = 0;

    /**
     * Sends HTTP 401 Unauthorized with a WWW-Authenticate Bearer challenge.
     *
     * @param realm  Challenge realm string (e.g. "ESP32").
     */
    virtual common::Result sendUnauthorized(const char *realm) = 0;
};

} // namespace http