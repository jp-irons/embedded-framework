#include "wifi_manager/EmbeddedServer.hpp"

#include "auth/AuthConfig.hpp"
#include "auth/AuthStore.hpp"
#include "common/Result.hpp"
#include "credential_store/CredentialApiHandler.hpp"
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
                               WiFiApiHandler &wifiApi,
                               credential_store::CredentialApiHandler &credentialApi,
                               device::DeviceApiHandler &deviceHandler,
                               ota::OtaApiHandler &otaApi)
    : ctx(ctx)
    , server()
    , embeddedFileHandler("/", "index.html")
    , wifiHandler(wifiApi)
    , credentialHandler(credentialApi)
    , deviceHandler(deviceHandler)
    , otaHandler(otaApi) {
    log.debug("constructor");
}

EmbeddedServer::~EmbeddedServer() {
    log.info("destructor");
    stop();
}

bool EmbeddedServer::start() {
    log.debug("Starting EmbeddedServer");
    server.start();

    if (!routesRegistered) {
        log.debug("start() registering routes");
        routes = {
            {ctx.rootUri + "/credentials/", &credentialHandler},
            {ctx.rootUri + "/device/",       &deviceHandler},
            {ctx.rootUri + "/firmware/",     &otaHandler},
            {ctx.rootUri + "/wifi/",         &wifiHandler},
            {"/",                            &embeddedFileHandler},
        };
        if (authApiHandler_) {
            routes.insert(routes.begin(),
                Route{ctx.rootUri + "/auth/", authApiHandler_});
        }
        server.addRoutes("/*", this);
        routesRegistered = true;
    }

	log.debug("EmbeddedServer up");
	IpAddress ip = ctx.wifiInterface->getStaIp();
	if (ip.valid) {
	    log.info("EmbeddedServer started on http://%s", ip.value.c_str());
	} else {
	    log.warn("EmbeddedServer started but STA IP unknown");
	}

    // HTTP server is listening → system is healthy enough to validate the
    // running OTA image and cancel the automatic rollback timer.
    ota::OtaManager::markValid();

    return true;
}

void EmbeddedServer::setCert(std::string certPem, std::string keyPem) {
    server.setCert(std::move(certPem), std::move(keyPem));
}

void EmbeddedServer::stop() {
    log.debug("Stopping EmbeddedServer");
    server.stop();
}

void EmbeddedServer::startRuntimeMode() {
    log.debug("startRuntimeMode (stub)");
}

void EmbeddedServer::startProvisioningMode() {
    log.debug("startProvisioningMode (stub)");
}

void EmbeddedServer::setAuth(auth::AuthStore &store,
                              const auth::AuthConfig &config,
                              auth::AuthApiHandler &authHandler) {
    authStore_      = &store;
    authConfig_     = &config;
    authApiHandler_ = &authHandler;
    log.debug("auth configured");
}

// ---------------------------------------------------------------------------
// Auth middleware
// ---------------------------------------------------------------------------

common::Result EmbeddedServer::checkAuth(http::HttpRequest &req,
                                          http::HttpResponse &res,
                                          const std::string &path) {
    // Auth is optional until setAuth() is called — skip if not configured
    if (!authStore_ || !authConfig_) {
        return common::Result::Ok;
    }

    // Only API paths require authentication — static files are served freely
    // so the browser can load the UI and display its built-in login dialog
    if (path.rfind(ctx.rootUri, 0) != 0) {
        return common::Result::Ok;
    }

    // ── Verify credentials ────────────────────────────────────────────────
    auto basicAuth = req.extractBasicAuth();
    if (!basicAuth) {
        log.warn("Auth failed for '%s' — no Authorization header", path.c_str());
        res.sendUnauthorized("ESP32");
        return common::Result::Forbidden;
    }
    if (!authStore_->verify(basicAuth->password)) {
        log.warn("Auth failed for '%s' — wrong password (len=%u, stored len=%u)",
                 path.c_str(),
                 static_cast<unsigned>(basicAuth->password.size()),
                 static_cast<unsigned>(authStore_->getPasswordLen()));
        res.sendUnauthorized("ESP32");
        return common::Result::Forbidden;
    }

    // ── requireChangeOnFirstBoot policy ──────────────────────────────────
    // Block all API endpoints except the change-password path itself,
    // so the operator has a route to lift the restriction.
    if (authConfig_->isRequireChangeOnFirstBoot() && !authStore_->isPasswordChanged()) {
        const std::string changePasswordPath = ctx.rootUri + AUTH_PASSWORD_SUFFIX;
        if (path != changePasswordPath) {
            log.warn("requireChangeOnFirstBoot: blocking '%s'", path.c_str());
            res.sendJsonError(403, "Password change required before access is granted");
            return common::Result::Forbidden;
        }
    }

    // ── restrictIfDefault policy ──────────────────────────────────────────
    // Allow read-only (GET) requests through; block mutating methods until
    // the operator has changed the password from its default.
    if (authConfig_->isRestrictIfDefault() && !authStore_->isPasswordChanged()) {
        if (req.method() != http::HttpMethod::Get) {
            const std::string changePasswordPath = ctx.rootUri + AUTH_PASSWORD_SUFFIX;
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
    log.debug("handle");
    const std::string path = req.path();
    log.debug("path '%s'", path.c_str());

    if (path.empty() || path == "/" || path == "/index.html") {
        log.debug("resolving path");
        return res.redirect("/runtime/index.html");
    }

    // Auth check — returns Forbidden (already responded) or Ok to proceed
    common::Result authResult = checkAuth(req, res, path);
    if (authResult != common::Result::Ok) {
        return authResult;
    }

    for (auto &r : routes) {
        if (path.rfind(r.prefix, 0) == 0) {
            log.debug("matched '%s' to '%s'", r.prefix.c_str(), path.c_str());
            common::Result result = r.handler->handle(req, res);
            if (result != common::Result::NotFound) {
                return result;
            }
        }
    }

    return common::Result::NotFound;
}

} // namespace wifi_manager