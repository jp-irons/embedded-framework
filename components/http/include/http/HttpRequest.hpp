#pragma once

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

/**
 * Abstract interface for an incoming HTTP request.
 *
 * No ESP-IDF types appear here.  The concrete implementation (EspHttpRequest)
 * lives in the esp_platform component and is constructed inside EspHttpServer.
 */
class HttpRequest {
  public:
    virtual ~HttpRequest() = default;

    /** The pre-read request body (empty for large/streaming bodies). */
    virtual std::string_view body() const = 0;

    /** Full request URI including any query string. */
    virtual std::string_view uri() const = 0;

    /** URI as a null-terminated C string (same value as uri()). */
    virtual const char *path() const = 0;

    virtual HttpMethod method() const = 0;

    /**
     * Returns the Content-Length of the request body in bytes.
     * Zero if no body was sent.
     */
    virtual size_t contentLength() const = 0;

    /**
     * Read up to len bytes from the request body into buf.
     *
     * Retries once on a transient socket timeout, then returns an error.
     * Returns the number of bytes read (positive), or a negative value on
     * unrecoverable error.  Callers should treat any non-positive return as
     * a failure.
     */
    virtual int receiveChunk(char *buf, size_t len) = 0;

    /**
     * Extracts HTTP Basic Auth credentials from the Authorization header.
     *
     * Returns the decoded username and password if the header is present and
     * well-formed ("Basic <base64(username:password)>").
     * Returns std::nullopt if the header is absent, not Basic scheme, or
     * cannot be decoded.
     */
    virtual std::optional<BasicAuth> extractBasicAuth() const = 0;

    /**
     * Extracts a Bearer token from the Authorization header.
     *
     * Returns the raw token string if the header is present and has the form
     * "Bearer <token>".  Returns std::nullopt if the header is absent, uses a
     * different scheme, or the token portion is empty.
     */
    virtual std::optional<std::string> extractBearerToken() const = 0;
};

} // namespace http
