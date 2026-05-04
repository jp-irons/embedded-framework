#pragma once
#include "common/Result.hpp"
#include "esp_http_server.h"

#include <string_view>

namespace http {

class HttpResponse {
  public:
    explicit HttpResponse(httpd_req *r);

    /* 
	 "200 Ok"
	 "301 Moved Permanently"
	 "302 Found"
	 "400 Bad Request"
	 "403 Forbidden"
	 "404 Not Found"
	 "405 Method Not allowed"
	 "500 Internal Server Error"
	 "501 Not Implemented"
	*/
    common::Result send(int code, std::string_view body, const char *type);

    common::Result send(std::string_view body, const char *type);

    common::Result redirect(const char *target);

    common::Result send(const unsigned char *data, unsigned int size, const char *type);

    common::Result send(std::string_view body);

    common::Result sendText(std::string_view body);

    common::Result sendJson(std::string_view body);

    /* 
	 "200 Ok"
	 "301 Moved Permanently"
	 "302 Found"
	 "400 Bad Request"
	 "403 Forbidden"
	 "404 Not Found"
	 "405 Method Not allowed"
	 "500 Internal Server Error"
	 "501 Not Implemented"
	*/
    common::Result sendJson(int code, std::string_view body);

    /* 
 "200 Ok"
 "301 Moved Permanently"
 "302 Found"
 "400 Bad Request"
 "403 Forbidden"
 "404 Not Found"
 "405 Method Not allowed"
 "500 Internal Server Error"
 "501 Not Implemented"
*/
    common::Result sendJsonError(int code, std::string_view message);

    common::Result sendJsonStatus(std::string_view status);

    /**
     * Sends HTTP 401 Unauthorized with a WWW-Authenticate Basic challenge.
     * The browser will show its built-in login dialog in response.
     *
     * @param realm  Shown in the browser login dialog (e.g. "ESP32").
     */
    common::Result sendUnauthorized(const char *realm);

  private:
    httpd_req *req;
    void warn_err(esp_err_t err);
};

} // namespace http