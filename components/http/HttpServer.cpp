#include "common/Result.hpp"
#include "http/HttpServer.hpp"

#include "device/EspTypeAdapter.hpp"
#include "http/HttpMethod.hpp"
#include "logger/Logger.hpp"
#include "esp_err.h"

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

void HttpServer::addRoutes(const std::string &path, HttpHandler *handler) {
	addGetRoute(path, handler);
	addPostRoute(path, handler);
	addDeleteRoute(path, handler);
}

void HttpServer::addGetRoute(const std::string &path, HttpHandler *handler) {
	return addRoute(HttpMethod::Get, path, handler);
}

void HttpServer::addPostRoute(const std::string &path, HttpHandler *handler) {
	return addRoute(HttpMethod::Post, path, handler);
}

void HttpServer::addDeleteRoute(const std::string &path, HttpHandler *handler) {
	return addRoute(HttpMethod::Delete, path, handler);
}

void HttpServer::addRoute(HttpMethod method, const std::string &path, HttpHandler *handler) {
    log.debug("addRoute %s '%s'", toString(method).c_str(), path.c_str());
	
	
	ownedPaths.push_back(path);
	httpd_uri_t uri = {
	    .uri = ownedPaths.back().c_str(),
	    .method = device::toEspIdfMethod(method),
	    .handler = &HttpServer::handlerAdapter,
	    .user_ctx = handler
	};
	httpd_register_uri_handler(server, &uri);
}

esp_err_t HttpServer::handlerAdapter(httpd_req_t *req) {
	log.debug("handlerAdapter '%s'", req->uri);
    auto *handler = static_cast<http::HttpHandler *>(req->user_ctx);
    http::HttpRequest request(req);
    http::HttpResponse response(req);
    common::Result handlerResp = handler->handle(request, response);
	if (handlerResp != Result::Ok) {
		log.error("handlerAdapter fail '%s'", req->uri);
	}
    return ESP_OK;
}

} // namespace http