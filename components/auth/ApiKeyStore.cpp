#include "auth/ApiKeyStore.hpp"

#include "device/EspTypeAdapter.hpp"
#include "logger/Logger.hpp"

#include "esp_random.h"
#include "nvs.h"

#include <cstdio>
#include <vector>

namespace auth {

using namespace common;

static logger::Logger log{"ApiKeyStore"};

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::string ApiKeyStore::generateToken() {
    uint8_t bytes[32];
    esp_fill_random(bytes, sizeof(bytes));

    char hex[65];
    for (int i = 0; i < 32; ++i) {
        snprintf(hex + i * 2, 3, "%02x", bytes[i]);
    }
    return std::string(hex, 64);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Result ApiKeyStore::init() {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        log.debug("No API key stored (namespace not found)");
        return Result::NotFound;
    }
    if (err != ESP_OK) {
        log.warn("NVS open failed (%d)", (int)err);
        return device::toResult(err);
    }

    size_t len = 0;
    err = nvs_get_str(h, NVS_KEY, nullptr, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        log.debug("No API key stored (key not found)");
        return Result::NotFound;
    }
    if (err != ESP_OK) {
        nvs_close(h);
        return device::toResult(err);
    }

    std::vector<char> tmp(len);
    err = nvs_get_str(h, NVS_KEY, tmp.data(), &len);
    nvs_close(h);
    if (err != ESP_OK) {
        return device::toResult(err);
    }

    key_.assign(tmp.data(), len - 1); // strip null terminator
    log.info("API key loaded from NVS");
    return Result::Ok;
}

std::string ApiKeyStore::generate() {
    const std::string token = generateToken();

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        log.error("NVS open failed, cannot persist API key (%d)", (int)err);
        return {};
    }

    err = nvs_set_str(h, NVS_KEY, token.c_str());
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err != ESP_OK) {
        log.error("NVS write failed, API key not persisted (%d)", (int)err);
        return {};
    }

    key_ = token;
    log.info("New API key generated and persisted");
    return token;
}

bool ApiKeyStore::validate(const std::string &token) const {
    if (key_.empty() || key_.size() != token.size()) {
        return false;
    }

    // Constant-time comparison
    uint8_t diff = 0;
    for (size_t i = 0; i < key_.size(); ++i) {
        diff |= static_cast<uint8_t>(key_[i]) ^ static_cast<uint8_t>(token[i]);
    }
    return diff == 0;
}

Result ApiKeyStore::revoke() {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return device::toResult(err);
    }

    err = nvs_erase_key(h, NVS_KEY);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Nothing stored — treat as success
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err == ESP_OK) {
        key_.clear();
        log.info("API key revoked");
    } else {
        log.warn("Failed to revoke API key from NVS (%d)", (int)err);
    }

    return device::toResult(err);
}

} // namespace auth
