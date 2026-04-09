#include "http/HttpServer.hpp"
#include "logger/Logger.hpp"

namespace http {

static logger::Logger log{"HttpServer"};

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
    log.debug("addRoute '%s'", path.c_str());
    httpd_uri_t uri = {
        .uri = path.c_str(), .method = HTTP_GET, .handler = &HttpServer::handlerAdapter, .user_ctx = handler};

    httpd_register_uri_handler(server, &uri);
}

esp_err_t HttpServer::handlerAdapter(httpd_req_t *req) {
	log.debug("handlerAdapter '%s'", req->uri);
    auto *handler = static_cast<http::HttpHandler *>(req->user_ctx);
    http::HttpRequest request(req);
    http::HttpResponse response(req);
    bool resp_ok = handler->handle(request, response);
	if (!resp_ok) {
		log.warn("handlerAdapter fail '%s'", req->uri);
	}
    return ESP_OK;
}

} // namespace http