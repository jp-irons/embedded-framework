#pragma once

#include "http/HttpHandler.hpp"
#include <string>

namespace http {

/**
 * Abstract interface for the HTTPS server.
 *
 * No ESP-IDF types appear here.  The concrete implementation (EspHttpServer)
 * lives in the esp_platform component and is constructed in FrameworkContext.
 */
class HttpServer {
  public:

    virtual ~HttpServer() = default;

    /**
     * Override the embedded self-signed cert with a runtime-supplied one.
     * Must be called before start().  Both strings must be null-terminated PEM.
     * If not called, the cert embedded via EMBED_TXTFILES is used as a fallback.
     */
    virtual void setCert(std::string certPem, std::string keyPem) = 0;

    /** Starts the HTTPS server on port 443 and an HTTP→HTTPS redirector on port 80. */
    virtual void start() = 0;
    virtual void stop()  = 0;

    /** Register handler for GET, POST and DELETE on the given path pattern. */
    virtual void addRoutes(const std::string &path, HttpHandler *handler) = 0;

    virtual void addPostRoute(const std::string &path, HttpHandler *handler) = 0;
    virtual void addGetRoute(const std::string &path, HttpHandler *handler) = 0;
    virtual void addDeleteRoute(const std::string &path, HttpHandler *handler) = 0;
    virtual void addRoute(HttpMethod method, const std::string &pathPattern,
                          HttpHandler *handler) = 0;
};

} // namespace http
