#include "ota/OtaPuller.hpp"

// TODO: OtaPuller currently calls ESP-IDF APIs directly (esp_http_client,
// esp_https_ota, esp_crt_bundle, esp_app_desc, nvs, FreeRTOS tasks).
// These should be abstracted behind interfaces in esp_platform (following the
// pattern of EspNvsStore, EspTimerInterface, EspWiFiInterface, etc.) so that
// OtaPuller depends only on portable interfaces and can be tested without
// hardware.  Candidate interfaces:
//   - OtaTransport  (fetch version.txt, stream firmware.bin)
//   - OtaFlashWriter (wrap esp_https_ota_begin/perform/finish)
//   - A KeyValueStore injection (already exists) replacing raw nvs_open calls
//   - Task scheduling via TimerInterface or a new TaskInterface

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_system.h"
#include "ota/OtaManager.hpp"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep — must precede task.h
#include "freertos/task.h"
#include "logger/Logger.hpp"
#include "nvs.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

namespace ota {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr const char* NVS_NS              = "ota_pull";
static constexpr const char* NVS_KEY_URL         = "base_url";
static constexpr const char* NVS_KEY_AUTO_UPD    = "auto_upd_en";
static constexpr size_t      VER_BUF_LEN         = 32;   // version strings are tiny
static constexpr size_t      TASK_STACK          = 8192;  // TLS needs headroom

static logger::Logger log{"OtaPuller"};

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static OtaPullConfig     s_config;
static std::string       s_activeUrl;
static std::atomic<bool> s_autoUpdateEnabled{true};
static bool              s_uiSettable = true;

// ── Pull-check state machine ──────────────────────────────────────────────
//
// Written by the OTA task (checkNow / markCheckStarted); read by the HTTP
// handler (handlePullCheckStatus).  s_checkMessage is written *before*
// s_checkState is updated (release store), so the HTTP task always sees a
// consistent pair when it reads state (acquire load) then message.

static std::atomic<int>    s_checkState     {static_cast<int>(PullCheckState::Idle)};
static char                s_checkMessage   [64] = {};
static std::atomic<bool>   s_checkRunning   {false};
static std::atomic<size_t> s_downloadedBytes{0};
static std::atomic<size_t> s_totalBytes     {0};

// Captured via http_client_init_cb so we can read Content-Length from the
// response headers after esp_https_ota_begin() completes the HTTP exchange.
// Only written/read from within checkNow(), which is single-instance (guard).
static esp_http_client_handle_t s_otaHttpClient = nullptr;

static esp_err_t otaHttpInitCb(esp_http_client_handle_t client) {
    s_otaHttpClient = client;
    return ESP_OK;
}

static void setCheckState(PullCheckState state, const char* msg = "") {
    strncpy(s_checkMessage, msg, sizeof(s_checkMessage) - 1);
    s_checkMessage[sizeof(s_checkMessage) - 1] = '\0';
    s_checkState.store(static_cast<int>(state), std::memory_order_release);
}

// RAII guard that claims s_checkRunning on construction and releases it on
// destruction, covering all early-return paths inside checkNow() without
// requiring a manual clear before every return statement.
struct CheckGuard {
    const bool acquired;
    CheckGuard()
        : acquired([]() {
              bool expected = false;
              return s_checkRunning.compare_exchange_strong(
                  expected, true,
                  std::memory_order_acq_rel,
                  std::memory_order_relaxed);
          }()) {}
    ~CheckGuard() {
        if (acquired) s_checkRunning.store(false, std::memory_order_release);
    }
    // Non-copyable
    CheckGuard(const CheckGuard&)            = delete;
    CheckGuard& operator=(const CheckGuard&) = delete;
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Semantic version helpers
// ---------------------------------------------------------------------------

struct SemVer {
    int  major = 0;
    int  minor = 0;
    int  patch = 0;
    bool valid = false;
};

/**
 * Parse a version string of the form "X.Y.Z" or "vX.Y.Z".
 * Returns an invalid SemVer if the string cannot be parsed.
 */
static SemVer parseSemVer(const std::string& s) {
    SemVer v;
    const char* p = s.c_str();
    if (*p == 'v' || *p == 'V') ++p;   // strip optional leading 'v'
    if (std::sscanf(p, "%d.%d.%d", &v.major, &v.minor, &v.patch) == 3) {
        v.valid = true;
    }
    return v;
}

/**
 * Returns true only if @p remote is strictly newer than @p local.
 * Both must be valid SemVers; if either is unparseable returns false
 * (safe default — do not update when version format is unknown).
 */
static bool isRemoteNewer(const SemVer& remote, const SemVer& local) {
    if (!remote.valid || !local.valid) return false;
    if (remote.major != local.major) return remote.major > local.major;
    if (remote.minor != local.minor) return remote.minor > local.minor;
    return remote.patch > local.patch;
}

// ---------------------------------------------------------------------------

/** Strip leading/trailing whitespace (including CR/LF) from a string. */
static std::string trim(const std::string& s) {
    static constexpr std::string_view WS = " \t\r\n";
    const size_t start = s.find_first_not_of(WS);
    if (start == std::string::npos) return "";
    const size_t end = s.find_last_not_of(WS);
    return s.substr(start, end - start + 1);
}

/**
 * Event handler for esp_http_client_perform().
 * Captures up to VER_BUF_LEN bytes of the response body into user_data.
 */
struct FetchCtx {
    char buf[VER_BUF_LEN + 1];
    int  len;
};

static esp_err_t onFetchEvent(esp_http_client_event_t* evt) {
    auto* ctx = static_cast<FetchCtx*>(evt->user_data);
    if (evt->event_id == HTTP_EVENT_ON_DATA && ctx->len < (int)VER_BUF_LEN) {
        int copy = std::min((int)evt->data_len, (int)VER_BUF_LEN - ctx->len);
        memcpy(ctx->buf + ctx->len, evt->data, copy);
        ctx->len += copy;
    }
    return ESP_OK;
}

/**
 * Fetch a small HTTPS resource and return its body as a string.
 * Follows redirects; uses the ESP certificate bundle for TLS.
 * Returns an empty string on any error.
 */
static std::string fetchSmall(const std::string& url) {
    FetchCtx ctx = {};

    esp_http_client_config_t cfg = {};
    cfg.url               = url.c_str();
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.timeout_ms        = 10000;
    cfg.buffer_size       = 8192;  // GitHub CDN presigned S3 URLs + headers can be large
    cfg.buffer_size_tx    = 2048;
    cfg.event_handler     = onFetchEvent;
    cfg.user_data         = &ctx;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        log.error("fetchSmall: esp_http_client_init failed for %s", url.c_str());
        return "";
    }

