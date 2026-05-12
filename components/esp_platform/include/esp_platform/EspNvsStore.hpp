#pragma once

#include "common/KeyValueStore.hpp"

namespace esp_platform {

/**
 * ESP-IDF NVS concrete implementation of KeyValueStore.
 *
 * Each instance is bound to a single NVS namespace supplied at construction.
 * All write operations commit immediately.
 *
 * All NVS and esp_err_t types are confined to EspNvsStore.cpp.
 */
class EspNvsStore : public common::KeyValueStore {
  public:
    explicit EspNvsStore(const char* nvsNamespace);

    common::Result getString(const char* key, std::string& out) const override;
    common::Result setString(const char* key, const std::string& value)  override;

    common::Result getU8(const char* key, uint8_t& out) const override;
    common::Result setU8(const char* key, uint8_t value)        override;

    common::Result getBlob(const char* key, std::vector<uint8_t>& out) const override;
    common::Result setBlob(const char* key, const uint8_t* data, size_t len) override;

    common::Result eraseKey(const char* key) override;

  private:
    const char* ns_;
};

} // namespace esp_platform
