#pragma once

class WiFiManager;

namespace http {
    class HttpRequest;
    class HttpResponse;
}

using namespace http;


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