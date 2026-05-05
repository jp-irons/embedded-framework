#pragma once

#include "auth/ApiKeyStore.hpp"
#include "auth/AuthStore.hpp"
#include "auth/SessionStore.hpp"
#include "common/Result.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

namespace auth {

/**
 * HTTP handler for authentication management endpoints.
 *
 * Routes (relative to rootUri/auth/):
 *
 *   GET  status    — returns {"passwordChanged": bool}
 *                    Allows the UI to show a "change your password" prompt.
 *
 *   POST password  — change the API password.
 *                    Body: {"newPassword": "<value>"}
 *                    Constraints: 8–64 characters.
 *                    Also invalidates all active sessions so open browser tabs
 *                    must re-authenticate with the new password.
 *
 *   POST login     — exchange Basic Auth credentials for a session token.
 *                    EXEMPT from Bearer-token auth check (EmbeddedServer).
 *                    Body: none.  Credentials via Authorization: Basic header.
 *                    Response: {"token": "<64-hex>"}
 *
 *   POST logout    — invalidate the caller's session token.
 *                    Credentials via Authorization: Bearer header.
 *                    Response: {"status": "ok"}
 *
 *   GET  apikey    — returns {"isSet": bool}
 *
 *   POST apikey    — generate (or rotate) the device API key.
 *                    Response: {"key": "<64-hex>"}
 *                    The key is shown once here; it is NOT stored in plaintext
 *                    anywhere else.
 *
 *   DELETE apikey  — revoke the current API key.
 *                    Response: {"status": "ok"}
 */
class AuthApiHandler : public http::HttpHandler {
  public:
    explicit AuthApiHandler(AuthStore    &store,
                            SessionStore &sessionStore,
                            ApiKeyStore  &apiKeyStore);
    ~AuthApiHandler() override = default;

    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    static constexpr size_t MIN_PASSWORD_LEN = 8;
    static constexpr size_t MAX_PASSWORD_LEN = 64;

    common::Result handleGet   (http::HttpRequest &req, http::HttpResponse &res);
    common::Result handlePost  (http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleDelete(http::HttpRequest &req, http::HttpResponse &res);

    common::Result handleStatus        (http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleLogin         (http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleLogout        (http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleChangePassword(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleApiKeyGet     (http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleApiKeyPost    (http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleApiKeyDelete  (http::HttpRequest &req, http::HttpResponse &res);

    AuthStore    &store_;
    SessionStore &sessionStore_;
    ApiKeyStore  &apiKeyStore_;
};

} // namespace auth
