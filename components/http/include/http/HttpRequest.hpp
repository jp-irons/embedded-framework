#pragma once

#include "esp_http_server.h"
#include "device/EspTypeAdapter.hpp"
#include "http/HttpTypes.hpp"

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
    explicit HttpRequest(httpd_req_t *r)
        : req(r) {
        readBody();
    }

    std::string_view body() const {
        return std::string_view(bodyStorage_.data(), bodyStorage_.size());
    }

    std::string_view uri() const {
        return req->uri;
    }

    const char *path() const {
        return req->uri;
    }

    HttpMethod method() const {
        return device::toHttpMethod(req->method);
    }

    httpd_req_t *raw() const {
        return req;
    }

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
    // Bodies larger than this threshold are NOT pre-loaded into memory.
    // The handler must read them directly via req->raw() + httpd_req_recv().
    // This prevents OTA firmware uploads (typically 0.5–2 MB) from exhausting
    // the ESP32-S3's internal SRAM (~512 KB).
    static constexpr size_t MAX_PRELOAD_BYTES = 65536; // 64 KB

    httpd_req_t *req;
    std::string bodyStorage_; // owns the memory; empty when skipped

    void readBody() {
        const size_t len = req->content_len;

        if (len == 0) {
            bodyStorage_.clear();
            return;
        }

        if (len > MAX_PRELOAD_BYTES) {
            // Large body (e.g. firmware upload) — leave data in the socket
            // buffer so the handler can stream it via httpd_req_recv().
            bodyStorage_.clear();
            return;
        }

        bodyStorage_.resize(len);

        int received = httpd_req_recv(req, bodyStorage_.data(), len);
        if (received <= 0) {
            bodyStorage_.clear();
            return;
        }

        // Shrink to actual bytes received
        bodyStorage_.resize(received);
    }
};

} // namespace http