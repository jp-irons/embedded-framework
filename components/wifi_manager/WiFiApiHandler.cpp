#include "wifi_manager/WiFiApiHandler.hpp"

#include "cJSON.h"
#include "common/Result.hpp"
#include "logger/Logger.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiInterface.hpp"

#include <vector>

using namespace http;

namespace wifi_manager {

static logger::Logger log{"WiFiApiHandler"};

WiFiApiHandler::WiFiApiHandler(WiFiContext &w)
    : wifiCtx(w) {
    log.debug("constructor");
}

// handle requests not handled elsewhere
bool WiFiApiHandler::handle(http::HttpRequest &req, http::HttpResponse &res) {
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
    return false;
}

std::string WiFiApiHandler::extractAction(const char *uri) {
    std::string path(uri);
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos || pos == path.length() - 1) {
        return {}; // no action found
    }
    return path.substr(pos + 1);
}

static void formatBssid(const uint8_t bssid[6], char out[18])
{
    // Produces "AA:BB:CC:DD:EE:FF"
    snprintf(out, 18,
             "%02X:%02X:%02X:%02X:%02X:%02X",
             bssid[0], bssid[1], bssid[2],
             bssid[3], bssid[4], bssid[5]);
}


bool WiFiApiHandler::handleScan(HttpResponse &res) {
    log.debug("handleScan");
    std::vector<WiFiAp> aps;
    common::Result r = wifiCtx.wifiInterface->scan(aps);
    log.debug("scan result");

    switch (r) {
        case common::Result::Ok: {
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
            char *json = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            res.json(json);
            return true;
        }

        case common::Result::NotFound:
            log.debug("result NotFound");
            res.jsonStatus("no_access_points_found");
            return true;

        case common::Result::Unsupported:
            log.warn("result Unsupported");
            res.jsonStatus("wifi_not_ready");
            return false;

        case common::Result::BadRequest:
            log.warn("result BadRequest");
            res.jsonStatus("bad_request");
            return false;

        case common::Result::Forbidden:
            log.warn("result Forbidden");
            res.jsonStatus("forbidden");
            return false;

        case common::Result::InternalError:
            log.warn("result InternalError");
            res.jsonStatus("internal_error");
            return false;

        default:
            log.warn("result unknown result");
            res.jsonStatus("unknown result");
            return false;
    }
}

bool WiFiApiHandler::handleStatus(HttpResponse &res) {
    res.jsonStatus("not_implemented");
    return false;
}

bool WiFiApiHandler::handleConnect(const HttpRequest &req, HttpResponse &res) {
    res.jsonStatus("not_implemented");
    return false;
}

bool WiFiApiHandler::handleDisconnect(HttpResponse &res) {
    res.jsonStatus("not_implemented");
    return false;
}


} // namespace wifi_manager