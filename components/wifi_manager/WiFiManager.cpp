#include "wifi_manager/WiFiManager.hpp"

#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/ProvisioningStateMachine.hpp"
#include "credential_store/CredentialStore.hpp"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"

namespace wifi_manager {

static const char* TAG = "WiFiManager";

WiFiManager::WiFiManager(WiFiContext* ctx)
    : ctx(ctx)
{
    // Register WiFi event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &WiFiManager::wifiEventHandler,
        this,
        nullptr));

    // Register IP event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &WiFiManager::ipEventHandler,
        this,
        nullptr));
}

void WiFiManager::start()
{
    // Currently no explicit startup logic; FrameworkContext drives provisioning.
}

void WiFiManager::loop()
{
    // No periodic logic yet; placeholder for future enhancements.
}

// -----------------------------------------------------------------------------
// Multi-SSID fallback entry point
// -----------------------------------------------------------------------------

void WiFiManager::startSTAWithFallback()
{
    ESP_LOGI(TAG, "Starting STA with fallback");

    credentials = ctx->creds->loadAllSortedByPriority();
    currentIndex = 0;

    if (credentials.empty()) {
        ESP_LOGW(TAG, "No credentials found — reporting provisioning failure");
        state = WiFiState::UNPROVISIONED_AP;
        ctx->stateMachine->wifiConnectionFailed(ProvisioningError::MissingCredentials);
        return;
    }

    state = WiFiState::PROVISIONING_STA_TEST;
    tryNextCredential();
}

// -----------------------------------------------------------------------------
// Fallback loop
// -----------------------------------------------------------------------------

void WiFiManager::tryNextCredential()
{
    if (currentIndex >= credentials.size()) {
        ESP_LOGW(TAG, "All SSIDs failed — reporting failure");
        state = WiFiState::PROVISIONING_FAILED;
        ctx->stateMachine->wifiConnectionFailed(ProvisioningError::StaTestFailed);
        return;
    }

    const auto& cred = credentials[currentIndex];
    ESP_LOGI(TAG, "Trying SSID %s (priority %d)",
             cred.ssid.c_str(), cred.priority);

    connectTo(cred);
}

void WiFiManager::connectTo(const credential_store::WiFiCredential& cred)
{
    wifi_config_t cfg = {};
    strncpy(reinterpret_cast<char*>(cfg.sta.ssid),
            cred.ssid.c_str(),
            sizeof(cfg.sta.ssid));
    strncpy(reinterpret_cast<char*>(cfg.sta.password),
            cred.password.c_str(),
            sizeof(cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

// -----------------------------------------------------------------------------
// Static → instance forwarding
// -----------------------------------------------------------------------------

void WiFiManager::wifiEventHandler(void* arg,
                                   esp_event_base_t base,
                                   int32_t id,
                                   void* data)
{
    auto* self = static_cast<WiFiManager*>(arg);
    self->handleWiFiEvent(base, id, data);
}

void WiFiManager::ipEventHandler(void* arg,
                                 esp_event_base_t base,
                                 int32_t id,
                                 void* data)
{
    auto* self = static_cast<WiFiManager*>(arg);
    self->handleIPEvent(base, id, data);
}

// -----------------------------------------------------------------------------
// Instance-level event handlers
// -----------------------------------------------------------------------------

void WiFiManager::handleWiFiEvent(esp_event_base_t base,
                                  int32_t id,
                                  void* data)
{
    switch (id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA started");
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            auto* evt = static_cast<wifi_event_sta_disconnected_t*>(data);
            ESP_LOGW(TAG, "STA disconnected, reason=%d", evt->reason);
            onSTADisconnected(evt->reason);
            break;
        }

        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "AP started");
            break;

        default:
            break;
    }
}

void WiFiManager::handleIPEvent(esp_event_base_t base,
                                int32_t id,
                                void* data)
{
    if (id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "STA got IP");
        onSTAConnected();
    }
}

// -----------------------------------------------------------------------------
// High-level callbacks
// -----------------------------------------------------------------------------

void WiFiManager::onSTAConnected()
{
    ESP_LOGI(TAG, "STA connected");

    state = WiFiState::RUNTIME_STA;

    // Notify provisioning state machine
    ctx->stateMachine->wifiConnected();
}

void WiFiManager::onSTADisconnected(uint8_t reason)
{
    if (state != WiFiState::PROVISIONING_STA_TEST) {
        ESP_LOGW(TAG, "Runtime STA disconnect (reason=%d) — ignoring", reason);
        return;
    }

    ESP_LOGW(TAG, "Provisioning STA test failed (reason=%d), trying next SSID", reason);

    currentIndex++;
    tryNextCredential();
}

// -----------------------------------------------------------------------------
// Introspection helpers (for RuntimeServer, etc.)
// -----------------------------------------------------------------------------

WiFiState WiFiManager::getState() const
{
    return state;
}

int WiFiManager::getCurrentCredentialIndex() const
{
    return static_cast<int>(currentIndex);
}

const std::vector<credential_store::WiFiCredential>&
WiFiManager::getLoadedCredentials() const
{
    return credentials;
}

const char* WiFiManager::getCurrentSSID() const
{
    if (currentIndex < credentials.size()) {
        return credentials[currentIndex].ssid.c_str();
    }
    return "";
}

} // namespace wifi_manager