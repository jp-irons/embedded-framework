#include "esp_platform/EspNvsStore.hpp"
#include "esp_platform/EspTypeAdapter.hpp"

#include "nvs.h"

#include <vector>

namespace esp_platform {

using common::Result;

EspNvsStore::EspNvsStore(const char* nvsNamespace)
    : ns_(nvsNamespace) {}

// ---------------------------------------------------------------------------
// String
// ---------------------------------------------------------------------------

Result EspNvsStore::getString(const char* key, std::string& out) const {
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns_, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return Result::NotFound;
    if (err != ESP_OK)                return toResult(err);

    size_t len = 0;
    err = nvs_get_str(h, key, nullptr, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) { nvs_close(h); return Result::NotFound; }
    if (err != ESP_OK)                { nvs_close(h); return toResult(err); }

    std::vector<char> tmp(len);
    err = nvs_get_str(h, key, tmp.data(), &len);
    nvs_close(h);
    if (err != ESP_OK) return toResult(err);

    out.assign(tmp.data(), len - 1); // strip NUL terminator
    return Result::Ok;
}

Result EspNvsStore::setString(const char* key, const std::string& value) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns_, NVS_READWRITE, &h);
    if (err != ESP_OK) return toResult(err);

    err = nvs_set_str(h, key, value.c_str());
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return toResult(err);
}

// ---------------------------------------------------------------------------
// Unsigned byte
// ---------------------------------------------------------------------------

Result EspNvsStore::getU8(const char* key, uint8_t& out) const {
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns_, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return Result::NotFound;
    if (err != ESP_OK)                return toResult(err);

    err = nvs_get_u8(h, key, &out);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return Result::NotFound;
    return toResult(err);
}

Result EspNvsStore::setU8(const char* key, uint8_t value) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns_, NVS_READWRITE, &h);
    if (err != ESP_OK) return toResult(err);

    err = nvs_set_u8(h, key, value);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return toResult(err);
}

// ---------------------------------------------------------------------------
// Blob
// ---------------------------------------------------------------------------

Result EspNvsStore::getBlob(const char* key, std::vector<uint8_t>& out) const {
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns_, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return Result::NotFound;
    if (err != ESP_OK)                return toResult(err);

    size_t size = 0;
    err = nvs_get_blob(h, key, nullptr, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) { nvs_close(h); return Result::NotFound; }
    if (err != ESP_OK)                { nvs_close(h); return toResult(err); }

    out.resize(size);
    err = nvs_get_blob(h, key, out.data(), &size);
    nvs_close(h);
    if (err != ESP_OK) { out.clear(); return toResult(err); }
    return Result::Ok;
}

Result EspNvsStore::setBlob(const char* key, const uint8_t* data, size_t len) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns_, NVS_READWRITE, &h);
    if (err != ESP_OK) return toResult(err);

    err = nvs_set_blob(h, key, data, len);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return toResult(err);
}

// ---------------------------------------------------------------------------
// Erase
// ---------------------------------------------------------------------------

Result EspNvsStore::eraseKey(const char* key) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns_, NVS_READWRITE, &h);
    if (err != ESP_OK) return toResult(err);

    err = nvs_erase_key(h, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK; // already gone — not an error
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return toResult(err);
}

} // namespace esp_platform
