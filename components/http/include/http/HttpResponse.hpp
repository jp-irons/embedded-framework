#pragma once
#include "esp_http_server.h"
#include "http/HttpTypes.hpp"

#include <string_view>

// TODO move method defns back from header to cpp
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
    HandlerResult send(int code, std::string_view body, const char *type);

    HandlerResult send(std::string_view body, const char *type);

    HandlerResult redirect(const char *target);

    HandlerResult send(const unsigned char *data, unsigned int size, const char *type);

    HandlerResult send(std::string_view body);

    //    void setType(const char *type);

    HandlerResult sendText(std::string_view body);

    HandlerResult sendJson(std::string_view body);

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
    HandlerResult sendJson(int code, std::string_view body);

    //    HandlerResult sendJsonOk(std::string_view message = "Ok");
    //
    //    HandlerResult sendJsonResult(Result r);
    //
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
    HandlerResult sendJsonError(int code, std::string_view message);

    HandlerResult sendJsonStatus(std::string_view status);

  private:
    httpd_req *req;
    void warn_err(esp_err_t err);
};

} // namespace http