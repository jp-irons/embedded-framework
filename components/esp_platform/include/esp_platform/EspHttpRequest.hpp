#pragma once

#include "http/HttpRequest.hpp"
#include "esp_http_server.h"

#include <string>

namespace esp_platform {

/**
 * ESP-IDF concrete implementation of http::HttpRequest.
 *
 * Wraps a httpd_req_t* and satisfies the HttpRequest interface.
 * All ESP-IDF types are confined to this header and its .cpp.
 * Nothing outside esp_platform needs to include this file — consumers
 * depend only on HttpRequest.hpp.
 */
class EspHttpRequest : public http::HttpRequest {
  public:
    explicit EspHttpRequest(httpd_req_t *r);

    std::string_view body() const override {
        return std::string_view(bodyStorage_.data(), bodyStorage_.size());
    }

    std::string_view uri() const override { return req_->uri; }

    const char *path() const override { return req_->uri; }

    http::HttpMethod method() const override { return method_; }

    size_t contentLength() const override { return req_->content_len; }

    int receiveChunk(char *buf, size_t len) override;

    std::optional<http::BasicAuth> extractBasicAuth() const override;

    std::optional<std::string> extractBearerToken() const override;

  private:
    static constexpr size_t MAX_PRELOAD_BYTES = 65536; // 64 KB

    httpd_req_t    *req_;
    http::HttpMethod method_;
    std::string      bodyStorage_; // owns the memory; empty when skipped

    void readBody();
};

} // namespace esp_platform
