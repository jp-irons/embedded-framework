#include "ota/OtaWriter.hpp"

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logger/Logger.hpp"

#include <algorithm>

namespace ota {

static logger::Logger log{"OtaWriter"};

bool OtaWriter::writeFromRequest(http::HttpRequest &req, http::HttpResponse &res) {
    const size_t totalLen = req.contentLength();
    if (totalLen == 0) {
        log.error("writeFromRequest: content_len == 0");
        res.sendJson(400, "No firmware data in request");
        return false;
    }
    log.info("writeFromRequest: expecting %zu bytes", totalLen);

    // ── Find the inactive OTA partition ────────────────────────────────────
    const esp_partition_t *updatePart = esp_ota_get_next_update_partition(nullptr);
    if (!updatePart) {
        log.error("writeFromRequest: no OTA update partition available");
        res.sendJson(500, "No OTA partition available");
        return false;
    }
    log.info("writeFromRequest: target partition '%s' @ 0x%08" PRIx32 " (%zu KB)",
             updatePart->label, updatePart->address, updatePart->size / 1024);

    // ── Begin OTA ──────────────────────────────────────────────────────────
    esp_ota_handle_t otaHandle = 0;
    esp_err_t err = esp_ota_begin(updatePart, OTA_WITH_SEQUENTIAL_WRITES, &otaHandle);
    if (err != ESP_OK) {
        log.error("esp_ota_begin failed: %s", esp_err_to_name(err));
        res.sendJson(500, "OTA begin failed");
        return false;
    }

    // ── Stream chunks ──────────────────────────────────────────────────────
    char   buf[CHUNK_SIZE];
    size_t remaining = totalLen;
    size_t written   = 0;

    while (remaining > 0) {
        const size_t toRead  = std::min(remaining, CHUNK_SIZE);
        const int    received = req.receiveChunk(buf, toRead);

        if (received <= 0) {
            log.error("writeFromRequest: recv error %d at offset %zu / %zu",
                      received, written, totalLen);
            esp_ota_abort(otaHandle);
            res.sendJson(500, "Receive error during upload");
            return false;
        }

        err = esp_ota_write(otaHandle, buf, (size_t)received);
        if (err != ESP_OK) {
            log.error("esp_ota_write failed at offset %zu: %s",
                      written, esp_err_to_name(err));
            esp_ota_abort(otaHandle);
            res.sendJson(500, "Flash write failed");
            return false;
        }

        written   += (size_t)received;
        remaining -= (size_t)received;

        // Log progress every 64 KB
        if ((written % (64 * 1024)) < CHUNK_SIZE) {
            log.info("OTA progress: %zu / %zu bytes (%.0f%%)",
                     written, totalLen,
                     100.0 * (double)written / (double)totalLen);
        }
    }

    log.info("writeFromRequest: streamed %zu bytes — finalising", written);

    // ── Finalise ───────────────────────────────────────────────────────────
    err = esp_ota_end(otaHandle);
    if (err != ESP_OK) {
        log.error("esp_ota_end failed: %s", esp_err_to_name(err));
        res.sendJson(500, "OTA end failed — image may be corrupt");
        return false;
    }

    err = esp_ota_set_boot_partition(updatePart);
    if (err != ESP_OK) {
        log.error("esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        res.sendJson(500, "Failed to set boot partition");
        return false;
    }

    log.info("OTA complete — next boot: '%s' — rebooting", updatePart->label);

    // Send the success response before restarting so the browser receives it
    res.sendJson(
        "{\"status\":\"ok\","
        "\"message\":\"Firmware written successfully. Device is rebooting...\"}");

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return true; // unreachable
}

} // namespace ota
