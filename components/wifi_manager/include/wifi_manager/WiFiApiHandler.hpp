#pragma once

#include "http/HttpHandler.hpp"
namespace http {
class HttpRequest;
class HttpResponse;
} // namespace http

namespace wifi_manager {
struct WiFiContext;
}

namespace wifi_manager {

class WiFiApiHandler : public http::HttpHandler {
  public:
    explicit WiFiApiHandler(wifi_manager::WiFiContext &wifi);

    void handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    wifi_manager::WiFiContext &wifiCtx;
    void handleScan(http::HttpResponse &res);
    void handleStatus(http::HttpResponse &res);
    void handleConnect(const http::HttpRequest &req, http::HttpResponse &res);
    void handleDisconnect(http::HttpResponse &res);
	std::string extractAction(const char *uri);

};

} // namespace wifi_manager