#include "http/HttpServer.hpp"

namespace http {

HttpServer::HttpServer() {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);
}

void HttpServer::stop() {
    if (server) {
        httpd_stop(server);
        server = nullptr;
    }
}

void HttpServer::registerHandler(const char* uri,
                                 httpd_method_t method,
                                 HandlerFn fn)
{
    httpd_uri_t handler = {
        .uri       = uri,
        .method    = method,
        .handler   = fn,
        .user_ctx  = nullptr
    };

    httpd_register_uri_handler(server, &handler);
}

} // namespace http