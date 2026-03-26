#pragma once
#include <string>
#include "esp_http_server.h"

class ApplicationContext; 
class CredentialApiHandler {
public:
    explicit CredentialApiHandler(ApplicationContext& ctx);

    // Returns true if this handler should process the given path
    bool handles(const std::string& path) const;

    // Dispatches the request to the correct method
    esp_err_t handle(httpd_req_t* req);

private:
    ApplicationContext& ctx;

    // Endpoint handlers
    esp_err_t handleList(httpd_req_t* req);
    esp_err_t handleClear(httpd_req_t* req);
    esp_err_t handleDelete(httpd_req_t* req);
    esp_err_t handleSubmit(httpd_req_t* req);

    // Helpers
    std::string getQueryParam(httpd_req_t* req, const char* key);
};