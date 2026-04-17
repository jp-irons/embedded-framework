#pragma once
#include "common/Result.hpp"
#include "esp_http_server.h"

#include <string>
#include <string_view>
#include "device/EspTypeAdapter.hpp"

// TODO move method defns back from header to cpp
namespace http {

using namespace common;

class HttpResponse {
  public:
    explicit HttpResponse(httpd_req *r)
        : req(r) {}

    Result send(const unsigned char *data, unsigned int size) {
        esp_err_t err = httpd_resp_send(req, reinterpret_cast<const char *>(data), size);
        return device::toResult(err);
    }

    Result send(std::string_view data) {
        esp_err_t err = httpd_resp_send(req, data.data(), data.size());
        return device::toResult(err);
    }

    void setType(std::string_view type) {
        httpd_resp_set_type(req, type.data());
    }

    // -----------------

    Result sendText(std::string_view body) {
        httpd_resp_set_type(req, "text/plain");
        esp_err_t err = httpd_resp_send(req, body.data(), body.size());
        return device::toResult(err);
    }

    // "400 Bad Request"
    // "404 Not Found"
    // "403 Forbidden"
    // "500 Internal Server Error"
    Result sendBadRequest400(std::string_view body = "Bad Request") {
        httpd_resp_set_status(req, "400 Bad Request");
        return sendText(body);
    }

    Result sendNotFound404(std::string_view body = "Not Found") {
        httpd_resp_set_status(req, "404 Not Found");
        return sendText(body);
    }

    Result sendForbidden403(std::string_view body = "Forbidden") {
        httpd_resp_set_status(req, "403 Forbidden");
        return sendText(body);
    }

    Result sendInternalError500(std::string_view body = "Internal Server Error") {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return sendText(body);
    }

	Result sendJson(std::string_view body) {
	    httpd_resp_set_type(req, "application/json");
	    esp_err_t err = httpd_resp_send(req, body.data(), body.size());
	    return device::toResult(err);
	}

	Result sendJsonOk(std::string_view message = "Ok") {
	    std::string body;
	    body.reserve(message.size() + 10); // {"ok":""}

	    body.append("{\"ok\":\"");
	    body.append(message);
	    body.append("\"}");

	    return sendJson(body);
	}
	
	Result sendJsonResult(Result r) {
	    std::string_view text = toString(r);

	    std::string body;
	    body.reserve(text.size() + 12); // {"result":""}

	    body.append("{\"result\":\"");
	    body.append(text);
	    body.append("\"}");

	    return sendJson(body);
	}

    Result sendJsonError(std::string_view message) {
        httpd_resp_set_status(req, "400 Bad Request");

        // Build {"error":"<message>"} with one allocation
        std::string body;
        body.reserve(message.size() + 12); // {"error":""}

        body.append("{\"error\":\"");
        body.append(message);
        body.append("\"}");

        return sendJson(body);
    }

    Result sendJsonStatus(std::string_view status) {
        std::string body;
        body.reserve(status.size() + 13); // {"status":""}

        body.append("{\"status\":\"");
        body.append(status);
        body.append("\"}");

        return sendJson(body);
    }

    /* 
	 "400 Bad Request"
	 "404 Not Found"
	 "403 Forbidden"
	 "500 Internal Server Error"
	*/
    Result sendJsonError(int code, std::string_view message) {
        char statusBuf[32];
        snprintf(statusBuf, sizeof(statusBuf), "%d Error", code);
        httpd_resp_set_status(req, statusBuf);

        std::string body;
        body.reserve(message.size() + 12);
        body.append("{\"error\":\"");
        body.append(message);
        body.append("\"}");

        return sendJson(body);
    }

  private:
    httpd_req *req;
};

} // namespace http