#include "device/DeviceApiHandler.hpp"

#include "common/Result.hpp"
#include "logger/Logger.hpp"

namespace device {

static logger::Logger log{"DeviceApiHandler"};

DeviceApiHandler::DeviceApiHandler(DeviceInterface& device, TimerInterface& timer)
    : device_(device)
    , timer_(timer) {
    log.debug("constructor");
}

using namespace common;
using namespace http;

common::Result DeviceApiHandler::handle(HttpRequest& req, HttpResponse& res) {
    log.debug("handle()");
    switch (req.method()) {
        case HttpMethod::Get:
            log.debug("handle GET");
            return handleGet(req, res);
        case HttpMethod::Post:
            return handlePost(req, res);
        default:
            res.sendJson(405, std::string("Method ") + toString(req.method()) +
                              " not allowed");
            return Result::Ok;
    }
}

common::Result DeviceApiHandler::handleGet(HttpRequest& req, HttpResponse& res) {
    log.debug("handleGet");
    const std::string target = HttpHandler::extractTarget(req.path());
    if (target == "info") return handleInfo(req, res);
    return Result::Ok;
}

common::Result DeviceApiHandler::handlePost(HttpRequest& req, HttpResponse& res) {
    const std::string target = HttpHandler::extractTarget(req.path());
    if (target == "reboot")   return handleReboot  (req, res);
    if (target == "clearNvs") return handleClearNvs(req, res);

    res.sendJson(404, "target '" + target + "' not found");
    return Result::Ok;
}

common::Result DeviceApiHandler::handleClearNvs(HttpRequest& /*req*/,
                                                  HttpResponse& res) {
    log.info("handleClearNvs()");
    Result r = device_.clearNvs();
    if (r != Result::Ok) {
        res.sendJson(500, std::string("Error ") + toString(r) + " clearing NVS");
        return Result::Ok;
    }
    res.sendJson("NVS cleared");
    return Result::Ok;
}

common::Result DeviceApiHandler::handleReboot(HttpRequest& req,
                                               HttpResponse& res) {
    log.debug("handleReboot()");
    if (req.method() != HttpMethod::Post) {
        res.sendJson(405, "{\"error\":\"method not allowed\"}");
        return Result::Ok;
    }
    res.sendJson("{\"status\":\"rebooting\"}");
    log.debug("scheduling reboot in 500ms");
    timer_.runAfter(500, [this]() {
        log.info("rebooting device");
        device_.reboot();
    });
    return Result::Ok;
}

common::Result DeviceApiHandler::handleInfo(HttpRequest& /*req*/,
                                              HttpResponse& res) {
    log.debug("handleInfo()");
    DeviceInfo i = device_.info();

    std::string json =
        "{"
        "\"chipModel\":\""    + i.chipModel                       + "\","
        "\"revision\":"       + std::to_string(i.revision)        + ","
        "\"mac\":\""          + i.mac                             + "\","
        "\"flashSize\":"      + std::to_string(i.flashSize)       + ","
        "\"psramSize\":"      + std::to_string(i.psramSize)       + ","
        "\"freeHeap\":"       + std::to_string(i.freeHeap)        + ","
        "\"minFreeHeap\":"    + std::to_string(i.minFreeHeap)     + ","
        "\"cpuFreqMhz\":"     + std::to_string(i.cpuFreqMhz)     + ","
        "\"idfVersion\":\""   + i.idfVersion                      + "\","
        "\"lastReset\":\""    + i.lastReset                       + "\","
        "\"temperature\":"    + [&]{ char b[16];
                                     snprintf(b, sizeof(b), "%.1f",
                                              i.temperature);
                                     return std::string(b); }()   + ","
        "\"uptime\":\""       + i.uptime                          + "\","
        "\"otaPartition\":\"" + i.otaPartition                    + "\""
        "}";

    res.sendJson(json);
    return Result::Ok;
}

} // namespace device
