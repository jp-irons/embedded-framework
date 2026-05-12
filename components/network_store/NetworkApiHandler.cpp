#include "network_store/NetworkApiHandler.hpp"

#include "cJSON.h"
#include "common/Result.hpp"
#include "network_store/NetworkStore.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "logger/Logger.hpp"

#include <cJSON.h>

namespace network_store {

using namespace http;
using namespace common;

static logger::Logger log{"NetworkApiHandler"};

NetworkApiHandler::NetworkApiHandler(NetworkStore &s)
    : store(s) {
    log.debug("constructor");
}

common::Result NetworkApiHandler::handle(http::HttpRequest &req, http::HttpResponse &res) {
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

common::Result NetworkApiHandler::handleGet(http::HttpRequest &req, http::HttpResponse &res) {
    const std::string &path = req.path();
    std::string target = extractTarget(req.path());
    log.debug("target '%s'", target.c_str());
    if (target == "list") {
        return handleList(req, res);
    }
    log.error("handle action '%s' unsupported", target.c_str());
    return res.sendJsonError(501, "handleGet '" + target + "' unsupported");
}

common::Result NetworkApiHandler::handlePost(http::HttpRequest &req, http::HttpResponse &res) {
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

common::Result NetworkApiHandler::handleList(http::HttpRequest& req, http::HttpResponse& res) {
	log.debug("handleList");
    std::vector<WiFiNetwork> entries;

    store.loadAllSortedByPriority(entries);

    // Create the top-level JSON array
    cJSON *root = cJSON_CreateArray();
    if (!root) {
		log.error("Internal error loading networks");
		return res.sendJsonError(500, "Internal error loading networks");
    }

    for (const auto &c : entries) {
		log.debug("List: ssid=%s priority=%d", c.ssid.c_str(), c.priority);
        cJSON *obj = cJSON_CreateObject();
        if (!obj) {
            cJSON_Delete(root);
			log.error("Internal error creating network list");
			return res.sendJsonError(500, "Internal error creating network list");
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
		log.error("Internal error preparing network list response");
		return res.sendJsonError(500, "Internal error preparing network list response");
    }
    common::Result r = res.sendJson(json_response);
    cJSON_free(json_response);
    return r;
}

common::Result NetworkApiHandler::handleSubmit(http::HttpRequest& req, http::HttpResponse& res) {
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

    WiFiNetwork entry;
    entry.ssid = ssid->valuestring;

    // Optional: password
    cJSON *password = cJSON_GetObjectItem(root, "password");
    entry.password = (password && cJSON_IsString(password)) ? password->valuestring : "";

    // Optional: priority
    cJSON *priority = cJSON_GetObjectItem(root, "priority");
    entry.priority = (priority && cJSON_IsNumber(priority)) ? priority->valueint : 0;

    std::vector<WiFiNetwork> entries;
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
        log.error("Network not saved");
        return res.sendJsonError(500, "Network not saved");
    }
    return res.sendJson("{\"status\":\"ok\"}");
}

common::Result NetworkApiHandler::handleDelete(http::HttpRequest& req, http::HttpResponse& res) {
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

common::Result NetworkApiHandler::handleClear(http::HttpRequest& req, http::HttpResponse& res) {
    log.info("Clearing all networks");
    Result r = store.clear();
    if (r != common::Result::Ok) {
        return res.sendJsonError(404, std::string("error ") + common::toString(r) + " clearing ");
    }
    return res.sendJson("Networks cleared");
}

common::Result NetworkApiHandler::handleMakeFirst(http::HttpRequest& req, http::HttpResponse& res) {
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

    // Promote network
    Result r = store.makeFirst(ssid);
    cJSON_Delete(root);

    if (r != common::Result::Ok) {
		log.error(("Error promoting " + ssid).c_str());
        return res.sendJsonError(
            404,
            std::string("error ") + common::toString(r) + " promoting " + ssid
        );
    }
    return res.sendJson("Network for " + ssid + " promoted");
}

} // namespace network_store
