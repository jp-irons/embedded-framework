#include "esp_platform/EspHttpServer.hpp"

#include "esp_platform/EspHttpRequest.hpp"
#include "esp_platform/EspHttpResponse.hpp"
#include "http/HttpHandler.hpp"
#include "logger/Logger.hpp"
#include "esp_err.h"
#include "esp_https_server.h"

#include <cstring>

namespace esp_platform {

static httpd_method_t toEspIdfMethod(http::HttpMethod method) {
    switch (method) {
        case http::HttpMethod::Get:     return HTTP_GET;
        case http::HttpMethod::Post:    return HTTP_POST;
        case http::HttpMethod::Put:     return HTTP_PUT;
        case http::HttpMethod::Delete:  return HTTP_DELETE;
        case http::HttpMethod::Patch:   return HTTP_PATCH;
        case http::HttpMethod::Head:    return HTTP_HEAD;
        case http::HttpMethod::Options: return HTTP_OPTIONS;
    }
    return HTTP_GET;
}

static logger::Logger log{"EspHttpServer"};

// Embedded self-signed cert + key (see components/esp_platform/certs/ and CMakeLists.txt EMBED_TXTFILES)
extern const uint8_t servercert_pem_start[] asm("_binary_servercert_pem_start");
extern const uint8_t servercert_pem_end[]   asm("_binary_servercert_pem_end");
extern const uint8_t prvtkey_pem_start[]    asm("_binary_prvtkey_pem_start");
extern const uint8_t prvtkey_pem_end[]      asm("_binary_prvtkey_pem_end");

EspHttpServer::EspHttpServer()
    : server_(nullptr), redirectServer_(nullptr) {}

EspHttpServer::~EspHttpServer() {
    stop();
}

void EspHttpServer::setCert(std::string certPem, std::string keyPem) {
    runtimeCertPem_ = std::move(certPem);
    runtimeKeyPem_  = std::move(keyPem);
}

void EspHttpServer::start() {
    if (server_) {
        return;
    }

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.httpd.uri_match_fn    = httpd_uri_match_wildcard;
    conf.httpd.lru_purge_enable = true;
    // The framework UI loads ~10 files as parallel ES-module fetches.
    // HTTPD_SSL_CONFIG_DEFAULT gives max_open_sockets=9 which is too tight
    // once the redirect server also holds a few slots.  Set 13 here and keep
    // CONFIG_LWIP_MAX_SOCKETS >= 16 in sdkconfig so lwIP has headroom.
    conf.httpd.max_open_sockets = 13;

    if (!runtimeCertPem_.empty()) {
        // Use the per-device cert generated/loaded by DeviceCert
        conf.servercert     = reinterpret_cast<const uint8_t *>(runtimeCertPem_.c_str());
        conf.servercert_len = runtimeCertPem_.size() + 1;  // include null terminator
        conf.prvtkey_pem    = reinterpret_cast<const uint8_t *>(runtimeKeyPem_.c_str());
        conf.prvtkey_len    = runtimeKeyPem_.size() + 1;
        log.info("Using runtime (per-device) TLS cert");
    } else {
        // Fall back to the embedded dev cert (EMBED_TXTFILES)
        conf.servercert     = servercert_pem_start;
        conf.servercert_len = servercert_pem_end - servercert_pem_start;
        conf.prvtkey_pem    = prvtkey_pem_start;
        conf.prvtkey_len    = prvtkey_pem_end - prvtkey_pem_start;
        log.warn("Using embedded fallback TLS cert — call setCert() for per-device certs");
    }

    esp_err_t err = httpd_ssl_start(&server_, &conf);
    if (err != ESP_OK) {
        log.error("httpd_ssl_start failed: %s", esp_err_to_name(err));
        server_ = nullptr;
        return;
    }
    log.info("HTTPS server started on port %d", conf.port_secure);

    startRedirectServer();
}

void EspHttpServer::stop() {
    if (server_) {
        httpd_ssl_stop(server_);
        server_ = nullptr;
    }
    stopRedirectServer();
}

void EspHttpServer::startRedirectServer() {
    if (redirectServer_) {
        return;
    }
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = 80;
    cfg.ctrl_port        = 32767;           // must differ from the HTTPS server's ctrl_port
    cfg.uri_match_fn     = httpd_uri_match_wildcard;
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 8;

    esp_err_t err = httpd_start(&redirectServer_, &cfg);
    if (err != ESP_OK) {
        log.error("HTTP redirect httpd_start failed: %s", esp_err_to_name(err));
        redirectServer_ = nullptr;
        return;
    }

    static const httpd_method_t methods[] = {
        HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE, HTTP_HEAD, HTTP_PATCH, HTTP_OPTIONS
    };
    for (auto method : methods) {
        httpd_uri_t uri = {
            .uri      = "/*",
            .method   = method,
            .handler  = &EspHttpServer::redirectHandler,
            .user_ctx = nullptr,
        };
        httpd_register_uri_handler(redirectServer_, &uri);
    }
    log.info("HTTP->HTTPS redirector started on port %d", cfg.server_port);
}

void EspHttpServer::stopRedirectServer() {
    if (redirectServer_) {
        httpd_stop(redirectServer_);
        redirectServer_ = nullptr;
    }
}

esp_err_t EspHttpServer::redirectHandler(httpd_req_t *req) {
    char host[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK) {
        host[0] = '\0';
    } else {
        // Strip any :port suffix so we land on the HTTPS default port (443).
        char *colon = strchr(host, ':');
        if (colon) {
            *colon = '\0';
        }
    }

    std::string location = "https://";
    location += host;
    location += req->uri;

    log.debug("redirect '%s' -> '%s'", req->uri, location.c_str());

    httpd_resp_set_status(req, "301 Moved Permanently");
    httpd_resp_set_hdr(req, "Location", location.c_str());
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Redirecting to HTTPS\n", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void EspHttpServer::addRoutes(const std::string &path, http::HttpHandler *handler) {
    addGetRoute(path, handler);
    addPostRoute(path, handler);
    addDeleteRoute(path, handler);
}

void EspHttpServer::addGetRoute(const std::string &path, http::HttpHandler *handler) {
    addRoute(http::HttpMethod::Get, path, handler);
}

void EspHttpServer::addPostRoute(const std::string &path, http::HttpHandler *handler) {
    addRoute(http::HttpMethod::Post, path, handler);
}

void EspHttpServer::addDeleteRoute(const std::string &path, http::HttpHandler *handler) {
    addRoute(http::HttpMethod::Delete, path, handler);
}

void EspHttpServer::addRoute(http::HttpMethod method, const std::string &path,
                             http::HttpHandler *handler) {
    log.debug("addRoute %s '%s'", http::toString(method).c_str(), path.c_str());

    ownedPaths_.push_back(path);
    httpd_uri_t uri = {
        .uri      = ownedPaths_.back().c_str(),
        .method   = toEspIdfMethod(method),
        .handler  = &EspHttpServer::handlerAdapter,
        .user_ctx = handler
    };
    httpd_register_uri_handler(server_, &uri);
}

esp_err_t EspHttpServer::handlerAdapter(httpd_req_t *req) {
    log.debug("handlerAdapter '%s'", req->uri);
    auto *handler = static_cast<http::HttpHandler *>(req->user_ctx);
    EspHttpRequest  request(req);
    EspHttpResponse response(req);
    common::Result handlerResp = handler->handle(request, response);
    if (common::Result::Ok != handlerResp) {
        log.error("handlerAdapter fail '%s'", req->uri);
    }
    return ESP_OK;
}

} // namespace esp_platform
