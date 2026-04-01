#include "framework/FrameworkContext.hpp"

#include "core_api/CredentialApiHandler.hpp"
#include "core_api/WiFiApiHandler.hpp"
#include "esp_log.h"
#include "wifi_manager/ProvisioningServer.hpp"
#include "wifi_manager/RuntimeServer.hpp"
#include "wifi_manager/WiFiInterface.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"

namespace framework {

static const char *TAG = "FrameworkContext";

FrameworkContext::FrameworkContext() {
	ESP_LOGD(TAG, "constructor");

    //
    // 1. Wire WiFiContext (static pointers only)
    //
    wifiCtx.creds = &credentialStore;

    //
    // 2. Create servers (AP + Runtime HTTP)
    //
    provisioningServer = new wifi_manager::ProvisioningServer(wifiCtx);
    runtimeServer = new wifi_manager::RuntimeServer(wifiCtx);

    wifiCtx.provisioningServer = provisioningServer;
    wifiCtx.runtimeServer = runtimeServer;

    //
    // 3. Create WiFiManager
    //
    wifiInterface = new wifi_manager::WiFiInterface(wifiCtx);
    wifiCtx.wifiInterface = wifiInterface;

    //
    // 4. Create WiFiStateMachine
    //
    wifiStateMachine = new wifi_manager::WiFiStateMachine(wifiCtx);
    wifiCtx.stateMachine = wifiStateMachine;

    //
    // 5. Create API handlers
    //
    credentialApi = new core_api::CredentialApiHandler(credentialStore);
    wifiApi = new core_api::WiFiApiHandler(wifiCtx);
}

FrameworkContext::~FrameworkContext() {
    stop();

    delete wifiApi;
    delete credentialApi;

    delete wifiStateMachine;

    delete wifiInterface;
    delete runtimeServer;
    delete provisioningServer;
}

void FrameworkContext::start() {
    ESP_LOGD(TAG, "start");
    wifiStateMachine->start();
}

void FrameworkContext::stop() {
}

} // namespace framework