#pragma once

#include "wifi_types/WiFiTypes.hpp"
namespace credential_store {
class CredentialStore;
}

namespace wifi_types {
enum class WiFiState;
}

namespace wifi_manager {

class WiFiInterface;
class WiFiStateMachine;
class EmbeddedServer;

struct WiFiContext {
    EmbeddedServer *embeddedServer = nullptr;
    credential_store::CredentialStore *credentialStore = nullptr;

    WiFiInterface *wifiInterface = nullptr;
    WiFiStateMachine *stateMachine = nullptr;

    wifi_types::ApConfig apConfig;
    wifi_types::WiFiCredential currentWiFiCred;
	
	std::string rootUri;

};

} // namespace wifi_manager
