#pragma once

#include "common/Result.hpp"
#include "device/DeviceInterface.hpp"
#include "http/HttpHandler.hpp"

namespace ota {

class OtaApiHandler : public http::HttpHandler {
  public:
    static constexpr const char* TAG = "OtaApiHandler";

    explicit OtaApiHandler(device::DeviceInterface& device);

    common::Result handle(http::HttpRequest& req, http::HttpResponse& res) override;

  private:
    device::DeviceInterface& device_;

    // ── Method dispatchers ─────────────────────────────────────────��──────
    common::Result handleGet (http::HttpRequest& req, http::HttpResponse& res);
    common::Result handlePost(http::HttpRequest& req, http::HttpResponse& res);

    // ── GET targets ──────────────────────────────────────────────���────────

    /// GET /firmware/status     -- partition table JSON
    common::Result handleStatus    (http::HttpRequest& req, http::HttpResponse& res);

    /// GET /firmware/pullStatus      -- active pull URL
    common::Result handlePullStatus     (http::HttpRequest& req, http::HttpResponse& res);

    /// GET /firmware/pullCheckStatus -- current state of the pull-check state machine
    common::Result handlePullCheckStatus(http::HttpRequest& req, http::HttpResponse& res);

    // ── POST targets ──────────────────────────────────────────────────────

    /// POST /firmware/upload       -- stream a .bin onto the inactive OTA partition,
    ///                                set it as next-boot, reboot.
    common::Result handleUpload      (http::HttpRequest& req, http::HttpResponse& res);

    /// POST /firmware/rollback     -- set the other VALID OTA partition as next-boot
    ///                                and reboot.  Returns 409 if none exists.
    common::Result handleRollback    (http::HttpRequest& req, http::HttpResponse& res);

    /// POST /firmware/factoryReset -- erase the OTA-data partition and reboot.
    common::Result handleFactoryReset(http::HttpRequest& req, http::HttpResponse& res);

    /// POST /firmware/checkUpdate  -- trigger an immediate OTA pull check.
    ///                                Returns immediately; check runs in background.
    common::Result handleCheckUpdate (http::HttpRequest& req, http::HttpResponse& res);

    /// POST /firmware/pullConfig   -- update the pull base URL (plain-text body).
    ///                                Persists to NVS.
    common::Result handlePullConfig  (http::HttpRequest& req, http::HttpResponse& res);
};

} // namespace ota
