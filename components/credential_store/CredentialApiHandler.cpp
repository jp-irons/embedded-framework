#include "credential_store/CredentialApiHandler.hpp"

#include "cJSON.h"
#include "common/Result.hpp"
#include "credential_store/CredentialStore.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "logger/Logger.hpp"

#include <cJSON.h>

namespace credential_store {

using namespace http;
using namespace common;
using namespace wifi_types;

static logger::Logger log{"CredentialApiHandler"};

CredentialApiHandler::CredentialApiHandler(CredentialStore &s)
    : store(s) {
    log.debug("constructor");
}

Result CredentialApiHandler::handle(http::HttpRequest &req, http::HttpResponse &res) {
    log.debug("handle");
    const std::string &path = req.path();
    std::string action = extractAction(req.path());
    if (req.method() == HttpMethod::Delete) {
        // DELETE /credentials/{ssid}
        return handleDelete(action, res);
    }
	
    log.debug("action '%s'", action.c_str());
    if (action == "list") {
        log.debug("handleList");
        return handleList(res);
    }
    if (action == "submit") {
        log.info("handleSubmit");
        return handleSubmit(req, res);
    }
    if (action == "clear") {
        log.info("handleClear");
        return handleClear(res);
    }
    if (action == "clearNvs") {
        log.info("handleClearNvs");
        return handleClearNvs(res);
    }
    log.error("handle action '%s' unsupported", action.c_str());
    return res.sendJsonError(403, "Unsupported");
}

std::string CredentialApiHandler::extractAction(const char *uri) {
    std::string path(uri);
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos || pos == path.length() - 1) {
        return {}; // no action found
    }
    return path.substr(pos + 1);
}

Result CredentialApiHandler::handleList(HttpResponse &res) {
    std::vector<WiFiCredential> entries;

    store.loadAllSortedByPriority(entries); // <-- correct API usage

    // Create the top-level JSON array
    cJSON *root = cJSON_CreateArray();
    if (!root) {
        return Result::InternalError;
    }

    for (const auto &c : entries) {
        cJSON *obj = cJSON_CreateObject();
        if (!obj) {
            cJSON_Delete(root);
            return Result::InternalError;
        }

        // Add fields
        cJSON_AddStringToObject(obj, "ssid", c.ssid.c_str());
        cJSON_AddNumberToObject(obj, "priority", c.priority);

        // Append to array
        cJSON_AddItemToArray(root, obj);
    }

    // Convert to string (pretty = false)
    char *json_response = cJSON_PrintUnformatted(root);

    // Free the cJSON tree; the printed string is independent
    cJSON_Delete(root);
    if (!json_response) {
        return Result::InternalError;
    }
    Result result = res.sendJson(json_response);
    cJSON_free(json_response);
    return result;
}

Result CredentialApiHandler::handleSubmit(const HttpRequest &req, HttpResponse &res) {
    // Read body from your HttpRequest abstraction
    std::string_view body = req.body();

    if (body.empty()) {
		log.error("Empty body");
        return res.sendBadRequest400("Empty body");
    }

    cJSON *root = cJSON_Parse(body.data());
    if (!root) {
		log.error("Invalid JSON");
        return res.sendBadRequest400("Invalid JSON");
    }

    // Required: ssid
    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    if (!ssid || !cJSON_IsString(ssid)) {
        cJSON_Delete(root);
		log.error("Missing ssid");
        return res.sendBadRequest400("Missing ssid");
    }

    WiFiCredential entry;
    entry.ssid = ssid->valuestring;

    // Optional: password
    cJSON *password = cJSON_GetObjectItem(root, "password");
    entry.password = (password && cJSON_IsString(password)) ? password->valuestring : "";

    // Optional: priority
    cJSON *priority = cJSON_GetObjectItem(root, "priority");
    entry.priority = (priority && cJSON_IsNumber(priority)) ? priority->valueint : 0;

    std::vector<WiFiCredential> entries;
    store.loadAll(entries);

    bool replaced = false;
    for (auto &e : entries) {
        if (e.ssid == entry.ssid) {
            e = entry;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        entries.push_back(entry);
    }

    Result r = store.saveAll(entries);
    cJSON_Delete(root);

    if (Result::Ok != r) {
		log.error("Credential not saved");
        return res.sendInternalError500("Credential not saved");
    }

    r = res.sendJson("{\"status\":\"ok\"}");
    return r;
}

Result CredentialApiHandler::handleDelete(std::string ssid, http::HttpResponse &res) {
    log.debug("delete ssid '%s'", ssid.c_str());
	Result result = store.erase(ssid);
	if (result != common::Result::Ok) {
        return res.sendJsonError(404, std::string("error ") + common::toString(result) + " deleting " + ssid);
    }
	return res.sendJson(ssid + " deleted");
}

Result CredentialApiHandler::handleClear(HttpResponse &res) {
    log.info("Clearing all credentials");
    store.clear();
    Result r = res.sendJsonOk("credentials cleared");
    return r;
}

Result CredentialApiHandler::handleClearNvs(HttpResponse &res) {
    res.sendJsonError(404, "not_implemented");
	return res.sendJsonError(404, "not_implemented");
}

} // namespace credential_store