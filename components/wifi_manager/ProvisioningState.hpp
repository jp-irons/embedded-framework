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

} // namespace wifi_manager