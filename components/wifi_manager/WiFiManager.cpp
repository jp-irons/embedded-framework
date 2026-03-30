#include "wifi_manager/WiFiManager.hpp"
#include "wifi_manager/ProvisioningServer.hpp"
#include "wifi_manager/RuntimeServer.hpp"

#include "esp_wifi.h"
#include "esp_log.h"

namespace wifi_manager {

static const char* TAG = "WiFiManager";

WiFiManager::WiFiManager(WiFiContext* ctx)
    : ctx(ctx)
{
    // Register WiFi + IP event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiManager::wifiEventHandler, this, nullptr));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &WiFiManager::ipEventHandler, this, nullptr));
}

WiFiManager* create(WiFiContext& ctx)
{
    auto* mgr = new WiFiManager(&ctx);
    ctx.manager = mgr;
    return mgr;
}

void WiFiManager::start()
{
    ESP_LOGI(TAG, "WiFiManager start");

    // Basic WiFi init
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    loadCredentials();

    if (ctx->loadedCreds.empty()) {
        transitionTo(WiFiState::UNPROVISIONED_AP);
    } else {
        ctx->currentCredIndex = 0;
        transitionTo(WiFiState::PROVISIONING_STA_TEST);
    }
}

void WiFiManager::loop()
{
    static uint32_t hb = 0;
    if ((hb++ % 200) == 0) {
        ESP_LOGD(TAG, "[HB] state=%s, credIndex=%zu",
                 toString(ctx->state).c_str(),
                 ctx->currentCredIndex);
    }
}

void WiFiManager::loadCredentials()
{
    ctx->loadedCreds.clear();
    ctx->creds->loadAll(ctx->loadedCreds);

    ESP_LOGI(TAG, "Loaded %zu credential(s)", ctx->loadedCreds.size());
}

void WiFiManager::transitionTo(WiFiState s)
{
    ESP_LOGI(TAG, "Transition %s → %s",
             toString(ctx->state).c_str(),
             toString(s).c_str());

    switch (s) {

	case WiFiState::UNPROVISIONED_AP:
		stopSta();
		if (ctx->runtime)      ctx->runtime->stop();
		startAp();
		if (ctx->provisioning) ctx->provisioning->start();
		break;

		
		
	case WiFiState::PROVISIONING_STA_TEST:
		stopAp();
		if (ctx->provisioning) ctx->provisioning->stop();
		if (ctx->runtime)      ctx->runtime->stop();
		startStaWithCurrent();
		break;

    case WiFiState::PROVISIONING_FAILED:
		stopSta();
		stopAp();
		if (ctx->provisioning) ctx->provisioning->stop();
		if (ctx->runtime)      ctx->runtime->stop();
		break;

    case WiFiState::RUNTIME_STA:
		stopAp();
		if (ctx->provisioning) ctx->provisioning->stop();
		if (ctx->runtime)      ctx->runtime->start();
		break;

    }

    ctx->state = s;
}

void WiFiManager::startAp()
{
    ESP_LOGI(TAG, "Starting AP for provisioning");

    wifi_config_t ap_cfg = {};
    strcpy((char*)ap_cfg.ap.ssid, "esp-provision");
    ap_cfg.ap.ssid_len = strlen("esp-provision");
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFiManager::stopAp()
{
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK && (mode & WIFI_MODE_AP)) {
        ESP_LOGI(TAG, "Stopping AP");
        ESP_ERROR_CHECK(esp_wifi_stop());
    }
}

void WiFiManager::startStaWithCurrent()
{
    if (ctx->loadedCreds.empty() ||
        ctx->currentCredIndex >= ctx->loadedCreds.size()) {
        transitionTo(WiFiState::UNPROVISIONED_AP);
        return;
    }

    const auto& cred = ctx->loadedCreds[ctx->currentCredIndex];

    ESP_LOGI(TAG, "Testing STA credential SSID='%s'", cred.ssid.c_str());

    wifi_config_t sta_cfg = {};
    strncpy((char*)sta_cfg.sta.ssid, cred.ssid.c_str(), sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char*)sta_cfg.sta.password, cred.password.c_str(), sizeof(sta_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void WiFiManager::stopSta()
{
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK && (mode & WIFI_MODE_STA)) {
        ESP_LOGI(TAG, "Stopping STA");
        esp_wifi_disconnect();
        esp_wifi_stop();
    }
}

void WiFiManager::tryNextCredential()
{
    ctx->currentCredIndex++;

    if (ctx->currentCredIndex >= ctx->loadedCreds.size()) {
        ESP_LOGW(TAG, "All credentials failed");
        transitionTo(WiFiState::PROVISIONING_FAILED);
        transitionTo(WiFiState::UNPROVISIONED_AP);
        return;
    }

    ESP_LOGI(TAG, "Trying next credential index=%zu", ctx->currentCredIndex);
    transitionTo(WiFiState::PROVISIONING_STA_TEST);
}

// -------------------------
// Event handlers
// -------------------------

void WiFiManager::wifiEventHandler(void* arg,
                                   esp_event_base_t base,
                                   int32_t id,
                                   void* data)
{
    auto* self = static_cast<WiFiManager*>(arg);

    if (base == WIFI_EVENT) {
        switch (id) {

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "STA disconnected");
            self->tryNextCredential();
            break;

        default:
            break;
        }
    }
}

void WiFiManager::ipEventHandler(void* arg,
                                 esp_event_base_t base,
                                 int32_t id,
                                 void* data)
{
    auto* self = static_cast<WiFiManager*>(arg);

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Got IP — entering runtime");
        self->transitionTo(WiFiState::RUNTIME_STA);
    }
}

} // namespace wifi_manager