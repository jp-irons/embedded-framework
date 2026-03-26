#include "serveEmbedded.hpp"
#include "EmbeddedFiles.hpp"

#include "esp_log.h"

#include <string>

static const char* TAG = "serveEmbedded";

esp_err_t serveEmbedded(httpd_req_t* req, const char* basePath)
{
    ESP_LOGD(TAG, "URI to serve: %s - base path: %s", req->uri, basePath);

    std::string path = basePath;

    // Ensure basePath ends with '/'
    if (!path.empty() && path.back() != '/')
        path.push_back('/');

    // Append request URI (skip leading '/')
    const char* uri = req->uri;
    if (uri[0] == '/')
        uri++;

    path += uri;

    // Determine if the request looks like a directory
    bool isDirectory = false;

    // Case 1: URI is empty after stripping leading slash
    if (uri[0] == '\0')
        isDirectory = true;

    // Case 2: Path ends with '/'
    else if (path.back() == '/')
        isDirectory = true;

    // Case 3: No '.' in the last path segment → treat as directory
    else if (strchr(uri, '.') == nullptr)
        isDirectory = true;

    if (isDirectory) {
        if (path.back() != '/')
            path.push_back('/');
        path += "index.html";
    }


    ESP_LOGD(TAG, "Full static file path: %s", path.c_str());

    const EmbeddedFile* file = getEmbeddedFile(path);
    if (!file) {
        ESP_LOGW(TAG, "NOT FOUND for path: %s", path.c_str());
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_OK;
    }

    httpd_resp_set_type(req, file->mime);

    return httpd_resp_send(
        req,
        reinterpret_cast<const char*>(file->start),
        file->size
    );
}