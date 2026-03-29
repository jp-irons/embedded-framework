#pragma once

namespace wifi_manager {

enum class ProvisioningState {
    Idle,
    StartingProvisioning,
    Provisioning,
    ProvisioningComplete,
    StartingSTA,
    ConnectingSTA,
    STAConnected,
    StartingRuntime,
    Runtime
};

constexpr const char* toString(ProvisioningState s)
{
    switch (s) {
        case ProvisioningState::Idle:                  return "Idle";
        case ProvisioningState::StartingProvisioning:  return "StartingProvisioning";
        case ProvisioningState::Provisioning:          return "Provisioning";
        case ProvisioningState::ProvisioningComplete:  return "ProvisioningComplete";
        case ProvisioningState::StartingSTA:           return "StartingSTA";
        case ProvisioningState::ConnectingSTA:         return "ConnectingSTA";
        case ProvisioningState::STAConnected:          return "STAConnected";
        case ProvisioningState::StartingRuntime:       return "StartingRuntime";
        case ProvisioningState::Runtime:               return "Runtime";
        default:                                       return "Unknown";
    }
}

} // namespace wifi_manager