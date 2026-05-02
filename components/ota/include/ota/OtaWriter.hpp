#pragma once

#include "esp_http_server.h"
#include "http/HttpResponse.hpp"

namespace ota {

/**
 * Streaming OTA flash writer.
 *
 * Reads the firmware binary from the live HTTP socket in 4 KB chunks using
 * httpd_req_recv() — no large heap allocation required.  On success, sets the
 * newly-written partition as next-boot and calls esp_restart().
 *
 * The caller MUST NOT have pre-read the request body before calling
 * writeFromRequest().  HttpRequest enforces this by skipping body preload when
 * content_len exceeds its internal threshold (see HttpRequest::MAX_PRELOAD_BYTES).
 */
class OtaWriter {
  public:
    static constexpr size_t CHUNK_SIZE = 4096;

    /**
     * Perform the OTA update by streaming from a raw httpd_req_t*.
     *
     * On success  : sends a 200 JSON reply, waits 500 ms, calls esp_restart().
     *               Does not return.
     * On failure  : sends an appropriate JSON error reply and returns false.
     */
    static bool writeFromRequest(httpd_req_t *req, http::HttpResponse &res);
};

} // namespace ota
