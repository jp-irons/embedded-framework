#pragma once

#include "credential_store/CredentialStore.hpp"
#include "wifi_manager/WiFiContext.hpp"

namespace core_api {
class CredentialApiHandler;
class WiFiApiHandler;
} // namespace core_api

namespace credential_store {
class CredentialStore;
}

namespace framework {

class FrameworkContext {
  public:
    FrameworkContext(const wifi_manager::ApConfig &provisioningApConfig);

    credential_store::CredentialStore &getCredentialStore() const;
    core_api::CredentialApiHandler &getCredentialApi() const;
    core_api::WiFiApiHandler &getWiFiApi() const;
    wifi_manager::WiFiContext &getWiFiContext() const;
    wifi_manager::WiFiStateMachine &getWiFiStateMachine() const;
    wifi_manager::ProvisioningServer &getProvisioningServer() const;
    wifi_manager::RuntimeServer &getRuntimeServer() const;

    ~FrameworkContext();

    void start();
    void stop();

  private:
    // WiFi subsystem (internal to wifi_manager)
    wifi_manager::WiFiContext wifiCtx;
    wifi_manager::ProvisioningServer *provisioningServer = nullptr;
    wifi_manager::RuntimeServer *runtimeServer = nullptr;
    wifi_manager::WiFiInterface *wifiInterface = nullptr;

    // Framework-level components
    credential_store::CredentialStore credentialStore;
    wifi_manager::WiFiStateMachine *wifiStateMachine = nullptr;

    core_api::CredentialApiHandler *credentialApi = nullptr;
    core_api::WiFiApiHandler *wifiApi = nullptr;
};

} // namespace framework