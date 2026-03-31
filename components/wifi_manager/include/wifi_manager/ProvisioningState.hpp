#pragma once

namespace wifi_manager {

enum class ProvisioningState {
    Idle = 0,

    // AP + provisioning UI
    StartingProvisioning,
    Provisioning,

    // STA test phase
    StartingSTA,
    ConnectingSTA,
    StaConnected,
    StaTestFailed,

    // Success
    ProvisioningComplete,

    // Runtime
    StartingRuntime,
    Runtime
};

// Optional but extremely useful for logging
inline const char *toString(ProvisioningState state) {
    switch (state) {
        case ProvisioningState::Idle:
            return "Idle";
        case ProvisioningState::StartingProvisioning:
            return "StartingProvisioning";
        case ProvisioningState::Provisioning:
            return "Provisioning";
        case ProvisioningState::ProvisioningComplete:
            return "ProvisioningComplete";
        case ProvisioningState::StartingSTA:
            return "StartingSTA";
        case ProvisioningState::ConnectingSTA:
            return "ConnectingSTA";
        case ProvisioningState::StaConnected:
            return "StaConnected";
        case ProvisioningState::StaTestFailed:
            return "StaTestFailed";
        case ProvisioningState::StartingRuntime:
            return "StartingRuntime";
        case ProvisioningState::Runtime:
            return "Runtime";
        default:
            return "Unknown";
    }
}

} // namespace wifi_manager