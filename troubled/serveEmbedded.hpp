#pragma once

#include <esp_http_server.h>
#include <string>

esp_err_t serveEmbedded(httpd_req_t* req, const char* path);
