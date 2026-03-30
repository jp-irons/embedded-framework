#pragma once

namespace http {
    class HttpRequest;
    class HttpResponse;
}

namespace wifi_manager {
	class WiFiManager;
}

using namespace http;
using namespace wifi_manager;


namespace core_api {

class WiFiApiHandler {
public:
    explicit WiFiApiHandler(WiFiManager& wifi);

    bool handle(const HttpRequest& req, HttpResponse& res);

private:
    void handleScan(HttpResponse& res);
    void handleStatus(HttpResponse& res);
    void handleConnect(const HttpRequest& req, HttpResponse& res);
    void handleDisconnect(HttpResponse& res);

    WiFiManager& wifi;
};

} // namespace core_api