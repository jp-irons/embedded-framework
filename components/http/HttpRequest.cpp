#include "http/HttpRequest.hpp"

namespace http {

HttpRequest::HttpRequest(httpd_req_t* r)
    : req(r)
{
}

const char* HttpRequest::path() const {
    return req->uri;
}

std::string HttpRequest::body() const {
    int remaining = req->content_len;
    std::string result;
    result.reserve(remaining);

    char buf[128];

    while (remaining > 0) {
        int received = httpd_req_recv(req, buf, sizeof(buf));
        if (received <= 0) {
            break;
        }
        result.append(buf, received);
        remaining -= received;
    }

    return result;
}

} // namespace http