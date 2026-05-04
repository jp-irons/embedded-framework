#pragma once

#include "auth/AuthApiHandler.hpp"
#include "auth/AuthConfig.hpp"
#include "auth/AuthStore.hpp"
#include "common/Result.hpp"
#include "credential_store/CredentialApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "embedded_files/EmbeddedFileHandler.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpServer.hpp"
#include "ota/OtaApiHandler.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"

namespace wifi_manager {

struct WiFiContext;

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

    /** Pass a per-device cert through to HttpServer. Call before start(). */
    void setCert(std::string certPem, std::string keyPem);

    /**
     * Wire in authentication.  Call once from FrameworkContext before start().
     * Until this is called, all requests are passed through without auth checks.
     */
    void setAuth(auth::AuthStore &store,
                 const auth::AuthConfig &config,
                 auth::AuthApiHandler &authHandler);

    void startProvisioningMode();
    void startRuntimeMode();

    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    // Path suffix for the change-password endpoint.
    // Requests to this path are allowed through even when requireChangeOnFirstBoot
    // is blocking all other API access.
    static constexpr const char *AUTH_PASSWORD_SUFFIX = "/auth/password";

    struct Route {
        std::string prefix;
        http::HttpHandler *handler;
    };

    WiFiContext &ctx;

    http::HttpServer server;
    embedded_files::EmbeddedFileHandler embeddedFileHandler;
    wifi_manager::WiFiApiHandler wifiHandler;
    credential_store::CredentialApiHandler credentialHandler;
    device::DeviceApiHandler deviceHandler;
    ota::OtaApiHandler otaHandler;

    std::vector<Route> routes;
    bool routesRegistered = false;

    // Auth — null until setAuth() is called
    auth::AuthStore        *authStore_      = nullptr;
    const auth::AuthConfig *authConfig_     = nullptr;
    auth::AuthApiHandler   *authApiHandler_ = nullptr;

    // Returns Forbidden if the auth policy blocks the current request,
    // Unauthorized if credentials are missing/wrong, or Ok to proceed.
    common::Result checkAuth(http::HttpRequest &req, http::HttpResponse &res,
                             const std::string &path);
};

} // namespace wifi_manager