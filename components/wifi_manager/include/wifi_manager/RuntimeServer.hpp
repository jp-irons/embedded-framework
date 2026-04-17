#pragma once

#include "credential_store/CredentialApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpServer.hpp"
#include "static_assets/StaticFileHandler.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"

namespace wifi_manager {

struct WiFiContext;

class RuntimeServer : public http::HttpHandler {
  public:
    explicit RuntimeServer(WiFiContext &ctx, WiFiApiHandler &wifiApi,
                           credential_store::CredentialApiHandler &credentialApi, device::DeviceApiHandler &deviceApi);
    ~RuntimeServer();

    bool start(); // start HTTP server
    void stop(); // stop HTTP server

    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    struct Route {
        std::string prefix;
        http::HttpHandler* handler;
    };
	
    WiFiContext &ctx;

    http::HttpServer server;
    static_assets::StaticFileHandler staticHandler;
    static_assets::StaticFileHandler fallbackHandler;
    wifi_manager::WiFiApiHandler wifiHandler;
    credential_store::CredentialApiHandler credentialHandler;
    device::DeviceApiHandler deviceHandler;
	
	std::vector<Route> routes;
    bool routesRegistered = false;
};

} // namespace wifi_manager