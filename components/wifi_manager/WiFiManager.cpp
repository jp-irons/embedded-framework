#include "WiFiManager.hpp"
#include "ProvisioningServer.hpp"
#include "RuntimeServer.hpp"
#include "WiFiState.hpp"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"

namespace wifi_manager {

static const char* TAG = "WiFiManager";

WiFiManager::WiFiManager(WiFiContext& ctx)
    : ctx(ctx)
{
	WiFiState state = WiFiState::UNPROVISIONED_AP;
}

void WiFiManager::startProvisioning() {
    transitionTo(WiFiState::UNPROVISIONED_AP);
}

void WiFiManager::onProvisioningComplete()
{
    ESP_LOGI(TAG, "Provisioning complete → testing STA");
    transitionTo(WiFiState::PROVISIONING_STA_TEST);
}

void WiFiManager::onProvisioningFailed() {
    transitionTo(WiFiState::PROVISIONING_FAILED);
}

void WiFiManager::onSTAConnected() {
    transitionTo(WiFiState::RUNTIME_STA);
}

void WiFiManager::onSTADisconnected() {
    transitionTo(WiFiState::RUNTIME_STA);
}

void WiFiManager::transitionTo(WiFiState newState)
{
    ESP_LOGI(TAG, "WiFiManager: %s → %s",
             toString(state).c_str(),
             toString(newState).c_str());

    state = newState;

    switch (state) {

    case WiFiState::UNPROVISIONED_AP:
        ctx.runtime->stop();
        ctx.provisioning->start();
        break;

	case WiFiState::PROVISIONING_STA_TEST:
		ctx.provisioning->stop();
		connectSTAWithStoredCredentials();
		break;

    case WiFiState::PROVISIONING_FAILED:
        ctx.runtime->stop();
        ctx.provisioning->start();
        break;

    case WiFiState::RUNTIME_STA:
		ctx.provisioning->stop();  // stops AP + provisioning HTTP
		ctx.runtime->start();      // starts runtime HTTP
        break;
    }
}

void WiFiManager::connectSTAWithStoredCredentials()
{
    std::vector<credential_store::WifiCredential> creds;

    if (!ctx.creds->loadAll(creds) || creds.empty()) {
        ESP_LOGW(TAG, "No stored credentials → returning to AP mode");
        transitionTo(WiFiState::UNPROVISIONED_AP);
        return;
    }

    // For now: use the first credential
    const auto& c = creds[0];

    ESP_LOGI(TAG, "Connecting STA to SSID: %s", c.ssid.c_str());

    wifi_config_t cfg = {};
    strncpy((char*)cfg.sta.ssid, c.ssid.c_str(), sizeof(cfg.sta.ssid));
    strncpy((char*)cfg.sta.password, c.password.c_str(), sizeof(cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void WiFiManager::loop() {
//possibly needed in future
}

// ---------------- Factory ----------------

WiFiManager* create(WiFiContext& ctx) {
    ctx.manager      = new WiFiManager(ctx);
    ctx.provisioning = new ProvisioningServer(ctx);
    ctx.runtime      = new RuntimeServer(ctx);
    return ctx.manager;
}

} // namespace wifi_manager