#include "credential_store/CredentialApiHandler.hpp"
#include "credential_store/CredentialStore.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "esp_log.h"

using namespace http;

namespace credential_store {

static const char *TAG = "CredentialApiHandler";

CredentialApiHandler::CredentialApiHandler(CredentialStore &s)
    : store(s) {
    ESP_LOGD(TAG, "constructor");
}

void CredentialApiHandler::handle(http::HttpRequest& req,
                http::HttpResponse& res) {
	ESP_LOGD(TAG, "handle");
    const std::string &path = req.path();

    if (path == "/api/credentials/list") {
        handleList(res);
        return;
    }
    if (path == "/api/credentials/submit") {
        handleSubmit(req, res);
        return;
    }
    if (path == "/api/credentials/delete") {
        handleDelete(req, res);
        return;
    }
    if (path == "/api/credentials/clear") {
        handleClear(res);
        return;
    }
// TODO gotta do somethin better hear
	ESP_LOGD(TAG, "Gotta do somethin better");
    return;
}

void CredentialApiHandler::handleList(HttpResponse &res) {
    res.jsonStatus("not_implemented");
}

void CredentialApiHandler::handleSubmit(const HttpRequest &req, HttpResponse &res) {
    res.jsonStatus("not_implemented");
}

void CredentialApiHandler::handleDelete(const HttpRequest &req, HttpResponse &res) {
    res.jsonStatus("not_implemented");
}

void CredentialApiHandler::handleClear(HttpResponse &res) {
    res.jsonStatus("not_implemented");
}

} // namespace core_api