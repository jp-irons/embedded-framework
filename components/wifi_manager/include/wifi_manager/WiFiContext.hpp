#pragma once

namespace credential_store {
class CredentialStore;
}

namespace wifi_manager {

class WiFiInterface;
class WiFiStateMachine;
class ProvisioningServer;
class RuntimeServer;

enum class WiFiState;

struct WiFiContext {
    ProvisioningServer* provisioningServer = nullptr;
    RuntimeServer* runtimeServer = nullptr;
    credential_store::CredentialStore* creds = nullptr;

    WiFiInterface* wifiInterface = nullptr;
    WiFiStateMachine* stateMachine = nullptr;
};

} // namespace wifi_manager
