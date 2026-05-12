#pragma once

#include "common/Result.hpp"
#include "device/DeviceInterface.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

namespace device {

class DeviceApiHandler : public http::HttpHandler {
  public:
    explicit DeviceApiHandler(DeviceInterface& device);
    virtual ~DeviceApiHandler() = default;

    common::Result handle(http::HttpRequest& req, http::HttpResponse& res) override;

  private:
    DeviceInterface& device_;

    common::Result handleGet    (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handlePost   (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleClearNvs(http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleReboot  (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleInfo    (http::HttpRequest& req, http::HttpResponse& res);
};

} // namespace device
