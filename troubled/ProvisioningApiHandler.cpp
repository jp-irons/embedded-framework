#include "ProvisioningApiHandler.hpp"
#include "WiFiManager.hpp"
#include "ApplicationContext.hpp"
#include "cJSON.h"
#include "esp_log.h"

using wifi_manager::WiFiManager;

static const char* TAG = "ProvisioningApiHandler";

ProvisioningApiHandler::ProvisioningApiHandler(ApplicationContext& ctx)
    : ctx(ctx)
{}

bool ProvisioningApiHandler::handles(const std::string& path) const {
    // Accept anything under /api/provisioning/*
    return path.rfind("/api/provision/", 0) == 0;
}

esp_err_t ProvisioningApiHandler::handle(httpd_req_t* req) {
    std::string path = req->uri;
    ESP_LOGD(TAG, "Handling API request: %s", path.c_str());
    if (path == "/api/provision/status") {
        ESP_LOGD(TAG, "path = /api/provision/status");
        return handleStatus(req);
    }

    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unknown provisioning endpoint");
}

esp_err_t ProvisioningApiHandler::handleStatus(httpd_req_t* req)
{
    static const char* msg_unknown =
        "{\"state\":\"UNKNOWN\",\"connected\":false,\"ssid\":\"\",\"lastErrorReason\":0}";
    static const char* msg_no_wifi =
        "{\"state\":\"NO_WIFI\",\"connected\":false,\"ssid\":\"\",\"lastErrorReason\":0}";
    static const char* msg_ok =
        "{\"state\":\"OK\",\"connected\":false,\"ssid\":\"\",\"lastErrorReason\":0}";

    // *** NEW: read ctx.wifiManager, but don't log, don't deref ***
    WiFiManager* wifi = ctx.wifiManager.get();

    const char* msg =
        (wifi == nullptr) ? msg_no_wifi : msg_ok;

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, msg, strlen(msg));
}
