#pragma once

#include "common/Result.hpp"

#include <string>

namespace auth {

/**
 * NVS-backed API key store for machine-to-machine access.
 *
 * A single API key is supported per device.  The key is a 64-character
 * lowercase hex string (32 random bytes from the hardware RNG), identical
 * in format to session tokens.
 *
 * Unlike session tokens, API keys:
 *   - Survive device reboots (stored in NVS under namespace "auth", key "api_key")
 *   - Have no idle timeout
 *   - Are intended for headless / automated clients
 *
 * Call init() once at boot.  If no key has been stored yet, isSet() returns
 * false until the operator explicitly generates one via generate().
 *
 * Validation is constant-time to prevent timing attacks.
 */
class ApiKeyStore {
  public:
    /**
     * Load any persisted API key from NVS.
     * Must be called before validate() / isSet().
     * Returns Ok on success, NotFound if no key is stored (not an error),
     * or an error code if NVS fails unexpectedly.
     */
    common::Result init();

    /**
     * Generate a new random API key, persist it to NVS, and return it.
     * The new key overwrites any previously stored key.
     * Returns the key string on success, or an empty string if NVS write fails.
     */
    std::string generate();

    /**
     * Returns true if the supplied token matches the stored API key.
     * Returns false immediately if no key has been generated yet.
     * Comparison is constant-time.
     */
    bool validate(const std::string &token) const;

    /**
     * Delete the stored API key from NVS and clear it from RAM.
     * After this call isSet() returns false until generate() is called again.
     */
    common::Result revoke();

    /** Returns true if an API key has been generated and is currently set. */
    bool isSet() const { return !key_.empty(); }

  private:
    static std::string generateToken();

    std::string key_;

    static constexpr const char *NVS_NAMESPACE = "auth";
    static constexpr const char *NVS_KEY       = "api_key";
};

} // namespace auth
