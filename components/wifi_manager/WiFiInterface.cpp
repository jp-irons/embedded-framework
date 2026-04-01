#include "wifi_manager/WiFiContext.hpp"
#include "credential_store/CredentialStore.hpp"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "include/wifi_manager/WiFiInterface.hpp"

#include "include/wifi_manager/WiFiStateMachine.hpp"
#include "wifi_manager/WiFiTypes.hpp"

namespace wifi_manager {

static const char* TAG = "WiFiManager";

WiFiInterface::WiFiInterface(WiFiContext& ctx)
    : ctx(ctx)
{
    // Register WiFi event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &WiFiInterface::wifiEventHandler,
        this,
        nullptr));

    // Register IP event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &WiFiInterface::ipEventHandler,
        this,
        nullptr));
}

void WiFiInterface::connectTo(const credential_store::WiFiCredential& cred)
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

void WiFiInterface::wifiEventHandler(void* arg,
                                   esp_event_base_t base,
                                   int32_t id,
                                   void* data)
{
    auto* self = static_cast<WiFiInterface*>(arg);
    self->handleWiFiEvent(base, id, data);
}

void WiFiInterface::ipEventHandler(void* arg,
                                 esp_event_base_t base,
                                 int32_t id,
                                 void* data)
{
    auto* self = static_cast<WiFiInterface*>(arg);
    self->handleIPEvent(base, id, data);
}

// -----------------------------------------------------------------------------
// Instance-level event handlers
// -----------------------------------------------------------------------------

void WiFiInterface::handleWiFiEvent(esp_event_base_t base,
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
		    // Optional: notify state machine that AP is ready
		    ctx.stateMachine->onApStarted();
		    break;

        default:
            break;
    }
}

void WiFiInterface::handleIPEvent(esp_event_base_t base,
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

void WiFiInterface::onSTAConnected()
{
    ESP_LOGI(TAG, "STA connected");
    // Notify provisioning state machine
    ctx.stateMachine->onStaConnected();
}

void WiFiInterface::onSTADisconnected(uint8_t reason)
{
	ESP_LOGI(TAG, "STA disconnected");
	// Notify provisioning state machine
	ctx.stateMachine->onStaDisconnected(translateReason(reason));
}


// -----------------------------------------------------------------------------
// Introspection helpers (e.g. for querying hardware state.)
// -----------------------------------------------------------------------------

} // namespace wifi_manager