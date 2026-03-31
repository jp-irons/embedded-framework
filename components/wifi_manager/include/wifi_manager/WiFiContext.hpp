#pragma once

namespace credential_store {
class CredentialStore;
}

namespace wifi_manager {

class WiFiManager;
class ProvisioningStateMachine;
class ProvisioningServer;
class RuntimeServer;

enum class WiFiState;

struct WiFiContext {
    ProvisioningServer* provisioningServer = nullptr;
    RuntimeServer* runtimeServer = nullptr;
    credential_store::CredentialStore* creds = nullptr;

    WiFiManager* wifiManager = nullptr;
    ProvisioningStateMachine* stateMachine = nullptr;

    WiFiState wifiState;
};

} // namespace wifi_manager
