#include "ProvisioningServer.hpp"
#include "WiFiManager.hpp"  // for calling back via ctx.manager if needed
#include "CredentialStore.hpp"

#include "cJSON.h"
#include "esp_log.h"


namespace wifi_manager {

static const char* TAG = "ProvisioningServer";

ProvisioningServer::ProvisioningServer(WiFiContext& ctx)
    : ctx(ctx)
{
}

bool ProvisioningServer::start()
{
    if (server) {
        ESP_LOGW(TAG, "Provisioning server already running");
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.server_port = 80;   // or your chosen port

    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start provisioning server: %s", esp_err_to_name(err));
        server = nullptr;
        return false;
    }

    // --- Register /api/credentials/submit ---
    httpd_uri_t submitCreds = {
        .uri      = "/api/credentials/submit",
        .method   = HTTP_POST,
        .handler  = [](httpd_req_t* req) {
            auto* self = static_cast<ProvisioningServer*>(req->user_ctx);
            self->handleSubmitCredentials(req);
            return ESP_OK;
        },
        .user_ctx = this
    };

    httpd_register_uri_handler(server, &submitCreds);

    ESP_LOGI(TAG, "Provisioning server started");
    return true;
}

void ProvisioningServer::stop() {
    // stop provisioning server here
}

void ProvisioningServer::handleCredentials(const char* ssid, const char* password)
{
    credential_store::WifiCredential cred;
    cred.ssid     = ssid;
    cred.password = password;
    cred.priority = 0;

    if (ctx.creds) {
        ctx.creds->add(cred);
    }

    if (ctx.manager) {
        ctx.manager->onProvisioningComplete();
    }
}

void ProvisioningServer::handleSubmitCredentials(httpd_req_t* req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return;
    }
    buf[len] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return;
    }

    cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON* password = cJSON_GetObjectItem(root, "password");

    if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid/password");
        return;
    }

    handleCredentials(ssid->valuestring, password->valuestring);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

} // namespace wifi_manager