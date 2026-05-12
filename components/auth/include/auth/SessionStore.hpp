#pragma once

#include "device/ClockInterface.hpp"
#include "device/RandomInterface.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace auth {

/**
 * In-RAM session token store for browser-initiated sessions.
 *
 * Sessions are created after a successful password login via
 * POST /framework/api/auth/login and presented as Bearer tokens on
 * subsequent framework API requests.
 *
 * All sessions are lost on device reboot (intentional — sessionStorage on
 * the browser side mirrors this lifetime).  invalidateAll() is called on
 * password change to force re-login on all open browser tabs.
 *
 * Tokens are 64 lowercase hex characters (32 random bytes from the
 * hardware RNG).  Validation is constant-time to prevent timing attacks.
 *
 * When the store is full the oldest (least-recently-seen) session is
 * silently evicted to make room.
 *
 * Call init() once at boot before create() / validate().
 */
class SessionStore {
  public:
    /**
     * Bind the store to its RNG and clock.
     * Must be called before create() / validate().
     */
    void init(device::RandomInterface& rng, device::ClockInterface& clock);

    /**
     * Generate a new session token and record it in the store.
     * @returns the raw token string — send this to the client once.
     */
    std::string create();

    /**
     * Returns true if the token is known and has not exceeded the idle
     * timeout.  Updates the last-seen timestamp on success.
     * Comparison is constant-time.
     */
    bool validate(const std::string& token);

    /** Remove a specific token (logout). */
    void invalidate(const std::string& token);

    /** Remove all tokens — call on password change. */
    void invalidateAll();

  private:
    struct Session {
        std::string token;
        int64_t     createdAt = 0; // monotonic microseconds since boot
        int64_t     lastSeen  = 0;
        bool        active    = false;
    };

    // Maximum concurrent sessions.  When full the oldest is evicted.
    static constexpr size_t  MAX_SESSIONS    = 8;

    // Idle timeout: a session not seen within this window is expired.
    static constexpr int64_t IDLE_TIMEOUT_US = 3600LL * 1000000LL; // 1 hour

    std::array<Session, MAX_SESSIONS> sessions_{};

    device::RandomInterface* rng_   = nullptr;
    device::ClockInterface*  clock_ = nullptr;

    std::string generateToken();
};

} // namespace auth
