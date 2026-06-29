// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Result.hpp"
#include "device/DeviceInterface.hpp"
#include "device/TimerInterface.hpp"
#include "http/HttpHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "persistent_log/PersistentLogSink.hpp"

namespace device {

class DeviceApiHandler : public http::HttpHandler {
  public:
    static constexpr const char* TAG = "DeviceApiHandler";

    DeviceApiHandler(DeviceInterface& device, TimerInterface& timer);
    virtual ~DeviceApiHandler() = default;

    /** Optional. If never called, GET .../device/logs returns 501. */
    void setLogSink(persistent_log::PersistentLogSink* sink) { logSink_ = sink; }

    common::Result handle(http::HttpRequest& req, http::HttpResponse& res) override;

  private:
    DeviceInterface& device_;
    TimerInterface&  timer_;
    persistent_log::PersistentLogSink* logSink_ = nullptr;

    common::Result handleGet    (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handlePost   (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleClearNvs          (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleReboot            (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleInfo              (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleHostnameConfigGet (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleHostnameConfigPost(http::HttpRequest& req, http::HttpResponse& res);
    common::Result handleLogs              (http::HttpRequest& req, http::HttpResponse& res);
};

} // namespace device
