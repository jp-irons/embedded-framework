#pragma once

#include <string>

namespace device {

/**
 * Persists user-configurable device identity settings (hostname prefix,
 * AP SSID prefix) in NVS under the "device_cfg" namespace.
 *
 * Usage:
 *   FrameworkContext::start() calls init() with the app-set defaults.
 *   init() loads any NVS-stored user override; otherwise keeps the defaults.
 *   FrameworkContext::start() then calls setEffectiveNames() with the
 *   post-suffix strings so the GET endpoint can report them.
 *   DeviceApiHandler calls save*() on POST and reads config() on GET.
 */
class DeviceConfigStore {
  public:
    struct Config {
        std::string hostnamePrefix;    // resolved prefix (NVS override or app default)
        std::string apSsidPrefix;      // resolved prefix (NVS override or app default)
        std::string effectiveHostname; // final hostname (with or without MAC suffix)
        std::string effectiveApSsid;   // final AP SSID  (with or without MAC suffix)
        bool hostnameFromNvs = false;  // true → user set this; suffix suppressed
        bool apSsidFromNvs   = false;  // true → user set this; suffix suppressed
    };

    /**
     * Load NVS-stored overrides; fall back to the supplied app defaults if no
     * override is present.  Must be called from FrameworkContext::start() before
     * applySuffix() is called.
     */
    static void init(const std::string& defaultHostnamePrefix,
                     const std::string& defaultApSsidPrefix);

    /**
     * Store the final effective (post-suffix) names.
     * Called by FrameworkContext::start() after applySuffix() is run.
     */
    static void setEffectiveNames(const std::string& hostname,
                                  const std::string& apSsid);

    /** Persist a new hostname prefix to NVS.  Returns true on success. */
    static bool saveHostnamePrefix(const std::string& prefix);

    /** Persist a new AP SSID prefix to NVS.  Returns true on success. */
    static bool saveApSsidPrefix(const std::string& prefix);

    /** Delete the NVS hostname override — reverts to the app default on next boot. */
    static bool clearHostnamePrefix();

    /** Delete the NVS AP SSID override — reverts to the app default on next boot. */
    static bool clearApSsidPrefix();

    /** Current resolved configuration (prefixes + effective names). */
    static const Config& config();

  private:
    static Config      s_config_;
    static std::string s_defaultHostnamePrefix_;
    static std::string s_defaultApSsidPrefix_;
};

} // namespace device
