#pragma once

#include "esp_http_server.h"
#include <string>

namespace http {

class HttpResponse {
public:
    explicit HttpResponse(httpd_req_t* r);

    void text(const std::string& body);
    void json(const std::string& body);
    void jsonStatus(const char* status);

    httpd_req_t* raw() const { return req; }

private:
    httpd_req_t* req;
};

} // namespace http