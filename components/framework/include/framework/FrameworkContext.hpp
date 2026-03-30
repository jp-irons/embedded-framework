#pragma once

#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiManager.hpp"
#include "wifi_manager/ProvisioningServer.hpp"
#include "wifi_manager/RuntimeServer.hpp"

#include "credential_store/CredentialStore.hpp"
#include "wifi_manager/ProvisioningStateMachine.hpp"

#include "http/HttpServer.hpp"

namespace core_api {
    class CredentialApiHandler;
    class ProvisioningApiHandler;
    class WiFiApiHandler;
}

namespace framework {

class FrameworkContext {
public:
    FrameworkContext();
    ~FrameworkContext();

    void start();
    void stop();

private:
    // Dispatcher
    static esp_err_t dispatchTrampoline(httpd_req_t* req);
    esp_err_t dispatch(httpd_req_t* req);

    // WiFi subsystem (internal to wifi_manager)
    wifi_manager::WiFiContext wifiCtx;
    wifi_manager::ProvisioningServer* provisioningServer = nullptr;
    wifi_manager::RuntimeServer* runtimeServer = nullptr;
    wifi_manager::WiFiManager* wifiManager = nullptr;

    // Framework-level components
    credential_store::CredentialStore credentialStore;
    wifi_manager::ProvisioningStateMachine* provisioningStateMachine = nullptr;

    core_api::CredentialApiHandler* credentialApi = nullptr;
    core_api::ProvisioningApiHandler* provisioningApi = nullptr;
    core_api::WiFiApiHandler* wifiApi = nullptr;

    http::HttpServer httpServer;

    static FrameworkContext* instance;
};

} // namespace framework