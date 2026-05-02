#pragma once

#include "common/Result.hpp"
#include "http/HttpHandler.hpp"

namespace ota {

class OtaApiHandler : public http::HttpHandler {
  public:
    OtaApiHandler();
    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override;

  private:
    // ── Method dispatchers ────────────────────────────────────────────────
    common::Result handleGet (http::HttpRequest &req, http::HttpResponse &res);
    common::Result handlePost(http::HttpRequest &req, http::HttpResponse &res);

    // ── GET targets ───────────────────────────────────────────────────────

    /// GET /firmware/status — partition table JSON
    common::Result handleStatus(http::HttpRequest &req, http::HttpResponse &res);

    // ── POST targets ──────────────────────────────────────────────────────

    /// POST /firmware/upload  — stream a .bin onto the inactive OTA partition,
    ///                          set it as next-boot, reboot.
    ///                          Body is read via httpd_req_recv() in OtaWriter;
    ///                          HttpRequest must NOT have pre-loaded the body.
    common::Result handleUpload      (http::HttpRequest &req, http::HttpResponse &res);

    /// POST /firmware/rollback — set the other VALID OTA partition as next-boot
    ///                           and reboot.  Returns 409 if none exists.
    common::Result handleRollback    (http::HttpRequest &req, http::HttpResponse &res);

    /// POST /firmware/factoryReset — erase the OTA-data partition (clears all
    ///                               OTA state) and reboot to factory image.
    common::Result handleFactoryReset(http::HttpRequest &req, http::HttpResponse &res);
};

} // namespace ota
