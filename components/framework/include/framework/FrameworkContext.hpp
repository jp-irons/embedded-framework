#pragma once

#include "credential_store/CredentialApiHandler.hpp"
#include "credential_store/CredentialStore.hpp"
#include "device/DeviceApiHandler.hpp"
#include "device_cert/DeviceCert.hpp"
#include "wifi_manager/WiFiContext.hpp"

namespace credential_store {
class CredentialStore;
}

namespace wifi_manager {
class WiFiApiHandler;
}

namespace framework {

class FrameworkContext {
  public:
    explicit FrameworkContext();

    /**
     * @param apConfig      AP-mode configuration (SSID, password, etc.)
     * @param rootUri       API root path, e.g. "/framework/api"
     * @param mdnsPrefix    Prefix for the mDNS hostname; last 3 MAC bytes are
     *                      appended automatically, e.g. "esp32" → "esp32-a1b2c3".
     *                      Defaults to "esp32".
     */
    FrameworkContext(const wifi_manager::ApConfig &apConfig,
                     std::string rootUri,
                     std::string mdnsPrefix = "esp32");

    ~FrameworkContext();

    const std::string &getRootUri() const { return rootUri_; }

    const wifi_manager::ApConfig &getApConfig() const { return apConfig; }

    void start();
    void stop();

  private:
    wifi_manager::ApConfig apConfig = {
        .ssid = "ESP32 FW Test", .password = "password", .channel = 1, .maxConnections = 4};
    std::string rootUri_    = "/framework/api";
    std::string mdnsPrefix_ = "esp32";

    // Per-device TLS cert (generated on first boot, persisted in NVS)
    device_cert::DeviceCert deviceCert_;

    // Always-present value types
    wifi_manager::WiFiContext        wifiCtx;
    credential_store::CredentialStore credentialStore;

    // Owned heap objects
    wifi_manager::EmbeddedServer             *embeddedServer = nullptr;
    wifi_manager::WiFiInterface              *wifiInterface  = nullptr;
    wifi_manager::WiFiManager                *wifiManager    = nullptr;
    wifi_manager::WiFiApiHandler             *wifiApi        = nullptr;
    credential_store::CredentialApiHandler   *credentialApi  = nullptr;
    device::DeviceApiHandler                 *deviceApi      = nullptr;

    void initialize(const wifi_manager::ApConfig &apConfig);
};

} // namespace framework
