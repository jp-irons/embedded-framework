#pragma once

#include "common/Result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace common {

/**
 * Abstract namespace-scoped key-value store.
 *
 * Each instance represents a single named namespace.  All write operations
 * are committed immediately (no explicit commit step required).
 *
 * String values are NUL-terminated C strings stored as std::string.
 * Blob values are raw byte buffers.
 *
 * Returns Result::NotFound when a key does not exist (not an error in
 * contexts such as first-boot initialisation).
 *
 * Concrete implementation: esp_platform::EspNvsStore.
 */
class KeyValueStore {
  public:
    virtual ~KeyValueStore() = default;

    // -----------------------------------------------------------------------
    // String
    // -----------------------------------------------------------------------

    virtual Result getString(const char* key, std::string& out) const = 0;
    virtual Result setString(const char* key, const std::string& value) = 0;

    // -----------------------------------------------------------------------
    // Unsigned byte
    // -----------------------------------------------------------------------

    virtual Result getU8(const char* key, uint8_t& out) const = 0;
    virtual Result setU8(const char* key, uint8_t value) = 0;

    // -----------------------------------------------------------------------
    // Blob
    // -----------------------------------------------------------------------

    virtual Result getBlob(const char* key, std::vector<uint8_t>& out) const = 0;
    virtual Result setBlob(const char* key, const uint8_t* data, size_t len) = 0;

    // -----------------------------------------------------------------------
    // Erase
    // -----------------------------------------------------------------------

    /**
     * Erase a single key.
     * Returns Ok if the key was erased, or if it did not exist.
     */
    virtual Result eraseKey(const char* key) = 0;
};

} // namespace common
