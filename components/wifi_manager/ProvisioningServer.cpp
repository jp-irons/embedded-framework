#include "ProvisioningServer.hpp"

#include "esp_log.h"
#include "esp_wifi.h"

#include <string>

namespace wifi_manager {

static const char *TAG = "ProvisioningServer";

ProvisioningServer::ProvisioningServer(WiFiContext *ctx)
    : ctx(ctx)
    , server(nullptr) {
    // Intentionally empty: no side effects in constructor.
}

bool ProvisioningServer::start() {
    if (server) {
        ESP_LOGW(TAG, "Provisioning server already running");
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.server_port = 80;

    ESP_LOGI(TAG, "Starting provisioning HTTP server");

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start provisioning server");
        server = nullptr;
        return false;
    }

    return registerHandlers();
}

void ProvisioningServer::stop() {
    if (server) {
        ESP_LOGI(TAG, "Stopping provisioning HTTP server");
        httpd_stop(server);
        server = nullptr;
    }
}

bool ProvisioningServer::registerHandlers() {
    httpd_uri_t api_get = {.uri = "/api/*", .method = HTTP_GET, .handler = dispatchApi, .user_ctx = this};

    httpd_uri_t files = {.uri = "/*",
                         .method = HTTP_GET,
                         .handler = handleStaticFile,
                         //	    .handler  = [](httpd_req_t* req) {
                         //	        return serveEmbedded(req, "/");   // <-- BASE PATH HERE
                         //	    },
                         .user_ctx = this};
    httpd_register_uri_handler(server, &api_get);
    httpd_register_uri_handler(server, &files);
//    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &files));

    return true;
}

ProvisioningServer *ProvisioningServer::fromReq(httpd_req_t *req) {
    return static_cast<ProvisioningServer *>(req->user_ctx);
}

// -------------------------
// Handlers
// -------------------------

esp_err_t ProvisioningServer::dispatchApi(httpd_req_t *req) {
    ESP_LOGD(TAG, "dispatchApi");
    std::string json = "[ok dispatchApi]";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.size());
    return ESP_OK;
}

// replace with serveEmbedded()
esp_err_t ProvisioningServer::handleStaticFile(httpd_req_t *req) {
    ESP_LOGD(TAG, "handleStaticFile");
    std::string json = "[ok static file]";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.size());
    return ESP_OK;
}

} // namespace wifi_manager