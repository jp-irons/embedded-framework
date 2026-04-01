#include "core_api/CredentialApiHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "credential_store/CredentialStore.hpp"

using namespace http;
using namespace credential_store;

namespace core_api {

CredentialApiHandler::CredentialApiHandler(credential_store::CredentialStore& s)
    : store(s)
{
}

bool CredentialApiHandler::handle(const HttpRequest& req, HttpResponse& res)
{
    const std::string& path = req.path();

    if (path == "/api/credentials/list") {
        handleList(res);
        return true;
    }
    if (path == "/api/credentials/submit") {
        handleSubmit(req, res);
        return true;
    }
    if (path == "/api/credentials/delete") {
        handleDelete(req, res);
        return true;
    }
    if (path == "/api/credentials/clear") {
        handleClear(res);
        return true;
    }

    return false;
}

void CredentialApiHandler::handleList(HttpResponse& res)
{
    res.jsonStatus("not_implemented");
}

void CredentialApiHandler::handleSubmit(const HttpRequest& req, HttpResponse& res)
{
    res.jsonStatus("not_implemented");
}

void CredentialApiHandler::handleDelete(const HttpRequest& req, HttpResponse& res)
{
    res.jsonStatus("not_implemented");
}

void CredentialApiHandler::handleClear(HttpResponse& res)
{
    res.jsonStatus("not_implemented");
}

} // namespace core_api