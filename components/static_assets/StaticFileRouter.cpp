#include "static_assets/StaticFileRouter.hpp"
#include "esp_log.h"
#include <cstring>

using namespace static_assets;

namespace static_assets {

static const char* TAG = "StaticFileRouter";

StaticFileRouter::StaticFileRouter(const char* basePath,
                                   const char* defaultFile)
    : basePath(basePath),
      defaultFile(defaultFile)
{
    // Ensure basePath always starts with '/'
    if (!this->basePath.empty() && this->basePath[0] != '/') {
        this->basePath.insert(0, "/");
    }

    // Ensure defaultFile always starts with '/'
    if (!this->defaultFile.empty() && this->defaultFile[0] != '/') {
        this->defaultFile.insert(0, "/");
    }
}

esp_err_t StaticFileRouter::handle(httpd_req_t* req)
{
	ESP_LOGD(TAG, "handle");
    const char* uri = req->uri;
    const char* path = uri;

    // Strip base path prefix if present
    if (!basePath.empty() &&
        strncmp(uri, basePath.c_str(), basePath.size()) == 0)
    {
        path = uri + basePath.size();
    }

    // If the stripped path is empty or "/", serve default file
    if (strcmp(path, "") == 0 || strcmp(path, "/") == 0) {
        path = defaultFile.c_str();
    }

    ESP_LOGD(TAG, "Resolved path: %s", path);

    // 1. Try embedded assets
    if (serveEmbedded(req, path) == ESP_OK) {
        return ESP_OK;
    }

//    // 2. Try filesystem (optional)
//    if (serveFilesystem(req, path) == ESP_OK) {
//        return ESP_OK;
//    }
//	
	ESP_LOGD(TAG, "Asset '%s' not found", path);


    // 3. Fallback
    return serveNotFound(req);
}

esp_err_t StaticFileRouter::serveEmbedded(httpd_req_t* req, const char* path)
{
	ESP_LOGD(TAG, "serveEmbedded");
    size_t size = 0;
    const uint8_t* data = EmbeddedAssetTable::find(path, size);

    if (!data) {
        return ESP_FAIL;
    }

    const char* type = ContentType::fromPath(path);
    httpd_resp_set_type(req, type);

    return httpd_resp_send(req, reinterpret_cast<const char*>(data), size);
}

esp_err_t StaticFileRouter::serveFilesystem(httpd_req_t* req, const char* path)
{
	ESP_LOGD(TAG, "serveFilesystem");
    // Optional future extension: SPIFFS/LittleFS
    return ESP_FAIL;
}

esp_err_t StaticFileRouter::serveNotFound(httpd_req_t* req)
{
    const char* msg = "404 Not Found";
    httpd_resp_set_status(req, "404 NOT FOUND");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, msg, strlen(msg));
}

} // namespace static_assets