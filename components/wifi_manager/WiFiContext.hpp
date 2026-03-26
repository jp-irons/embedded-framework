#pragma once

#include "ProvisioningState.hpp"   // Needed because WiFiContext stores a ProvisioningState
#include "CredentialStore.hpp"


namespace wifi_manager {

// Forward declarations
class WiFiManager;
class ProvisioningServer;
class RuntimeServer;

enum class ProvisioningState;

struct WiFiContext {
    WiFiManager* manager = nullptr;
    ProvisioningServer* provisioning = nullptr;
    RuntimeServer* runtime = nullptr;
	credential_store::CredentialStore* creds = nullptr;

    ProvisioningState state = ProvisioningState::Idle;
};

} // namespace wifi_manager