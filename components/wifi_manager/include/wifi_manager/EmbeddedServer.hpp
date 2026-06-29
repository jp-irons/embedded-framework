// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

#include "framework_files/EmbeddedFileHandler.hpp"
#include "framework_files/EmbeddedFileTable.hpp"
#include "auth/ApiKeyStore.hpp"
#include "auth/AuthApiHandler.hpp"
#include "auth/AuthConfig.hpp"
#include "auth/AuthStore.hpp"
#include "auth/SessionStore.hpp"
#include "common/Result.hpp"
#include "network_store/NetworkApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpServer.hpp"
#include "http_types/HttpTypes.hpp"
#include "ota/OtaApiHandler.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"

namespace wifi_manager {

struct WiFiContext;

/**
 * The central HTTP dispatcher for the framework.
 *
 * Dispatch order (first match wins):
 *   1. Root redirect  — / and /index.html → entryPoint (set via setEntryPoint)
 *   2. Auth check     — framework API paths only
 *   3. Framework API  — /framework/api/...
 *   4. App routes     — registered via addAppRoute()     (method + prefix match)
 *   5. App files      — registered via addAppFileHandler() (prefix match)
 *   6. Framework files — /framework/ui/... (fallback)
 */
class EmbeddedServer : public http::HttpHandler {
  public:
    static constexpr const char* TAG = "EmbeddedServer";

    explicit EmbeddedServer(WiFiContext &ctx,
                            http::HttpServer &server,
                            WiFiApiHandler &wifiApi,
                            network_store::NetworkApiHandler &networkApi,
                            device::DeviceApiHandler &deviceApi,
                            ota::OtaApiHandler &otaApi);
    ~EmbeddedServer();

    bool start();
    void stop();

    /** Pass a per-device cert through to HttpServer.  Call before start(). */
    void setCert(std::string certPem, std::string keyPem);

    /**
     * Wire in authentication.  Call once from FrameworkContext before start().
     * Until this is called, all requests are passed through without auth checks.
     *
     * @param sessionStore  In-RAM session token store (browser sessions).
     * @param apiKeyStore   NVS-backed API key store (M2M access).
     */
    void setAuth(auth::AuthStore       &store,
                 const auth::AuthConfig &config,
                 auth::AuthApiHandler   &authHandler,
                 auth::SessionStore     &sessionStore,
                 auth::ApiKeyStore      &apiKeyStore);

    // -----------------------------------------------------------------------
    // App injection API — call these in ApplicationContext::start() BEFORE
    // calling FrameworkContext::start().
    // -----------------------------------------------------------------------

    /**
     * Set the URL the root path (/) redirects to.
     * Default: "/framework/ui/" (the framework's own management UI).
     * Typical app override: "/app/ui/"
     */
    void setEntryPoint(std::string path);

    /**
     * Register an app API route.  Requests whose path starts with `prefix`
     * AND whose HTTP method equals `method` are dispatched to `handler`.
     * Prefix must NOT start with "/framework/" (a warning is logged if it does).
     *
     * Must be called before start().
     */
    void addAppRoute(http::HttpMethod method, std::string prefix,
                     http::HttpHandler *handler);

    /**
     * Register an app static-file handler.  Requests whose path starts with
     * `prefix` are dispatched to `handler` (regardless of method).
     * Prefix must NOT start with "/framework/" (a warning is logged if it does).
     *
     * Must be called before start().
     */
    void addAppFileHandler(std::string prefix, http::HttpHandler *handler);


    void startProvisioningMode();
    void startRuntimeMode();

    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    // Path suffixes for auth endpoints with special treatment.
    static constexpr const char *AUTH_CONFIG_SUFFIX   = "/auth/config";
    static constexpr const char *AUTH_STATUS_SUFFIX   = "/auth/status";
    static constexpr const char *AUTH_PASSWORD_SUFFIX = "/auth/password";
    static constexpr const char *AUTH_LOGIN_SUFFIX    = "/auth/login";

    // ── Framework-internal route table ──────────────────────────────────────
    struct Route {
        std::string        prefix;
        http::HttpHandler *handler;
    };

    // ── App-supplied routes (registered via addAppRoute) ────────────────────
    struct AppRoute {
        http::HttpMethod   method;
        std::string        prefix;
        http::HttpHandler *handler;
    };

    // ── App-supplied file handlers (registered via addAppFileHandler) ────────
    struct AppFileHandler {
        std::string        prefix;
        http::HttpHandler *handler;
    };

    WiFiContext &ctx;
    std::string  apiUri_;         // ctx.rootUri + "/api"
    std::string  entryPoint_ = "/framework/ui/";

    http::HttpServer &server_;

    // Framework file table + handler (fallback for /framework/ui/*).
    // frameworkFileTable_ MUST be declared before frameworkFileHandler_ so it is
    // initialised first.
    framework_files::FrameworkFileTable  frameworkFileTable_;
    framework_files::EmbeddedFileHandler frameworkFileHandler_;


    // Framework API handlers.
    //
    // wifiHandler/networkHandler/otaHandler are value members — copy-
    // constructed from the ctor args. That's only safe because nothing ever
    // calls a setter on the *original* handler object after this copy is
    // made. deviceHandler used to follow the same pattern and broke exactly
    // that way: FrameworkContext::setLogSink() called setLogSink() on the
    // original DeviceApiHandler *after* EmbeddedServer had already copied it,
    // so the copy held a permanently-stale (null) log sink and every
    // /device/logs request 501'd regardless of how setLogSink() was wired.
    // Fixed by making deviceHandler a reference instead of a copy.
    //
    // If you ever add a post-construction setter to WiFiApiHandler,
    // NetworkApiHandler, or OtaApiHandler, convert that member to a
    // reference too — same landmine.
    wifi_manager::WiFiApiHandler                  wifiHandler;
    network_store::NetworkApiHandler              networkHandler;
    device::DeviceApiHandler                      &deviceHandler;
    ota::OtaApiHandler                            otaHandler;

    // Framework route table (built once in start())
    std::vector<Route>          frameworkRoutes_;
    bool                        routesRegistered_ = false;

    // App-supplied routes and file handlers
    std::vector<AppRoute>       appRoutes_;
    std::vector<AppFileHandler> appFileHandlers_;

    // Set to true after the first completed HTTPS request; used to call
    // markValid() exactly once to cancel the OTA rollback timer.
    bool otaMarkedValid_ = false;

    // Auth — null until setAuth() is called
    auth::AuthStore        *authStore_        = nullptr;
    const auth::AuthConfig *authConfig_       = nullptr;
    auth::AuthApiHandler   *authApiHandler_   = nullptr;
    auth::SessionStore     *sessionStore_     = nullptr;
    auth::ApiKeyStore      *apiKeyStore_      = nullptr;

    // Returns Forbidden if the auth policy blocks the current request,
    // or Ok to proceed.
    common::Result checkAuth(http::HttpRequest &req, http::HttpResponse &res,
                             const std::string &path);

    // Logs a warning when an app tries to claim the /framework/ namespace.
    void warnIfFrameworkNamespace(const std::string &prefix) const;
};

} // namespace wifi_manager
