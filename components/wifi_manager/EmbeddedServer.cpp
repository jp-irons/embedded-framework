#include "wifi_manager/EmbeddedServer.hpp"

#include "auth/AuthConfig.hpp"
#include "auth/AuthStore.hpp"
#include "common/Result.hpp"
#include "network_store/NetworkApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "logger/Logger.hpp"
#include "ota/OtaApiHandler.hpp"
#include "ota/OtaManager.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiInterface.hpp"
#include "wifi_manager/WiFiTypes.hpp"

namespace wifi_manager {

static logger::Logger log{"EmbeddedServer"};

EmbeddedServer::EmbeddedServer(WiFiContext &ctx,
                               http::HttpServer &server,
                               WiFiApiHandler &wifiApi,
                               network_store::NetworkApiHandler &networkApi,
                               device::DeviceApiHandler &deviceApi,
                               ota::OtaApiHandler &otaApi)
    : ctx(ctx)
    , apiUri_(ctx.rootUri + "/api")
    , server_(server)
    , frameworkFileTable_()
    , frameworkFileHandler_(ctx.rootUri + "/ui", "index.html", frameworkFileTable_)
    , wifiHandler(wifiApi)
    , networkHandler(networkApi)
    , deviceHandler(deviceApi)
    , otaHandler(otaApi) {
    log.debug("constructor");
}

EmbeddedServer::~EmbeddedServer() {
    log.info("destructor");
    stop();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool EmbeddedServer::start() {
    log.debug("Starting EmbeddedServer");
    server_.start();

    if (!routesRegistered_) {
        log.debug("start() registering framework routes");
        frameworkRoutes_ = {
            {apiUri_ + "/networks/",     &networkHandler},
            {apiUri_ + "/device/",      &deviceHandler},
            {apiUri_ + "/firmware/",    &otaHandler},
            {apiUri_ + "/wifi/",        &wifiHandler},
        };
        if (authApiHandler_) {
            frameworkRoutes_.insert(frameworkRoutes_.begin(),
                Route{apiUri_ + "/auth/", authApiHandler_});
        }
        server_.addRoutes("/*", this);
        routesRegistered_ = true;
    }

    log.debug("EmbeddedServer up — entry point: %s", entryPoint_.c_str());
    IpAddress ip = ctx.wifiInterface->getStaIp();
    if (ip.valid) {
        log.info("EmbeddedServer started on https://%s", ip.value.c_str());
    } else {
        log.warn("EmbeddedServer started but STA IP unknown");
    }

    return true;
}

void EmbeddedServer::setCert(std::string certPem, std::string keyPem) {
    server_.setCert(std::move(certPem), std::move(keyPem));
}

void EmbeddedServer::stop() {
    log.debug("Stopping EmbeddedServer");
    server_.stop();
}

void EmbeddedServer::startRuntimeMode() {
    log.debug("startRuntimeMode (stub)");
}

void EmbeddedServer::startProvisioningMode() {
    log.debug("startProvisioningMode (stub)");
}

// ---------------------------------------------------------------------------
// App injection API
// ---------------------------------------------------------------------------

void EmbeddedServer::setEntryPoint(std::string path) {
    log.info("setEntryPoint '%s'", path.c_str());
    entryPoint_ = std::move(path);
}

void EmbeddedServer::addAppRoute(http::HttpMethod method, std::string prefix,
                                  http::HttpHandler *handler) {
    warnIfFrameworkNamespace(prefix);
    log.info("addAppRoute [%s] '%s'", http::toString(method).c_str(), prefix.c_str());
    appRoutes_.push_back({method, std::move(prefix), handler});
}

void EmbeddedServer::addAppFileHandler(std::string prefix, http::HttpHandler *handler) {
    warnIfFrameworkNamespace(prefix);
    log.info("addAppFileHandler '%s'", prefix.c_str());
    appFileHandlers_.push_back({std::move(prefix), handler});
}


void EmbeddedServer::warnIfFrameworkNamespace(const std::string &prefix) const {
    if (prefix.rfind("/framework/", 0) == 0) {
        log.warn("App attempting to register under reserved /framework/ namespace: '%s'",
                 prefix.c_str());
    }
}

// ---------------------------------------------------------------------------
// Auth middleware
// ---------------------------------------------------------------------------

void EmbeddedServer::setAuth(auth::AuthStore        &store,
                              const auth::AuthConfig  &config,
                              auth::AuthApiHandler    &authHandler,
                              auth::SessionStore      &sessionStore,
                              auth::ApiKeyStore       &apiKeyStore) {
    authStore_      = &store;
    authConfig_     = &config;
    authApiHandler_ = &authHandler;
    sessionStore_   = &sessionStore;
    apiKeyStore_    = &apiKeyStore;
    log.debug("auth configured");
}

common::Result EmbeddedServer::checkAuth(http::HttpRequest &req,
                                          http::HttpResponse &res,
                                          const std::string &path) {
    // Auth is optional until setAuth() is called — skip if not configured
    if (!authStore_ || !authConfig_ || !sessionStore_ || !apiKeyStore_) {
        return common::Result::Ok;
    }

    // Only framework API paths require authentication by default — static files
    // and app routes are served freely so the browser can load the UI and the
    // app can implement its own auth on top.
    if (path.rfind(apiUri_, 0) != 0) {
        return common::Result::Ok;
    }

    // ── Login is exempt — it handles its own Basic-Auth verification ──────
    // (All other /auth/* endpoints require a valid Bearer token.)
    const std::string loginPath = apiUri_ + AUTH_LOGIN_SUFFIX;
    if (path == loginPath) {
        return common::Result::Ok;
    }

    // ── Verify Bearer token ───────────────────────────────────────────────
    auto bearerToken = req.extractBearerToken();
    if (!bearerToken) {
        log.warn("Auth failed for '%s' — no Bearer token", path.c_str());
        res.sendUnauthorized("ESP32");
        return common::Result::Forbidden;
    }

    // Accept a valid session token (browser) or a valid API key (M2M)
    const bool tokenOk = sessionStore_->validate(*bearerToken) ||
                         apiKeyStore_->validate(*bearerToken);
    if (!tokenOk) {
        log.warn("Auth failed for '%s' — invalid or expired token", path.c_str());
        res.sendUnauthorized("ESP32");
        return common::Result::Forbidden;
    }

    // ── requireChangeOnFirstBoot policy ──────────────────────────────────
    if (authConfig_->isRequireChangeOnFirstBoot() && !authStore_->isPasswordChanged()) {
        const std::string changePasswordPath = apiUri_ + AUTH_PASSWORD_SUFFIX;
        if (path != changePasswordPath) {
            log.warn("requireChangeOnFirstBoot: blocking '%s'", path.c_str());
            res.sendJsonError(403, "Password change required before access is granted");
            return common::Result::Forbidden;
        }
    }

    // ── restrictIfDefault policy ──────────────────────────────────────────
    if (authConfig_->isRestrictIfDefault() && !authStore_->isPasswordChanged()) {
        if (req.method() != http::HttpMethod::Get) {
            const std::string changePasswordPath = apiUri_ + AUTH_PASSWORD_SUFFIX;
            if (path != changePasswordPath) {
                log.warn("restrictIfDefault: blocking mutating request to '%s'", path.c_str());
                res.sendJsonError(403, "Password must be changed before write access is granted");
                return common::Result::Forbidden;
            }
        }
    }

    return common::Result::Ok;
}

// ---------------------------------------------------------------------------
// Request dispatch
// ---------------------------------------------------------------------------

common::Result EmbeddedServer::handle(http::HttpRequest &req, http::HttpResponse &res) {
    const std::string path = req.path();
    log.debug("handle '%s'", path.c_str());

    // ── 1. Root redirect ───────────────────────────────────────────────────
    if (path.empty() || path == "/" || path == "/index.html") {
        log.debug("root redirect → %s", entryPoint_.c_str());
        return res.redirect(entryPoint_.c_str());
    }

    // ── 2. Auth check (framework API paths only) ───────────────────────────
    common::Result authResult = checkAuth(req, res, path);
    if (authResult != common::Result::Ok) {
        return authResult;
    }

    // ── 3. Framework API routes ────────────────────────────────────────────
    for (auto &r : frameworkRoutes_) {
        if (path.rfind(r.prefix, 0) == 0) {
            log.debug("framework route matched '%s'", r.prefix.c_str());
            common::Result result = r.handler->handle(req, res);
            if (result != common::Result::NotFound) {
                return result;
            }
        }
    }

    // ── 4. App routes (method + prefix) ───────────────────────────────────
    for (auto &r : appRoutes_) {
        if (path.rfind(r.prefix, 0) == 0 && req.method() == r.method) {
            log.debug("app route matched [%s] '%s'",
                      http::toString(r.method).c_str(), r.prefix.c_str());
            common::Result result = r.handler->handle(req, res);
            if (result != common::Result::NotFound) {
                return result;
            }
        }
    }

    // ── 5. App file handlers (prefix) ─────────────────────────────────────
    for (auto &r : appFileHandlers_) {
        if (path.rfind(r.prefix, 0) == 0) {
            log.debug("app file handler matched '%s'", r.prefix.c_str());
            common::Result result = r.handler->handle(req, res);
            if (result != common::Result::NotFound) {
                return result;
            }
        }
    }

    // ── 6. Framework file handler fallback (framework UI assets) ──────────────
    log.debug("falling back to framework file handler");
    common::Result result = frameworkFileHandler_.handle(req, res);
    if (result == common::Result::NotFound) {
        log.warn("no handler found for '%s'", path.c_str());
        result = res.sendJsonError(404, "Not found: " + path);
    }

    // First completed HTTPS request — any status — proves the full stack is
    // healthy.  Mark the running OTA image valid to cancel rollback.
    if (!otaMarkedValid_) {
        otaMarkedValid_ = true;
        ota::OtaManager::markValid();
    }

    return result;
}

} // namespace wifi_manager
