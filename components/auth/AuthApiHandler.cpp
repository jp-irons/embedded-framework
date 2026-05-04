#include "auth/AuthApiHandler.hpp"

#include "logger/Logger.hpp"

#include "cJSON.h"

#include <string>

namespace auth {

using namespace common;
using namespace http;

static logger::Logger log{"AuthApiHandler"};

AuthApiHandler::AuthApiHandler(AuthStore &store)
    : store_(store) {
    log.debug("constructor");
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

Result AuthApiHandler::handle(HttpRequest &req, HttpResponse &res) {
    log.debug("handle");
    switch (req.method()) {
        case HttpMethod::Get:
            return handleGet(req, res);
        case HttpMethod::Post:
            return handlePost(req, res);
        default:
            res.sendJsonError(405, "Method not allowed");
            return Result::Ok;
    }
}

Result AuthApiHandler::handleGet(HttpRequest &req, HttpResponse &res) {
    const std::string target = HttpHandler::extractTarget(req.path());
    if (target == "status") {
        return handleStatus(req, res);
    }
    res.sendJsonError(404, "Not found");
    return Result::Ok;
}

Result AuthApiHandler::handlePost(HttpRequest &req, HttpResponse &res) {
    const std::string target = HttpHandler::extractTarget(req.path());
    if (target == "password") {
        return handleChangePassword(req, res);
    }
    res.sendJsonError(404, "Not found");
    return Result::Ok;
}

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

Result AuthApiHandler::handleStatus(HttpRequest &req, HttpResponse &res) {
    log.debug("handleStatus");
    const char *body = store_.isPasswordChanged()
        ? "{\"passwordChanged\":true}"
        : "{\"passwordChanged\":false}";
    res.sendJson(body);
    return Result::Ok;
}

Result AuthApiHandler::handleChangePassword(HttpRequest &req, HttpResponse &res) {
    log.debug("handleChangePassword");

    // ── Parse body ────────────────────────────────────────────────────────
    const auto bodyView = req.body();
    if (bodyView.empty()) {
        res.sendJsonError(400, "Request body is required");
        return Result::Ok;
    }

    // cJSON requires a null-terminated string
    std::string bodyStr(bodyView);
    cJSON *root = cJSON_Parse(bodyStr.c_str());
    if (!root) {
        res.sendJsonError(400, "Invalid JSON");
        return Result::Ok;
    }

    const cJSON *newPasswordItem = cJSON_GetObjectItemCaseSensitive(root, "newPassword");
    if (!cJSON_IsString(newPasswordItem) || !newPasswordItem->valuestring) {
        cJSON_Delete(root);
        res.sendJsonError(400, "newPassword field is required");
        return Result::Ok;
    }

    std::string newPassword(newPasswordItem->valuestring);
    cJSON_Delete(root);

    // ── Validate ──────────────────────────────────────────────────────────
    if (newPassword.size() < MIN_PASSWORD_LEN) {
        res.sendJsonError(400, "Password must be at least 8 characters");
        return Result::Ok;
    }
    if (newPassword.size() > MAX_PASSWORD_LEN) {
        res.sendJsonError(400, "Password must be 64 characters or fewer");
        return Result::Ok;
    }

    // ── Persist ───────────────────────────────────────────────────────────
    Result r = store_.changePassword(newPassword);
    if (r != Result::Ok) {
        log.error("changePassword failed (%s)", toString(r));
        res.sendJsonError(500, "Failed to save new password");
        return Result::Ok;
    }

    log.info("Password changed successfully");
    res.sendJsonStatus("ok");
    return Result::Ok;
}

} // namespace auth
