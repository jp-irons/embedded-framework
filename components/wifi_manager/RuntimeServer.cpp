#include "wifi_manager/RuntimeServer.hpp"
#include "esp_log.h"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"
#include "wifi_manager/WiFiTypes.hpp"

namespace wifi_manager {

static const char* TAG = "RuntimeServer";

RuntimeServer::RuntimeServer(WiFiContext &ctx)
    : ctx(ctx),
      server(nullptr)
{
    // Constructor intentionally empty.
}

bool RuntimeServer::start()
{
    if (server) {
        ESP_LOGW(TAG, "Runtime server already running");
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    ESP_LOGI(TAG, "Starting runtime HTTP server");

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start runtime server");
        server = nullptr;
        return false;
    }

    return registerHandlers();
}

void RuntimeServer::stop()
{
    if (server) {
        ESP_LOGI(TAG, "Stopping runtime HTTP server");
        httpd_stop(server);
        server = nullptr;
    }
}

bool RuntimeServer::registerHandlers()
{
    httpd_uri_t root = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = handleRoot,
        .user_ctx = this
    };

    httpd_uri_t info = {
        .uri      = "/info",
        .method   = HTTP_GET,
        .handler  = handleInfo,
        .user_ctx = this
    };

    if (httpd_register_uri_handler(server, &root) != ESP_OK ||
        httpd_register_uri_handler(server, &info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register runtime handlers");
        return false;
    }

    return true;
}

RuntimeServer* RuntimeServer::fromReq(httpd_req_t* req)
{
    return static_cast<RuntimeServer*>(req->user_ctx);
}

// -------------------------
// Handlers
// -------------------------

esp_err_t RuntimeServer::handleRoot(httpd_req_t* req)
{
    const char* html =
        "<html><body>"
        "<h1>Device Runtime</h1>"
        "<p><a href=\"/info\">Device Info</a></p>"
        "</body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t RuntimeServer::handleInfo(httpd_req_t* req)
{
    auto* self = fromReq(req);
    WiFiContext ctx  = self->ctx;
	

	std::string state = toString(ctx.stateMachine->getState());
	size_t index = ctx.stateMachine->getCredentialIndex();
	std::string ssid = ctx.stateMachine->getCurrentSSID();

	char json[256];
	snprintf(json, sizeof(json),
	         "{"
	         "\"state\": %s,"
	         "\"currentCred\": %d,"
	         "\"ssid\": \"%s\""
	         "}",
	         state.c_str(),
	         index,
	         ssid.c_str());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

} // namespace wifi_manager