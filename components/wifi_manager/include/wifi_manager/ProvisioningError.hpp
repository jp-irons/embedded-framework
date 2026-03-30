#pragma once

namespace wifi_manager {

enum class ProvisioningError {
    None = 0,

    // Input / validation
    InvalidInput,
    MissingCredentials,

    // WiFi operations
    ApStartFailed,
    StaConnectFailed,
    StaTestFailed,

    // Storage
    CredentialSaveFailed,

    // Internal
    InternalError
};

} // namespace wifi_manager