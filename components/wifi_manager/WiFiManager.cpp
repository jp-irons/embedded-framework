#include "WiFiManager.hpp"
#include "ProvisioningServer.hpp"
#include "RuntimeServer.hpp"

#include "esp_log.h"

namespace wifi_manager {

static const char* TAG = "WiFiManager";

WiFiManager::WiFiManager(WiFiContext& ctx)
    : ctx(ctx)
{
    ctx.state = ProvisioningState::Idle;
}

void WiFiManager::startProvisioning() {
    transitionTo(ProvisioningState::StartingProvisioning);
}

void WiFiManager::onProvisioningComplete() {
    transitionTo(ProvisioningState::ProvisioningComplete);
}

void WiFiManager::onProvisioningFailed() {
    transitionTo(ProvisioningState::StartingProvisioning);
}

void WiFiManager::onSTAConnected() {
    transitionTo(ProvisioningState::STAConnected);
}

void WiFiManager::onSTADisconnected() {
    transitionTo(ProvisioningState::StartingProvisioning);
}

void WiFiManager::transitionTo(ProvisioningState newState) {
    ESP_LOGI(TAG, "State: %d → %d",
             static_cast<int>(ctx.state),
             static_cast<int>(newState));

    ctx.state = newState;
}

void WiFiManager::loop() {
    switch (ctx.state) {
        case ProvisioningState::Idle: break;
        case ProvisioningState::StartingProvisioning: handleStartingProvisioning(); break;
        case ProvisioningState::Provisioning: handleProvisioning(); break;
        case ProvisioningState::ProvisioningComplete: handleProvisioningComplete(); break;
        case ProvisioningState::StartingSTA: handleStartingSTA(); break;
        case ProvisioningState::ConnectingSTA: handleConnectingSTA(); break;
        case ProvisioningState::STAConnected: handleSTAConnected(); break;
        case ProvisioningState::StartingRuntime: handleStartingRuntime(); break;
        case ProvisioningState::Runtime: handleRuntime(); break;
    }
}

// ---------------- State Handlers ----------------

void WiFiManager::handleStartingProvisioning() {
    ESP_LOGI(TAG, "Starting provisioning…");

    stopSTA();
    startAP();

    if (ctx.provisioning) {
        ctx.provisioning->start();
    }

    transitionTo(ProvisioningState::Provisioning);
}

void WiFiManager::handleProvisioning() {
    // Nothing to do — ProvisioningServer will call onProvisioningComplete()
}

void WiFiManager::handleProvisioningComplete() {
    ESP_LOGI(TAG, "Provisioning complete");

    if (ctx.provisioning) {
        ctx.provisioning->stop();
    }

    stopAP();
    transitionTo(ProvisioningState::StartingSTA);
}

void WiFiManager::handleStartingSTA() {
    ESP_LOGI(TAG, "Starting STA…");

    startSTA();
    transitionTo(ProvisioningState::ConnectingSTA);
}

void WiFiManager::handleConnectingSTA() {
    // Nothing here — STA events will call onSTAConnected() or onSTADisconnected()
}

void WiFiManager::handleSTAConnected() {
    ESP_LOGI(TAG, "STA connected");

    transitionTo(ProvisioningState::StartingRuntime);
}

void WiFiManager::handleStartingRuntime() {
    ESP_LOGI(TAG, "Starting runtime server…");

    if (ctx.runtime) {
        ctx.runtime->start();
    }

    transitionTo(ProvisioningState::Runtime);
}

void WiFiManager::handleRuntime() {
    // Runtime server handles everything
}

// ---------------- WiFi Actions (stubs) ----------------

void WiFiManager::startAP() {
    ESP_LOGI(TAG, "startAP() stub");
}

void WiFiManager::stopAP() {
    ESP_LOGI(TAG, "stopAP() stub");
}

void WiFiManager::startSTA() {
    ESP_LOGI(TAG, "startSTA() stub");
}

void WiFiManager::stopSTA() {
    ESP_LOGI(TAG, "stopSTA() stub");
}

void WiFiManager::startRuntimeServer() {
    ESP_LOGI(TAG, "startRuntimeServer() stub");
}

void WiFiManager::stopRuntimeServer() {
    ESP_LOGI(TAG, "stopRuntimeServer() stub");
}

// ---------------- Factory ----------------

WiFiManager* create(WiFiContext& ctx) {
    ctx.manager      = new WiFiManager(ctx);
    ctx.provisioning = new ProvisioningServer(ctx);
    ctx.runtime      = new RuntimeServer(ctx);
    return ctx.manager;
}

} // namespace wifi_manager