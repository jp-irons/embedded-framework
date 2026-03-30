#include "framework/FrameworkContext.hpp"
#include "credential_store/CredentialStore.hpp"
#include "wifi_manager/ProvisioningStateMachine.hpp"
#include "core_api/CredentialApiHandler.hpp"
#include "core_api/ProvisioningApiHandler.hpp"
#include "core_api/WiFiApiHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

namespace framework {

FrameworkContext* FrameworkContext::instance = nullptr;

FrameworkContext::FrameworkContext()
{
    instance = this;

    //
    // 1. Wire WiFiContext
    //
    wifiCtx.creds = &credentialStore;

    //
    // 2. Create servers (internal to wifi_manager)
    //
    provisioningServer = new wifi_manager::ProvisioningServer(&wifiCtx);
    runtimeServer      = new wifi_manager::RuntimeServer(&wifiCtx);

    wifiCtx.provisioning = provisioningServer;
    wifiCtx.runtime      = runtimeServer;

    //
    // 3. Create WiFiManager last (factory)
    //
    wifiManager = wifi_manager::create(wifiCtx);

    //
    // 4. Framework-level components
    //
    provisioningStateMachine =
        new wifi_manager::ProvisioningStateMachine(*wifiManager, credentialStore);

    credentialApi =
        new core_api::CredentialApiHandler(credentialStore, *provisioningStateMachine);

    provisioningApi =
        new core_api::ProvisioningApiHandler(*provisioningStateMachine);

    wifiApi =
        new core_api::WiFiApiHandler(*wifiManager);
}

FrameworkContext::~FrameworkContext()
{
    stop();

    delete wifiApi;
    delete provisioningApi;
    delete credentialApi;

    delete provisioningStateMachine;

    delete wifiManager;
    delete runtimeServer;
    delete provisioningServer;
}

void FrameworkContext::start()
{
    wifiManager->start();
    httpServer.start();

    httpServer.registerHandler(
        "/api",
        HTTP_GET,
        &FrameworkContext::dispatchTrampoline
    );
}

void FrameworkContext::stop()
{
    httpServer.stop();
}

esp_err_t FrameworkContext::dispatchTrampoline(httpd_req_t* req)
{
    return instance->dispatch(req);
}

esp_err_t FrameworkContext::dispatch(httpd_req_t* req)
{
    http::HttpRequest request(req);
    http::HttpResponse response(req);

    if (credentialApi->handle(request, response)) return ESP_OK;
    if (provisioningApi->handle(request, response)) return ESP_OK;
    if (wifiApi->handle(request, response)) return ESP_OK;

    response.jsonStatus("not_found");
    return ESP_OK;
}

} // namespace framework