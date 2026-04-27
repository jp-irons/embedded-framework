#pragma once

#include "common/Result.hpp"
#include "credential_store/CredentialApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "embedded_files/EmbeddedFileHandler.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpServer.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"

namespace wifi_manager {

struct WiFiContext;

class ProvisioningServer : public http::HttpHandler {
  public:
    explicit ProvisioningServer(  WiFiContext &ctx, WiFiApiHandler &wifiApi,
                             credential_store::CredentialApiHandler &credentialApi, device::DeviceApiHandler &deviceApi);
    ~ProvisioningServer();

    bool start();
    void stop();

    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
  struct Route {
      std::string prefix;
      http::HttpHandler* handler;
  };

    WiFiContext &ctx;

    http::HttpServer server;
    embedded_files::EmbeddedFileHandler embeddedFileHandler;
    wifi_manager::WiFiApiHandler wifiHandler;
    credential_store::CredentialApiHandler credentialHandler;
    device::DeviceApiHandler deviceHandler;

	std::vector<Route> routes;
    bool routesRegistered = false;
};

} // namespace wifi_manager