    esp_err_t err    = esp_http_client_perform(client);
    int       status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        log.error("fetchSmall: perform failed: %s", esp_err_to_name(err));
        return "";
    }
    if (status != 200) {
        log.warn("fetchSmall: HTTP %d from %s", status, url.c_str());
        return "";
    }

    ctx.buf[ctx.len] = '\0';
    return std::string(ctx.buf, ctx.len);
}

/** Attempt to load a saved URL override from NVS into s_activeUrl. */
static bool loadNvsUrl() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;

    size_t len = 0;
    if (nvs_get_str(h, NVS_KEY_URL, nullptr, &len) != ESP_OK || len == 0) {
        nvs_close(h);
        return false;
    }

    std::string buf(len, '\0');
    esp_err_t err = nvs_get_str(h, NVS_KEY_URL, buf.data(), &len);
    nvs_close(h);

    if (err != ESP_OK) return false;

    // NVS includes the null terminator in len — strip it
    if (!buf.empty() && buf.back() == '\0') buf.pop_back();

    s_activeUrl = buf;
    return true;
}

/**
 * FreeRTOS task: initial check shortly after boot, then periodic checks.
 *
 * The initial check is what marks a freshly-flashed partition as valid —
 * it retries every 30 s (up to 5 attempts) to give Wi-Fi time to connect
 * before falling through to the normal hourly cadence.
 */
