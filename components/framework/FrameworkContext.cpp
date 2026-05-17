#include "framework/FrameworkContext.hpp"

#include "network_store/NetworkApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "esp_platform/EspClockInterface.hpp"
#include "esp_platform/EspDeviceCert.hpp"
#include "esp_platform/EspHttpServer.hpp"
#include "esp_platform/EspDeviceInterface.hpp"
#include "esp_platform/EspMdnsManager.hpp"
#include "esp_platform/EspNvsStore.hpp"
#include "esp_platform/EspRandomInterface.hpp"
#include "esp_platform/EspTimerInterface.hpp"
#include "http/HttpHandler.hpp"
#include "http_types/HttpTypes.hpp"
#include "logger/Logger.hpp"
#include "ota/OtaApiHandler.hpp"
#include "ota/OtaPuller.hpp"
#include "wifi_manager/EmbeddedServer.hpp"
#include "esp_platform/EspWiFiInterface.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiManager.hpp"

#include <cstdio>

namespace framework {

// Default root URI is "/framework" — declared as an in-class default in FrameworkContext.hpp.
// API endpoints mount at rootUri/api/*, UI assets at rootUri/ui/*.

static logger::Logger log{FrameworkContext::TAG};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Applies a SuffixPolicy to a base string using the supplied MAC bytes.
 *   None      → returns base unchanged
 *   MacShort  → appends "-" + last 3 MAC bytes as 6 lowercase hex chars
 *   MacFull   → appends "-" + all 6 MAC bytes as 12 lowercase hex chars
 */
static std::string applySuffix(const std::string& base,
                                wifi_manager::SuffixPolicy policy,
                                const uint8_t* mac) {
    switch (policy) {
        case wifi_manager::SuffixPolicy::None:
            return base;
        case wifi_manager::SuffixPolicy::MacShort: {
            char buf[8]; // "-" + 6 hex + NUL
            snprintf(buf, sizeof(buf), "%02x%02x%02x", mac[3], mac[4], mac[5]);
            return base + "-" + buf;
        }
        case wifi_manager::SuffixPolicy::MacFull: {
            char buf[14]; // "-" + 12 hex + NUL
            snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return base + "-" + buf;
        }
    }
    return base; // unreachable, but keeps the compiler happy
}

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

FrameworkContext::FrameworkContext() {
    log.debug("constructor (defaults)");
    initialize();
}

FrameworkContext::FrameworkContext(const wifi_manager::ApConfig& apConfig,
                                   auth::AuthConfig authConfig,
                                   std::string rootUri,
                                   std::string mdnsPrefix)
    : apConfig(apConfig)
    , rootUri_(std::move(rootUri))
    , hostnamePrefix_(std::move(mdnsPrefix))
    , authConfig_(std::move(authConfig)) {
    log.debug("constructor");
    initialize();
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void FrameworkContext::initialize() {
    // Create the per-device TLS cert manager
    deviceCert_ = new esp_platform::EspDeviceCert();

    // Create the device implementation first — init() sets up NVS, event loop,
    // netif.  Everything else depends on those being ready.
    deviceInterface_ = new esp_platform::EspDeviceInterface();
    deviceInterface_->init();

    // Create the timer implementation — used by WiFiManager for retry delays.
    timerInterface_  = new esp_platform::EspTimerInterface();
    wifiCtx.timer    = timerInterface_;

    // Create the random and clock implementations — used by auth stores.
    randomInterface_ = new esp_platform::EspRandomInterface();
    clockInterface_  = new esp_platform::EspClockInterface();

    // Create the NVS-backed key-value stores — one per namespace.
    nvsAuth_    = new esp_platform::EspNvsStore("auth");
    nvsNetwork_ = new esp_platform::EspNvsStore("wifi_creds");

    // Initialise the stores with their backing KVS / RNG / clock.
    networkStore.init(*nvsNetwork_);
    sessionStore.init(*randomInterface_, *clockInterface_);

    // Create the mDNS implementation — injected into WiFiManager via context.
    mdnsInterface_        = new esp_platform::EspMdnsManager();
    wifiCtx.mdnsInterface = mdnsInterface_;

    // Read device info once — MAC drives both the mDNS hostname and AuthStore.
    // Stored in mac_[] so start() can apply suffix policies without re-reading.
    const device::DeviceInfo devInfo = deviceInterface_->info();
    sscanf(devInfo.mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &mac_[0], &mac_[1], &mac_[2], &mac_[3], &mac_[4], &mac_[5]);

    // Initialise auth — derives/loads password according to AuthConfig policy
    common::Result authResult = authStore.init(authConfig_, mac_, *nvsAuth_, *randomInterface_);
    if (authResult != common::Result::Ok) {
        log.warn("AuthStore::init failed (%s) — auth will not be enforced",
                 common::toString(authResult));
    }

    // Load any persisted API key (NotFound is normal on first boot)
    common::Result apiKeyResult = apiKeyStore.init(*nvsAuth_, *randomInterface_);
    if (apiKeyResult != common::Result::Ok &&
        apiKeyResult != common::Result::NotFound) {
        log.warn("ApiKeyStore::init failed (%s) — API key unavailable",
                 common::toString(apiKeyResult));
    }

    // Populate WiFi context — hostname and effective SSID are finalised in start()
    // once the app has had a chance to call setHostnameConfig().
    wifiCtx.rootUri      = rootUri_;
    wifiCtx.networkStore = &networkStore;
    wifiCtx.onDriverFatal = [this]() { deviceInterface_->reboot(); };

    // Create state machine first (so it exists before any events fire)
    wifiManager = new wifi_manager::WiFiManager(wifiCtx);
    wifiCtx.wifiManager = wifiManager;

    // Create API handlers — inject device interface where needed
    wifiApi    = new wifi_manager::WiFiApiHandler(wifiCtx);
    networkApi = new network_store::NetworkApiHandler(networkStore);
    deviceApi  = new device::DeviceApiHandler(*deviceInterface_, *timerInterface_);
    otaApi     = new ota::OtaApiHandler(*deviceInterface_);

    // Create the HTTP server implementation, then EmbeddedServer which uses it
    httpServer_ = new esp_platform::EspHttpServer();
    embeddedServer = new wifi_manager::EmbeddedServer(
        wifiCtx, *httpServer_, *wifiApi, *networkApi, *deviceApi, *otaApi);
    // NOTE: embeddedServer->setCert() is called in start(), after deviceCert_->ensure()
    // has been called with the finalised hostname.
    embeddedServer->setAuth(authStore, authConfig_, authApi, sessionStore,
                            apiKeyStore);
    wifiCtx.embeddedServer = embeddedServer;

    // Create WiFiInterface LAST — registers event handlers, may trigger events
    wifiInterface = new esp_platform::EspWiFiInterface(wifiCtx);
    wifiCtx.wifiInterface = wifiInterface;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

FrameworkContext::~FrameworkContext() {
    log.info("destructor");
    stop();
    delete embeddedServer;
    delete httpServer_;
    delete wifiInterface;
    delete wifiManager;
    delete wifiApi;
    delete networkApi;
    delete deviceApi;
    delete otaApi;
    delete mdnsInterface_;
    delete timerInterface_;
    delete clockInterface_;
    delete randomInterface_;
    delete nvsNetwork_;
    delete nvsAuth_;
    delete deviceInterface_;
    delete deviceCert_;
}

// ---------------------------------------------------------------------------
// App injection API — delegates to EmbeddedServer
// ---------------------------------------------------------------------------

device::DeviceInterface& FrameworkContext::getDevice() {
    return *deviceInterface_;
}

void FrameworkContext::setEntryPoint(std::string path) {
    embeddedServer->setEntryPoint(std::move(path));
}

void FrameworkContext::addFileHandler(std::string prefix,
                                       http::HttpHandler* handler) {
    embeddedServer->addAppFileHandler(std::move(prefix), handler);
}

void FrameworkContext::addRoute(http::HttpMethod method, std::string prefix,
                                 http::HttpHandler* handler) {
    embeddedServer->addAppRoute(method, std::move(prefix), handler);
}

void FrameworkContext::setOtaPullConfig(ota::OtaPullConfig config) {
    otaPullConfig_ = std::move(config);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void FrameworkContext::setHostnameConfig(std::string prefix,
                                         wifi_manager::SuffixPolicy suffix) {
    hostnamePrefix_ = std::move(prefix);
    hostnameSuffix_ = suffix;
}

void FrameworkContext::setApSsidConfig(std::string prefix,
                                       wifi_manager::SuffixPolicy suffix) {
    apConfig.ssid       = std::move(prefix);
    apConfig.ssidSuffix = suffix;
}

void FrameworkContext::setApPassword(std::string password) {
    apConfig.password = std::move(password);
    apConfig.auth     = password.empty() ? wifi_manager::WiFiAuthMode::Open
                                         : wifi_manager::WiFiAuthMode::WPA2_PSK;
}

void FrameworkContext::start() {
    log.debug("start");

    // Build the effective hostname from the configured prefix + suffix policy.
    const std::string hostname = applySuffix(hostnamePrefix_, hostnameSuffix_, mac_);
    log.info("Device hostname: %s.local", hostname.c_str());

    // Ensure a per-device TLS cert exists (generates + stores on first boot).
    // Done here (not in initialize()) so the app can call setHostnameConfig()
    // between construction and start().
    common::Result certErr = deviceCert_->ensure(hostname);
    if (certErr != common::Result::Ok) {
        log.warn("DeviceCert::ensure failed — falling back to embedded cert");
    }
    if (deviceCert_->isLoaded()) {
        embeddedServer->setCert(deviceCert_->certPem(), deviceCert_->keyPem());
    }

    // Build the effective AP SSID — apply MAC suffix if configured.
    wifi_manager::ApConfig effectiveApConfig = apConfig;
    effectiveApConfig.ssid = applySuffix(apConfig.ssid, apConfig.ssidSuffix, mac_);
    log.info("AP SSID: %s", effectiveApConfig.ssid.c_str());

    // Populate the remaining WiFi context fields that depend on the above.
    wifiCtx.apConfig     = effectiveApConfig;
    wifiCtx.mdnsHostname = hostname;

    ota::OtaPuller::init(otaPullConfig_);
    ota::OtaPuller::start();
    wifiManager->start();
}

void FrameworkContext::stop() {}

} // namespace framework
