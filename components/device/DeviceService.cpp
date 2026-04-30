#include "device/DeviceService.hpp"

#include "device/EspTypeAdapter.hpp"
#include "logger/Logger.hpp"

#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

namespace device {

static logger::Logger log{"DeviceService"};

using namespace common;

DeviceService::DeviceService() {
    log.debug("constructor");
}

Result init() {
    log.debug("init()");
    // 1. Initialize NVS
    log.debug("nvs_flash_init");
    Result r = toResult(nvs_flash_init());
    if (r != Result::Ok) {
        log.error("Failed to init NVS: %s - erase than init", toString(r));
        r = toResult(nvs_flash_erase());
        if (r != Result::Ok) {
            log.error("Failed to erase flash: %s", toString(r));
            return r;
        }
        if (r != Result::Ok) {
            log.error("Failed to init: %s", toString(r));
            return r;
        }
    }

    // 2. Initialize event loop
    log.debug("init event loop");
    r = toResult(esp_event_loop_create_default());
    if (r != Result::Ok) {
        log.error("Failed to initialise event loop: %s", toString(r));
        return r;
    }

    // 3. Initialize netif
    log.debug("init netif");
	r = toResult(esp_netif_init());
	if (r != Result::Ok) {
	    log.error("Failed to initialise netif: %s", toString(r));
	    return r;
	}
    return Result::Ok;
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
} // namespace device
