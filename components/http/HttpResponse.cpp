#include "http/HttpResponse.hpp"

#include "http/HttpTypes.hpp"
#include "logger/Logger.hpp"

#include "esp_http_server.h"

#include <string>
#include <string_view>

namespace http {

static logger::Logger log{"HttpResponse"};

HttpResponse::HttpResponse(httpd_req *r)
    : req(r) {}

/*
 "200 Ok"
 "301 Moved Permanently"
 "302 Found"
 "400 Bad Request"
 "403 Forbidden"
 "404 Not Found"
 "405 Method Not allowed"
 "500 Internal Server Error"
 "501 Not Implemented"
*/
common::Result HttpResponse::send(int code, std::string_view body, const char *type) {
    // httpd_resp_set_status stores a pointer, not a copy — keep the string alive
    // until httpd_resp_send() by storing it in a local variable.
    std::string status = httpStatusToString(code);
    warn_err(httpd_resp_set_status(req, status.c_str()));
    warn_err(httpd_resp_set_type(req, type));
    warn_err(httpd_resp_send(req, body.data(), body.size()));
    return common::Result::Ok;
}

common::Result HttpResponse::redirect(const char *target) {
    // target must remain valid until httpd_resp_send() — callers pass string
    // literals or c_str() of a string that outlives this call.
    warn_err(httpd_resp_set_status(req, "302 Found"));
    warn_err(httpd_resp_set_hdr(req, "Location", target));
    warn_err(httpd_resp_send(req, nullptr, 0));
    return common::Result::Ok;
}

common::Result HttpResponse::send(const unsigned char *data, unsigned int size, const char *type) {
    warn_err(httpd_resp_set_type(req, type));
    warn_err(httpd_resp_send(req, reinterpret_cast<const char *>(data), size));
    return common::Result::Ok;
}

common::Result HttpResponse::send(std::string_view data) {
    warn_err(httpd_resp_send(req, data.data(), data.size()));
    return common::Result::Ok;
}

common::Result HttpResponse::sendText(std::string_view body) {
    warn_err(httpd_resp_set_type(req, "text/plain"));
    warn_err(httpd_resp_send(req, body.data(), body.size()));
    return common::Result::Ok;
}

common::Result HttpResponse::sendJson(std::string_view body) {
    warn_err(httpd_resp_set_type(req, "application/json"));
    warn_err(httpd_resp_send(req, body.data(), body.size()));
    return common::Result::Ok;
}

common::Result HttpResponse::sendJson(int code, std::string_view body) {
    // Keep the status string alive until httpd_resp_send() — httpd stores a
    // pointer, not a copy, so httpStatusToString(code).c_str() would dangle.
    std::string status = httpStatusToString(code);
    warn_err(httpd_resp_set_status(req, status.c_str()));
    return sendJson(body);
}

common::Result HttpResponse::sendJsonError(int code, std::string_view message) {
    // Keep the status string alive until httpd_resp_send() — same dangling-
    // pointer risk as sendJson(int, ...) above.
    std::string status = httpStatusToString(code);
    httpd_resp_set_status(req, status.c_str());

    // Build {"error":"<message>"} with one allocation
    std::string body;
    body.reserve(message.size() + 12); // {"error":""}

    body.append("{\"error\":\"");
    body.append(message);
    body.append("\"}");

    return sendJson(body);
}

common::Result HttpResponse::sendJsonStatus(std::string_view status) {
    std::string body;
    body.reserve(status.size() + 13); // {"status":""}

    body.append("{\"status\":\"");
    body.append(status);
    body.append("\"}");

    return sendJson(body);
}

common::Result HttpResponse::sendUnauthorized(const char *realm) {
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
    warn_err(httpd_resp_set_hdr(req, "WWW-Authenticate", challenge.c_str()));
    return sendJsonError(401, "Unauthorized");
}

void HttpResponse::warn_err(esp_err_t err) {
    if (err != ESP_OK) {
        log.warn(esp_err_to_name(err));
    }
}

} // namespace http
