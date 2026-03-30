#pragma once

#include "esp_http_server.h"
#include <string>

namespace http {

class HttpRequest {
public:
    explicit HttpRequest(httpd_req_t* r);

    const char* path() const;
    std::string body() const;

    httpd_req_t* raw() const { return req; }

private:
    httpd_req_t* req;
};

} // namespace http