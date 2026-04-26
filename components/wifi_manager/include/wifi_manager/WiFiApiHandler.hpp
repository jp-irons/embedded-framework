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

    http::HandlerResult handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    wifi_manager::WiFiContext &wifiCtx;
    http::HandlerResult handleScan(http::HttpRequest &req, http::HttpResponse &res);
    http::HandlerResult handleStatus(http::HttpRequest &req, http::HttpResponse &res);
    http::HandlerResult handleConnect(http::HttpRequest &req, http::HttpResponse &res);
    http::HandlerResult handleDisconnect(http::HttpRequest &req, http::HttpResponse &res);

};

} // namespace wifi_manager