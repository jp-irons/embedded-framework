#pragma once

#include "wifi_manager/WiFiContext.hpp"
#include "credential_store/CredentialStore.hpp"

namespace core_api {
    class CredentialApiHandler;
    class WiFiApiHandler;
}

namespace credential_store {
    class CredentialStore;
}

namespace framework {

class FrameworkContext {
public:
    FrameworkContext();
    ~FrameworkContext();

    void start();
    void stop();

private:
    // WiFi subsystem (internal to wifi_manager)
    wifi_manager::WiFiContext wifiCtx;
    wifi_manager::ProvisioningServer* provisioningServer = nullptr;
    wifi_manager::RuntimeServer* runtimeServer = nullptr;
    wifi_manager::WiFiInterface* wifiInterface = nullptr;

    // Framework-level components
    credential_store::CredentialStore credentialStore;
    wifi_manager::WiFiStateMachine* wifiStateMachine = nullptr;

    core_api::CredentialApiHandler* credentialApi = nullptr;
    core_api::WiFiApiHandler* wifiApi = nullptr;

    static FrameworkContext* instance;
};

} // namespace framework