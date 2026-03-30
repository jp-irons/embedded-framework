#pragma once

#include "wifi_manager/WiFiContext.hpp"
#include "wifi_manager/WiFiManager.hpp"
#include "credential_store/CredentialStore.hpp"

class ApplicationContext {
public:
    ApplicationContext();
    ~ApplicationContext();

	credential_store::CredentialStore creds;
    wifi_manager::WiFiContext wifiCtx;
    wifi_manager::WiFiManager* wifiManager = nullptr;
	wifi_manager::ProvisioningServer* provisioningServer = nullptr;
	wifi_manager::RuntimeServer* runtimeServer = nullptr;

};