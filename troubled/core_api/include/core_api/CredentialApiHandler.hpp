#pragma once

namespace credential_store {
class CredentialStore;
}

namespace http {
class HttpRequest;
class HttpResponse;
} // namespace http

namespace core_api {

class CredentialApiHandler {
  public:
    CredentialApiHandler(credential_store::CredentialStore &store);
    bool handle(const http::HttpRequest &req, http::HttpResponse &res);

  private:
    void handleList(http::HttpResponse &res);
    void handleSubmit(const http::HttpRequest &req, http::HttpResponse &res);
    void handleDelete(const http::HttpRequest &req, http::HttpResponse &res);
    void handleClear(http::HttpResponse &res);

    credential_store::CredentialStore &store;
};

} // namespace core_api