#pragma once

#include "wifi_manager/WiFiTypes.hpp"
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
    credential_store::CredentialStore* credentialStore = nullptr;

    WiFiInterface* wifiInterface = nullptr;
    WiFiStateMachine* stateMachine = nullptr;
	
	ApConfig apConfig;
	StaConfig testStaConfig;
	StaConfig currentStaConfig;
};

} // namespace wifi_manager
