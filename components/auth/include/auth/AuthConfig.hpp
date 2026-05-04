#pragma once

#include <string>

namespace auth {

/**
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  SECURITY NOTICE — READ BEFORE CHANGING                                 ║
 * ║                                                                          ║
 * ║  AuthConfig is consumed once at FrameworkContext construction and is     ║
 * ║  immutable thereafter.  It has no public setters by design.              ║
 * ║                                                                          ║
 * ║  Weakening these settings — supplying an empty password, disabling       ║
 * ║  both policy flags, or swapping to a different AuthConfig between        ║
 * ║  boots — removes all API access control.                                 ║
 * ║                                                                          ║
 * ║  Things may go horribly wrong if you ignore this warning.                ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 *
 * Describes how the framework should handle API authentication.
 *
 * Create via one of the three factory methods, then chain optional policy flags:
 *
 *   // Fixed password — simplest, good for development
 *   AuthConfig::withPassword("my-secret")
 *
 *   // Unique per device, derived from MAC.  No NVS required.
 *   AuthConfig::fromMac()             // MAC bytes only
 *   AuthConfig::fromMac("myapp")      // Recommended when multiple apps share
 *                                     // the same framework on the same device.
 *                                     // Hashes "myapp" + MAC so passwords
 *                                     // differ across apps.
 *
 *   // Random, generated on first boot, persisted in NVS
 *   AuthConfig::generated()
 *
 * Chain optional policy flags:
 *
 *   AuthConfig::fromMac("myapp")
 *       .restrictIfDefault(true)
 *       .requireChangeOnFirstBoot(true)
 */
class AuthConfig {
  public:

    // ── Factory methods ───────────────────────────────────────────────────

    /**
     * Fixed password supplied by the app developer.
     * The same password is used on every device and every boot.
     */
    static AuthConfig withPassword(const std::string &password) {
        AuthConfig cfg;
        cfg.source_        = Source::Fixed;
        cfg.fixedPassword_ = password;
        return cfg;
    }

    /**
     * Password derived from the device's WiFi STA MAC address.
     * Unique per device without requiring NVS.  Stable across reboots.
     *
     * @param stub  Optional string mixed into the derivation.  Use this when
     *              multiple apps are built on the same framework so that each
     *              app produces a different password on the same hardware.
     *              e.g. AuthConfig::fromMac("myapp")
     */
    static AuthConfig fromMac(const std::string &stub = "") {
        AuthConfig cfg;
        cfg.source_   = Source::FromMac;
        cfg.macStub_  = stub;
        return cfg;
    }

    /**
     * Password generated randomly on first boot and stored in NVS.
     * Subsequent boots load the same password from NVS.
     *
     * The generated password is displayed in the AP-mode provisioning UI
     * so the user can record it before the device connects to their network.
     */
    static AuthConfig generated() {
        AuthConfig cfg;
        cfg.source_ = Source::Generated;
        return cfg;
    }

    // ── Policy flags (chainable) ──────────────────────────────────────────

    /**
     * When true, restricts destructive endpoints (OTA upload, reboot, NVS
     * clear, credential and password changes) if the password has never been
     * changed from its initial default value.  Read-only endpoints remain
     * accessible.
     *
     * Default: false
     */
    AuthConfig &restrictIfDefault(bool value = true) {
        restrictIfDefault_ = value;
        return *this;
    }

    /**
     * When true, blocks all protected endpoints with HTTP 403 until the
     * password has been changed at least once via the change-password
     * endpoint (POST /framework/api/auth/password).
     *
     * Default: false
     */
    AuthConfig &requireChangeOnFirstBoot(bool value = true) {
        requireChangeOnFirstBoot_ = value;
        return *this;
    }

    // ── Accessors (used internally by the framework — not for app code) ───

    enum class Source {
        Fixed,      ///< Developer-supplied fixed string
        FromMac,    ///< Derived from MAC address + optional stub
        Generated   ///< Random, generated on first boot, stored in NVS
    };

    Source             source()                     const { return source_; }
    const std::string &fixedPassword()              const { return fixedPassword_; }
    const std::string &macStub()                    const { return macStub_; }
    bool               isRestrictIfDefault()        const { return restrictIfDefault_; }
    bool               isRequireChangeOnFirstBoot() const { return requireChangeOnFirstBoot_; }

  private:
    AuthConfig() = default;

    Source      source_                    = Source::Generated;
    std::string fixedPassword_;
    std::string macStub_;
    bool        restrictIfDefault_         = false;
    bool        requireChangeOnFirstBoot_  = false;
};

} // namespace auth
