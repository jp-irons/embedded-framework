#pragma once

#include "http/HttpServer.hpp"
#include "esp_http_server.h"

#include <string>
#include <vector>

namespace esp_platform {

/**
 * ESP-IDF concrete implementation of http::HttpServer.
 *
 * Runs an HTTPS server on port 443 and an HTTP→HTTPS redirect server on
 * port 80.  All ESP-IDF types (httpd_handle_t, httpd_req_t*, esp_err_t)
 * are confined to this header and its .cpp.
 * Nothing outside esp_platform needs to include this file — consumers
 * depend only on HttpServer.hpp.
 */
class EspHttpServer : public http::HttpServer {
  public:
  	static constexpr const char* TAG = "EspHttpServer";
    EspHttpServer();
    ~EspHttpServer() override;

    void setCert(std::string certPem, std::string keyPem) override;

    void start() override;
    void stop()  override;

    void addRoutes(const std::string &path, http::HttpHandler *handler) override;
    void addPostRoute(const std::string &path, http::HttpHandler *handler) override;
    void addGetRoute(const std::string &path, http::HttpHandler *handler) override;
    void addDeleteRoute(const std::string &path, http::HttpHandler *handler) override;
    void addRoute(http::HttpMethod method, const std::string &pathPattern,
                  http::HttpHandler *handler) override;

  private:
    httpd_handle_t server_;         // HTTPS server (port 443)
    httpd_handle_t redirectServer_; // HTTP redirect-only server (port 80)
    std::vector<std::string> ownedPaths_;

    // Runtime cert (set via setCert()). Empty = use embedded fallback.
    std::string runtimeCertPem_;
    std::string runtimeKeyPem_;

    void startRedirectServer();
    void stopRedirectServer();

    static esp_err_t handlerAdapter(httpd_req_t *req);
    static esp_err_t redirectHandler(httpd_req_t *req);
};

} // namespace esp_platform
