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
    case WiFiState::AP_Mode:        return "AP_Mode";
    case WiFiState::STA_Connecting: return "STA_Connecting";
    case WiFiState::STA_Connected:  return "STA_Connected";
    default:                        return "Unknown";
    }
}

} // namespace
