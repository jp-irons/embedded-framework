#pragma once

#include "credential_store/CredentialStore.hpp"
#include "wifi_manager/WiFiTypes.hpp"
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

    ApConfig apConfig;
	
	std::string rootUri;

};

} // namespace wifi_manager
