#pragma once

#include "auth/ApiKeyStore.hpp"
#include "auth/AuthApiHandler.hpp"
#include "auth/AuthConfig.hpp"
#include "auth/AuthStore.hpp"
#include "auth/SessionStore.hpp"
#include "network_store/NetworkApiHandler.hpp"
#include "network_store/NetworkStore.hpp"
#include "device/DeviceApiHandler.hpp"
#include "device/DeviceInterface.hpp"
#include "device/TimerInterface.hpp"
#include "device_cert/DeviceCert.hpp"
#include "http/HttpHandler.hpp"
#include "http_types/HttpTypes.hpp"
#include "ota/OtaApiHandler.hpp"
#include "wifi_manager/WiFiContext.hpp"

namespace network_store {
class NetworkStore;
}

namespace wifi_manager {
class MdnsInterface;
class WiFiApiHandler;
class WiFiInterface;
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
    FrameworkContext(const wifi_manager::ApConfig& apConfig,
                     auth::AuthConfig authConfig,
                     std::string rootUri    = "/framework",
                     std::string mdnsPrefix = "esp32");

    ~FrameworkContext();

    const std::string& getRootUri() const { return rootUri_; }

    const wifi_manager::ApConfig& getApConfig() const { return apConfig; }

    /**
     * Returns the device interface for injection into app-tier handlers
     * (e.g. TemperatureHandler).  The reference is valid for the lifetime of
     * this FrameworkContext.
     */
    device::DeviceInterface& getDevice();

    // -----------------------------------------------------------------------
    // App injection API
    // Call these from ApplicationContext::start() BEFORE calling fw_.start().
    // -----------------------------------------------------------------------

    /**
     * Set the URL that the root path (/) redirects to.
     * Default: "/framework/ui/"
     */
    void setEntryPoint(std::string path);

    /**
     * Register an app static-file handler for the given URL prefix.
     */
    void addFileHandler(std::string prefix, http::HttpHandler* handler);

    /**
     * Register an app API route for the given method + URL prefix.
     */
    void addRoute(http::HttpMethod method, std::string prefix,
                  http::HttpHandler* handler);

    void start();
    void stop();

  private:
    wifi_manager::ApConfig apConfig = {
        .ssid = "ESP32 FW Test", .password = "password",
        .channel = 1, .maxConnections = 4};
    std::string      rootUri_    = "/framework";
    std::string      mdnsPrefix_ = "esp32";
    auth::AuthConfig authConfig_ = auth::AuthConfig::withPassword("esp32admin")
                                                    .restrictIfDefault();

    // Per-device TLS cert (generated on first boot, persisted in NVS)
    device_cert::DeviceCert deviceCert_;

    // Always-present value types
    // NOTE: SessionStore and ApiKeyStore must be declared before authApi so
    // they are initialised first (authApi constructor takes references to them).
    wifi_manager::WiFiContext      wifiCtx;
    network_store::NetworkStore    networkStore;
    auth::AuthStore                authStore;
    auth::SessionStore             sessionStore;
    auth::ApiKeyStore              apiKeyStore;
    auth::AuthApiHandler           authApi{authStore, sessionStore, apiKeyStore};

    // Owned heap objects — abstract pointers; concrete types
    // (EspDeviceInterface, EspWiFiInterface, EspTimerInterface, EspMdnsManager)
    // are created in initialize() and only referenced by name in FrameworkContext.cpp.
    device::DeviceInterface*              deviceInterface_ = nullptr;
    device::TimerInterface*               timerInterface_  = nullptr;
    wifi_manager::MdnsInterface*          mdnsInterface_   = nullptr;
    wifi_manager::WiFiInterface*          wifiInterface    = nullptr;
    wifi_manager::EmbeddedServer*         embeddedServer   = nullptr;
    wifi_manager::WiFiManager*            wifiManager      = nullptr;
    wifi_manager::WiFiApiHandler*         wifiApi          = nullptr;
    network_store::NetworkApiHandler*     networkApi       = nullptr;
    device::DeviceApiHandler*             deviceApi        = nullptr;
    ota::OtaApiHandler*                   otaApi           = nullptr;

    void initialize();
};

} // namespace framework
