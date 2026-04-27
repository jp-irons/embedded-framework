#include "wifi_manager/ProvisioningServer.hpp"

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

static logger::Logger log{"ProvisioningServer"};

ProvisioningServer::ProvisioningServer(WiFiContext &ctx, WiFiApiHandler &wifiApi,
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

ProvisioningServer::~ProvisioningServer() {
    log.info("destructor");
    stop();
}

bool ProvisioningServer::start() {
    log.debug("Starting ProvisioningServer");
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

	log.debug("ProvisioningServer up");
	wifi_types::IpAddress ip = ctx.wifiInterface->getApIp();
	if (ip.valid) {
	    log.info("ProvisioningServer started on http://%s", ip.value.c_str());
	} else {
	    log.warn("ProvisioningServer started but STA IP unknown");
	}
    return true;
}

void ProvisioningServer::stop() {
    log.debug("Stopping ProvisioningServer");
    server.stop();
}

// handle requests not handled elsewhere
common::Result ProvisioningServer::handle(http::HttpRequest &req, http::HttpResponse &res) {
	    log.debug("handle");
	    const std::string &path = req.path();
	    log.debug("path '%s'", path.c_str());

		std::string effectivePath = path;
		if (path.empty() || path == "/" || path == "/index.html") {
			log.debug("resolving path");
			return res.redirect("/provision/index.html");
		}
		
		for (auto& r : routes) {
			log.debug("check route '%s' uri '%s'", r.prefix.c_str(), effectivePath.c_str());

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