#pragma once

#include <cstdint>

namespace ota {

/**
 * Boot-time OTA guardian.
 *
 * With CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y the bootloader already handles
 * one-crash rollback automatically (PENDING_VERIFY → ABORTED → boot previous).
 * OtaManager adds on top of that:
 *
 *   - Multi-attempt tracking via RTC memory (survives soft resets, cleared on
 *     power-cycle).  Useful when the new image boots but hangs before reaching
 *     the health-check, so it never crashes but also never self-validates.
 *   - Deliberate escalation after MAX_BOOT_ATTEMPTS:
 *       1. Set previous VALID OTA partition as next-boot, reboot.
 *       2. If no previous VALID OTA partition exists, erase the OTA data
 *          partition (clears all OTA state) and reboot → factory image.
 *
 * Usage
 * ─────
 *   // Very early in app_main, before any tasks are started:
 *   ota::OtaManager::checkOnBoot();
 *
 *   // After WiFi + HTTP server are confirmed running:
 *   ota::OtaManager::markValid();
 */
class OtaManager {
  public:
    /// Maximum number of boot attempts allowed for a PENDING_VERIFY image
    /// before escalating to rollback/factory-reset.
    static constexpr uint32_t MAX_BOOT_ATTEMPTS = 3;

    /**
     * Check the running partition's OTA state early in startup.
     *
     * - If state is not PENDING_VERIFY: resets the attempt counter and returns.
     * - If state is PENDING_VERIFY: increments the RTC counter.
     * - If counter > MAX_BOOT_ATTEMPTS: attempts rollback, then factory reset.
     *
     * May call esp_restart() — does NOT return in the escalation path.
     */
    static void checkOnBoot();

    /**
     * Mark the running partition VALID and cancel the automatic rollback timer.
     * Call this once the system has passed its health checks (WiFi up, HTTP
     * server listening).  Idempotent — safe to call when already VALID.
     */
    static void markValid();

    /**
     * Returns true if the running partition is still PENDING_VERIFY
     * (i.e. markValid() has not been called yet this session).
     */
    static bool needsValidation();
};

} // namespace ota
