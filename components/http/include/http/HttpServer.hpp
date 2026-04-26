#pragma once

#include "esp_http_server.h"
#include "http/HttpHandler.hpp"
#include <string>
#include <vector>

namespace http {

class HttpServer {
  public:
    HttpServer();
    ~HttpServer();

    void start();
    void stop();

    void addRoutes(const std::string &path, HttpHandler *handler);
	void addPostRoute(const std::string &path, HttpHandler *handler);
	void addGetRoute(const std::string &path, HttpHandler *handler);
	void addDeleteRoute(const std::string &path, HttpHandler *handler);
    void addRoute(HttpMethod method, const std::string &pathPattern, HttpHandler *handler);

  private:
    httpd_handle_t server;
	std::vector<std::string> ownedPaths;

    static esp_err_t handlerAdapter(httpd_req_t *req);
};

} // namespace http
