#pragma once

#include "auth/AuthConfig.hpp"
#include "common/Result.hpp"

#include <cstdint>
#include <string>

namespace auth {

/**
 * Manages the API password at runtime.
 *
 * Implements the developer-vs-operator ownership rule:
 *
 *   passwordChanged == false  →  developer owns the default.
 *     The default is always re-derived from the current AuthConfig and
 *     persisted on every boot.  Exception: Generated source keeps its
 *     existing NVS value so the password is stable across reboots.
 *
 *   passwordChanged == true   →  operator owns the password.
 *     The stored password is loaded from NVS unchanged, regardless of
 *     what the current AuthConfig says.
 *
 * Call init() once at boot before any other method.
 */
class AuthStore {
  public:
    /**
     * Initialise the store.
     *
     * @param authConfig  The compiled-in auth policy from AuthConfig.
     * @param mac         WiFi STA MAC address (6 bytes).  Used only when
     *                    authConfig.source() == Source::FromMac.
     */
    common::Result init(const AuthConfig &authConfig, const uint8_t mac[6]);

    /**
     * Returns true if the supplied password matches the stored password.
     * Comparison is constant-time to avoid timing side-channels.
     */
    bool verify(const std::string &password) const;

    /**
     * Change the password.  Persists the new value and sets passwordChanged
     * to true.  Subsequent boots will load this password from NVS regardless
     * of AuthConfig.
     */
    common::Result changePassword(const std::string &newPassword);

    /**
     * Returns the current effective password.
     * Exposed so the provisioning UI can display the default to the user.
     */
    const std::string &password() const { return password_; }

    /**
     * Returns true if the operator has changed the password at least once
     * via changePassword().  Used by the framework to enforce the
     * restrictIfDefault and requireChangeOnFirstBoot policies.
     */
    bool isPasswordChanged() const { return passwordChanged_; }

    /**
     * Returns the length of the current stored password.
     * Useful for diagnostics — avoids logging the actual password value.
     */
    size_t getPasswordLen() const { return password_.size(); }

  private:
    std::string deriveFromMac(const std::string &stub, const uint8_t mac[6]) const;
    std::string generateRandom() const;

    common::Result persistToNvs(const std::string &password, bool changed);
    common::Result loadFromNvs(std::string &out) const;

    std::string password_;
    bool        passwordChanged_ = false;

    static constexpr const char *NVS_NAMESPACE    = "auth";
    static constexpr const char *NVS_KEY_PASSWORD = "password";
    static constexpr const char *NVS_KEY_CHANGED  = "pw_changed";
};

} // namespace auth
