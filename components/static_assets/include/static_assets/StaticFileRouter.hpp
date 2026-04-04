#pragma once

#include <string>
// TODO replace esp_http_server?
#include "esp_http_server.h"
#include "EmbeddedAssetTable.hpp"
#include "ContentType.hpp"

namespace static_assets {

class StaticFileRouter {
public:
    StaticFileRouter(const char* basePath,
                     const char* defaultFile = "/index.html");

    esp_err_t handle(httpd_req_t* req);

private:
    std::string basePath;
    std::string defaultFile;

    esp_err_t serveEmbedded(httpd_req_t* req, const char* path);
    esp_err_t serveFilesystem(httpd_req_t* req, const char* path);
    esp_err_t serveNotFound(httpd_req_t* req);
};

} // namespace static_assets