static void pullTask(void* /*arg*/) {
    // ── Initial check ────────────────────────────────────────────────────
    // Retry a few times in case Wi-Fi hasn't finished connecting yet.
    static constexpr int     INITIAL_RETRIES    = 5;
    static constexpr uint32_t INITIAL_DELAY_MS  = 30000;  // 30 s between attempts

    for (int attempt = 0; attempt < INITIAL_RETRIES; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(INITIAL_DELAY_MS));
        if (!s_autoUpdateEnabled.load(std::memory_order_acquire)) {
            log.info("pullTask: auto-update disabled — skipping initial check");
            break;
        }
        if (OtaPuller::checkNow()) break;  // success — stop retrying early
    }

    // ── Periodic checks ───────────────────────────────────────────────────
    while (true) {
        vTaskDelay(pdMS_TO_TICKS((uint32_t)s_config.checkIntervalS * 1000UL));
        if (s_autoUpdateEnabled.load(std::memory_order_acquire)) {
            OtaPuller::checkNow();
        } else {
            log.info("pullTask: auto-update disabled — skipping periodic check");
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void OtaPuller::init(const OtaPullConfig& config) {
    s_config     = config;
    s_activeUrl  = config.baseUrl;
    s_uiSettable = config.uiSettable;

    if (loadNvsUrl()) {
        log.info("init: NVS URL override active: %s", s_activeUrl.c_str());
    } else {
        log.info("init: using default URL: %s", s_activeUrl.c_str());
    }

    // Auto-update: start from the app-supplied default, then reconcile with NVS.
    //
    // uiSettable=true  — the user may have changed the setting at runtime; load
    //                    any persisted NVS value and let it override the default.
    //
    // uiSettable=false — the config value is authoritative.  Write it to NVS so
    //                    that any stale user preference from a previous deployment
    //                    (when uiSettable was true) is overwritten.  If uiSettable
    //                    is later set back to true, NVS will reflect the developer's
    //                    intended default rather than a stale toggle state.
    s_autoUpdateEnabled.store(config.autoUpdateEnabled, std::memory_order_release);
    if (config.uiSettable) {
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
            uint8_t val = 0;
            if (nvs_get_u8(h, NVS_KEY_AUTO_UPD, &val) == ESP_OK) {
                s_autoUpdateEnabled.store(val != 0, std::memory_order_release);
                log.info("init: auto-update NVS override: %s", val ? "enabled" : "disabled");
            }
            nvs_close(h);
        }
    } else {
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
            nvs_set_u8(h, NVS_KEY_AUTO_UPD, config.autoUpdateEnabled ? 1 : 0);
            nvs_commit(h);
            nvs_close(h);
            log.info("init: auto-update locked by config (%s) — NVS updated",
                     config.autoUpdateEnabled ? "enabled" : "disabled");
        }
    }
    log.info("init: auto-update=%s uiSettable=%s",
             s_autoUpdateEnabled.load() ? "on" : "off",
             s_uiSettable ? "yes" : "no");
}

void OtaPuller::start() {
    if (s_config.checkIntervalS == 0) {
        log.info("start: periodic checks disabled");
        return;
    }
    log.info("start: check every %us", (unsigned)s_config.checkIntervalS);
    xTaskCreate(pullTask, "ota_pull", TASK_STACK, nullptr,
                tskIDLE_PRIORITY + 1, nullptr);
}

bool OtaPuller::checkNow() {
    const CheckGuard guard;
    if (!guard.acquired) {
        log.warn("checkNow: check already in progress — skipping");
        return false;
    }

    if (s_activeUrl.empty()) {
        log.error("checkNow: no base URL configured");
        setCheckState(PullCheckState::Error, "No URL configured");
        return false;
    }

    setCheckState(PullCheckState::Checking);

    const std::string versionUrl  = s_activeUrl + "/version.txt";
    const std::string firmwareUrl = s_activeUrl + "/firmware.bin";

    // ── Step 1: fetch remote version ──────────────────────────────────────
    log.info("checkNow: fetching %s", versionUrl.c_str());
    const std::string remote = trim(fetchSmall(versionUrl));
    if (remote.empty()) {
        log.error("checkNow: failed to fetch version.txt");
        setCheckState(PullCheckState::Error, "Failed to fetch version info");
        return false;
    }

    // Successful outbound HTTPS fetch — the full network + TLS stack is healthy.
    // For pull OTA there may never be an inbound HTTP request to trigger the
    // EmbeddedServer's markValid(), so we call it here as the equivalent proof
    // of health.  OtaManager::markValid() is a no-op if already VALID.
    OtaManager::markValid();

    const std::string local = esp_app_get_description()->version;
    log.info("checkNow: local=%s  remote=%s", local.c_str(), remote.c_str());

    const SemVer localVer  = parseSemVer(local);
    const SemVer remoteVer = parseSemVer(remote);

    if (!localVer.valid || !remoteVer.valid) {
        log.warn("checkNow: cannot parse version strings — skipping update");
        setCheckState(PullCheckState::Error, "Cannot parse version strings");
        return true;
    }

    if (!isRemoteNewer(remoteVer, localVer)) {
        log.info("checkNow: remote %s is not newer than local %s — skipping",
                 remote.c_str(), local.c_str());
        setCheckState(PullCheckState::UpToDate, remote.c_str());
        return true;
    }

    // ── Step 2: download and flash ────────────────────────────────────────
    log.info("checkNow: update available — downloading %s", firmwareUrl.c_str());
    setCheckState(PullCheckState::Downloading, remote.c_str());
    s_downloadedBytes.store(0, std::memory_order_release);
    s_totalBytes.store(0, std::memory_order_release);
    s_otaHttpClient = nullptr;

    esp_http_client_config_t http_cfg = {};
    http_cfg.url               = firmwareUrl.c_str();
    http_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    http_cfg.timeout_ms        = 60000;  // firmware downloads can be slow
    http_cfg.buffer_size       = 8192;   // GitHub CDN presigned S3 URLs + headers can be large
    http_cfg.buffer_size_tx    = 2048;

    esp_https_ota_config_t ota_cfg = {};
    ota_cfg.http_config          = &http_cfg;
    ota_cfg.http_client_init_cb  = otaHttpInitCb;  // saves handle so we can read Content-Length

    // Use the advanced API so we can report download progress via s_downloadedBytes.
    esp_https_ota_handle_t ota_handle = nullptr;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &ota_handle);
    if (err != ESP_OK) {
        log.error("checkNow: esp_https_ota_begin failed: %s", esp_err_to_name(err));
        setCheckState(PullCheckState::Error, "Download failed");
        return false;
    }

    // begin() has completed the HTTP exchange including redirect to CDN —
    // Content-Length is now available from the response headers.
    if (s_otaHttpClient) {
        const int64_t cl = esp_http_client_get_content_length(s_otaHttpClient);
        if (cl > 0) {
            s_totalBytes.store(static_cast<size_t>(cl), std::memory_order_release);
            log.info("checkNow: firmware content-length: %lld bytes", cl);
        }
    }

    while (true) {
        err = esp_https_ota_perform(ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;
        s_downloadedBytes.store(
            static_cast<size_t>(esp_https_ota_get_image_len_read(ota_handle)),
            std::memory_order_release);
    }

    if (err != ESP_OK) {
        log.error("checkNow: esp_https_ota_perform failed: %s", esp_err_to_name(err));
        esp_https_ota_abort(ota_handle);
        setCheckState(PullCheckState::Error, "Download failed");
        return false;
    }

    if (!esp_https_ota_is_complete_data_received(ota_handle)) {
        log.error("checkNow: incomplete OTA data received");
        esp_https_ota_abort(ota_handle);
        setCheckState(PullCheckState::Error, "Incomplete download");
        return false;
    }

    err = esp_https_ota_finish(ota_handle);
    if (err != ESP_OK) {
        log.error("checkNow: esp_https_ota_finish failed: %s", esp_err_to_name(err));
        setCheckState(PullCheckState::Error, "Flash failed");
        return false;
    }

    log.info("checkNow: flash successful — rebooting");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return true;  // unreachable
}

