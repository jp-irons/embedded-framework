// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

#include "http/HttpServer.hpp"
#include "esp_http_server.h"

#include <functional>
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

    void setOnRequestCallback(std::function<void()> cb) override;

    void start() override;
    void stop()  override;

    int activeSocketCount() const override;

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

    // Optional pre-dispatch hook — called on every request before the handler.
    std::function<void()> onRequest_;

    void startRedirectServer();
    void stopRedirectServer();

    static esp_err_t handlerAdapter(httpd_req_t *req);
    static esp_err_t redirectHandler(httpd_req_t *req);

    // Diagnostic only — logs the peer IP:port of a session. Added 2026-07-13
    // investigating esp_tls_create_server_session 0xffff7ff7 (handshake
    // timeout) failures. IMPORTANT (corrected after reading esp_https_server's
    // actual source): for the SSL server, httpd_ssl_start() reassigns
    // conf.httpd.open_fn to its own httpd_ssl_open() internally, and only
    // calls *our* callback (saved as ssl_ctx->open_fn) AFTER
    // esp_tls_server_session_create() has already SUCCEEDED — this fires on
    // successful handshakes only, not on raw TCP accept, and NOT for failed/
    // timed-out handshake attempts. It cannot identify who is behind a
    // -0x7280/-0x7780/0xffff7ff7 failure. It's still useful for confirming
    // who successfully connects and how long that took.
    // Always returns ESP_OK (never rejects the connection based on this).
    static esp_err_t logPeerOnOpen(httpd_handle_t hd, int sockfd);
};

} // namespace esp_platform
