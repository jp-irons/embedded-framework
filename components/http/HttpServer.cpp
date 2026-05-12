#include "http/HttpServer.hpp"

#include "http/HttpHandler.hpp"
#include "http/HttpRequest.hpp"
#include "logger/Logger.hpp"
#include "esp_err.h"
#include "esp_https_server.h"

#include <cstring>

namespace http {

static httpd_method_t toEspIdfMethod(HttpMethod method) {
    switch (method) {
        case HttpMethod::Get:     return HTTP_GET;
        case HttpMethod::Post:    return HTTP_POST;
        case HttpMethod::Put:     return HTTP_PUT;
        case HttpMethod::Delete:  return HTTP_DELETE;
        case HttpMethod::Patch:   return HTTP_PATCH;
        case HttpMethod::Head:    return HTTP_HEAD;
        case HttpMethod::Options: return HTTP_OPTIONS;
    }
    return HTTP_GET;
}

using namespace http;

static logger::Logger log{"HttpServer"};

// Embedded self-signed cert + key (see components/http/certs/ and CMakeLists.txt EMBED_TXTFILES)
extern const uint8_t servercert_pem_start[] asm("_binary_servercert_pem_start");
extern const uint8_t servercert_pem_end[]   asm("_binary_servercert_pem_end");
extern const uint8_t prvtkey_pem_start[]    asm("_binary_prvtkey_pem_start");
extern const uint8_t prvtkey_pem_end[]      asm("_binary_prvtkey_pem_end");

HttpServer::HttpServer()
    : server(nullptr), redirectServer(nullptr) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::setCert(std::string certPem, std::string keyPem) {
    runtimeCertPem_ = std::move(certPem);
    runtimeKeyPem_  = std::move(keyPem);
}

void HttpServer::start() {
    if (server) {
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

    esp_err_t err = httpd_ssl_start(&server, &conf);
    if (err != ESP_OK) {
        log.error("httpd_ssl_start failed: %s", esp_err_to_name(err));
        server = nullptr;
        return;
    }
    log.info("HTTPS server started on port %d", conf.port_secure);

    startRedirectServer();
}

void HttpServer::stop() {
    if (server) {
        httpd_ssl_stop(server);
        server = nullptr;
    }
    stopRedirectServer();
}

void HttpServer::startRedirectServer() {
    if (redirectServer) {
        return;
    }
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port     = 80;
    cfg.ctrl_port       = 32767;            // must differ from the HTTPS server's ctrl_port
    cfg.uri_match_fn    = httpd_uri_match_wildcard;
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 8;

    esp_err_t err = httpd_start(&redirectServer, &cfg);
    if (err != ESP_OK) {
        log.error("HTTP redirect httpd_start failed: %s", esp_err_to_name(err));
        redirectServer = nullptr;
        return;
    }

    static const httpd_method_t methods[] = {
        HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE, HTTP_HEAD, HTTP_PATCH, HTTP_OPTIONS
    };
    for (auto method : methods) {
        httpd_uri_t uri = {
            .uri      = "/*",
            .method   = method,
            .handler  = &HttpServer::redirectHandler,
            .user_ctx = nullptr,
        };
        httpd_register_uri_handler(redirectServer, &uri);
    }
    log.info("HTTP->HTTPS redirector started on port %d", cfg.server_port);
}

void HttpServer::stopRedirectServer() {
    if (redirectServer) {
        httpd_stop(redirectServer);
        redirectServer = nullptr;
    }
}

esp_err_t HttpServer::redirectHandler(httpd_req_t *req) {
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
        .uri      = ownedPaths.back().c_str(),
        .method   = toEspIdfMethod(method),
        .handler  = &HttpServer::handlerAdapter,
        .user_ctx = handler
    };
    httpd_register_uri_handler(server, &uri);
}

esp_err_t HttpServer::handlerAdapter(httpd_req_t *req) {
    log.debug("handlerAdapter '%s'", req->uri);
    auto *handler = static_cast<HttpHandler *>(req->user_ctx);
    HttpRequest request(req);
    HttpResponse response(req);
    common::Result handlerResp = handler->handle(request, response);
    if (common::Result::Ok != handlerResp) {
        log.error("handlerAdapter fail '%s'", req->uri);
    }
    return ESP_OK;
}

} // namespace http
