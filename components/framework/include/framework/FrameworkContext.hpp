#pragma once

#include "auth/ApiKeyStore.hpp"
#include "auth/AuthApiHandler.hpp"
#include "auth/AuthConfig.hpp"
#include "auth/AuthStore.hpp"
#include "auth/SessionStore.hpp"
#include "network_store/NetworkApiHandler.hpp"
#include "network_store/NetworkStore.hpp"
#include "common/KeyValueStore.hpp"
#include "device/ClockInterface.hpp"
#include "device/DeviceApiHandler.hpp"
#include "device/DeviceInterface.hpp"
#include "device/RandomInterface.hpp"
#include "device/TimerInterface.hpp"
#include "device_cert/DeviceCertInterface.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpServer.hpp"
#include "http_types/HttpTypes.hpp"
#include "ota/OtaApiHandler.hpp"
#include "ota/OtaPuller.hpp"
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
    static constexpr const char* TAG = "FrameworkContext";

    /**
     * Default constructor.  Uses built-in AP config, root URI, and a fixed
     * default password of "esp32admin" with restrictIfDefault() — non-GET API
     * calls are blocked until the operator changes the password via the
     * Security page.
     */
    explicit FrameworkContext();

    /**
     * @param apConfig      AP-mode configuration (SSID, password, etc.).
     *                      Set apConfig.ssidSuffix to control whether MAC bytes
     *                      are appended to the SSID (default: SuffixPolicy::None).
     * @param authConfig    Authentication policy.  See AuthConfig for options.
     * @param rootUri       Framework root path.  API endpoints are mounted at
     *                      rootUri/api/..., UI assets at rootUri/ui/...
     *                      e.g. "/framework" → "/framework/api/wifi",
     *                                          "/framework/ui/index.html"
     * @param mdnsPrefix    Prefix for the mDNS hostname.  The suffix applied at
     *                      start() is controlled by setHostnameConfig(); the
     *                      default is SuffixPolicy::MacShort, e.g. "esp32-a1b2c3".
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
     * Configures the mDNS hostname for this device.
     *
     * @param prefix  Base name, e.g. "van-monitor".
     * @param suffix  SuffixPolicy controlling MAC byte appending:
     *                  None      — use prefix as-is
     *                  MacShort  — append last 3 MAC bytes, e.g. "van-monitor-a1b2c3"
     *                  MacFull   — append all 6 MAC bytes
     *                Default: MacShort (matches legacy behaviour).
     *
     * Must be called before fw_.start().
     */
    void setHostnameConfig(std::string prefix,
                           wifi_manager::SuffixPolicy suffix = wifi_manager::SuffixPolicy::MacShort);

    /**
     * Configures the Wi-Fi AP SSID for this device.
     *
     * @param prefix  Base SSID, e.g. "VanMonitor".
     * @param suffix  SuffixPolicy controlling MAC byte appending:
     *                  None      — use prefix as-is (default for ApConfig.ssidSuffix)
     *                  MacShort  — append last 3 MAC bytes, e.g. "VanMonitor-a1b2c3"
     *                  MacFull   — append all 6 MAC bytes
     *                Default: MacShort.
     *
     * Only changes the SSID and suffix policy; all other ApConfig fields
     * (password, channel, etc.) are unchanged.
     * Must be called before fw_.start().
     */
    void setApSsidConfig(std::string prefix,
                         wifi_manager::SuffixPolicy suffix = wifi_manager::SuffixPolicy::MacShort);

    /**
     * Sets the Wi-Fi AP password.  Pass an empty string for an open (no-password) AP.
     * Must be called before fw_.start().
     */
    void setApPassword(std::string password);

    /**
     * Register an app static-file handler for the given URL prefix.
     */
    void addFileHandler(std::string prefix, http::HttpHandler* handler);

    /**
     * Register an app API route for the given method + URL prefix.
     */
    void addRoute(http::HttpMethod method, std::string prefix,
                  http::HttpHandler* handler);

    /**
     * Configure pull-based OTA updates.  The framework calls OtaPuller::init()
     * and OtaPuller::start() during start().  Call this before fw_.start().
     *
     * @param config  baseUrl — GitHub Releases download directory, e.g.
     *                  "https://github.com/user/repo/releases/latest/download"
     *                checkIntervalS — seconds between background checks (0 = disabled).
     */
    void setOtaPullConfig(ota::OtaPullConfig config);

    void start();
    void stop();

  private:
    wifi_manager::ApConfig apConfig = {
        .ssid = "ESP32 FW Test", .password = "password",
        .channel = 1, .maxConnections = 4};
    std::string               rootUri_         = "/framework";
    std::string               hostnamePrefix_  = "esp32";
    wifi_manager::SuffixPolicy hostnameSuffix_ = wifi_manager::SuffixPolicy::MacShort;
    auth::AuthConfig authConfig_ = auth::AuthConfig::withPassword("esp32admin")
                                                    .restrictIfDefault();

    // MAC address read once in initialize() — used in start() to build hostname
    // and effective AP SSID.
    uint8_t mac_[6] = {};

    // Per-device TLS cert (generated on first boot, persisted in NVS)
    device_cert::DeviceCertInterface* deviceCert_ = nullptr;

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
    // (EspDeviceInterface, EspWiFiInterface, EspTimerInterface, EspMdnsManager,
    //  EspNvsStore, EspRandomInterface, EspClockInterface)
    // are created in initialize() and only referenced by name in FrameworkContext.cpp.
    device::DeviceInterface*              deviceInterface_ = nullptr;
    device::TimerInterface*               timerInterface_  = nullptr;
    device::RandomInterface*              randomInterface_ = nullptr;
    device::ClockInterface*               clockInterface_  = nullptr;
    wifi_manager::MdnsInterface*          mdnsInterface_   = nullptr;
    wifi_manager::WiFiInterface*          wifiInterface    = nullptr;
    http::HttpServer*                     httpServer_      = nullptr;
    wifi_manager::EmbeddedServer*         embeddedServer   = nullptr;
    wifi_manager::WiFiManager*            wifiManager      = nullptr;
    wifi_manager::WiFiApiHandler*         wifiApi          = nullptr;
    network_store::NetworkApiHandler*     networkApi       = nullptr;
    device::DeviceApiHandler*             deviceApi        = nullptr;
    ota::OtaApiHandler*                   otaApi           = nullptr;
    ota::OtaPullConfig                    otaPullConfig_;
    common::KeyValueStore*                nvsAuth_         = nullptr;
    common::KeyValueStore*                nvsNetwork_      = nullptr;

    void initialize();
};

} // namespace framework
