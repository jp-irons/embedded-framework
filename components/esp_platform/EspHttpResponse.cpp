#include "esp_platform/EspHttpResponse.hpp"

#include "http_types/HttpTypes.hpp"
#include "logger/Logger.hpp"

#include <string>
#include <string_view>

namespace esp_platform {

static logger::Logger log{"EspHttpResponse"};

EspHttpResponse::EspHttpResponse(httpd_req_t *r)
    : req_(r) {}

common::Result EspHttpResponse::send(int code, std::string_view body, const char *type) {
    // httpd_resp_set_status stores a pointer, not a copy — keep the string alive
    // until httpd_resp_send() by storing it in a local variable.
    std::string status = http::httpStatusToString(code);
    warnErr(httpd_resp_set_status(req_, status.c_str()));
    warnErr(httpd_resp_set_type(req_, type));
    warnErr(httpd_resp_send(req_, body.data(), body.size()));
    return common::Result::Ok;
}

common::Result EspHttpResponse::send(std::string_view body, const char *type) {
    warnErr(httpd_resp_set_type(req_, type));
    warnErr(httpd_resp_send(req_, body.data(), body.size()));
    return common::Result::Ok;
}

common::Result EspHttpResponse::redirect(const char *target) {
    // target must remain valid until httpd_resp_send() — callers pass string
    // literals or c_str() of a string that outlives this call.
    warnErr(httpd_resp_set_status(req_, "302 Found"));
    warnErr(httpd_resp_set_hdr(req_, "Location", target));
    warnErr(httpd_resp_send(req_, nullptr, 0));
    return common::Result::Ok;
}

common::Result EspHttpResponse::send(const unsigned char *data, unsigned int size,
                                     const char *type) {
    warnErr(httpd_resp_set_type(req_, type));
    warnErr(httpd_resp_send(req_, reinterpret_cast<const char *>(data), size));
    return common::Result::Ok;
}

common::Result EspHttpResponse::send(std::string_view data) {
    warnErr(httpd_resp_send(req_, data.data(), data.size()));
    return common::Result::Ok;
}

common::Result EspHttpResponse::sendText(std::string_view body) {
    warnErr(httpd_resp_set_type(req_, "text/plain"));
    warnErr(httpd_resp_send(req_, body.data(), body.size()));
    return common::Result::Ok;
}

common::Result EspHttpResponse::sendJson(std::string_view body) {
    warnErr(httpd_resp_set_type(req_, "application/json"));
    warnErr(httpd_resp_send(req_, body.data(), body.size()));
    return common::Result::Ok;
}

common::Result EspHttpResponse::sendJson(int code, std::string_view body) {
    // Keep the status string alive until httpd_resp_send() — httpd stores a
    // pointer, not a copy, so httpStatusToString(code).c_str() would dangle.
    std::string status = http::httpStatusToString(code);
    warnErr(httpd_resp_set_status(req_, status.c_str()));
    return sendJson(body);
}

common::Result EspHttpResponse::sendJsonError(int code, std::string_view message) {
    // Keep the status string alive until httpd_resp_send() — same dangling-
    // pointer risk as sendJson(int, ...) above.
    std::string status = http::httpStatusToString(code);
    httpd_resp_set_status(req_, status.c_str());

    // Build {"error":"<message>"} with one allocation
    std::string body;
    body.reserve(message.size() + 12); // {"error":""}

    body.append("{\"error\":\"");
    body.append(message);
    body.append("\"}");

    return sendJson(body);
}

common::Result EspHttpResponse::sendJsonStatus(std::string_view status) {
    std::string body;
    body.reserve(status.size() + 13); // {"status":""}

    body.append("{\"status\":\"");
    body.append(status);
    body.append("\"}");

    return sendJson(body);
}

common::Result EspHttpResponse::sendUnauthorized(const char *realm) {
    // Build:  Bearer realm="<realm>"
    std::string challenge;
    challenge.reserve(strlen(realm) + 15); // Bearer realm=""
    challenge.append("Bearer realm=\"");
    challenge.append(realm);
    challenge.append("\"");

    // Use Bearer scheme, not Basic — Basic triggers the browser's native
    // credential dialog, which is not appropriate here because the device
    // uses session tokens (not per-request Basic Auth) and the self-signed
    // certificate prevents Chrome from offering to save credentials anyway.
    // Bearer suppresses the native dialog; the JS login overlay handles it.
    warnErr(httpd_resp_set_hdr(req_, "WWW-Authenticate", challenge.c_str()));
    return sendJsonError(401, "Unauthorized");
}

void EspHttpResponse::warnErr(esp_err_t err) {
    if (err != ESP_OK) {
        log.warn(esp_err_to_name(err));
    }
}

} // namespace esp_platform
