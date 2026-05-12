#include "http/HttpRequest.hpp"

#include "mbedtls/base64.h"

#include <cstring>
#include <vector>

namespace http {

// ---------------------------------------------------------------------------
// Private helper — file scope only
// ---------------------------------------------------------------------------

static HttpMethod toHttpMethod(int method) {
    switch (method) {
        case HTTP_GET:     return HttpMethod::Get;
        case HTTP_POST:    return HttpMethod::Post;
        case HTTP_PUT:     return HttpMethod::Put;
        case HTTP_DELETE:  return HttpMethod::Delete;
        case HTTP_PATCH:   return HttpMethod::Patch;
        case HTTP_HEAD:    return HttpMethod::Head;
        case HTTP_OPTIONS: return HttpMethod::Options;
        default:           return HttpMethod::Get;
    }
}

// ---------------------------------------------------------------------------
// Constructor / readBody
// ---------------------------------------------------------------------------

HttpRequest::HttpRequest(httpd_req_t *r)
    : req(r)
    , method_(toHttpMethod(r->method)) {
    readBody();
}

void HttpRequest::readBody() {
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

// ---------------------------------------------------------------------------
// Streaming body read
// ---------------------------------------------------------------------------

int HttpRequest::receiveChunk(char *buf, size_t len) {
    int received = httpd_req_recv(req, buf, len);
    if (received == HTTPD_SOCK_ERR_TIMEOUT) {
        // One transient timeout — retry once before giving up
        received = httpd_req_recv(req, buf, len);
    }
    return received;
}

// ---------------------------------------------------------------------------
// Auth helpers
// ---------------------------------------------------------------------------

std::optional<BasicAuth> HttpRequest::extractBasicAuth() const {
    static constexpr const char *HEADER_NAME = "Authorization";
    static constexpr const char *BASIC_PREFIX = "Basic ";
    static constexpr size_t      BASIC_PREFIX_LEN = 6; // strlen("Basic ")

    // ── Read the Authorization header ────────────────────────────────────
    size_t hdrLen = httpd_req_get_hdr_value_len(req, HEADER_NAME);
    if (hdrLen == 0) {
        return std::nullopt;
    }

    // httpd_req_get_hdr_value_len returns length excluding null terminator
    std::vector<char> hdrBuf(hdrLen + 1);
    if (httpd_req_get_hdr_value_str(req, HEADER_NAME,
                                    hdrBuf.data(), hdrBuf.size()) != ESP_OK) {
        return std::nullopt;
    }

    // ── Check for "Basic " scheme ─────────────────────────────────────────
    if (hdrLen <= BASIC_PREFIX_LEN ||
        strncmp(hdrBuf.data(), BASIC_PREFIX, BASIC_PREFIX_LEN) != 0) {
        return std::nullopt;
    }

    const char  *encoded    = hdrBuf.data() + BASIC_PREFIX_LEN;
    const size_t encodedLen = hdrLen - BASIC_PREFIX_LEN;

    // ── Base64-decode ─────────────────────────────────────────────────────
    // First call with dst=nullptr to obtain the required output size.
    size_t decodedLen = 0;
    int rc = mbedtls_base64_decode(
        nullptr, 0, &decodedLen,
        reinterpret_cast<const unsigned char *>(encoded), encodedLen);

    // MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL is expected when dst is nullptr
    if (rc != 0 && rc != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        return std::nullopt;
    }

    std::vector<unsigned char> decoded(decodedLen);
    rc = mbedtls_base64_decode(
        decoded.data(), decoded.size(), &decodedLen,
        reinterpret_cast<const unsigned char *>(encoded), encodedLen);
    if (rc != 0) {
        return std::nullopt;
    }

    // ── Split on the first ':' ────────────────────────────────────────────
    // username may not contain ':', but password may — split on first only.
    const char *data  = reinterpret_cast<const char *>(decoded.data());
    const char *colon = static_cast<const char *>(memchr(data, ':', decodedLen));
    if (colon == nullptr) {
        return std::nullopt;
    }

    BasicAuth auth;
    auth.username.assign(data, colon - data);
    auth.password.assign(colon + 1, decodedLen - (colon - data) - 1);
    return auth;
}

std::optional<std::string> HttpRequest::extractBearerToken() const {
    static constexpr const char *HEADER_NAME    = "Authorization";
    static constexpr const char *BEARER_PREFIX  = "Bearer ";
    static constexpr size_t      BEARER_PREFIX_LEN = 7; // strlen("Bearer ")

    size_t hdrLen = httpd_req_get_hdr_value_len(req, HEADER_NAME);
    if (hdrLen == 0) {
        return std::nullopt;
    }

    std::vector<char> hdrBuf(hdrLen + 1);
    if (httpd_req_get_hdr_value_str(req, HEADER_NAME,
                                    hdrBuf.data(), hdrBuf.size()) != ESP_OK) {
        return std::nullopt;
    }

    if (hdrLen <= BEARER_PREFIX_LEN ||
        strncmp(hdrBuf.data(), BEARER_PREFIX, BEARER_PREFIX_LEN) != 0) {
        return std::nullopt;
    }

    // Token is everything after "Bearer "
    std::string token(hdrBuf.data() + BEARER_PREFIX_LEN,
                      hdrLen - BEARER_PREFIX_LEN);
    if (token.empty()) {
        return std::nullopt;
    }
    return token;
}

} // namespace http
