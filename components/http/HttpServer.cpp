#include "http/HttpServer.hpp"
#include "esp_log.h"

namespace http {

static const char *TAG = "HttpServer";

HttpServer::HttpServer()
    : server(nullptr) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    if (server) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.uri_match_fn = httpd_uri_match_wildcard; 
    httpd_start(&server, &config);
}

void HttpServer::stop() {
    if (server) {
        httpd_stop(server);
        server = nullptr;
    }
}

void HttpServer::addRoute(const std::string &path, HttpHandler *handler) {
    ESP_LOGD(TAG, "addRoute '%s'", path.c_str());
    httpd_uri_t uri = {
        .uri = path.c_str(), .method = HTTP_GET, .handler = &HttpServer::handlerAdapter, .user_ctx = handler};

    httpd_register_uri_handler(server, &uri);
}

esp_err_t HttpServer::handlerAdapter(httpd_req_t *req) {
	ESP_LOGD(TAG, "handlerAdapter '%s'", req->uri);
    auto *handler = static_cast<http::HttpHandler *>(req->user_ctx);
    http::HttpRequest request(req);
    http::HttpResponse response(req);
    handler->handle(request, response);
    return ESP_OK;
}

} // namespace http