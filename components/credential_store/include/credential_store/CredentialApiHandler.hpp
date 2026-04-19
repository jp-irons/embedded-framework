#pragma once

#include "common/Result.hpp"
#include "http/HttpHandler.hpp"
namespace http {
class HttpRequest;
class HttpResponse;
} // namespace http

namespace credential_store {

class CredentialStore;

class CredentialApiHandler : public http::HttpHandler {
  public:
    CredentialApiHandler(credential_store::CredentialStore &store);
    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    common::Result handleList(http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleSubmit(http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleClear(http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleDelete(http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleMakeFirst(http::HttpRequest& req, http::HttpResponse& res);

    credential_store::CredentialStore &store;
};

} // namespace credential_store