bool OtaPuller::setBaseUrl(const std::string& url) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        log.error("setBaseUrl: nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(h, NVS_KEY_URL, url.c_str());
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err != ESP_OK) {
        log.error("setBaseUrl: NVS write failed: %s", esp_err_to_name(err));
        return false;
    }

    s_activeUrl = url;
    log.info("setBaseUrl: saved and active: %s", url.c_str());
    return true;
}

std::string OtaPuller::getBaseUrl() {
    return s_activeUrl;
}

// ---------------------------------------------------------------------------
// Pull-check status
// ---------------------------------------------------------------------------

void OtaPuller::markCheckStarted() {
    // Called by the API handler before spawning the check task so that the
    // very first poll after a manual request sees Checking, not Idle.
    setCheckState(PullCheckState::Checking);
}

PullCheckState OtaPuller::checkState() {
    return static_cast<PullCheckState>(
        s_checkState.load(std::memory_order_acquire));
}

const char* OtaPuller::checkMessage() {
    // Safe to read after checkState() due to acquire/release ordering.
    return s_checkMessage;
}

size_t OtaPuller::downloadedBytes() {
    return s_downloadedBytes.load(std::memory_order_acquire);
}

size_t OtaPuller::totalBytes() {
    return s_totalBytes.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------------
// Auto-update enable / disable
// ---------------------------------------------------------------------------

bool OtaPuller::isAutoUpdateEnabled() {
    return s_autoUpdateEnabled.load(std::memory_order_acquire);
}

bool OtaPuller::isUiSettable() {
    return s_uiSettable;
}

bool OtaPuller::setAutoUpdateEnabled(bool enabled) {
    if (!s_uiSettable) {
        log.warn("setAutoUpdateEnabled: setting is not UI-settable — ignoring");
        return false;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        log.error("setAutoUpdateEnabled: nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_u8(h, NVS_KEY_AUTO_UPD, enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err != ESP_OK) {
        log.error("setAutoUpdateEnabled: NVS write failed: %s", esp_err_to_name(err));
        return false;
    }

    s_autoUpdateEnabled.store(enabled, std::memory_order_release);
    log.info("setAutoUpdateEnabled: %s", enabled ? "enabled" : "disabled");
    return true;
}

} // namespace ota
