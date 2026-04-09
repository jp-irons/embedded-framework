
#include "wifi_manager/ProvisioningServer.hpp"

#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "logger/Logger.hpp"
#include "static_assets/StaticFileHandler.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"

namespace wifi_manager {

static logger::Logger log{"ProvisioningServer"};

ProvisioningServer::ProvisioningServer(WiFiContext &ctx)
    : ctx(ctx)
    , server()
    // TODO sort this out properly?
    , staticHandler("/provision", "index.html")
    , fallbackHandler("/", "index.html")
    , wifiHandler(ctx)
    , credentialHandler(*ctx.credentialStore) {
		log.debug("constructor");
	}

ProvisioningServer::~ProvisioningServer() {
    stop();
}

bool ProvisioningServer::start() {
	log.info("Starting ProvisioningServer");

    server.start();

    if (!routesRegistered) {
        log.debug("start() registering routes");
        server.addRoute("/provision/*", &staticHandler);
        server.addRoute("/api/framework/credentials/*", &credentialHandler);
        server.addRoute("/api/framework/wifi/*", &wifiHandler);
        server.addRoute("/*", &fallbackHandler);

        routesRegistered = true;
    }

    return true;
}

void ProvisioningServer::stop() {
    log.debug("Stopping ProvisioningServer");
    server.stop();
}

// handle requests not handled elsewhere
bool ProvisioningServer::handle(http::HttpRequest &req, http::HttpResponse &res) {
    const std::string &path = req.path();
    log.debug("handle");

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