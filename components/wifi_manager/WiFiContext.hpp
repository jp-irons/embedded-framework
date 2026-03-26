#pragma once

#include "WiFiState.hpp"   // Needed because WiFiContext stores a ProvisioningState
#include "CredentialStore.hpp"


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
};



} // namespace wifi_manager