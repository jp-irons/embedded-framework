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
    common::Result handleList(http::HttpResponse &res);
    common::Result handleSubmit(const http::HttpRequest &req, http::HttpResponse &res);
	common::Result handleClear(http::HttpResponse &res);
	common::Result handleClearNvs(http::HttpResponse &res);
    common::Result handleDelete(std::string ssid, http::HttpResponse &res);
	std::string extractAction(const char* uri);

    credential_store::CredentialStore &store;
};

} // namespace credential_store