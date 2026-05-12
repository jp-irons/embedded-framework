#pragma once

#include "auth/ApiKeyStore.hpp"
#include "auth/AuthApiHandler.hpp"
#include "auth/AuthConfig.hpp"
#include "auth/AuthStore.hpp"
#include "auth/SessionStore.hpp"
#include "credential_store/CredentialApiHandler.hpp"
#include "credential_store/CredentialStore.hpp"
#include "device/DeviceApiHandler.hpp"
#include "device_cert/DeviceCert.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpTypes.hpp"
#include "ota/OtaApiHandler.hpp"
#include "wifi_manager/WiFiContext.hpp"

namespace credential_store {
class CredentialStore;
}

namespace wifi_manager {
class WiFiApiHandler;
}

namespace framework {

class FrameworkContext {
  public:
    /**
     * Default constructor.  Uses built-in AP config, root URI, and a fixed
     * default password of "esp32admin" with restrictIfDefault() — non-GET API
     * calls are blocked until the operator changes the password via the
     * Security page.
     */
    explicit FrameworkContext();

    /**
     * @param apConfig      AP-mode configuration (SSID, password, etc.)
     * @param authConfig    Authentication policy.  See AuthConfig for options.
     * @param rootUri       Framework root path.  API endpoints are mounted at
     *                      rootUri/api/..., UI assets at rootUri/ui/...
     *                      e.g. "/framework" → "/framework/api/wifi",
     *                                          "/framework/ui/index.html"
     * @param mdnsPrefix    Prefix for the mDNS hostname; last 3 MAC bytes are
     *                      appended automatically, e.g. "esp32" → "esp32-a1b2c3".
     *                      Defaults to "esp32".
     */
    FrameworkContext(const wifi_manager::ApConfig &apConfig,
                     auth::AuthConfig authConfig,
                     std::string rootUri    = "/framework",
                     std::string mdnsPrefix = "esp32");

    ~FrameworkContext();

    const std::string &getRootUri() const { return rootUri_; }

    const wifi_manager::ApConfig &getApConfig() const { return apConfig; }

    // -----------------------------------------------------------------------
    // App injection API
    // Call these from ApplicationContext::start() BEFORE calling fw_.start().
    // -----------------------------------------------------------------------

    /**
     * Set the URL that the root path (/) redirects to.
     * Default: "/framework/ui/"
     * Typical override: fw_.setEntryPoint("/app/ui/");
     */
    void setEntryPoint(std::string path);

    /**
     * Register an app static-file handler for the given URL prefix.
     * The handler is tried after all framework API routes but before the
     * framework's own file handler.
     *
     * Example:
     *   fw_.addFileHandler("/app/ui/", &myAppFileHandler_);
     */
    void addFileHandler(std::string prefix, http::HttpHandler *handler);

    /**
     * Register an app API route for the given method + URL prefix.
     * The handler is tried after all framework API routes.
     *
     * Example:
     *   fw_.addRoute(http::HttpMethod::Get, "/app/api/status", &statusHandler_);
     */
    void addRoute(http::HttpMethod method, std::string prefix, http::HttpHandler *handler);


    void start();
    void stop();

  private:
    wifi_manager::ApConfig apConfig = {
        .ssid = "ESP32 FW Test", .password = "password", .channel = 1, .maxConnections = 4};
    std::string      rootUri_    = "/framework";
    std::string      mdnsPrefix_ = "esp32";
    auth::AuthConfig authConfig_ = auth::AuthConfig::withPassword("esp32admin")
                                                    .restrictIfDefault();

    // Per-device TLS cert (generated on first boot, persisted in NVS)
    device_cert::DeviceCert deviceCert_;

    // Always-present value types
    // NOTE: SessionStore and ApiKeyStore must be declared before authApi so
    // they are initialised first (authApi constructor takes references to them).
    wifi_manager::WiFiContext         wifiCtx;
    credential_store::CredentialStore credentialStore;
    auth::AuthStore                   authStore;
    auth::SessionStore                sessionStore;
    auth::ApiKeyStore                 apiKeyStore;
    auth::AuthApiHandler              authApi{authStore, sessionStore, apiKeyStore};

    // Owned heap objects
    wifi_manager::EmbeddedServer             *embeddedServer = nullptr;
    wifi_manager::WiFiInterface              *wifiInterface  = nullptr;
    wifi_manager::WiFiManager                *wifiManager    = nullptr;
    wifi_manager::WiFiApiHandler             *wifiApi        = nullptr;
    credential_store::CredentialApiHandler   *credentialApi  = nullptr;
    device::DeviceApiHandler                 *deviceApi      = nullptr;
    ota::OtaApiHandler                       *otaApi         = nullptr;

    void initialize();
};

} // namespace framework
