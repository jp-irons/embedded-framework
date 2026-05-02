#include "device/DeviceApiHandler.hpp"

#include "common/Result.hpp"
#include "device/DeviceInterface.hpp"
#include "esp_system.h"
#include "freertos/task.h"
#include "logger/Logger.hpp"

namespace device {

static logger::Logger log{"DeviceApiHandler"};

DeviceApiHandler::DeviceApiHandler() {
    log.debug("constructor");
}

using namespace common;
using namespace http;

common::Result DeviceApiHandler::handle(HttpRequest &req, HttpResponse &res) {
    log.debug("handle()");
    HttpMethod method = req.method();
    switch (method) {
        case HttpMethod::Get:
		  log.debug("handle GET");
            return handleGet(req, res);
        case HttpMethod::Post:
            return handlePost(req, res);
        default:
            res.sendJson(405, std::string("Method") + toString(method) + "not allowed");
            return common::Result::Ok;
    }
}

common::Result DeviceApiHandler::handleGet(http::HttpRequest &req, http::HttpResponse &res) {
	log.debug("handleGet");
    const std::string target = http::HttpHandler::extractTarget(req.path());
    if (target == "info") {
        return handleInfo(req, res);
    }
    return common::Result::Ok;
}

common::Result DeviceApiHandler::handlePost(http::HttpRequest &req, http::HttpResponse &res) {
    const std::string target = http::HttpHandler::extractTarget(req.path());

    if (target == "reboot") {
        return handleReboot(req, res);
    }
    if (target == "clearNvs") {
        return handleClearNvs(req, res);
    }

    res.sendJson(404, "target '" + target + "' not found");
    return common::Result::Ok;
}

common::Result DeviceApiHandler::handleClearNvs(http::HttpRequest &req, http::HttpResponse &res) {
    log.info("handleClearNvs()");
    Result r = device::clearNvs();
    if (r != common::Result::Ok) {
        res.sendJson(500, std::string("Error ") + toString(r) + " clearing NVS");
        return common::Result::Ok;
    }
    res.sendJson("NVS cleared");
    return common::Result::Ok;
}

common::Result DeviceApiHandler::handleReboot(http::HttpRequest &req, http::HttpResponse &res) {
    log.debug("handleReboot()");
    if (req.method() != http::HttpMethod::Post) {
        res.sendJson(405, "{\"error\":\"method not allowed\"}");
        return common::Result::Ok;
    }
    // Respond BEFORE rebooting
    res.sendJson("{\"status\":\"rebooting\"}");
    // Allow TCP stack to flush
    log.debug("waiting 500ms for TCP stack to flush");

    vTaskDelay(pdMS_TO_TICKS(500));
    log.info("rebooting device");
    // Reboot the device
    esp_restart();

    return common::Result::Ok;
}

common::Result DeviceApiHandler::handleInfo(http::HttpRequest &req, http::HttpResponse &res) {
    log.debug("handleInfo()");

    device::DeviceInfo info = device::info();

    // Build JSON
    std::string json =
        "{"
        "\"chipModel\":\""    + info.chipModel                       + "\","
        "\"revision\":"       + std::to_string(info.revision)        + ","
        "\"mac\":\""          + info.mac                             + "\","
        "\"flashSize\":"      + std::to_string(info.flashSize)       + ","
        "\"psramSize\":"      + std::to_string(info.psramSize)       + ","
        "\"freeHeap\":"       + std::to_string(info.freeHeap)        + ","
        "\"minFreeHeap\":"    + std::to_string(info.minFreeHeap)     + ","
        "\"cpuFreqMhz\":"     + std::to_string(info.cpuFreqMhz)     + ","
        "\"idfVersion\":\""   + info.idfVersion                      + "\","
        "\"lastReset\":\""    + info.lastReset                       + "\","
        "\"temperature\":"    + [&]{ char b[16]; snprintf(b, sizeof(b), "%.1f", info.temperature); return std::string(b); }() + ","
        "\"uptime\":\""       + info.uptime                          + "\","
        "\"otaPartition\":\"" + info.otaPartition                    + "\""
        "}";

    res.sendJson(json);
    return common::Result::Ok;
}

} // namespace device