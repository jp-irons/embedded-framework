#pragma once
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

namespace http {
	

class HttpHandler {

  public:
    virtual ~HttpHandler() = default;

    virtual HandlerResult handle(HttpRequest &req, HttpResponse &res) = 0;

  protected:
    static std::string extractTarget(const char *uri) {
        std::string path(uri);
        auto pos = path.find_last_of('/');

        if (pos == std::string::npos || pos == path.length() - 1) {
            return {}; // no action found
        }

        std::string action = path.substr(pos + 1);

        // Strip query parameters
        auto qpos = action.find('?');
        if (qpos != std::string::npos) {
            action = action.substr(0, qpos);
        }

        return action;
    }
	
};

} // namespace http