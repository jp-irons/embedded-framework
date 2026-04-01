#pragma once

namespace http {
    class HttpRequest;
    class HttpResponse;
}

namespace wifi_manager {
	class WiFiContext;
}

namespace core_api {

class WiFiApiHandler {
public:
  explicit WiFiApiHandler(wifi_manager::WiFiContext& wifi);

  bool handle(const http::HttpRequest &req, http::HttpResponse &res);

private:
    void handleScan(http::HttpResponse& res);
    void handleStatus(http::HttpResponse& res);
    void handleConnect(const http::HttpRequest& req, http::HttpResponse& res);
    void handleDisconnect(http::HttpResponse& res);

    wifi_manager::WiFiContext& wifiCtx;
};

} // namespace core_api