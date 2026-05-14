#pragma once

#include <cstdint>
#include <string>

namespace ota {

/**
 * Configuration supplied by the app at startup.
 *
 * baseUrl        — Base URL of the GitHub release asset directory, e.g.
 *                  "https://github.com/user/repo/releases/latest/download"
 *                  OtaPuller appends "/version.txt" and "/firmware.bin".
 *
 * checkIntervalS — Seconds between automatic background checks.
 *                  Set to 0 to disable the periodic task (manual / MQTT
 *                  triggered checks via checkNow() still work).
 */
struct OtaPullConfig {
    std::string baseUrl;
    uint32_t    checkIntervalS = 0;
};

/**
 * Pull-based OTA updater.
 *
 * Typical usage
 * ─────────────
 *   // In app startup, after Wi-Fi is confirmed up:
 *   ota::OtaPuller::init({
 *       .baseUrl        = "https://github.com/user/repo/releases/latest/download",
 *       .checkIntervalS = 3600,
 *   });
 *   ota::OtaPuller::start();   // starts the periodic background task
 *
 * The base URL can be overridden at runtime via setBaseUrl() — the new value
 * is persisted to NVS and survives reboots.  The compiled-in default from
 * OtaPullConfig is used when no NVS override exists.
 *
 * No ESP-IDF types appear in this header.
 */
class OtaPuller {
  public:
    /**
     * Initialise with the app-supplied default config.
     * Loads a previously saved NVS URL override if one exists; falls back to
     * config.baseUrl otherwise.  Must be called before start() or checkNow().
     */
    static void init(const OtaPullConfig& config);

    /**
     * Start the background periodic-check FreeRTOS task.
     * No-op if config.checkIntervalS == 0.
     */
    static void start();

    /**
     * Perform an immediate version check and update if a newer build exists.
     *
     * - Fetches {baseUrl}/version.txt over HTTPS.
     * - Compares with the running firmware version.
     * - If different: fetches and flashes {baseUrl}/firmware.bin, then reboots.
     *   Does not return on a successful update.
     * - If up to date: logs and returns true.
     * - On any network or flash error: logs and returns false.
     */
    static bool checkNow();

    /**
     * Persist a new base URL to NVS, replacing the compiled-in default.
     * Takes effect immediately for subsequent checkNow() calls.
     */
    static bool setBaseUrl(const std::string& url);

    /** Return the currently active base URL (NVS override or compiled default). */
    static std::string getBaseUrl();
};

} // namespace ota
