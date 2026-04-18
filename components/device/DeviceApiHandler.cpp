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
	res.sendNotFound404("action '" + action + "' not found");
    return common::Result::Ok;
}

common::Result DeviceApiHandler::handleReboot(http::HttpRequest& req, http::HttpResponse& res)
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