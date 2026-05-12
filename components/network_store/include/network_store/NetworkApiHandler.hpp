#pragma once

#include "common/Result.hpp"
#include "http/HttpHandler.hpp"

namespace http {
class HttpRequest;
class HttpResponse;
} // namespace http

namespace network_store {

class NetworkStore;

class NetworkApiHandler : public http::HttpHandler {
  public:
    NetworkApiHandler(network_store::NetworkStore &store);
    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
	common::Result handleDelete(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleGet(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handlePost(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleList(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleSubmit(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleClear(http::HttpRequest &req, http::HttpResponse &res);
    common::Result handleMakeFirst(http::HttpRequest &req, http::HttpResponse &res);

    network_store::NetworkStore &store;
};

} // namespace network_store
