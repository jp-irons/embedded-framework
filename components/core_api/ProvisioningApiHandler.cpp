#include "core_api/ProvisioningApiHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

using namespace http;

namespace core_api {

ProvisioningApiHandler::ProvisioningApiHandler(ProvisioningStateMachine& p)
    : provisioning(p)
{
}

bool ProvisioningApiHandler::handle(const HttpRequest& req, HttpResponse& res)
{
    const std::string& path = req.path();

    if (path == "/api/provisioning/status") {
        handleStatus(res);
        return true;
    }
    if (path == "/api/provisioning/start") {
        handleStart(res);
        return true;
    }
    if (path == "/api/provisioning/complete") {
        handleComplete(res);
        return true;
    }

    return false;
}

void ProvisioningApiHandler::handleStatus(HttpResponse& res)
{
    res.jsonStatus("not_implemented");
}

void ProvisioningApiHandler::handleStart(HttpResponse& res)
{
    res.jsonStatus("not_implemented");
}

void ProvisioningApiHandler::handleComplete(HttpResponse& res)
{
    res.jsonStatus("not_implemented");
}

} // namespace core_api