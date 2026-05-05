#include "wifi_manager/WiFiStateMachine.hpp"

namespace wifi_manager{

void WiFiStateMachine::onEvent(WiFiEvent ev)
{
    switch (state) {

    case WiFiState::Idle:
        if (ev == WiFiEvent::StartProvisioning) {
            transitionTo(WiFiState::AP_Mode);
        } else if (ev == WiFiEvent::CredentialsProvided) {
            transitionTo(WiFiState::STA_Connecting);
        }
        break;

    case WiFiState::AP_Mode:
        if (ev == WiFiEvent::CredentialsProvided) {
            transitionTo(WiFiState::STA_Connecting);
        }
        break;

    case WiFiState::STA_Connecting:
        if (ev == WiFiEvent::ConnectSuccess) {
            transitionTo(WiFiState::STA_Connected);
        } else if (ev == WiFiEvent::ConnectFail) {
            transitionTo(WiFiState::AP_Mode);
        }
        break;

    case WiFiState::STA_Connected:
        if (ev == WiFiEvent::Disconnect) {
            transitionTo(WiFiState::STA_Connecting);
        }
        break;

    default:
        // TODO WiFiEvent::FatalError is fired by WiFiManager::onFatalError() when
        // startDriver() fails, but is not handled in any state.  Currently falls
        // through silently — device stays alive and OTA rollback handles recovery
        // after MAX_BOOT_ATTEMPTS.  Revisit once markValid() placement is settled:
        // consider a dedicated DriverFailed state with a timed retry or safe-mode AP.
        break;
    }
}

void WiFiStateMachine::transitionTo(WiFiState newState)
{
    if (newState == state) return;

    WiFiState old = state;
    state = newState;

    if (listener) {
        listener(old, newState);
    }
}

const char* WiFiStateMachine::toString(WiFiState s)
{
    switch (s) {
    case WiFiState::Idle:           return "Idle";
    case WiFiState::AP_Mode:        return "AP Mode";
    case WiFiState::STA_Connecting: return "STA Connecting";
    case WiFiState::STA_Connected:  return "STA Connected";
    default:                        return "Unknown";
    }
}

} // namespace
