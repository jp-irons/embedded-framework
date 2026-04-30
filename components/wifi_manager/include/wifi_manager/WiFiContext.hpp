#pragma once

#include "wifi_types/WiFiTypes.hpp"
namespace credential_store {
class CredentialStore;
}

namespace wifi_manager {

class WiFiInterface;
class WiFiManager;
class EmbeddedServer;

struct WiFiContext {
    EmbeddedServer *embeddedServer = nullptr;
    credential_store::CredentialStore *credentialStore = nullptr;

    WiFiInterface *wifiInterface = nullptr;
	WiFiManager *wifiManager = nullptr;

    wifi_types::ApConfig apConfig;
    wifi_types::WiFiCredential currentWiFiCred;
	
	std::string rootUri;

};

} // namespace wifi_manager
