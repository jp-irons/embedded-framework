#include "ApplicationContext.hpp"
#include "wifi_manager/ProvisioningServer.hpp"
#include "wifi_manager/RuntimeServer.hpp"

using namespace wifi_manager;

ApplicationContext::ApplicationContext()
    : creds("wifi_creds")
{
    wifiCtx.creds = &creds;           // <-- critical
	// Create servers
	provisioningServer = new wifi_manager::ProvisioningServer(&wifiCtx);
	runtimeServer      = new wifi_manager::RuntimeServer(&wifiCtx);

	// Wire them into the context
	wifiCtx.provisioning = provisioningServer;
	wifiCtx.runtime      = runtimeServer;

	// Create WiFiManager last
	wifiManager = wifi_manager::create(wifiCtx);

}

ApplicationContext::~ApplicationContext() {
    // Clean up if needed
	delete wifiManager;
	delete provisioningServer;
	delete runtimeServer;
}