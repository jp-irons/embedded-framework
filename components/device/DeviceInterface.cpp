#include "device/DeviceInterface.hpp"

#include "device/EspTypeAdapter.hpp"
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "logger/Logger.hpp"
#include "nvs_flash.h"

namespace device {

static logger::Logger log{"DeviceInterface"};

using namespace common;

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

Result clearNvs() {
	log.debug("clearNvs()");
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

static std::string formatMac(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
    return std::string(buf);
}

size_t detectFlashSize() {
    size_t maxEnd = 0;

    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it != NULL) {
        const esp_partition_t* p = esp_partition_get(it);
        size_t end = p->address + p->size;
        if (end > maxEnd) {
            maxEnd = end;
        }
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);

    return maxEnd;  // total flash size in bytes
}

DeviceInfo info() {
	log.debug("info()");
    DeviceInfo i;

    // Chip info
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    i.chipModel = "ESP32-S3";
    i.revision = chip.revision;

    // MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    i.mac = formatMac(mac);

    // Flash size
    size_t flashSize = detectFlashSize();
    i.flashSize = flashSize;

    // Free heap
    i.freeHeap = esp_get_free_heap_size();

    return i;
}

} // namespace device
