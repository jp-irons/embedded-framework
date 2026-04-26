#pragma once

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
    http::HandlerResult handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
	http::HandlerResult handleDelete(http::HttpRequest &req, http::HttpResponse &res);
    http::HandlerResult handleGet(http::HttpRequest &req, http::HttpResponse &res);
    http::HandlerResult handlePost(http::HttpRequest &req, http::HttpResponse &res);
    http::HandlerResult handleList(http::HttpRequest &req, http::HttpResponse &res);
    http::HandlerResult handleSubmit(http::HttpRequest &req, http::HttpResponse &res);
    http::HandlerResult handleClear(http::HttpRequest &req, http::HttpResponse &res);
    http::HandlerResult handleMakeFirst(http::HttpRequest &req, http::HttpResponse &res);

    credential_store::CredentialStore &store;
};

} // namespace credential_store