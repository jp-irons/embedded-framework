#include "core_api/WiFiApiHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "wifi_manager/WiFiManager.hpp"

using namespace http;
using namespace wifi_manager;


namespace core_api {

WiFiApiHandler::WiFiApiHandler(WiFiManager& w)
    : wifi(w)
{
}

bool WiFiApiHandler::handle(const HttpRequest& req, HttpResponse& res)
{
    const std::string& path = req.path();

    if (path == "/api/wifi/scan") {
        handleScan(res);
        return true;
    }
    if (path == "/api/wifi/status") {
        handleStatus(res);
        return true;
    }
    if (path == "/api/wifi/connect") {
        handleConnect(req, res);
        return true;
    }
    if (path == "/api/wifi/disconnect") {
        handleDisconnect(res);
        return true;
    }

    return false;
}

void WiFiApiHandler::handleScan(HttpResponse& res)
{
    res.jsonStatus("not_implemented");
}

void WiFiApiHandler::handleStatus(HttpResponse& res)
{
    res.jsonStatus("not_implemented");
}

void WiFiApiHandler::handleConnect(const HttpRequest& req, HttpResponse& res)
{
    res.jsonStatus("not_implemented");
}

void WiFiApiHandler::handleDisconnect(HttpResponse& res)
{
    res.jsonStatus("not_implemented");
}

} // namespace core_api