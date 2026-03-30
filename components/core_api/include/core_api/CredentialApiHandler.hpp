#pragma once

class CredentialStore;
class ProvisioningStateMachine;

namespace http {
    class HttpRequest;
    class HttpResponse;
}

using namespace http;

namespace core_api {

class CredentialApiHandler {
public:
    CredentialApiHandler(CredentialStore& store,
                         ProvisioningStateMachine& provisioning);

    bool handle(const HttpRequest& req, HttpResponse& res);

private:
    void handleList(HttpResponse& res);
    void handleSubmit(const HttpRequest& req, HttpResponse& res);
    void handleDelete(const HttpRequest& req, HttpResponse& res);
    void handleClear(HttpResponse& res);

    CredentialStore& store;
    ProvisioningStateMachine& provisioning;
};

} // namespace core_api