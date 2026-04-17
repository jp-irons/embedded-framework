#include "wifi_manager/RuntimeServer.hpp"

#include "credential_store/CredentialApiHandler.hpp"
#include "device/DeviceApiHandler.hpp"
#include "logger/Logger.hpp"
#include "wifi_manager/WiFiApiHandler.hpp"
#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiStateMachine.hpp"

namespace wifi_manager {

using namespace common;

static logger::Logger log{"RuntimeServer"};

RuntimeServer::RuntimeServer(WiFiContext &ctx, WiFiApiHandler &wifiApi,
                             credential_store::CredentialApiHandler &credentialApi,
                             device::DeviceApiHandler &deviceHandler)
    : ctx(ctx)
    , server()
    , staticHandler("/", "index.html")
    , fallbackHandler("/", "index.html")
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
    log.info("Starting RuntimeServer");

    server.start();

    if (!routesRegistered) {
        log.debug("start() registering routes");
		//
		routes = {
		{ctx.rootUri + "/credentials/", &credentialHandler},
		{ctx.rootUri + "/device/", &deviceHandler},
		{ctx.rootUri + "/wifi/", &wifiHandler},
		{"/", &fallbackHandler}
		};

        server.addRoutes("/*", this);
        routesRegistered = true;
    }

    return true;
}

void RuntimeServer::stop() {
    log.debug("Stopping RuntimeServer");
    server.stop();
}

// handle requests not handled elsewhere
Result RuntimeServer::handle(http::HttpRequest &req, http::HttpResponse &res) {
    log.debug("handle");
    const std::string &path = req.path();
    log.debug("path '%s'", path.c_str());
	
	for (auto& r : routes) {
		log.debug("check route '%s' path '%s'", r.prefix.c_str(), path.c_str());

	    if (path.rfind(r.prefix, 0) == 0) {
			log.debug("matched '%s' to '%s'", r.prefix.c_str(), path.c_str());
	        Result result = r.handler->handle(req, res);
			if (result != Result::NotFound) {
				return result;
			}
	    }
	}
	
	return res.sendNotFound404();
}

} // namespace wifi_manager