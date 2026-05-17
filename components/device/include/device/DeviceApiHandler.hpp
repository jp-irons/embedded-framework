#pragma once

#include "common/Result.hpp"
#include "device/DeviceInterface.hpp"
#include "device/TimerInterface.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

namespace device {

class DeviceApiHandler : public http::HttpHandler {
  public:
    static constexpr const char* TAG = "DeviceApiHandler";

    DeviceApiHandler(DeviceInterface& device, TimerInterface& timer);
    virtual ~DeviceApiHandler() = default;

    common::Result handle(http::HttpRequest& req, http::HttpResponse& res) override;

  private:
    DeviceInterface& device_;
    TimerInterface&  timer_;

    common::Result handleGet    (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handlePost   (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleClearNvs          (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleReboot            (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleInfo              (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleHostnameConfigGet (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleHostnameConfigPost(http::HttpRequest& req, http::HttpResponse& res);
};

} // namespace device
