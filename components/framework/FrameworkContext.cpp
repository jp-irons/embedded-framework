#include "framework/FrameworkContext.hpp"

#include "credential_store/CredentialApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "esp_event.h"
#include "esp_netif.h"
#include "logger/Logger.hpp"
#include "nvs_flash.h"
#include "wifi_manager/EmbeddedServer.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiInterface.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"

namespace framework {

static constexpr const char *DEFAULT_ROOT_URI = "/framework/api";

static logger::Logger log{"FrameworkContext"};

FrameworkContext::FrameworkContext() {
    log.debug("constructor default apConfig and rootUri");
    initialize(apConfig);
}

FrameworkContext::FrameworkContext(const wifi_types::ApConfig &apConfig, std::string rootUri)
    : apConfig(apConfig)
    , rootUri_(std::move(rootUri)) {
    log.debug("constructor");
    initialize(apConfig);
}

void FrameworkContext::initialize(const wifi_types::ApConfig &apConfig) {
    log.debug("initializing framework context with root: {}", rootUri_.c_str());

    // 1. Initialize NVS
    log.debug("nvs_flash_init");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        log.error("flash erase then init");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 2. Initialize event loop
    log.debug("init event loop");
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Initialize netif
    log.debug("init netif");
    ESP_ERROR_CHECK(esp_netif_init());

    // 4. Populate wifiCtx with config and core pointers
    log.debug("AP SSID %s", apConfig.ssid.c_str());
    wifiCtx.apConfig = apConfig;
    wifiCtx.rootUri = rootUri_;
    wifiCtx.credentialStore = &credentialStore;

    // 5. Ensure credential store is ready before anything uses it
    //    credentialStore.loadAll();   // or initialize(), if you have one

    // 6. Create state machine first (so it exists before any events)
    wifiStateMachine = new wifi_manager::WiFiStateMachine(wifiCtx);
    wifiCtx.stateMachine = wifiStateMachine;

    // 7. Create API handlers
    wifiApi = new wifi_manager::WiFiApiHandler(wifiCtx);
    credentialApi = new credential_store::CredentialApiHandler(credentialStore);
	deviceApi = new device::DeviceApiHandler();

    // 8. Create servers
    embeddedServer= new wifi_manager::EmbeddedServer(wifiCtx, *wifiApi, *credentialApi, *deviceApi);
    wifiCtx.embeddedServer = embeddedServer;

    // 9. Create WiFiInterface LAST (this registers event handlers and may trigger events)
    wifiInterface = new wifi_manager::WiFiInterface(wifiCtx);
    wifiCtx.wifiInterface = wifiInterface;
}

FrameworkContext::~FrameworkContext() {
    log.info("destructor");
    stop();
    delete embeddedServer;
    delete wifiInterface;
    delete wifiStateMachine;
    delete wifiApi;
    delete credentialApi;
}

void FrameworkContext::start() {
    log.debug("start");
    wifiStateMachine->start();
}

void FrameworkContext::stop() {}

} // namespace framework