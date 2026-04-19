#include "device/DeviceApiHandler.hpp"
#include "http/HttpMethod.hpp"
#include "logger/Logger.hpp"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace device {
	
static logger::Logger log{"DeviceApiHandler"};

DeviceApiHandler::DeviceApiHandler() {
	log.debug("constructor");
}

using namespace common;
Result DeviceApiHandler::handle(http::HttpRequest& req, http::HttpResponse& res)
{
	log.debug("handle");
    const std::string action = http::HttpHandler::extractAction(req.path());

    if (action == "reboot") {
        return handleReboot(req, res);
    }
	if (action == "clearNvs") {
	    return handleClearNvs(req, res);
	}
	res.sendNotFound404("action '" + action + "' not found");
    return common::Result::Ok;
}

Result DeviceApiHandler::handleClearNvs(http::HttpRequest& req, http::HttpResponse& res) {
    log.info("handleClearNvs not implemented");
	Result r = deviceService.clearNvs();
    if (r != common::Result::Ok) {
        return res.sendJsonError(500, std::string("Error ") + toString(r) + " clearing NVS");
    }
    return res.sendJsonOk("NVS cleared");
}

Result DeviceApiHandler::handleReboot(http::HttpRequest& req, http::HttpResponse& res)
{
	log.debug("handleReboot");
	if (req.method()!= http::HttpMethod::Post) {
		res.sendJsonError(405, "{\"error\":\"method not allowed\"}");
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

} // namespace