#include "wifi_manager/WiFiApiHandler.hpp"

#include "esp_log.h"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "wifi_manager/WiFiContext.hpp"

using namespace http;

namespace wifi_manager {

static const char *TAG = "WiFiApiHandler";

WiFiApiHandler::WiFiApiHandler(WiFiContext &w)
    : wifiCtx(w) {
    ESP_LOGD(TAG, "constructor");
}


// handle requests not handled elsewhere
void WiFiApiHandler::handle(http::HttpRequest &req, http::HttpResponse &res) {
    const std::string &path = req.path();
	ESP_LOGD(TAG, "handle");

//    if (path == "/provision/status") {
//        return handleStatus(req, res);
//    }
//
//    if (path == "/provision/reset") {
//        return handleReset(req, res);
//    }
//
//    if (path == "/provision/retry") {
//        return handleRetry(req, res);
//    }
//

//    if (path == "/api/wifi/scan") {
//        handleScan(res);
//        return true;
//    }
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
//    return false;
}

void WiFiApiHandler::handleScan(HttpResponse &res) {
    res.jsonStatus("not_implemented");
}

void WiFiApiHandler::handleStatus(HttpResponse &res) {
    res.jsonStatus("not_implemented");
}

void WiFiApiHandler::handleConnect(const HttpRequest &req, HttpResponse &res) {
    res.jsonStatus("not_implemented");
}

void WiFiApiHandler::handleDisconnect(HttpResponse &res) {
    res.jsonStatus("not_implemented");
}

} // namespace core_api