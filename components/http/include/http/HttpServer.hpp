#pragma once

#include "esp_http_server.h"
#include "esp_https_server.h"
#include "http/HttpHandler.hpp"
#include <string>
#include <vector>

namespace http {

class HttpServer {
  public:
    HttpServer();
    ~HttpServer();

    /**
     * Override the embedded self-signed cert with a runtime-supplied one.
     * Must be called before start().  Both strings must be null-terminated PEM.
     * If not called, the cert embedded via EMBED_TXTFILES is used as a fallback.
     */
    void setCert(std::string certPem, std::string keyPem);

    // Starts the HTTPS server on port 443 and an HTTP→HTTPS redirector on port 80.
    void start();
    void stop();

    void addRoutes(const std::string &path, HttpHandler *handler);
    void addPostRoute(const std::string &path, HttpHandler *handler);
    void addGetRoute(const std::string &path, HttpHandler *handler);
    void addDeleteRoute(const std::string &path, HttpHandler *handler);
    void addRoute(HttpMethod method, const std::string &pathPattern, HttpHandler *handler);

  private:
    httpd_handle_t server;         // HTTPS server (port 443)
    httpd_handle_t redirectServer; // HTTP redirect-only server (port 80)
    std::vector<std::string> ownedPaths;

    // Runtime cert (set via setCert()). Empty = use embedded fallback.
    std::string runtimeCertPem_;
    std::string runtimeKeyPem_;

    void startRedirectServer();
    void stopRedirectServer();

    static esp_err_t handlerAdapter(httpd_req_t *req);
    static esp_err_t redirectHandler(httpd_req_t *req);
};

} // namespace http
