#pragma once

#include "framework_files/EmbeddedFileHandler.hpp"
#include "framework_files/EmbeddedFileTable.hpp"
#include "auth/ApiKeyStore.hpp"
#include "auth/AuthApiHandler.hpp"
#include "auth/AuthConfig.hpp"
#include "auth/AuthStore.hpp"
#include "auth/SessionStore.hpp"
#include "common/Result.hpp"
#include "credential_store/CredentialApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpServer.hpp"
#include "http/HttpTypes.hpp"
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
 *   6. Favicon        — app table first (if set via setFaviconTable), then framework fallback
 *   7. Framework files — /framework/ui/... (fallback)
 */
class EmbeddedServer : public http::HttpHandler {
  public:
    explicit EmbeddedServer(WiFiContext &ctx,
                            WiFiApiHandler &wifiApi,
                            credential_store::CredentialApiHandler &credentialApi,
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

    /**
     * Register the app's file table as the primary source for /favicon.ico.
     * When set, the server checks this table before falling back to the
     * framework's built-in generic icon.  If the app's table has no
     * "/favicon.ico" entry the framework default is served instead.
     *
     * Must be called before start().
     */
    void setFaviconTable(framework_files::EmbeddedFileTable *table);

    void startProvisioningMode();
    void startRuntimeMode();

    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    // Path suffixes for auth endpoints with special treatment.
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

    http::HttpServer server;

    // Framework file table + handler (fallback for /framework/ui/*).
    // frameworkFileTable_ MUST be declared before frameworkFileHandler_ so it is
    // initialised first.
    framework_files::FrameworkFileTable  frameworkFileTable_;
    framework_files::EmbeddedFileHandler frameworkFileHandler_;

    // App-supplied favicon table — checked before the framework's built-in icon.
    // Null until setFaviconTable() is called.
    framework_files::EmbeddedFileTable  *appFaviconTable_ = nullptr;

    // Framework API handlers (value members — constructed from ctor args)
    wifi_manager::WiFiApiHandler                  wifiHandler;
    credential_store::CredentialApiHandler        credentialHandler;
    device::DeviceApiHandler                      deviceHandler;
    ota::OtaApiHandler                            otaHandler;

    // Framework route table (built once in start())
    std::vector<Route>          frameworkRoutes_;
    bool                        routesRegistered_ = false;

    // App-supplied routes and file handlers
    std::vector<AppRoute>       appRoutes_;
    std::vector<AppFileHandler> appFileHandlers_;

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
