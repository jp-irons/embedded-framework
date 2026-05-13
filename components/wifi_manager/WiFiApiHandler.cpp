#include "wifi_manager/WiFiApiHandler.hpp"

#include "cJSON.h"
#include "common/Result.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http_types/HttpTypes.hpp"
#include "logger/Logger.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiInterface.hpp"
#include "wifi_manager/WiFiManager.hpp"

#include <vector>

namespace wifi_manager {

using namespace http;
using namespace common;

static logger::Logger log{WiFiApiHandler::TAG};

WiFiApiHandler::WiFiApiHandler(WiFiContext &w)
    : wifiCtx(w) {
    log.debug("constructor");
}

// handle events
common::Result WiFiApiHandler::handle(http::HttpRequest &req, http::HttpResponse &res) {
    const std::string &path = req.path();
    std::string target = extractTarget(req.path());
    log.debug("handle action '%s'", target.c_str());

    if (target == "scan") {
        return handleScan(req, res);
    }
    if (target == "status") {
        return handleStatus(req, res);
    }
    //    if (path == "/api/wifi/connect") {
    //        handleConnect(req, res);
    //        return true;
    //    }
    //    if (path == "/api/wifi/disconnect") {
    //        handleDisconnect(res);
    //        return true;
    //    }
    //
	log.error("handle target '%s' unsupported", target.c_str());
	return res.sendJsonError(403, "handlePost '" + target + "' unsupported");
}

static void formatBssid(const uint8_t bssid[6], char out[18]) {
    // Produces "AA:BB:CC:DD:EE:FF"
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
}

common::Result WiFiApiHandler::handleScan(HttpRequest &req, HttpResponse &res) {
    log.debug("handleScan");
    std::vector<WiFiAp> aps;
    Result r = wifiCtx.wifiInterface->scan(aps);

	log.debug("scan result");

    if (r == Result::Ok) {
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
        common::Result h_r = res.sendJson(json_response);
        cJSON_free(json_response);
		return h_r;
    } else {
        log.warn("result %s", common::toString(r));
        return res.sendJsonStatus(common::toString(r));
    }
}

common::Result WiFiApiHandler::handleStatus(HttpRequest &req, HttpResponse &res) {
    log.debug("handleStatus");

    if (req.method() != HttpMethod::Get) {
		return res.sendJsonError(403, "Method not supported");
    }

    WiFiStaStatus st = wifiCtx.wifiManager->getStaStatus();

    // Create root JSON object
    cJSON *root = cJSON_CreateObject();
    if (!root) {
		log.error("Internal error");
        return res.sendJsonError(500, "Internal Error");
    }

    // Add fields
    cJSON_AddStringToObject(root, "state", st.state.c_str());
    cJSON_AddStringToObject(root, "ssid", st.ssid.c_str());
    cJSON_AddStringToObject(root, "lastErrorReason", st.lastErrorReason.c_str());
    cJSON_AddBoolToObject(root, "connected", st.connected);

    // Serialize
    char *jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!jsonStr) {
		log.error("Internal error preparing response");
        return res.sendJsonError(500, "Internal Error");
    }

    // Send response
    common::Result r = res.sendJson(jsonStr);
    free(jsonStr); // cJSON allocates with malloc()

    return r;
}

common::Result WiFiApiHandler::handleConnect(HttpRequest &req, HttpResponse &res) {
	log.error("handleConnect not implemented");
    return res.sendJson(404, "Wi-Fi connect not implemented");
}

common::Result WiFiApiHandler::handleDisconnect(HttpRequest &req, HttpResponse &res) {
	log.error("handleDisconnect not implemented");
	return res.sendJson(404, "Wi-Fi disconnect not implemented");
}

} // namespace wifi_manager