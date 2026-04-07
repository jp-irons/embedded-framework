#pragma once

#include "wifi_manager/WiFiApiHandler.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpServer.hpp"
#include "static_assets/StaticFileHandler.hpp"

namespace http {
class HttpRequest;
class HttpResponse;
}

namespace wifi_manager {

struct WiFiContext;

class ProvisioningServer : public http::HttpHandler {
public:
    explicit ProvisioningServer(WiFiContext& ctx);
    ~ProvisioningServer();

    bool start();
    void stop();

	void handle(http::HttpRequest& req, http::HttpResponse& res) override;
	
private:
    WiFiContext& ctx;

    http::HttpServer server;
	static_assets::StaticFileHandler staticHandler;
	static_assets::StaticFileHandler fallbackHandler;
	wifi_manager::WiFiApiHandler wifiHandler;
// TODO re-add this	core_api::CredentialApiHandler credentialHandler;

    bool routesRegistered = false;
};

} // namespace wifi_manager