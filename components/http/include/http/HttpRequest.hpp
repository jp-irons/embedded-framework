#pragma once

#include "esp_http_server.h"
#include "device/EspTypeAdapter.hpp"
#include "http/HttpMethod.hpp"

#include <string>
#include <string_view>

namespace http {
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

  private:
    httpd_req_t *req;
    std::string bodyStorage_; // owns the memory

    void readBody() {
        size_t len = req->content_len;

        if (len == 0) {
            bodyStorage_.clear();
            return;
        }

        bodyStorage_.resize(len);

        int received = httpd_req_recv(req, bodyStorage_.data(), len);
        if (received <= 0) {
            bodyStorage_.clear();
            return;
        }

        // shrink to actual bytes received
        bodyStorage_.resize(received);
    }
};

} // namespace http