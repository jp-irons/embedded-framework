#pragma once

#include <string>

namespace http {

enum class HttpMethod {
    Get,
    Post,
    Put,
    Delete,
    Patch,
    Head,
    Options
};

inline std::string toString(HttpMethod method) {
    switch (method) {
        case HttpMethod::Get:     return "Get";
        case HttpMethod::Post:    return "Post";
        case HttpMethod::Put:     return "Put";
        case HttpMethod::Delete:  return "Delete";
        case HttpMethod::Patch:   return "Patch";
        case HttpMethod::Head:    return "Head";
        case HttpMethod::Options: return "Options";
    }
    return "Unknown";
}

inline std::string httpStatusToString(int code) {
    switch (code) {
        case 200: return "200 OK";
        case 201: return "201 Created";
        case 202: return "202 Accepted";
        case 204: return "204 No Content";

        case 301: return "301 Moved Permanently";
        case 302: return "302 Found";
        case 304: return "304 Not Modified";

        case 400: return "400 Bad Request";
        case 401: return "401 Unauthorized";
        case 403: return "403 Forbidden";
        case 404: return "404 Not Found";
        case 405: return "405 Method Not Allowed";
        case 408: return "408 Request Timeout";

        case 500: return "500 Internal Server Error";
        case 501: return "501 Not Implemented";
        case 503: return "503 Service Unavailable";

        default:  return "000 Unknown";
    }
}

} // namespace http
