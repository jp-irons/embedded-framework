#pragma once

class ProvisioningStateMachine;

namespace http {
    class HttpRequest;
    class HttpResponse;
}

using namespace http;


namespace core_api {

class ProvisioningApiHandler {
public:
    explicit ProvisioningApiHandler(ProvisioningStateMachine& provisioning);

    bool handle(const HttpRequest& req, HttpResponse& res);

private:
    void handleStatus(HttpResponse& res);
    void handleStart(HttpResponse& res);
    void handleComplete(HttpResponse& res);

    ProvisioningStateMachine& provisioning;
};

} // namespace core_api