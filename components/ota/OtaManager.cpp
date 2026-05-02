#include "ota/OtaManager.hpp"

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "logger/Logger.hpp"

// ── RTC memory ───────────────────────────────────────────────────────────────
// These survive soft resets (software, watchdog, exception) but are zeroed
// on a cold power-cycle — exactly what we want for boot-attempt tracking.
RTC_DATA_ATTR static uint32_t s_bootAttempts = 0;
RTC_DATA_ATTR static uint32_t s_bootMagic    = 0;

static constexpr uint32_t BOOT_MAGIC = 0xB007AB1Eu;

namespace ota {

static logger::Logger log{"OtaManager"};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/**
 * Walk all APP partitions looking for one that is VALID and not the running
 * partition.  Returns the first match, or nullptr if none exists.
 */
static const esp_partition_t *findPreviousValidPartition(const esp_partition_t *running) {
    esp_partition_iterator_t it =
        esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);

    const esp_partition_t *candidate = nullptr;
    while (it) {
        const esp_partition_t *p = esp_partition_get(it);
        if (p && p->address != running->address) {
            esp_ota_img_states_t st = ESP_OTA_IMG_UNDEFINED;
            esp_ota_get_state_partition(p, &st);
            if (st == ESP_OTA_IMG_VALID) {
                candidate = p;
                break;
            }
        }
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
    return candidate;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void OtaManager::checkOnBoot() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        log.warn("checkOnBoot: could not determine running partition");
        return;
    }

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    esp_ota_get_state_partition(running, &state);

    if (state != ESP_OTA_IMG_PENDING_VERIFY) {
        // Known-good partition (VALID, factory=UNDEFINED, …) — clear counter
        s_bootAttempts = 0;
        s_bootMagic    = 0;
        log.debug("checkOnBoot: partition '%s' state=%d — no action needed",
                  running->label, (int)state);
        return;
    }

    // ── New (unvalidated) OTA image ────────────────────────────────────────
    if (s_bootMagic != BOOT_MAGIC) {
        // First boot after a flash — initialise counter
        s_bootMagic    = BOOT_MAGIC;
        s_bootAttempts = 0;
    }
    s_bootAttempts++;

    log.warn("checkOnBoot: '%s' is PENDING_VERIFY — boot attempt %u / %u",
             running->label,
             (unsigned)s_bootAttempts,
             (unsigned)MAX_BOOT_ATTEMPTS);

    if (s_bootAttempts <= MAX_BOOT_ATTEMPTS) {
        return; // still within grace period — continue booting normally
    }

    // ── Exceeded max attempts → escalate ──────────────────────────────────
    log.error("checkOnBoot: max boot attempts exceeded — escalating");
    s_bootAttempts = 0;
    s_bootMagic    = 0;

    // Step 1: roll back to the most-recent VALID OTA partition
    const esp_partition_t *prev = findPreviousValidPartition(running);
    if (prev) {
        log.warn("checkOnBoot: rolling back to '%s'", prev->label);
        esp_ota_set_boot_partition(prev);
        esp_restart();
        return; // unreachable
    }

    // Step 2: no valid OTA partition — erase OTA data → reboot to factory
    log.warn("checkOnBoot: no valid OTA partition found — erasing otadata → factory");
    const esp_partition_t *otadata = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
    if (otadata) {
        esp_partition_erase_range(otadata, 0, otadata->size);
    }
    esp_restart();
}

void OtaManager::markValid() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) return;

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    esp_ota_get_state_partition(running, &state);

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        log.info("markValid: marking partition '%s' as VALID", running->label);
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err != ESP_OK) {
            log.error("markValid: esp_ota_mark_app_valid_cancel_rollback failed: %s",
                      esp_err_to_name(err));
        } else {
            s_bootAttempts = 0;
            s_bootMagic    = 0;
            log.info("markValid: partition validated successfully");
        }
    }
    // Already VALID or factory (UNDEFINED) — nothing to do
}

bool OtaManager::needsValidation() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) return false;
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    esp_ota_get_state_partition(running, &state);
    return (state == ESP_OTA_IMG_PENDING_VERIFY);
}

} // namespace ota
