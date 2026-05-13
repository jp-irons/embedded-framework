#include "ota/OtaApiHandler.hpp"
#include "ota/OtaWriter.hpp"

#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep — must precede task.h
#include "freertos/task.h"
#include "http/HttpHandler.hpp"
#include "logger/Logger.hpp"

#include <string>

namespace ota {

static logger::Logger log{OtaApiHandler::TAG};

OtaApiHandler::OtaApiHandler(device::DeviceInterface& device)
    : device_(device) {
    log.debug("constructor");
}

using namespace common;
using namespace http;

// ---------------------------------------------------------------------------
// Top-level routing
// ---------------------------------------------------------------------------

common::Result OtaApiHandler::handle(HttpRequest &req, HttpResponse &res) {
    log.debug("handle()");
    switch (req.method()) {
        case HttpMethod::Get:  return handleGet (req, res);
        case HttpMethod::Post: return handlePost(req, res);
        default:
            res.sendJson(405, "Method not allowed");
            return Result::Ok;
    }
}

common::Result OtaApiHandler::handleGet(HttpRequest &req, HttpResponse &res) {
    const std::string target = HttpHandler::extractTarget(req.path());
    if (target == "status") return handleStatus(req, res);

    res.sendJson(404, "target '" + target + "' not found");
    return Result::Ok;
}

common::Result OtaApiHandler::handlePost(HttpRequest &req, HttpResponse &res) {
    const std::string target = HttpHandler::extractTarget(req.path());
    if (target == "upload")       return handleUpload      (req, res);
    if (target == "rollback")     return handleRollback    (req, res);
    if (target == "factoryReset") return handleFactoryReset(req, res);

    res.sendJson(404, "target '" + target + "' not found");
    return Result::Ok;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Walk the ESP image header stored at the beginning of @p part and return the
 * total byte length of the firmware image (header + all segments + optional
 * SHA-256 hash).  Returns 0 if the partition does not contain a valid image.
 *
 * This reads only a few small structs from flash — no SHA verification —
 * so it is fast enough to call on every status request.
 */
static uint32_t getImageSize(const esp_partition_t *part) {
    esp_image_header_t img_hdr = {};
    if (esp_partition_read(part, 0, &img_hdr, sizeof(img_hdr)) != ESP_OK) {
        return 0;
    }
    if (img_hdr.magic != ESP_IMAGE_HEADER_MAGIC) {
        return 0;
    }

    uint32_t offset = sizeof(esp_image_header_t);
    for (uint8_t i = 0; i < img_hdr.segment_count; i++) {
        esp_image_segment_header_t seg = {};
        if (esp_partition_read(part, offset, &seg, sizeof(seg)) != ESP_OK) {
            return 0;
        }
        offset += sizeof(seg) + seg.data_len;
    }
    // Pad to 16-byte alignment
    offset = (offset + 15u) & ~15u;
    // Appended SHA-256 hash
    if (img_hdr.hash_appended) {
        offset += 32;
    }
    return offset;
}

static const char *otaStateStr(esp_ota_img_states_t s) {
    switch (s) {
        case ESP_OTA_IMG_NEW:            return "new";
        case ESP_OTA_IMG_PENDING_VERIFY: return "pending";
        case ESP_OTA_IMG_VALID:          return "valid";
        case ESP_OTA_IMG_INVALID:        return "invalid";
        case ESP_OTA_IMG_ABORTED:        return "aborted";
        case ESP_OTA_IMG_UNDEFINED:      return "empty";
        default:                         return "unknown";
    }
}

static std::string partitionJson(const esp_partition_t *part,
                                 const esp_partition_t *running,
                                 const esp_partition_t *nextBoot) {
    const bool isRunning  = running  && (part->address == running->address);
    const bool isNextBoot = nextBoot && (part->address == nextBoot->address);
    const bool isFactory  = (part->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY);

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    esp_ota_get_state_partition(part, &state);

    // Primary display state:
    //   running  → "running"
    //   factory  → "factory"  (always UNDEFINED but that's normal, not "empty")
    //   other    → OTA state string
    const char *stateStr = isRunning ? "running"
                         : isFactory ? "factory"
                         :             otaStateStr(state);

    // The actual OTA state (valid/pending/invalid/…) for the running partition,
    // so the UI can show e.g. "Running • Valid" or warn "Running • Pending".
    const char *otaStateStr_ = otaStateStr(state);

    esp_app_desc_t desc  = {};
    const bool     hasDesc = (esp_ota_get_partition_description(part, &desc) == ESP_OK);

    // Show version data only when the partition is actively managed by OTA.
    // Factory is always UNDEFINED — that's normal, so always show its info.
    // OTA slots with UNDEFINED state are unmanaged (e.g. after factory reset):
    // flash content may still be present but it has no system-level meaning,
    // so showing stale version strings alongside "Empty" is misleading.
    const bool showDesc = hasDesc && (isFactory || state != ESP_OTA_IMG_UNDEFINED);

    // Sizes: partition capacity is always known; firmware image size is only
    // meaningful when the partition has a valid image (showDesc guard).
    const uint32_t partitionSize = part->size;
    const uint32_t firmwareSize  = showDesc ? getImageSize(part) : 0;

    std::string j = "{";
    j += "\"label\":\""       + std::string(part->label)          + "\",";
    j += "\"state\":\""       + std::string(stateStr)             + "\",";
    j += "\"otaState\":\""    + std::string(otaStateStr_)         + "\",";
    j += "\"isRunning\":";    j += isRunning  ? "true" : "false"; j += ",";
    j += "\"isNextBoot\":";   j += isNextBoot ? "true" : "false"; j += ",";
    j += "\"partitionSize\":" + std::to_string(partitionSize)     + ",";
    j += "\"firmwareSize\":"  + std::to_string(firmwareSize)      + ",";

    if (showDesc) {
        std::string buildDate = std::string(desc.date) + " " + std::string(desc.time);
        j += "\"version\":\""    + std::string(desc.version)      + "\",";
        j += "\"project\":\""    + std::string(desc.project_name) + "\",";
        j += "\"buildDate\":\""  + buildDate                      + "\",";
        j += "\"idfVersion\":\"" + std::string(desc.idf_ver)      + "\"";
    } else {
        j += "\"version\":\"\",\"project\":\"\",\"buildDate\":\"\",\"idfVersion\":\"\"";
    }

    j += "}";
    return j;
}

// ---------------------------------------------------------------------------
// GET /framework/api/firmware/status
// ---------------------------------------------------------------------------

common::Result OtaApiHandler::handleStatus(HttpRequest & /*req*/, HttpResponse &res) {
    log.debug("handleStatus()");

    const esp_partition_t *running  = esp_ota_get_running_partition();
    const esp_partition_t *nextBoot = esp_ota_get_boot_partition();

    const esp_partition_t *parts[] = {
        esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                 ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr),
        esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                 ESP_PARTITION_SUBTYPE_APP_OTA_0,   nullptr),
        esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                 ESP_PARTITION_SUBTYPE_APP_OTA_1,   nullptr),
    };

    std::string json = "{\"partitions\":[";
    bool first = true;
    for (const esp_partition_t *p : parts) {
        if (!p) continue;
        if (!first) json += ",";
        json += partitionJson(p, running, nextBoot);
        first = false;
    }
    json += "]}";

    res.sendJson(json);
    return Result::Ok;
}

