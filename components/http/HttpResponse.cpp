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
    std::string statusMsg = httpStatusToString(code);
    warn_err(httpd_resp_set_type(req, type));
    warn_err(httpd_resp_set_status(req, "302 Found"));
    return common::Result::Ok;
}

common::Result HttpResponse::redirect(const char *target) {
    // No copy — use target.c_str() directly
    warn_err(httpd_resp_set_status(req, "302 Found"));
    warn_err(httpd_resp_set_hdr(req, "Location", target));
    warn_err(httpd_resp_send(req, nullptr, 0));
    return common::Result::Ok;
}

common::Result HttpResponse::send(const unsigned char *data, unsigned int size, const char * type) {
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
    warn_err(httpd_resp_set_status(req, httpStatusToString(code).c_str()));
	return sendJson(body);
}

common::Result HttpResponse::sendJsonError(int code, std::string_view message) {
    httpd_resp_set_status(req, httpStatusToString(code).c_str());

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
    // Build:  Basic realm="<realm>"
    std::string challenge;
    challenge.reserve(strlen(realm) + 14); // Basic realm=""
    challenge.append("Basic realm=\"");
    challenge.append(realm);
    challenge.append("\"");

    warn_err(httpd_resp_set_status(req, "401 Unauthorized"));
    warn_err(httpd_resp_set_hdr(req, "WWW-Authenticate", challenge.c_str()));
    return sendJsonError(401, "Unauthorized");
}

void HttpResponse::warn_err(esp_err_t err) {
    if (err != ESP_OK) {
        log.warn(esp_err_to_name(err));
    }
}

} // namespace http
