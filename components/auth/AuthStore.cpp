#include "auth/AuthStore.hpp"

#include "logger/Logger.hpp"

#include "psa/crypto.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace auth {

using namespace common;

static logger::Logger log{"AuthStore"};

// ---------------------------------------------------------------------------
// Password derivation helpers
// ---------------------------------------------------------------------------

// SHA256(stub + mac[0..5]), first 6 bytes encoded as 12 lowercase hex chars.
// Using only 3 MAC bytes (as the hostname does) would reduce entropy; all 6
// are used here to keep the password space as wide as possible.
//
// Uses the PSA Crypto API (TF-PSA-Crypto / mbedTLS 4.x) — the legacy
// mbedtls_sha256_* functions are not available in ESP-IDF 6.x.
std::string AuthStore::deriveFromMac(const std::string& stub, const uint8_t mac[6]) const {
    // Concatenate stub + mac into a single input buffer
    std::vector<uint8_t> input;
    input.reserve(stub.size() + 6);
    for (char c : stub) input.push_back(static_cast<uint8_t>(c));
    for (int i = 0; i < 6; ++i) input.push_back(mac[i]);

    uint8_t hash[PSA_HASH_LENGTH(PSA_ALG_SHA_256)];
    size_t  hashLen = 0;

    psa_status_t st = psa_hash_compute(
        PSA_ALG_SHA_256,
        input.data(), input.size(),
        hash, sizeof(hash), &hashLen);

    if (st != PSA_SUCCESS) {
        log.error("psa_hash_compute failed (%d) — returning empty derivation", (int)st);
        return {};
    }

    char result[13];
    snprintf(result, sizeof(result),
             "%02x%02x%02x%02x%02x%02x",
             hash[0], hash[1], hash[2], hash[3], hash[4], hash[5]);
    return std::string(result);
}

// 12-character alphanumeric password from the hardware RNG.
std::string AuthStore::generateRandom() const {
    static constexpr char   kChars[]  = "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "0123456789";
    static constexpr size_t kLen      = 12;
    static constexpr size_t kNumChars = sizeof(kChars) - 1; // 62

    char buf[kLen + 1];
    for (size_t i = 0; i < kLen; ++i) {
        buf[i] = kChars[rng_->random32() % kNumChars];
    }
    buf[kLen] = '\0';
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// Store helpers
// ---------------------------------------------------------------------------

Result AuthStore::loadFromStore(std::string& out) const {
    Result r = kvs_->getString(NVS_KEY_PASSWORD, out);
    if (r == Result::NotFound) {
        return Result::NotFound;
    }
    if (r != Result::Ok) {
        log.warn("Failed to load password from store (%s)", toString(r));
    }
    return r;
}

Result AuthStore::persistToStore(const std::string& password, bool changed) {
    Result r = kvs_->setString(NVS_KEY_PASSWORD, password);
    if (r != Result::Ok) {
        log.warn("Failed to persist password (%s)", toString(r));
        return r;
    }
    r = kvs_->setU8(NVS_KEY_CHANGED, changed ? 1 : 0);
    if (r != Result::Ok) {
        log.warn("Failed to persist pw_changed flag (%s)", toString(r));
    }
    return r;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Result AuthStore::init(const AuthConfig&        authConfig,
                       const uint8_t            mac[6],
                       KeyValueStore&           kvs,
                       device::RandomInterface& rng) {
    log.debug("init");
    kvs_ = &kvs;
    rng_ = &rng;

    // PSA must be initialised before psa_hash_compute can be called.
    // This is idempotent — safe to call even if DeviceCert already did it.
    psa_crypto_init();

    // ── Read the operator-changed flag ────────────────────────────────────
    uint8_t changedFlag = 0;
    kvs_->getU8(NVS_KEY_CHANGED, changedFlag); // NotFound → changedFlag stays 0

    // ── Operator owns the password ────────────────────────────────────────
    if (changedFlag) {
        Result r = loadFromStore(password_);
        if (r == Result::Ok) {
            passwordChanged_ = true;
            log.info("Loaded operator-set password from store");
            return Result::Ok;
        }
        // Store inconsistency — changed flag set but password missing.
        // Fall through to reapply the default and let the operator re-set it.
        log.warn("pw_changed flag set but password missing — reapplying default");
    }

    // ── Developer owns the default ────────────────────────────────────────
    passwordChanged_ = false;

    switch (authConfig.source()) {

        case AuthConfig::Source::Generated:
            // Keep the existing generated password if one is already stored,
            // so the password is stable across reboots until the operator
            // changes it.  Only generate a new one on a true first boot or
            // after NVS has been wiped.
            if (loadFromStore(password_) == Result::Ok) {
                log.debug("Loaded existing generated password from store");
                return Result::Ok;
            }
            password_ = generateRandom();
            log.info("Generated new password (first boot)");
            break;

        case AuthConfig::Source::Fixed:
            password_ = authConfig.fixedPassword();
            log.debug("Applying fixed default password");
            break;

        case AuthConfig::Source::FromMac:
            password_ = deriveFromMac(authConfig.macStub(), mac);
            log.debug("Derived MAC-based default password");
            break;
    }

    Result r = persistToStore(password_, false);
    if (r != Result::Ok) {
        log.warn("Failed to persist default password (%s)", toString(r));
        // Not fatal — password_ is still valid in memory for this boot
    }
    return Result::Ok;
}

bool AuthStore::verify(const std::string& password) const {
    const std::string& stored = password_;
    bool match = (password.size() == stored.size());
    uint8_t diff = 0;
    size_t  len  = match ? stored.size() : 0;
    for (size_t i = 0; i < len; ++i) {
        diff |= static_cast<uint8_t>(password[i]) ^ static_cast<uint8_t>(stored[i]);
    }
    return match && (diff == 0);
}

Result AuthStore::changePassword(const std::string& newPassword) {
    log.info("Operator changing password");
    Result r = persistToStore(newPassword, true);
    if (r == Result::Ok) {
        password_        = newPassword;
        passwordChanged_ = true;
    }
    return r;
}

} // namespace auth
