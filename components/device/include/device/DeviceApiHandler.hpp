#pragma once

#include "common/Result.hpp"
#include "device/DeviceService.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

namespace device {
	
class DeviceApiHandler : public http::HttpHandler {
public:
    explicit DeviceApiHandler();
    virtual ~DeviceApiHandler() = default;

    common::Result handle(http::HttpRequest& req, http::HttpResponse& res) override;

private:
	common::Result handleGet(http::HttpRequest& req, http::HttpResponse& res);
	common::Result handlePost(http::HttpRequest& req, http::HttpResponse& res);
	common::Result handleClearNvs(http::HttpRequest& req, http::HttpResponse& res);
	common::Result handleReboot(http::HttpRequest& req, http::HttpResponse& res);
	DeviceService deviceService;
	
};
} // namespace