#include "ApplicationContext.hpp"
#include "ProvisioningServer.hpp"
#include "RuntimeServer.hpp"

using namespace wifi_manager;

ApplicationContext::ApplicationContext()
    : creds("wifi_creds") {
    // Wire WiFi subsystem
    wifiManager = create(wifiCtx);
}

ApplicationContext::~ApplicationContext() {
    // Clean up if needed
    delete wifiCtx.provisioning;
    delete wifiCtx.runtime;
    delete wifiManager;
}