#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "ApplicationContext.hpp"

static const char* TAG = "app_main";

extern "C" void app_main(void)
{
    // Logging
//    esp_log_level_set(TAG, ESP_LOG_INFO);

	esp_log_level_set("app_main", ESP_LOG_DEBUG);
	esp_log_level_set("ApplicationContext", ESP_LOG_DEBUG);
	esp_log_level_set("CredentialApiHandler", ESP_LOG_DEBUG);
	esp_log_level_set("CredentialStore", ESP_LOG_DEBUG);
	esp_log_level_set("FrameworkContext", ESP_LOG_DEBUG);
	esp_log_level_set("HttpServer", ESP_LOG_DEBUG);
    esp_log_level_set("ProvisioningServer", ESP_LOG_DEBUG);
    esp_log_level_set("RuntimeServer", ESP_LOG_DEBUG);
	esp_log_level_set("WiFiApiHandler", ESP_LOG_DEBUG);
	esp_log_level_set("WiFiInterface", ESP_LOG_DEBUG);
	esp_log_level_set("WiFiStateMachine", ESP_LOG_DEBUG);
	
    esp_log_level_set("serveEmbedded", ESP_LOG_DEBUG);
    esp_log_level_set("EmbeddedFiles", ESP_LOG_DEBUG);
	
    esp_log_level_set("CredentialApiHandler", ESP_LOG_DEBUG);
    esp_log_level_set("WiFiApiHandler", ESP_LOG_DEBUG);

	// 1. Initialize NVS
	ESP_LOGD(TAG, "about to call nvs_flash_init");
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_LOGD(TAG, "flash erase then init");
	    ESP_ERROR_CHECK(nvs_flash_erase());
	    ESP_ERROR_CHECK(nvs_flash_init());
	}

	// 2. Initialize event loop
	ESP_LOGD(TAG, "init event loop");
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	// 3. Initialize netif
	ESP_LOGD(TAG, "init netif");
	ESP_ERROR_CHECK(esp_netif_init());

    // Create the application context (owns everything)
	ESP_LOGD(TAG, "creating app context");
    static ApplicationContext app;

    // Start WiFi provisioning (or runtime later)
    app.start();

    ESP_LOGI(TAG, "System initialised");

    // Main loop
    while (true) {
        app.loop();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}