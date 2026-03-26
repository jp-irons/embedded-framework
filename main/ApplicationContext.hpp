#pragma once

#include "WiFiContext.hpp"
#include "WiFiManager.hpp"
#include "CredentialStore.hpp"

class ApplicationContext {
public:
    ApplicationContext();
    ~ApplicationContext();

	credential_store::CredentialStore creds;
    wifi_manager::WiFiContext wifiCtx;
    wifi_manager::WiFiManager* wifiManager = nullptr;
};