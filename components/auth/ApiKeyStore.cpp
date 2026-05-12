#include "auth/ApiKeyStore.hpp"

#include "logger/Logger.hpp"

#include <cstdio>

namespace auth {

using namespace common;

static logger::Logger log{"ApiKeyStore"};

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::string ApiKeyStore::generateToken() {
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

Result ApiKeyStore::init(KeyValueStore& kvs, device::RandomInterface& rng) {
    kvs_ = &kvs;
    rng_ = &rng;

    Result r = kvs_->getString(NVS_KEY, key_);
    if (r == Result::NotFound) {
        log.debug("No API key stored");
        return Result::NotFound;
    }
    if (r != Result::Ok) {
        log.warn("Failed to load API key (%s)", toString(r));
        return r;
    }

    log.info("API key loaded");
    return Result::Ok;
}

std::string ApiKeyStore::generate() {
    const std::string token = generateToken();

    Result r = kvs_->setString(NVS_KEY, token);
    if (r != Result::Ok) {
        log.error("Failed to persist API key (%s)", toString(r));
        return {};
    }

    key_ = token;
    log.info("New API key generated and persisted");
    return token;
}

bool ApiKeyStore::validate(const std::string& token) const {
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
    Result r = kvs_->eraseKey(NVS_KEY);
    if (r == Result::Ok) {
        key_.clear();
        log.info("API key revoked");
    } else {
        log.warn("Failed to revoke API key (%s)", toString(r));
    }
    return r;
}

} // namespace auth
