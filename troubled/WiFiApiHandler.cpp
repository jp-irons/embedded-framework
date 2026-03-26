#include "WiFiApiHandler.hpp"
#include "esp_wifi.h"
#include "cJSON.h"

WiFiApiHandler::WiFiApiHandler(ApplicationContext& ctx)
    : ctx(ctx)
{}

bool WiFiApiHandler::handles(const std::string& path) const {
    return path.rfind("/api/wifi/", 0) == 0;
}

esp_err_t WiFiApiHandler::handle(httpd_req_t* req) {
    std::string path = req->uri;

    if (path == "/api/wifi/scan")
        return handleScan(req);

    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unknown WiFi API endpoint");
}

esp_err_t WiFiApiHandler::handleScan(httpd_req_t* req) {
    wifi_scan_config_t config = {};
    config.show_hidden = true;

    if (esp_wifi_scan_start(&config, true) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
    }

    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);

    wifi_ap_record_t* aps = (wifi_ap_record_t*)calloc(count, sizeof(wifi_ap_record_t));
    if (!aps) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    esp_wifi_scan_get_ap_records(&count, aps);

    cJSON* root = cJSON_CreateArray();

    for (int i = 0; i < count; i++) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (char*)aps[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", aps[i].rssi);
        cJSON_AddNumberToObject(item, "auth", aps[i].authmode);
        cJSON_AddItemToArray(root, item);
    }

    free(aps);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t res = httpd_resp_sendstr(req, json);
    free(json);

    return res;
}