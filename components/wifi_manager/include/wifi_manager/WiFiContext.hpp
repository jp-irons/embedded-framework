#pragma once

#include "credential_store/CredentialStore.hpp"
#include "wifi_manager/WiFiState.hpp"   // Needed because WiFiContext stores a ProvisioningState

namespace credential_store {
    class CredentialStore;   // ← forward declare instead of include
}


namespace wifi_manager {

// Forward declarations
class WiFiManager;
class ProvisioningServer;
class RuntimeServer;

enum class ProvisioningState;

struct WiFiContext {
    WiFiState state = WiFiState::UNPROVISIONED_AP;

    WiFiManager* manager = nullptr;
    ProvisioningServer* provisioning = nullptr;
    RuntimeServer* runtime = nullptr;

    credential_store::CredentialStore* creds = nullptr;
	
	std::vector<credential_store::WiFiCredential> loadedCreds;
	size_t currentCredIndex = 0;

};



} // namespace wifi_manager