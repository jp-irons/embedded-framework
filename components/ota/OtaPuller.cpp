#include "ota/OtaPuller.hpp"

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
#include <cstring>
#include <string>

namespace ota {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr const char* NVS_NS      = "ota_pull";
static constexpr const char* NVS_KEY_URL = "base_url";
static constexpr size_t      VER_BUF_LEN = 32;   // version strings are tiny
static constexpr size_t      TASK_STACK  = 8192;  // TLS needs headroom

static logger::Logger log{"OtaPuller"};

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static OtaPullConfig s_config;
static std::string   s_activeUrl;

// ---------------------------------------------------------------------------
// Internal helpers
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
        if (OtaPuller::checkNow()) break;  // success — stop retrying early
    }

    // ── Periodic checks ───────────────────────────────────────────────────
    while (true) {
        vTaskDelay(pdMS_TO_TICKS((uint32_t)s_config.checkIntervalS * 1000UL));
        OtaPuller::checkNow();
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void OtaPuller::init(const OtaPullConfig& config) {
    s_config    = config;
    s_activeUrl = config.baseUrl;

    if (loadNvsUrl()) {
        log.info("init: NVS URL override active: %s", s_activeUrl.c_str());
    } else {
        log.info("init: using default URL: %s", s_activeUrl.c_str());
    }
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
    if (s_activeUrl.empty()) {
        log.error("checkNow: no base URL configured");
        return false;
    }

    const std::string versionUrl  = s_activeUrl + "/version.txt";
    const std::string firmwareUrl = s_activeUrl + "/firmware.bin";

    // ── Step 1: fetch remote version ──────────────────────────────────────
    log.info("checkNow: fetching %s", versionUrl.c_str());
    const std::string remote = trim(fetchSmall(versionUrl));
    if (remote.empty()) {
        log.error("checkNow: failed to fetch version.txt");
        return false;
    }

    // Successful outbound HTTPS fetch — the full network + TLS stack is healthy.
    // For pull OTA there may never be an inbound HTTP request to trigger the
    // EmbeddedServer's markValid(), so we call it here as the equivalent proof
    // of health.  OtaManager::markValid() is a no-op if already VALID.
    OtaManager::markValid();

    const std::string local = esp_app_get_description()->version;
    log.info("checkNow: local=%s  remote=%s", local.c_str(), remote.c_str());

    if (remote == local) {
        log.info("checkNow: firmware is up to date");
        return true;
    }

    // ── Step 2: download and flash ────────────────────────────────────────
    log.info("checkNow: update available — downloading %s", firmwareUrl.c_str());

    esp_http_client_config_t http_cfg = {};
    http_cfg.url               = firmwareUrl.c_str();
    http_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    http_cfg.timeout_ms        = 60000;  // firmware downloads can be slow
    http_cfg.buffer_size       = 8192;   // GitHub CDN presigned S3 URLs + headers can be large
    http_cfg.buffer_size_tx    = 2048;

    esp_https_ota_config_t ota_cfg = {};
    ota_cfg.http_config = &http_cfg;

    esp_err_t err = esp_https_ota(&ota_cfg);
    if (err != ESP_OK) {
        log.error("checkNow: esp_https_ota failed: %s", esp_err_to_name(err));
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

} // namespace ota
