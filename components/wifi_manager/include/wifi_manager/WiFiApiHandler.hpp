#pragma once

#include "common/Result.hpp"
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

    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    wifi_manager::WiFiContext &wifiCtx;
    common::Result handleScan(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleStatus(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleConnect(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleDisconnect(http::HttpRequest &req, http::HttpResponse &res);

};

} // namespace wifi_manager