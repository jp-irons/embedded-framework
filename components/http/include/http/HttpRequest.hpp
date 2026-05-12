#pragma once

#include "esp_http_server.h"
#include "http_types/HttpTypes.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace http {

/** Decoded Basic Auth credentials extracted from an Authorization header. */
struct BasicAuth {
    std::string username;
    std::string password;
};

class HttpRequest {
  public:
    explicit HttpRequest(httpd_req_t *r);

    std::string_view body() const {
        return std::string_view(bodyStorage_.data(), bodyStorage_.size());
    }

    std::string_view uri() const {
        return req->uri;
    }

    const char *path() const {
        return req->uri;
    }

    HttpMethod method() const { return method_; }

    httpd_req_t *raw() const {
        return req;
    }

    /**
     * Returns the Content-Length of the request body in bytes.
     * Zero if no body was sent.
     */
    size_t contentLength() const { return req->content_len; }

    /**
     * Read up to len bytes from the request body into buf.
     *
     * Retries once on a transient socket timeout, then returns an error.
     * Returns the number of bytes read (positive), or a negative value on
     * unrecoverable error.  Callers should treat any non-positive return as
     * a failure.
     */
    int receiveChunk(char *buf, size_t len);

    /**
     * Extracts HTTP Basic Auth credentials from the Authorization header.
     *
     * Returns the decoded username and password if the header is present and
     * well-formed ("Basic <base64(username:password)>").
     * Returns std::nullopt if the header is absent, not Basic scheme, or
     * cannot be decoded.
     */
    std::optional<BasicAuth> extractBasicAuth() const;

    /**
     * Extracts a Bearer token from the Authorization header.
     *
     * Returns the raw token string if the header is present and has the form
     * "Bearer <token>".  Returns std::nullopt if the header is absent, uses a
     * different scheme, or the token portion is empty.
     */
    std::optional<std::string> extractBearerToken() const;

  private:
    static constexpr size_t MAX_PRELOAD_BYTES = 65536; // 64 KB

    httpd_req_t *req;
    HttpMethod   method_;
    std::string  bodyStorage_; // owns the memory; empty when skipped

    void readBody();
};

} // namespace http
