#pragma once
#include <functional>

namespace wifi_manager {
	
enum class WiFiState {
    Idle,
    AP_Mode,
    STA_Connecting,
    STA_Connected,
};

enum class WiFiEvent {
    StartProvisioning,
    NetworkProvided,
    ConnectSuccess,
    ConnectFail,
    Disconnect,
    FatalError
};

class WiFiStateMachine {
public:
    using Listener = std::function<void(WiFiState, WiFiState)>;

    WiFiStateMachine() = default;

    void setListener(Listener cb) { listener = std::move(cb); }

    WiFiState getState() const { return state; }

    void onEvent(WiFiEvent ev);

    static const char* toString(WiFiState s);

private:
    WiFiState state = WiFiState::Idle;
    Listener listener = nullptr;

    void transitionTo(WiFiState newState);
};
}
