#include "framework/FrameworkContext.hpp"

#include "wifi_manager/WiFiApiHandler.hpp"
#include "credential_store/CredentialStore.hpp"
#include "wifi_manager/ProvisioningServer.hpp"
#include "wifi_manager/RuntimeServer.hpp"
#include "wifi_manager/WiFiInterface.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"

namespace framework {

static const char *TAG = "FrameworkContext";

FrameworkContext::FrameworkContext(const wifi_manager::ApConfig &apCfg) {
    ESP_LOGD(TAG, "constructor");
	
	// 1. Initialize NVS
	ESP_LOGD(TAG, "nvs_flash_init");
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_LOGE(TAG, "flash erase then init");
	    ESP_ERROR_CHECK(nvs_flash_erase());
	    ESP_ERROR_CHECK(nvs_flash_init());
	}

	// 2. Initialize event loop
	ESP_LOGD(TAG, "init event loop");
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	// 3. Initialize netif
	ESP_LOGD(TAG, "init netif");
	ESP_ERROR_CHECK(esp_netif_init());



    ESP_LOGD(TAG, "AP SSID %s", apCfg.ssid.c_str());
    wifiCtx.apConfig = apCfg;
    wifiCtx.credentialStore = new credential_store::CredentialStore("wifi");

    // 2. Create servers (AP + Runtime HTTP)
    provisioningServer = new wifi_manager::ProvisioningServer(wifiCtx);
    runtimeServer = new wifi_manager::RuntimeServer(wifiCtx);
    wifiCtx.provisioningServer = provisioningServer;
    wifiCtx.runtimeServer = runtimeServer;

    // 3. Create WiFiInterface
    wifiInterface = new wifi_manager::WiFiInterface(wifiCtx);
    wifiCtx.wifiInterface = wifiInterface;

    // 4. Create WiFiStateMachine
    wifiStateMachine = new wifi_manager::WiFiStateMachine(wifiCtx);
    wifiCtx.stateMachine = wifiStateMachine;

    // 5. Create API handlers
    wifiApi = new wifi_manager::WiFiApiHandler(wifiCtx);
}

FrameworkContext::~FrameworkContext() {
    stop();

    delete wifiApi;
	// TODO delete credentialApi
//    delete credentialApi;

    delete wifiStateMachine;

    delete wifiInterface;
    delete runtimeServer;
    delete provisioningServer;
}

void FrameworkContext::start() {
    ESP_LOGD(TAG, "start");
    wifiStateMachine->start();
}

void FrameworkContext::stop() {}

} // namespace framework