
#include "wifi_manager/ProvisioningServer.hpp"

#include "esp_log.h"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "static_assets/StaticFileHandler.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"

//static_assert(std::is_base_of<http::HttpHandler, static_assets::StaticFileHandler>::value, "mismatch");
//static_assert(std::is_trivially_destructible<static_assets::StaticFileHandler>::value == false, "just to force instantiation");
//


namespace wifi_manager {

static const char *TAG = "ProvisioningServer";

ProvisioningServer::ProvisioningServer(WiFiContext &ctx)
    : ctx(ctx)
    , server()
	// TODO sort this out properly?
    , staticHandler("/provision", "index.html")
	, fallbackHandler("/", "index.html")
    , wifiHandler(ctx)
    , credentialHandler(*ctx.credentialStore) {}

ProvisioningServer::~ProvisioningServer() {
    stop();
}

bool ProvisioningServer::start() {
    ESP_LOGI(TAG, "Starting ProvisioningServer");

    server.start();

    if (!routesRegistered) {
        ESP_LOGD(TAG, "start() registering routes");
        server.addRoute("/provision/*", &staticHandler);
		server.addRoute("/*", &fallbackHandler);
        // TODO implement handler for below
        //		server.addRoute("/api/wifi/*", &wifiHandler);
        //		server.addRoute("/api/credentials/*", &credentialHandler);

        // TODO Remove these legacy registrations
        //        server.registerHandler(http::HttpMethod::Get, "/provision/*", this, &ProvisioningServer::handleStaticFile);
        //
        //        server.registerHandler(http::HttpMethod::Post, "/provision/submit", this, &ProvisioningServer::handleSubmit);
        //
        //        server.registerHandler(http::HttpMethod::Get, "/provision/status", this, &ProvisioningServer::handleStatus);
        //
        //        server.registerHandler(http::HttpMethod::Get, "/provision/scan", this, &ProvisioningServer::handleScan);
        //
		
        routesRegistered = true;
    }

    return true;
}

void ProvisioningServer::stop() {
    ESP_LOGI(TAG, "Stopping ProvisioningServer");
    server.stop();
}

// handle requests not handled elsewhere
void ProvisioningServer::handle(http::HttpRequest &req, http::HttpResponse &res) {
    const std::string &path = req.path();
	ESP_LOGD(TAG, "handle");

//    if (path == "/provision/status") {
//        return handleStatus(req, res);
//    }
//
//    if (path == "/provision/reset") {
//        return handleReset(req, res);
//    }
//
//    if (path == "/provision/retry") {
//        return handleRetry(req, res);
//    }
//
    // fallback: serve provisioning UI
    return staticHandler.handle(req, res);
}

} // namespace wifi_manager