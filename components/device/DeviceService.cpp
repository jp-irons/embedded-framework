#include "device/DeviceService.hpp"
#include "device/EspTypeAdapter.hpp"
#include "logger/Logger.hpp"

#include "nvs_flash.h"

namespace device {
	
static logger::Logger log{"DeviceService"};

using namespace common;

DeviceService::DeviceService() {
	log.debug("constructor");
}

Result DeviceService::clearNvs() {
    // Erase the entire NVS partition
    Result r = toResult(nvs_flash_erase());
    if (r != Result::Ok) {
        log.error("Failed to erase NVS: %s", toString(r));
        return r;
    }

    // Re‑initialise NVS so the system is in a clean state
    r = toResult(nvs_flash_init());
	if (r != Result::Ok) {
	    log.error("Failed to re-init NVS: %s", toString(r));
	    return r;
	}
    log.info("NVS successfully erased and re‑initialised");
    return Result::Ok;
}
} // namespace esp_err_t DeviceApiHandler::clearNvs()
 