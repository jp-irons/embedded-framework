#pragma once

#include "network_store/NetworkStore.hpp"
#include "wifi_manager/WiFiTypes.hpp"
#include <functional>
namespace network_store {
class NetworkStore;
}

namespace wifi_manager {

class WiFiInterface;
class WiFiManager;
class EmbeddedServer;

struct WiFiContext {
    EmbeddedServer *embeddedServer = nullptr;
    network_store::NetworkStore *networkStore = nullptr;

    WiFiInterface *wifiInterface = nullptr;
	WiFiManager *wifiManager = nullptr;

    ApConfig apConfig;

	std::string rootUri;

    // Hostname advertised via mDNS (without the .local suffix).
    // Set before WiFiManager::start() -- typically via FrameworkContext.
    std::string mdnsHostname = "esp32";

    // Called by WiFiManager when the WiFi driver fails and all retries are
    // exhausted.  Typically wired to device::reboot() by FrameworkContext so
    // the OTA boot counter increments and rollback can fire.  May be left null
    // in test environments.
    std::function<void()> onDriverFatal = nullptr;
};

} // namespace wifi_manager
