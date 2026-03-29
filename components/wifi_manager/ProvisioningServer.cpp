#include "ProvisioningServer.hpp"

#include "esp_log.h"
#include "esp_wifi.h"

#include <string>

namespace wifi_manager {

static const char* TAG = "ProvisioningServer";

ProvisioningServer::ProvisioningServer(WiFiContext* ctx)
    : ctx(ctx),
      server(nullptr)
{
    // Intentionally empty: no side effects in constructor.
}

bool ProvisioningServer::start()
{
    if (server) {
        ESP_LOGW(TAG, "Provisioning server already running");
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    ESP_LOGI(TAG, "Starting provisioning HTTP server");

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start provisioning server");
        server = nullptr;
        return false;
    }

    return registerHandlers();
}

void ProvisioningServer::stop()
{
    if (server) {
        ESP_LOGI(TAG, "Stopping provisioning HTTP server");
        httpd_stop(server);
        server = nullptr;
    }
}

bool ProvisioningServer::registerHandlers()
{
    httpd_uri_t root = {
        .uri      = "/provision",
        .method   = HTTP_GET,
        .handler  = handleRoot,
        .user_ctx = this
    };

    httpd_uri_t submit = {
        .uri      = "/provision/submit",
        .method   = HTTP_POST,
        .handler  = handleSubmit,
        .user_ctx = this
    };

    httpd_uri_t status = {
        .uri      = "/provision/status",
        .method   = HTTP_GET,
        .handler  = handleStatus,
        .user_ctx = this
    };

    httpd_uri_t scan = {
        .uri      = "/provision/scan",
        .method   = HTTP_GET,
        .handler  = handleScan,
        .user_ctx = this
    };

    if (httpd_register_uri_handler(server, &root)   != ESP_OK ||
        httpd_register_uri_handler(server, &submit) != ESP_OK ||
        httpd_register_uri_handler(server, &status) != ESP_OK ||
        httpd_register_uri_handler(server, &scan)   != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register provisioning handlers");
        return false;
    }

    return true;
}

ProvisioningServer* ProvisioningServer::fromReq(httpd_req_t* req)
{
    return static_cast<ProvisioningServer*>(req->user_ctx);
}

// -------------------------
// Handlers
// -------------------------

esp_err_t ProvisioningServer::handleRoot(httpd_req_t* req)
{
    const char* html =
        "<html><body>"
        "<h1>WiFi Provisioning</h1>"
        "<form action=\"/provision/submit\" method=\"post\">"
        "SSID: <input name=\"ssid\"><br>"
        "Password: <input name=\"password\" type=\"password\"><br>"
        "<input type=\"submit\" value=\"Connect\">"
        "</form>"
        "</body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t ProvisioningServer::handleSubmit(httpd_req_t* req)
{
    auto* self = fromReq(req);
    auto* ctx  = self->ctx;

    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        ESP_LOGW(TAG, "Empty provisioning submit body");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    std::string body(buf);
    auto ssidPos = body.find("ssid=");
    auto passPos = body.find("&password=");

    if (ssidPos == std::string::npos || passPos == std::string::npos) {
        ESP_LOGW(TAG, "Malformed provisioning submit body");
        return ESP_FAIL;
    }

    std::string ssid = body.substr(ssidPos + 5, passPos - (ssidPos + 5));
    std::string pass = body.substr(passPos + 10);

    credential_store::WiFiCredential cred{ssid, pass};
    ctx->creds->add(cred);

    // Optionally reset index / mark new credentials available
    ctx->currentCredIndex = 0;

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t ProvisioningServer::handleStatus(httpd_req_t* req)
{
    auto* self = fromReq(req);
    auto* ctx  = self->ctx;

    char json[128];
    snprintf(json, sizeof(json),
             "{\"state\": %d, \"current\": %zu}",
             static_cast<int>(ctx->state),
             ctx->currentCredIndex);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t ProvisioningServer::handleScan(httpd_req_t* req)
{
    wifi_ap_record_t aps[20];
    uint16_t count = 20;

    esp_wifi_scan_start(nullptr, true);
    esp_wifi_scan_get_ap_records(&count, aps);

    std::string json = "[";
    for (int i = 0; i < count; i++) {
        json += "{\"ssid\":\"";
        json += reinterpret_cast<const char*>(aps[i].ssid);
        json += "\"}";
        if (i < count - 1) json += ",";
    }
    json += "]";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.size());
    return ESP_OK;
}

} // namespace wifi_manager