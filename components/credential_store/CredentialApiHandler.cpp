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

static logger::Logger log{"CredentialApiHandler"};

CredentialApiHandler::CredentialApiHandler(CredentialStore &s)
    : store(s) {
    log.debug("constructor");
}

common::Result CredentialApiHandler::handle(http::HttpRequest &req, http::HttpResponse &res) {
	log.debug("handle");
	HttpMethod method = req.method();
	switch (method) {
		case HttpMethod::Get:
			return handleGet(req, res);
		case HttpMethod::Post:
			return handlePost(req, res);
		case HttpMethod::Delete:
			return handleDelete(req, res);
		default:
			return res.sendJsonError(405, std::string("Method ") + toString(method) + " not allowed");
	}
}

common::Result CredentialApiHandler::handleGet(http::HttpRequest &req, http::HttpResponse &res) {
    const std::string &path = req.path();
    std::string target = extractTarget(req.path());
    log.debug("target '%s'", target.c_str());
    if (target == "list") {
        return handleList(req, res);
    }
    log.error("handle action '%s' unsupported", target.c_str());
    return res.sendJsonError(501, "handleGet '" + target + "' unsupported");
}

common::Result CredentialApiHandler::handlePost(http::HttpRequest &req, http::HttpResponse &res) {
    const std::string &path = req.path();
    std::string target = extractTarget(req.path());
    log.debug("action '%s'", target.c_str());
    if (target == "submit") {
        return handleSubmit(req, res);
    }
    if (target == "clear") {
        return handleClear(req, res);
    }
    if (target == "makeFirst") {
        return handleMakeFirst(req, res);
    }

    log.error("handle target '%s' unsupported", target.c_str());
    return res.sendJsonError(403, "handlePost '" + target + "' unsupported");
}

common::Result CredentialApiHandler::handleList(http::HttpRequest& req, http::HttpResponse& res) {
	log.debug("handleList");
    std::vector<WiFiCredential> entries;

    store.loadAllSortedByPriority(entries); // <-- correct API usage

    // Create the top-level JSON array
    cJSON *root = cJSON_CreateArray();
    if (!root) {
		log.error("Internal error loading credentials");
		return res.sendJsonError(500, "Internal error loading credentials");
    }

    for (const auto &c : entries) {
		log.debug("List: ssid=%s priority=%d", c.ssid.c_str(), c.priority);
        cJSON *obj = cJSON_CreateObject();
        if (!obj) {
            cJSON_Delete(root);
			log.error("Internal error creating credential list");
			return res.sendJsonError(500, "Internal error creating credential list");
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
		log.error("Internal error preparing credential list response");
		return res.sendJsonError(500, "Internal error preparing credential list response");
    }
    common::Result r = res.sendJson(json_response);
    cJSON_free(json_response);
    return r;
}

common::Result CredentialApiHandler::handleSubmit(http::HttpRequest& req, http::HttpResponse& res) {
    log.debug("handleSubmit");
	// Read body from your HttpRequest abstraction
    std::string_view body = req.body();

    if (body.empty()) {
		log.error("Empty body");
		return res.sendJsonError(400, "Empty body");
    }

    cJSON *root = cJSON_Parse(body.data());
    if (!root) {
        log.error("Invalid JSON");
        return res.sendJsonError(400, "Invalid JSON");
    }

    // Required: ssid
    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    if (!ssid || !cJSON_IsString(ssid)) {
        cJSON_Delete(root);
        log.error("Missing ssid");
        return res.sendJsonError(400, "Missing ssid");
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
    store.loadAllSortedByPriority(entries);

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
        return res.sendJsonError(500, "Credential not saved");
    }
    return res.sendJson("{\"status\":\"ok\"}");
}

common::Result CredentialApiHandler::handleDelete(http::HttpRequest& req, http::HttpResponse& res) {
	log.debug("handleDelete");
	const std::string &path = req.path();
	std::string ssid = extractTarget(req.path());
    log.debug("delete ssid '%s'", ssid.c_str());
    Result result = store.erase(ssid);
    if (result != common::Result::Ok) {
        return res.sendJsonError(404, std::string("error ") + common::toString(result) + " deleting " + ssid);
    }
    return res.sendJson(ssid + " deleted");
}

common::Result CredentialApiHandler::handleClear(http::HttpRequest& req, http::HttpResponse& res) {
    log.info("Clearing all credentials");
    Result r = store.clear();
    if (r != common::Result::Ok) {
        return res.sendJsonError(404, std::string("error ") + common::toString(r) + " clearing ");
    }
    return res.sendJson("Credentials cleared");
}

common::Result CredentialApiHandler::handleMakeFirst(http::HttpRequest& req, http::HttpResponse& res) {
    // Parse JSON body
    cJSON* root = cJSON_Parse(req.body().data());
    if (!root) {
		log.error("Invalid JSON");
        return res.sendJsonError(400, "Invalid JSON");
    }

    cJSON* ssidItem = cJSON_GetObjectItem(root, "ssid");
    if (!ssidItem || !cJSON_IsString(ssidItem)) {
        cJSON_Delete(root);
		log.error("Missing ssid");
        return res.sendJsonError(400, "Missing ssid");
    }

    std::string ssid = ssidItem->valuestring;

    // Promote credential
    Result r = store.makeFirst(ssid);
    cJSON_Delete(root);

    if (r != common::Result::Ok) {
		log.error(("Error promoting " + ssid).c_str());
        return res.sendJsonError(
            404,
            std::string("error ") + common::toString(r) + " promoting " + ssid
        );
    }
    return res.sendJson("Credential for " + ssid + " promoted");
}

} // namespace credential_store