#include "WiFiState.hpp"
#include <string>

namespace wifi_manager {

std::string toString(WiFiState state)
{
    switch (state) {
    case WiFiState::UNPROVISIONED_AP:
        return "UNPROVISIONED_AP";
    case WiFiState::PROVISIONING_STA_TEST:
        return "PROVISIONING_STA_TEST";
    case WiFiState::PROVISIONING_FAILED:
        return "PROVISIONING_FAILED";
    case WiFiState::RUNTIME_STA:
        return "RUNTIME_STA";
    }
    return "UNKNOWN";
}

} // namespace wifi_manager