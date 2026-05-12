#include "framework/FrameworkContext.hpp"

#include "network_store/NetworkApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "esp_platform/EspDeviceInterface.hpp"
#include "esp_platform/EspTimerInterface.hpp"
#include "http/HttpHandler.hpp"
#include "http_types/HttpTypes.hpp"
#include "logger/Logger.hpp"
#include "ota/OtaApiHandler.hpp"
#include "wifi_manager/EmbeddedServer.hpp"
#include "esp_platform/EspWiFiInterface.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiManager.hpp"

#include <cstdio>

namespace framework {

// Default root URI is "/framework" — declared as an in-class default in FrameworkContext.hpp.
// API endpoints mount at rootUri/api/*, UI assets at rootUri/ui/*.

static const char* TAG = "FrameworkContext";

static logger::Logger log{TAG};

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

    // Create the timer implementation — used by WiFiManager for retry delays.
    timerInterface_  = new esp_platform::EspTimerInterface();
    wifiCtx.timer    = timerInterface_;

    // Read device info once — MAC drives both the mDNS hostname and AuthStore.
    const device::DeviceInfo devInfo = deviceInterface_->info();
    uint8_t mac[6] = {};
    sscanf(devInfo.mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

    // Build per-device hostname: prefix + last 3 MAC bytes, e.g. "esp32-a1b2c3"
    char macSuffix[8];
    snprintf(macSuffix, sizeof(macSuffix), "%02x%02x%02x", mac[3], mac[4], mac[5]);
    const std::string hostname = mdnsPrefix_ + "-" + macSuffix;
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
    delete timerInterface_;
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
