#include "http/HttpResponse.hpp"
#include <cstring>

namespace http {

HttpResponse::HttpResponse(httpd_req_t* r)
    : req(r)
{
}

void HttpResponse::text(const std::string& body) {
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, body.c_str(), body.size());
}

void HttpResponse::json(const std::string& body) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, body.c_str(), body.size());
}

void HttpResponse::jsonStatus(const char* status) {
    std::string body = std::string("{\"status\":\"") + status + "\"}";
    json(body);
}

} // namespace http