#include "auth/SessionStore.hpp"

#include "logger/Logger.hpp"

#include <algorithm>
#include <cstdio>

namespace auth {

static logger::Logger log{"SessionStore"};

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void SessionStore::init(device::RandomInterface& rng, device::ClockInterface& clock) {
    rng_   = &rng;
    clock_ = &clock;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::string SessionStore::generateToken() {
    uint8_t bytes[32];
    rng_->fillRandom(bytes, sizeof(bytes));

    char hex[65];
    for (int i = 0; i < 32; ++i) {
        snprintf(hex + i * 2, 3, "%02x", bytes[i]);
    }
    return std::string(hex, 64);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::string SessionStore::create() {
    const int64_t     now   = clock_->nowUs();
    const std::string token = generateToken();

    // Find a free slot first
    for (auto& s : sessions_) {
        if (!s.active) {
            s = {token, now, now, true};
            log.info("Session created");
            return token;
        }
    }

    // Store full — evict the least-recently-seen session
    auto oldest = std::min_element(sessions_.begin(), sessions_.end(),
        [](const Session& a, const Session& b) {
            return a.lastSeen < b.lastSeen;
        });
    log.warn("Session store full — evicting oldest session");
    *oldest = {token, now, now, true};
    return token;
}

bool SessionStore::validate(const std::string& token) {
    const int64_t now = clock_->nowUs();

    for (auto& s : sessions_) {
        if (!s.active || s.token.size() != token.size()) {
            continue;
        }

        // Constant-time comparison
        uint8_t diff = 0;
        for (size_t i = 0; i < s.token.size(); ++i) {
            diff |= static_cast<uint8_t>(s.token[i]) ^ static_cast<uint8_t>(token[i]);
        }
        if (diff != 0) {
            continue;
        }

        // Valid match — check idle timeout
        if ((now - s.lastSeen) > IDLE_TIMEOUT_US) {
            log.info("Session expired (idle timeout)");
            s.active = false;
            return false;
        }

        s.lastSeen = now;
        return true;
    }
    return false;
}

void SessionStore::invalidate(const std::string& token) {
    for (auto& s : sessions_) {
        if (s.active && s.token == token) {
            s.active = false;
            log.info("Session invalidated");
            return;
        }
    }
}

void SessionStore::invalidateAll() {
    size_t count = 0;
    for (auto& s : sessions_) {
        if (s.active) {
            s.active = false;
            ++count;
        }
    }
    if (count > 0) {
        log.info("All sessions invalidated (%u active)", static_cast<unsigned>(count));
    }
}

} // namespace auth
