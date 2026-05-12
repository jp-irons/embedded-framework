#include "framework/FrameworkContext.hpp"

#include "network_store/NetworkApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "device/EspDeviceInterface.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpTypes.hpp"
#include "logger/Logger.hpp"
#include "ota/OtaApiHandler.hpp"
#include "wifi_manager/EmbeddedServer.hpp"
#include "wifi_manager/EspWiFiInterface.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiManager.hpp"
#include "esp_mac.h"

#include <cstdio>

namespace framework {

// Default root URI is "/framework" — declared as an in-class default in FrameworkContext.hpp.
// API endpoints mount at rootUri/api/*, UI assets at rootUri/ui/*.

static const char* TAG = "FrameworkContext";

static logger::Logger log{TAG};

// ---------------------------------------------------------------------------
// Build a unique mDNS hostname from a prefix + last 3 bytes of WiFi STA MAC.
// e.g. prefix="esp32"  →  "esp32-a1b2c3"
// ---------------------------------------------------------------------------
static std::string macBasedHostname(const std::string& prefix) {
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char suffix[8];
    snprintf(suffix, sizeof(suffix), "%02x%02x%02x", mac[3], mac[4], mac[5]);
    return prefix + "-" + suffix;
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
    , mdnsPrefix_(std::move(mdnsPrefix))
    , authConfig_(std::move(authConfig)) {
    log.debug("constructor");
    initialize();
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void FrameworkContext::initialize() {
    // Create the device implementation first — init() sets up NVS, event loop,
    // netif.  Everything else depends on those being ready.
    deviceInterface_ = new device::EspDeviceInterface();
    deviceInterface_->init();

    // Read MAC once — used for hostname and AuthStore derivation
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Build the per-device hostname from MAC address
    const std::string hostname = macBasedHostname(mdnsPrefix_);
    log.info("Device hostname: %s.local", hostname.c_str());

    // Ensure a per-device TLS cert exists (generates + stores on first boot)
    esp_err_t certErr = deviceCert_.ensure(hostname);
    if (certErr != ESP_OK) {
        log.warn("DeviceCert::ensure failed (%s) — falling back to embedded cert",
                 esp_err_to_name(certErr));
    }

    // Initialise auth — derives/loads password according to AuthConfig policy
    common::Result authResult = authStore.init(authConfig_, mac);
    if (authResult != common::Result::Ok) {
        log.warn("AuthStore::init failed (%s) — auth will not be enforced",
                 common::toString(authResult));
    }

    // Load any persisted API key from NVS (NotFound is normal on first boot)
    common::Result apiKeyResult = apiKeyStore.init();
    if (apiKeyResult != common::Result::Ok &&
        apiKeyResult != common::Result::NotFound) {
        log.warn("ApiKeyStore::init failed (%s) — API key unavailable",
                 common::toString(apiKeyResult));
    }

    // Populate WiFi context
    log.debug("AP SSID %s", apConfig.ssid.c_str());
    wifiCtx.apConfig     = apConfig;
    wifiCtx.rootUri      = rootUri_;
    wifiCtx.mdnsHostname = hostname;
    wifiCtx.networkStore = &networkStore;
    wifiCtx.onDriverFatal = [this]() { deviceInterface_->reboot(); };

    // Create state machine first (so it exists before any events fire)
    wifiManager = new wifi_manager::WiFiManager(wifiCtx);
    wifiCtx.wifiManager = wifiManager;

    // Create API handlers — inject device interface where needed
    wifiApi    = new wifi_manager::WiFiApiHandler(wifiCtx);
    networkApi = new network_store::NetworkApiHandler(networkStore);
    deviceApi  = new device::DeviceApiHandler(*deviceInterface_);
    otaApi     = new ota::OtaApiHandler(*deviceInterface_);

    // Create server, inject the per-device cert, and wire in auth
    embeddedServer = new wifi_manager::EmbeddedServer(
        wifiCtx, *wifiApi, *networkApi, *deviceApi, *otaApi);
    if (deviceCert_.isLoaded()) {
        embeddedServer->setCert(deviceCert_.certPem(), deviceCert_.keyPem());
    }
    embeddedServer->setAuth(authStore, authConfig_, authApi, sessionStore,
                            apiKeyStore);
    wifiCtx.embeddedServer = embeddedServer;

    // Create WiFiInterface LAST — registers event handlers, may trigger events
    wifiInterface = new wifi_manager::EspWiFiInterface(wifiCtx);
    wifiCtx.wifiInterface = wifiInterface;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

FrameworkContext::~FrameworkContext() {
    log.info("destructor");
    stop();
    delete embeddedServer;
    delete wifiInterface;
    delete wifiManager;
    delete wifiApi;
    delete networkApi;
    delete deviceApi;
    delete otaApi;
    delete deviceInterface_;
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

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void FrameworkContext::start() {
    log.debug("start");
    wifiManager->start();
}

void FrameworkContext::stop() {}

} // namespace framework