// ---------------------------------------------------------------------------
// POST /framework/api/firmware/upload
// ---------------------------------------------------------------------------

common::Result OtaApiHandler::handleUpload(HttpRequest &req, HttpResponse &res) {
    log.info("handleUpload(): content_len=%zu", req.contentLength());

    // The body must NOT have been pre-read into memory — OtaWriter streams
    // it directly from the socket.  HttpRequest skips preload when content_len
    // exceeds MAX_PRELOAD_BYTES (64 KB), so any real firmware file is safe.
    OtaWriter::writeFromRequest(req, res);
    // writeFromRequest() either reboots (success) or sends an error and returns false.
    return Result::Ok;
}

// ---------------------------------------------------------------------------
// POST /framework/api/firmware/rollback
// ---------------------------------------------------------------------------

common::Result OtaApiHandler::handleRollback(HttpRequest & /*req*/, HttpResponse &res) {
    log.info("handleRollback()");

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        res.sendJson(500, "Cannot determine running partition");
        return Result::Ok;
    }

    // Search ota_0 and ota_1 for a VALID partition that is not currently running
    const esp_partition_t *candidates[] = {
        esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                 ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr),
        esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                 ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr),
    };

    const esp_partition_t *target = nullptr;
    for (const esp_partition_t *p : candidates) {
        if (!p || p->address == running->address) continue;
        esp_ota_img_states_t st = ESP_OTA_IMG_UNDEFINED;
        esp_ota_get_state_partition(p, &st);
        if (st == ESP_OTA_IMG_VALID) {
            target = p;
            break;
        }
    }

    if (!target) {
        log.warn("handleRollback: no valid OTA partition to roll back to");
        res.sendJson(409, "No valid OTA partition available for rollback");
        return Result::Ok;
    }

    log.info("handleRollback: setting next-boot to '%s'", target->label);
    esp_err_t err = esp_ota_set_boot_partition(target);
    if (err != ESP_OK) {
        log.error("esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        res.sendJson(500, "Failed to set boot partition");
        return Result::Ok;
    }

    res.sendJson(
        "{\"status\":\"ok\","
        "\"message\":\"Rolling back to previous firmware. Device is rebooting...\"}");
    vTaskDelay(pdMS_TO_TICKS(500));
    device_.reboot();

    return Result::Ok; // unreachable
}

// ---------------------------------------------------------------------------
// POST /framework/api/firmware/factoryReset
// ---------------------------------------------------------------------------

common::Result OtaApiHandler::handleFactoryReset(HttpRequest & /*req*/, HttpResponse &res) {
    log.warn("handleFactoryReset(): erasing OTA data partition");

    // Erasing the OTA data partition clears all OTA state; the bootloader will
    // boot the factory partition on the next reset.
    const esp_partition_t *otadata = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
    if (otadata) {
        esp_err_t err = esp_partition_erase_range(otadata, 0, otadata->size);
        if (err != ESP_OK) {
            log.error("esp_partition_erase_range failed: %s", esp_err_to_name(err));
            res.sendJson(500, "Failed to erase OTA data partition");
            return Result::Ok;
        }
    } else {
        log.warn("handleFactoryReset: OTA data partition not found — rebooting anyway");
    }

    res.sendJson(
        "{\"status\":\"ok\","
        "\"message\":\"Factory reset complete. Device is rebooting to factory firmware...\"}");
    vTaskDelay(pdMS_TO_TICKS(500));
    device_.reboot();

    return Result::Ok; // unreachable
}

} // namespace ota
