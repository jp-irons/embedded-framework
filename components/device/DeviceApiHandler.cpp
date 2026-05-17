#include "device/DeviceApiHandler.hpp"
#include "device/DeviceConfigStore.hpp"

#include "common/Result.hpp"
#include "logger/Logger.hpp"

namespace device {

static logger::Logger log{DeviceApiHandler::TAG};

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
    if (target == "info")           return handleInfo          (req, res);
    if (target == "hostnameConfig") return handleHostnameConfigGet(req, res);

    res.sendJson(404, "target '" + target + "' not found");
    return Result::Ok;
}

common::Result DeviceApiHandler::handlePost(HttpRequest& req, HttpResponse& res) {
    const std::string target = HttpHandler::extractTarget(req.path());
    if (target == "reboot")         return handleReboot           (req, res);
    if (target == "clearNvs")       return handleClearNvs         (req, res);
    if (target == "hostnameConfig") return handleHostnameConfigPost(req, res);

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

common::Result DeviceApiHandler::handleHostnameConfigGet(HttpRequest& /*req*/,
                                                           HttpResponse& res) {
    log.debug("handleHostnameConfigGet");
    const DeviceConfigStore::Config& cfg = DeviceConfigStore::config();

    std::string json =
        "{"
        "\"hostnamePrefix\":\""    + cfg.hostnamePrefix    + "\","
        "\"apSsidPrefix\":\""      + cfg.apSsidPrefix      + "\","
        "\"effectiveHostname\":\"" + cfg.effectiveHostname + "\","
        "\"effectiveApSsid\":\""   + cfg.effectiveApSsid   + "\""
        "}";

    res.sendJson(json);
    return Result::Ok;
}

common::Result DeviceApiHandler::handleHostnameConfigPost(HttpRequest& req,
                                                           HttpResponse& res) {
    log.debug("handleHostnameConfigPost");
    const std::string_view body = req.body();

    // Parse {"hostnamePrefix":"...","apSsidPrefix":"..."}.
    // Semantics:
    //   key absent          → leave that setting unchanged
    //   key present, value  → save as NVS override (MAC suffix suppressed)
    //   key present, empty  → delete NVS override (reverts to app default + MAC suffix)
    auto extractField = [&](const std::string& key, std::string& out) -> bool {
        const std::string needle = "\"" + key + "\":\"";
        const size_t start = body.find(needle);
        if (start == std::string_view::npos) return false; // key absent
        const size_t valueStart = start + needle.size();
        const size_t end = body.find('"', valueStart);
        if (end == std::string_view::npos) return false;
        out = std::string(body.substr(valueStart, end - valueStart));
        return true; // key present (out may be empty = clear intent)
    };

    std::string newHostname, newApSsid;
    const bool hasHostname = extractField("hostnamePrefix", newHostname);
    const bool hasApSsid   = extractField("apSsidPrefix",   newApSsid);

    if (!hasHostname && !hasApSsid) {
        res.sendJson(400, "Body must contain hostnamePrefix and/or apSsidPrefix");
        return Result::Ok;
    }

    bool ok = true;
    if (hasHostname) {
        ok &= newHostname.empty() ? DeviceConfigStore::clearHostnamePrefix()
                                  : DeviceConfigStore::saveHostnamePrefix(newHostname);
    }
    if (hasApSsid) {
        ok &= newApSsid.empty() ? DeviceConfigStore::clearApSsidPrefix()
                                 : DeviceConfigStore::saveApSsidPrefix(newApSsid);
    }

    if (!ok) {
        res.sendJson(500, "Failed to save config to NVS");
        return Result::Ok;
    }

    res.sendJson("{\"rebootRequired\":true}");
    return Result::Ok;
}

} // namespace device
