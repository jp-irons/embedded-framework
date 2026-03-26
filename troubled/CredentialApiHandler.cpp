#include "CredentialApiHandler.hpp"
#include "WiFiManager.hpp"
#include "ApplicationContext.hpp"
#include "esp_log.h"
#include <sstream>
#include <cJSON.h>

static const char* TAG = "CredentialApiHandler";

CredentialApiHandler::CredentialApiHandler(ApplicationContext& ctx)
    : ctx(ctx)
{}

bool CredentialApiHandler::handles(const std::string& path) const {
    return path.rfind("/api/credentials/", 0) == 0;
}

esp_err_t CredentialApiHandler::handle(httpd_req_t* req) {
    std::string path = req->uri;

    ESP_LOGI(TAG, "Handling API request: %s", path.c_str());

    if (path == "/api/credentials/list")
        return handleList(req);

    if (path == "/api/credentials/clear")
        return handleClear(req);

    if (path == "/api/credentials/delete")
        return handleDelete(req);

    if (path == "/api/credentials/scan")
        return handleList(req);

    if (path == "/api/credentials/submit")
        return handleSubmit(req);


    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unknown credentials API endpoint");
}

// ---------------------------------------------------------
// /api/credentials/list
// ---------------------------------------------------------
esp_err_t CredentialApiHandler::handleList(httpd_req_t* req) {
    std::vector<wifi_manager::WiFiEntry> entries;

    ctx.credentialStore.loadEntries(entries);   // <-- correct API usage

    std::stringstream json;
    json << "[";

    bool first = true;
    for (const auto& c : entries) {
        if (!first) json << ",";
        first = false;

        json << "{"
             << "\"ssid\":\"" << c.ssid << "\","
             << "\"priority\":" << c.priority
             << "}";
    }

    json << "]";

    std::string out = json.str();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, out.c_str(), out.size());
}

// ---------------------------------------------------------
// /api/credentials/clear
// ---------------------------------------------------------
esp_err_t CredentialApiHandler::handleClear(httpd_req_t* req) {
    ESP_LOGI(TAG, "Clearing all credentials");

    ctx.credentialStore.clearAll();

    httpd_resp_set_type(req, "application/json");
    const char* msg = "{\"status\":\"ok\"}";
    esp_err_t err = httpd_resp_send(req, msg, strlen(msg));
    // ctx.wifiManager->onCredentialsChanged();
    return err;
}

// ---------------------------------------------------------
// /api/credentials/delete?ssid=MyWifi
// ---------------------------------------------------------
esp_err_t CredentialApiHandler::handleDelete(httpd_req_t* req) {
    std::string ssid = getQueryParam(req, "ssid");

    if (ssid.empty()) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid parameter");
    }

    ESP_LOGI(TAG, "Deleting credential: %s", ssid.c_str());

    bool removed = ctx.credentialStore.erase(ssid);

    if (!removed) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "SSID not found");
    }

    httpd_resp_set_type(req, "application/json");
    const char* msg = "{\"status\":\"ok\"}";
    esp_err_t err = httpd_resp_send(req, msg, strlen(msg));
    ctx.wifiManager->onCredentialsChanged();
    return err;
}

esp_err_t CredentialApiHandler::handleSubmit(httpd_req_t* req) {
    // Read body
    char buf[512];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
    }
    buf[len] = '\0';

    // 🔥 Log the raw JSON body here
    ESP_LOGD(TAG, "Raw body (%d bytes): %s", len, buf);

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    // Required: ssid
    cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    if (!ssid || !cJSON_IsString(ssid)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid");
    }

    wifi_manager::WiFiEntry entry;
    entry.ssid = ssid->valuestring;

    // Optional: password
    cJSON* password = cJSON_GetObjectItem(root, "password");
    entry.password = (password && cJSON_IsString(password))
                     ? password->valuestring
                     : "";

    // Optional: priority
    cJSON* priority = cJSON_GetObjectItem(root, "priority");
    entry.priority = (priority && cJSON_IsNumber(priority))
                     ? priority->valueint
                     : 0;

    // Optional: bssid + bssidLocked
    memset(entry.bssid, 0, 6);
    entry.bssidLocked = false;

    cJSON* bssid = cJSON_GetObjectItem(root, "bssid");
    if (bssid && cJSON_IsString(bssid)) {
        uint8_t mac[6];
        if (sscanf(bssid->valuestring, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &mac[0], &mac[1], &mac[2],
                   &mac[3], &mac[4], &mac[5]) == 6)
        {
            memcpy(entry.bssid, mac, 6);
            entry.bssidLocked = true;
        }
    }

    // Optional: explicit bssidLocked override
    cJSON* bssidLocked = cJSON_GetObjectItem(root, "bssidLocked");
    if (bssidLocked && cJSON_IsBool(bssidLocked)) {
        entry.bssidLocked = cJSON_IsTrue(bssidLocked);
    }

    // Load → modify/replace → save
    std::vector<wifi_manager::WiFiEntry> entries;
    ctx.credentialStore.loadEntries(entries);

    bool replaced = false;
    for (auto& e : entries) {
        if (e.ssid == entry.ssid) {
            e = entry;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        entries.push_back(entry);
    }

    bool ok = ctx.credentialStore.saveEntries(entries);
    cJSON_Delete(root);

    if (!ok) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save credential");
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    // ctx.wifiManager->onCredentialsChanged();
    return err;
}

// ---------------------------------------------------------
// Helper: extract query parameter
// ---------------------------------------------------------
std::string CredentialApiHandler::getQueryParam(httpd_req_t* req, const char* key) {
    char buf[128];
    int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));

    if (ret != ESP_OK)
        return "";

    char value[64];
    if (httpd_query_key_value(buf, key, value, sizeof(value)) == ESP_OK)
        return std::string(value);

    return "";
}