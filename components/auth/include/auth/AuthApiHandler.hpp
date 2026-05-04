#pragma once

#include "auth/AuthStore.hpp"
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
 *                    The middleware exempts this endpoint from the
 *                    requireChangeOnFirstBoot and restrictIfDefault blocks
 *                    so the operator can always reach it.
 */
class AuthApiHandler : public http::HttpHandler {
  public:
    explicit AuthApiHandler(AuthStore &store);
    ~AuthApiHandler() override = default;

    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    static constexpr size_t MIN_PASSWORD_LEN = 8;
    static constexpr size_t MAX_PASSWORD_LEN = 64;

    common::Result handleGet(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handlePost(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleStatus(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleChangePassword(http::HttpRequest &req, http::HttpResponse &res);

    AuthStore &store_;
};

} // namespace auth
