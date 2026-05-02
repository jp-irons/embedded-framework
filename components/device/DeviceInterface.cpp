#include "device/DeviceInterface.hpp"

#include "device/EspTypeAdapter.hpp"
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_idf_version.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
//#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "logger/Logger.hpp"
#include "nvs_flash.h"
#include "sdkconfig.h"

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

static std::string resetReasonStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:    return "Power-on (ESP_RST_POWERON)";
        case ESP_RST_EXT:        return "External pin (ESP_RST_EXT)";
        case ESP_RST_SW:         return "Software (ESP_RST_SW)";
        case ESP_RST_PANIC:      return "Panic (ESP_RST_PANIC)";
        case ESP_RST_INT_WDT:    return "Interrupt watchdog (ESP_RST_INT_WDT)";
        case ESP_RST_TASK_WDT:   return "Task watchdog (ESP_RST_TASK_WDT)";
        case ESP_RST_WDT:        return "Other watchdog (ESP_RST_WDT)";
        case ESP_RST_DEEPSLEEP:  return "Deep sleep wake (ESP_RST_DEEPSLEEP)";
        case ESP_RST_BROWNOUT:   return "Brownout (ESP_RST_BROWNOUT)";
        case ESP_RST_SDIO:       return "SDIO (ESP_RST_SDIO)";
        case ESP_RST_USB:        return "USB (ESP_RST_USB)";
        case ESP_RST_JTAG:       return "JTAG (ESP_RST_JTAG)";
        default:                 return "Unknown (ESP_RST_UNKNOWN)";
    }
}

static std::string formatUptime(int64_t us) {
    uint32_t secs = (uint32_t)(us / 1000000LL);
    uint32_t h    = secs / 3600;
    uint32_t m    = (secs % 3600) / 60;
    uint32_t s    = secs % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
    return std::string(buf);
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

    // PSRAM (only available when CONFIG_SPIRAM=y)
#ifdef CONFIG_SPIRAM
    i.psramSize = esp_psram_get_size();
#else
    i.psramSize = 0;
#endif

    // Heap
    i.freeHeap    = esp_get_free_heap_size();
    i.minFreeHeap = esp_get_minimum_free_heap_size();

    // CPU frequency (compile-time configured value)
    i.cpuFreqMhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;

    // IDF version
    i.idfVersion = esp_get_idf_version();

    // Reset reason
    i.lastReset = resetReasonStr(esp_reset_reason());

    // Uptime
    i.uptime = formatUptime(esp_timer_get_time());

    // Running OTA partition
    const esp_partition_t *part = esp_ota_get_running_partition();
    i.otaPartition = part ? part->label : "unknown";

    return i;
}

} // namespace device
