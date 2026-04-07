#pragma once

#include "credential_store/CredentialStore.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiContext.hpp"
//#include "wifi_manager/WiFiApiHandler.hpp"

namespace credential_store {
class CredentialStore;
}

namespace framework {

class FrameworkContext {
  public:
    FrameworkContext(const wifi_manager::ApConfig &provisioningApConfig);
    ~FrameworkContext();

    credential_store::CredentialStore &getCredentialStore() const;
    wifi_manager::WiFiContext &getWiFiContext() const;
    wifi_manager::WiFiStateMachine &getWiFiStateMachine() const;
    wifi_manager::ProvisioningServer &getProvisioningServer() const;
    wifi_manager::RuntimeServer &getRuntimeServer() const;
    wifi_manager::WiFiApiHandler &getWiFiApi() const;

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

    wifi_manager::WiFiApiHandler *wifiApi = nullptr;
};

} // namespace framework