#include "auth/AuthApiHandler.hpp"

#include "logger/Logger.hpp"

#include "cJSON.h"

#include <string>

namespace auth {

using namespace common;
using namespace http;

static logger::Logger log{AuthApiHandler::TAG};

AuthApiHandler::AuthApiHandler(AuthStore    &store,
                               SessionStore &sessionStore,
                               ApiKeyStore  &apiKeyStore)
    : store_(store)
    , sessionStore_(sessionStore)
    , apiKeyStore_(apiKeyStore) {
    log.debug("constructor");
}

// ---------------------------------------------------------------------------
// Top-level dispatch
// ---------------------------------------------------------------------------

Result AuthApiHandler::handle(HttpRequest &req, HttpResponse &res) {
    log.debug("handle");
    switch (req.method()) {
        case HttpMethod::Get:    return handleGet(req, res);
        case HttpMethod::Post:   return handlePost(req, res);
        case HttpMethod::Delete: return handleDelete(req, res);
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
    if (target == "apikey") {
        return handleApiKeyGet(req, res);
    }
    res.sendJsonError(404, "Not found");
    return Result::Ok;
}

Result AuthApiHandler::handlePost(HttpRequest &req, HttpResponse &res) {
    const std::string target = HttpHandler::extractTarget(req.path());
    if (target == "password") {
        return handleChangePassword(req, res);
    }
    if (target == "login") {
        return handleLogin(req, res);
    }
    if (target == "logout") {
        return handleLogout(req, res);
    }
    if (target == "apikey") {
        return handleApiKeyPost(req, res);
    }
    res.sendJsonError(404, "Not found");
    return Result::Ok;
}

Result AuthApiHandler::handleDelete(HttpRequest &req, HttpResponse &res) {
    const std::string target = HttpHandler::extractTarget(req.path());
    if (target == "apikey") {
        return handleApiKeyDelete(req, res);
    }
    res.sendJsonError(404, "Not found");
    return Result::Ok;
}

// ---------------------------------------------------------------------------
// Individual handlers
// ---------------------------------------------------------------------------

Result AuthApiHandler::handleStatus(HttpRequest &req, HttpResponse &res) {
    log.debug("handleStatus");
    const char *body = store_.isPasswordChanged()
        ? "{\"passwordChanged\":true}"
        : "{\"passwordChanged\":false}";
    res.sendJson(body);
    return Result::Ok;
}

Result AuthApiHandler::handleLogin(HttpRequest &req, HttpResponse &res) {
    log.debug("handleLogin");

    // Credentials are supplied via Basic Auth — the endpoint is exempt from
    // the Bearer-token middleware so EmbeddedServer has NOT verified them yet.
    auto basicAuth = req.extractBasicAuth();
    if (!basicAuth) {
        log.warn("Login attempt with no Basic Auth header");
        res.sendUnauthorized("ESP32");
        return Result::Ok;
    }

    if (!store_.verify(basicAuth->password)) {
        log.warn("Login failed — wrong password");
        res.sendUnauthorized("ESP32");
        return Result::Ok;
    }

    const std::string token = sessionStore_.create();

    // Compose {"token":"<value>"}
    std::string body;
    body.reserve(10 + token.size() + 4);
    body  = "{\"token\":\"";
    body += token;
    body += "\"}";

    log.info("Login successful — session created");
    res.sendJson(body.c_str());
    return Result::Ok;
}

Result AuthApiHandler::handleLogout(HttpRequest &req, HttpResponse &res) {
    log.debug("handleLogout");

    // The Bearer token has already been validated by EmbeddedServer::checkAuth
    // before reaching here.  Extract it again to know which session to drop.
    auto token = req.extractBearerToken();
    if (token) {
        sessionStore_.invalidate(*token);
        log.info("Logout — session invalidated");
    }

    res.sendJsonStatus("ok");
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

    // Invalidate all browser sessions — open tabs must re-login with the
    // new password before they can issue further authenticated requests.
    sessionStore_.invalidateAll();

    log.info("Password changed successfully");
    res.sendJsonStatus("ok");
    return Result::Ok;
}

Result AuthApiHandler::handleApiKeyGet(HttpRequest &req, HttpResponse &res) {
    log.debug("handleApiKeyGet");
    const char *body = apiKeyStore_.isSet()
        ? "{\"isSet\":true}"
        : "{\"isSet\":false}";
    res.sendJson(body);
    return Result::Ok;
}

Result AuthApiHandler::handleApiKeyPost(HttpRequest &req, HttpResponse &res) {
    log.debug("handleApiKeyPost — generating API key");

    const std::string key = apiKeyStore_.generate();
    if (key.empty()) {
        res.sendJsonError(500, "Failed to generate API key");
        return Result::Ok;
    }

    // Compose {"key":"<value>"}
    std::string body;
    body.reserve(8 + key.size() + 3);
    body  = "{\"key\":\"";
    body += key;
    body += "\"}";

    log.info("API key generated");
    res.sendJson(body.c_str());
    return Result::Ok;
}

Result AuthApiHandler::handleApiKeyDelete(HttpRequest &req, HttpResponse &res) {
    log.debug("handleApiKeyDelete");

    Result r = apiKeyStore_.revoke();
    if (r != Result::Ok) {
        log.error("revoke failed (%s)", toString(r));
        res.sendJsonError(500, "Failed to revoke API key");
        return Result::Ok;
    }

    res.sendJsonStatus("ok");
    return Result::Ok;
}

} // namespace auth
