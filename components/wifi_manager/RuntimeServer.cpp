#include "wifi_manager/RuntimeServer.hpp"

#include "credential_store/CredentialApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "logger/Logger.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiInterface.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"
#include "wifi_types/WiFiTypes.hpp"

namespace wifi_manager {

using namespace http;

static logger::Logger log{"RuntimeServer"};

RuntimeServer::RuntimeServer(WiFiContext &ctx, WiFiApiHandler &wifiApi,
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

RuntimeServer::~RuntimeServer() {
    log.info("destructor");
    stop();
}

bool RuntimeServer::start() {
    log.debug("Starting RuntimeServer");
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

	log.debug("RuntimeServer up");
	wifi_types::IpAddress ip = ctx.wifiInterface->getStaIp();
	if (ip.valid) {
	    log.info("RuntimeServer started on http://%s", ip.value.c_str());
	} else {
	    log.warn("RuntimeServer started but STA IP unknown");
	}
    return true;
}

void RuntimeServer::stop() {
    log.debug("Stopping RuntimeServer");
    server.stop();
}

// handle requests not handled elsewhere
HandlerResult RuntimeServer::handle(http::HttpRequest &req, http::HttpResponse &res) {
    log.debug("handle");
    const std::string &path = req.path();
    log.debug("path '%s'", path.c_str());

	std::string effectivePath = path;
	if (path.empty() || path == "/" || path == "/index.html") {
		log.debug("resolving path");
		return res.redirect("/runtime/index.html");
	}
	
	for (auto& r : routes) {
		log.debug("check route '%s' uri '%s'", r.prefix.c_str(), effectivePath.c_str());

	    if (effectivePath.rfind(r.prefix, 0) == 0) {
			log.debug("matched '%s' to '%s'", r.prefix.c_str(), path.c_str());
	        HandlerResult result = r.handler->handle(req, res);
			if (result != HandlerResult::NotFound) {
				return result;
			}
	    }
	}
	
	return res.sendJsonError(404, "'" + path + "' not found");
}

} // namespace wifi_manager