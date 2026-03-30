#pragma once

#include <string>

namespace wifi_manager {

enum class WiFiState {
    UNPROVISIONED_AP,
    PROVISIONING_STA_TEST,
    PROVISIONING_FAILED,
    RUNTIME_STA
};

std::string toString(WiFiState state);

} // namespace wifi_manager