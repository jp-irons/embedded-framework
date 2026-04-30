#include "wifi_manager/EmbeddedServer.hpp"

#include "common/Result.hpp"
#include "credential_store/CredentialApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "logger/Logger.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiInterface.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"
#include "wifi_manager/WiFiTypes.hpp"

namespace wifi_manager {

static logger::Logger log{"EmbeddedServer"};

EmbeddedServer::EmbeddedServer(WiFiContext &ctx, WiFiApiHandler &wifiApi,
                             credential_store::CredentialApiHandler &credentialApi,
                             device::DeviceApiHandler &deviceHandler)
    : ctx(ctx)
    , server()
    , embeddedFileHandler("/", "index.html")
    , wifiHandler(wifiApi)
    , credentialHandler(credentialApi)
    , deviceHandler(deviceHandler) {
    log.debug("constructor");
}

EmbeddedServer::~EmbeddedServer() {
    log.info("destructor");
    stop();
}

bool EmbeddedServer::start() {
    log.debug("Starting EmbeddedServer");
    server.start();

    if (!routesRegistered) {
        log.debug("start() registering routes");
		routes = {
		{ctx.rootUri + "/credentials/", &credentialHandler},
		{ctx.rootUri + "/device/", &deviceHandler},
		{ctx.rootUri + "/wifi/", &wifiHandler},
		{"/", &embeddedFileHandler}
		};
        server.addRoutes("/*", this);
        routesRegistered = true;
    }

	log.debug("EmbeddedServer up");
	IpAddress ip = ctx.wifiInterface->getStaIp();
	if (ip.valid) {
	    log.info("EmbeddedServer started on http://%s", ip.value.c_str());
	} else {
	    log.warn("EmbeddedServer started but STA IP unknown");
	}
    return true;
}

void EmbeddedServer::stop() {
    log.debug("Stopping EmbeddedServer");
    server.stop();
}

void EmbeddedServer::startRuntimeMode() {
    log.debug("startRuntimeMode (stub)");
}

void EmbeddedServer::startProvisioningMode() {
    log.debug("startProvisioningMode (stub)");
}

// handle requests not handled elsewhere
common::Result EmbeddedServer::handle(http::HttpRequest &req, http::HttpResponse &res) {
    log.debug("handle");
    const std::string &path = req.path();
    log.debug("path '%s'", path.c_str());

	std::string effectivePath = path;
	if (path.empty() || path == "/" || path == "/index.html") {
		log.debug("resolving path");
		return res.redirect("/runtime/index.html");
	}
	
	for (auto& r : routes) {
//		log.debug("check route '%s' uri '%s'", r.prefix.c_str(), effectivePath.c_str());

	    if (effectivePath.rfind(r.prefix, 0) == 0) {
			log.debug("matched '%s' to '%s'", r.prefix.c_str(), path.c_str());
	        common::Result result = r.handler->handle(req, res);
			if (result != common::Result::NotFound) {
				return result;
			}
	    }
	}
	
	return common::Result::NotFound;
}

} // namespace wifi_manager