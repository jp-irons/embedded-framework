// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once
#include <functional>

namespace wifi_manager {
	
enum class WiFiState {
    Idle,
    AP_Mode,
    STA_Connecting,
    STA_Connected,
    DriverFailed,
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
    static constexpr const char* TAG = "WiFiStateMachine";

    using Listener = std::function<void(WiFiState, WiFiState)>;

    WiFiStateMachine() = default;

    void setListener(Listener cb) { listener = std::move(cb); }

    WiFiState getState() const { return state; }

    void onEvent(WiFiEvent ev);

    // Corrects the reported state without invoking the listener/action chain.
    // Used when a caller has already performed the corresponding action
    // itself (e.g. WiFiManager::onDisconnect() retrying via a direct
    // startSTA() call) and only needs getState()/getStaStatus() to stop
    // reporting a stale state — NOT a substitute for onEvent() in normal
    // control flow.
    void markState(WiFiState s) { state = s; }

    static const char* toString(WiFiState s);

private:
    WiFiState state = WiFiState::Idle;
    Listener listener = nullptr;

    void transitionTo(WiFiState newState);
};
}
