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
    void handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    void handleList(http::HttpResponse &res);
    void handleSubmit(const http::HttpRequest &req, http::HttpResponse &res);
    void handleDelete(const http::HttpRequest &req, http::HttpResponse &res);
    void handleClear(http::HttpResponse &res);

    credential_store::CredentialStore &store;
};

} // namespace credential_store