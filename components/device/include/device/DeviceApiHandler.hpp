#pragma once

#include "device/DeviceService.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

namespace device {
	
class DeviceApiHandler : public http::HttpHandler {
public:
    explicit DeviceApiHandler();
    virtual ~DeviceApiHandler() = default;

    http::HandlerResult handle(http::HttpRequest& req, http::HttpResponse& res) override;

private:
	http::HandlerResult handleGet(http::HttpRequest& req, http::HttpResponse& res);
	http::HandlerResult handlePost(http::HttpRequest& req, http::HttpResponse& res);
	http::HandlerResult handleClearNvs(http::HttpRequest& req, http::HttpResponse& res);
	http::HandlerResult handleReboot(http::HttpRequest& req, http::HttpResponse& res);
	DeviceService deviceService;
	
};
} // namespace