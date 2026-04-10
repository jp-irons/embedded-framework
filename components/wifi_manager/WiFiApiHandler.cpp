#include "wifi_manager/WiFiApiHandler.hpp"

#include "common/Result.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "logger/Logger.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiInterface.hpp"

#include "cJSON.h"
#include <vector>

using namespace http;

namespace wifi_manager {

static logger::Logger log{"WiFiApiHandler"};

WiFiApiHandler::WiFiApiHandler(WiFiContext &w)
    : wifiCtx(w) {
    log.debug("constructor");
}

// handle events
common::Result WiFiApiHandler::handle(http::HttpRequest &req, http::HttpResponse &res) {
    log.debug("handle");
    const std::string &path = req.path();
    std::string action = extractAction(req.path());
    log.debug("action '%s'", action.c_str());

    if (action == "scan") {
        log.debug("action scan matched");
        return handleScan(res);
    }
    //    if (path == "/api/wifi/status") {
    //        handleStatus(res);
    //        return;
    //    }
    //    if (path == "/api/wifi/connect") {
    //        handleConnect(req, res);
    //        return true;
    //    }
    //    if (path == "/api/wifi/disconnect") {
    //        handleDisconnect(res);
    //        return true;
    //    }
    //
    return common::Result::NotFound;
}

std::string WiFiApiHandler::extractAction(const char *uri) {
    std::string path(uri);
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos || pos == path.length() - 1) {
        return {}; // no action found
    }
    return path.substr(pos + 1);
}

static void formatBssid(const uint8_t bssid[6], char out[18]) {
    // Produces "AA:BB:CC:DD:EE:FF"
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
}

common::Result WiFiApiHandler::handleScan(HttpResponse &res) {
    log.debug("handleScan");
    std::vector<WiFiAp> aps;
    common::Result r = wifiCtx.wifiInterface->scan(aps);
    log.debug("scan result");

    if (r == common::Result::Ok) {
        log.debug("result Ok");
        uint16_t count = aps.size();
        cJSON *root = cJSON_CreateArray();

        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "ssid", aps[i].ssid.c_str());
            char bssidStr[18];
            formatBssid(aps[i].bssid, bssidStr);
            cJSON_AddStringToObject(item, "bssid", bssidStr);
            cJSON_AddNumberToObject(item, "rssi", aps[i].rssi);
            cJSON_AddStringToObject(item, "auth", toString(aps[i].auth));
            cJSON_AddItemToArray(root, item);
        }
        char *json_response = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        r = res.sendJson(json_response);
		cJSON_free(json_response);
    } else {
        log.warn("result %s", common::toString(r));
        res.sendJsonStatus(common::toString(r));
    }
    return r;
}

common::Result WiFiApiHandler::handleStatus(HttpResponse &res) {
    res.sendJsonStatus("not_implemented");
    return common::Result::Unsupported;
}

common::Result WiFiApiHandler::handleConnect(const HttpRequest &req, HttpResponse &res) {
    res.sendJsonStatus("not_implemented");
    return common::Result::Unsupported;
}

common::Result WiFiApiHandler::handleDisconnect(HttpResponse &res) {
    res.sendJsonStatus("not_implemented");
    return common::Result::Unsupported;
}

} // namespace wifi_manager