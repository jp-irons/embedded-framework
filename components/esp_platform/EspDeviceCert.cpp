#include "esp_platform/EspDeviceCert.hpp"
#include "esp_platform/EspTypeAdapter.hpp"

#include "esp_err.h"
#include "logger/Logger.hpp"
#include "nvs.h"

// PSA Crypto API (mbedTLS 4.x / TF-PSA-Crypto)
#include "psa/crypto.h"
#include "mbedtls/pk.h"
#include "mbedtls/oid.h"
#include "mbedtls/x509_crt.h"   // x509write_cert type + all write functions

#include <vector>
#include <cstring>
#include <cstdio>
#include <memory>

namespace esp_platform {

static logger::Logger log{"EspDeviceCert"};

static constexpr const char *NVS_NAMESPACE = "device_cert";
static constexpr const char *NVS_KEY_CERT  = "cert_pem";
static constexpr const char *NVS_KEY_KEY   = "key_pem";

// ---------------------------------------------------------------------------
// ASN.1 SAN extension builder
//
// Produces the DER encoding of:
//   SEQUENCE {
//     [2] "<hostname>.local"   -- dNSName
//     [2] "<hostname>"         -- dNSName
//     [7] 192 168 4 1          -- iPAddress
//   }
//
// mbedtls_x509write_crt_set_extension wraps this in an OCTET STRING
// automatically, so we supply the raw SEQUENCE here.
// ---------------------------------------------------------------------------

static bool buildSanDer(const std::string &hostname,
                        uint8_t *buf, size_t bufSize, size_t *outLen) {
    std::string fqhn = hostname + ".local";

    size_t d1 = fqhn.size();
    size_t d2 = hostname.size();
    // total payload: (tag+len+data) x2 DNS  +  (tag+len+4) IP
    size_t payload = (2 + d1) + (2 + d2) + (2 + 4);

    // SEQUENCE header: tag 0x30 + length (1 or 2 bytes)
    size_t headerLen = (payload < 128) ? 2 : 3;
    size_t total = headerLen + payload;

    if (total > bufSize) {
        log.error("SAN buffer too small: need %zu, have %zu", total, bufSize);
        return false;
    }

    uint8_t *p = buf;

    // SEQUENCE tag + length
    *p++ = 0x30;
    if (payload < 128) {
        *p++ = (uint8_t)payload;
    } else {
        *p++ = 0x81;
        *p++ = (uint8_t)payload;
    }

    // dNSName: hostname.local
    *p++ = 0x82;
    *p++ = (uint8_t)d1;
    memcpy(p, fqhn.c_str(), d1);
    p += d1;

    // dNSName: hostname (bare, without .local)
    *p++ = 0x82;
    *p++ = (uint8_t)d2;
    memcpy(p, hostname.c_str(), d2);
    p += d2;

    // iPAddress: 192.168.4.1
    *p++ = 0x87;
    *p++ = 0x04;
    *p++ = 192;
    *p++ = 168;
    *p++ = 4;
    *p++ = 1;

    *outLen = (size_t)(p - buf);
    return true;
}

// ---------------------------------------------------------------------------
// SEC1 DER builder for ECDSA P-256 private key
//
// mbedtls_pk_setup_opaque() requires MBEDTLS_USE_PSA_CRYPTO which is not
// enabled in this build.  Instead we export the raw key bytes from PSA and
// encode them as SEC1 / RFC 5915 DER so mbedtls_pk_parse_key() can load them.
//
// SEC1 DER structure for P-256 (total 121 bytes):
//   SEQUENCE (30 77)
//     INTEGER 1                       (02 01 01)           version
//     OCTET STRING [32 bytes]         (04 20 ...)           privateKey
//     [0] OID prime256v1              (a0 0a 06 08 ...)    parameters
//     [1] BIT STRING uncompressed pt  (a1 44 03 42 00 ...) publicKey
// ---------------------------------------------------------------------------

static bool buildSec1Der(const uint8_t *privKey, size_t privLen,
                         const uint8_t *pubKey,  size_t pubLen,
                         uint8_t *buf, size_t bufSize, size_t *outLen) {
    // Only handle P-256: 32-byte scalar + 65-byte uncompressed point
    if (privLen != 32 || pubLen != 65 || bufSize < 121) {
        log.error("buildSec1Der: unexpected key sizes priv=%zu pub=%zu", privLen, pubLen);
        return false;
    }

    uint8_t *p = buf;

    // SEQUENCE  (content = 119 = 0x77 bytes)
    *p++ = 0x30; *p++ = 0x77;

    // version = 1
    *p++ = 0x02; *p++ = 0x01; *p++ = 0x01;

    // privateKey OCTET STRING
    *p++ = 0x04; *p++ = 0x20;
    memcpy(p, privKey, 32); p += 32;

    // parameters [0]: OID 1.2.840.10045.3.1.7 (prime256v1)
    *p++ = 0xa0; *p++ = 0x0a;          // context [0], len 10
    *p++ = 0x06; *p++ = 0x08;          // OID, len 8
    *p++ = 0x2a; *p++ = 0x86; *p++ = 0x48; *p++ = 0xce;
    *p++ = 0x3d; *p++ = 0x03; *p++ = 0x01; *p++ = 0x07;

    // publicKey [1]: BIT STRING wrapping uncompressed EC point (04 || X || Y)
    *p++ = 0xa1; *p++ = 0x44;          // context [1], len 68
    *p++ = 0x03; *p++ = 0x42;          // BIT STRING, len 66
    *p++ = 0x00;                        // 0 unused bits
    memcpy(p, pubKey, 65); p += 65;    // 04 || X || Y

    *outLen = (size_t)(p - buf);  // must be 121
    return true;
}

// ---------------------------------------------------------------------------
// NVS helpers
// ---------------------------------------------------------------------------

esp_err_t EspDeviceCert::loadFromNvs() {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err;  // namespace not yet written — first boot
    }

    auto readStr = [&](const char *key, std::string &out) -> esp_err_t {
        size_t len = 0;
        esp_err_t e = nvs_get_str(h, key, nullptr, &len);
        if (e != ESP_OK) return e;
        std::vector<char> tmp(len);
        e = nvs_get_str(h, key, tmp.data(), &len);
        if (e != ESP_OK) return e;
        out.assign(tmp.data(), len - 1);  // strip null terminator
        return ESP_OK;
    };

    err = readStr(NVS_KEY_CERT, cert_);
    if (err == ESP_OK) err = readStr(NVS_KEY_KEY, key_);

    nvs_close(h);

    if (err != ESP_OK) {
        cert_.clear();
        key_.clear();
    }
    return err;
}

esp_err_t EspDeviceCert::storeToNvs() const {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_str(h, NVS_KEY_CERT, cert_.c_str());
    if (err == ESP_OK) err = nvs_set_str(h, NVS_KEY_KEY, key_.c_str());
    if (err == ESP_OK) err = nvs_commit(h);

    nvs_close(h);
    return err;
}

// ---------------------------------------------------------------------------
// Certificate generation (ECDSA P-256, self-signed, 10-year validity)
//
// mbedTLS 4.x API changes vs 3.x:
//   - Key generation:    psa_generate_key() + export/parse (no mbedtls_ecp_gen_key)
//   - pk_setup_opaque:   MBEDTLS_USE_PSA_CRYPTO not enabled → use SEC1 export+parse
//   - x509write_crt_pem: no RNG callback (PSA handles RNG internally)
// ---------------------------------------------------------------------------

esp_err_t EspDeviceCert::generateAndStore(const std::string &hostname) {
    log.info("Generating device cert for '%s' (first boot) ...", hostname.c_str());

    psa_key_id_t           keyId = PSA_KEY_ID_NULL;
    mbedtls_pk_context     pk;
    mbedtls_x509write_cert crt;

    mbedtls_pk_init(&pk);
    mbedtls_x509write_crt_init(&crt);

    int          rc    = 0;
    psa_status_t psaSt = PSA_SUCCESS;

    // PSA must be initialised before use; idempotent if already done by the system.
    psaSt = psa_crypto_init();
    if (psaSt != PSA_SUCCESS) {
        log.error("psa_crypto_init: %d", (int)psaSt);
        rc = -1;
        goto cleanup;
    }

    // -----------------------------------------------------------------------
    // Generate ECDSA P-256 key via PSA, then export into an mbedTLS PK context.
    //
    // We cannot use mbedtls_pk_setup_opaque() because MBEDTLS_USE_PSA_CRYPTO is
    // not enabled in this build.  Instead: export the raw private key scalar and
    // uncompressed public point from PSA, encode as SEC1 DER, and parse with
    // mbedtls_pk_parse_key() to get a standard (non-opaque) PK context.
    // -----------------------------------------------------------------------
    {
        psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_usage_flags(&attrs,
            PSA_KEY_USAGE_SIGN_HASH |
            PSA_KEY_USAGE_VERIFY_HASH |
            PSA_KEY_USAGE_EXPORT);
        psa_set_key_algorithm(&attrs, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
        psa_set_key_type(&attrs, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
        psa_set_key_bits(&attrs, 256);

        psaSt = psa_generate_key(&attrs, &keyId);
        if (psaSt != PSA_SUCCESS) {
            log.error("psa_generate_key: %d", (int)psaSt);
            rc = -1;
            goto cleanup;
        }
        log.debug("PSA key generated (id=%u)", (unsigned)keyId);
    }

    {
        uint8_t privKey[32], pubKey[65];
        size_t  privLen = 0,  pubLen  = 0;

        psaSt = psa_export_key(keyId, privKey, sizeof(privKey), &privLen);
        if (psaSt != PSA_SUCCESS) {
            log.error("psa_export_key: %d", (int)psaSt);
            rc = -1;
            goto cleanup;
        }

        psaSt = psa_export_public_key(keyId, pubKey, sizeof(pubKey), &pubLen);
        if (psaSt != PSA_SUCCESS) {
            log.error("psa_export_public_key: %d", (int)psaSt);
            rc = -1;
            goto cleanup;
        }

        // Build SEC1 DER and parse into a normal (non-opaque) PK context
        uint8_t sec1[128];
        size_t  sec1Len = 0;
        if (!buildSec1Der(privKey, privLen, pubKey, pubLen,
                          sec1, sizeof(sec1), &sec1Len)) {
            rc = -1;
            goto cleanup;
        }

        rc = mbedtls_pk_parse_key(&pk, sec1, sec1Len, nullptr, 0);
        if (rc != 0) {
            log.error("pk_parse_key: -0x%04x", -rc);
            goto cleanup;
        }

        // Wipe sensitive key material from the stack
        memset(privKey, 0, sizeof(privKey));
        memset(sec1,    0, sec1Len);
    }
    log.debug("PK context loaded");

    // PSA key no longer needed — material is now in the PK context
    psa_destroy_key(keyId);
    keyId = PSA_KEY_ID_NULL;

    // -----------------------------------------------------------------------
    // Build the certificate
    // -----------------------------------------------------------------------
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &pk);
    mbedtls_x509write_crt_set_issuer_key(&crt, &pk);  // self-signed

    {
        std::string cn = "CN=" + hostname + ".local";
        rc = mbedtls_x509write_crt_set_subject_name(&crt, cn.c_str());
        if (rc != 0) { log.error("set_subject_name: -0x%04x", -rc); goto cleanup; }
        rc = mbedtls_x509write_crt_set_issuer_name(&crt, cn.c_str());
        if (rc != 0) { log.error("set_issuer_name: -0x%04x", -rc); goto cleanup; }
    }

    // Serial = 1
    {
        static const unsigned char kSerial[] = {1};
        rc = mbedtls_x509write_crt_set_serial_raw(&crt, kSerial, sizeof(kSerial));
        if (rc != 0) { log.error("set_serial_raw: -0x%04x", -rc); goto cleanup; }
    }

    // Validity: 2024-01-01 → 2034-01-01 (10 years, fixed dates avoid RTC dependency)
    rc = mbedtls_x509write_crt_set_validity(&crt, "20240101000000", "20340101000000");
    if (rc != 0) { log.error("set_validity: -0x%04x", -rc); goto cleanup; }

    // Basic constraints (not a CA)
    rc = mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);
    if (rc != 0) { log.error("set_basic_constraints: -0x%04x", -rc); goto cleanup; }

    // Subject Alternative Name extension
    {
        uint8_t san[256];
        size_t  sanLen = 0;
        if (!buildSanDer(hostname, san, sizeof(san), &sanLen)) {
            rc = -1;
            goto cleanup;
        }
        rc = mbedtls_x509write_crt_set_extension(
                &crt,
                MBEDTLS_OID_SUBJECT_ALT_NAME,
                MBEDTLS_OID_SIZE(MBEDTLS_OID_SUBJECT_ALT_NAME),
                0,  // non-critical
                san, sanLen);
        if (rc != 0) { log.error("set_extension SAN: -0x%04x", -rc); goto cleanup; }
    }

    // -----------------------------------------------------------------------
    // Write private key to PEM  (heap-allocated — 512 B on stack would cascade)
    // -----------------------------------------------------------------------
    {
        auto keyBuf = std::make_unique<uint8_t[]>(512);
        rc = mbedtls_pk_write_key_pem(&pk, keyBuf.get(), 512);
        if (rc != 0) { log.error("pk_write_key_pem: -0x%04x", -rc); goto cleanup; }
        key_ = std::string(reinterpret_cast<char *>(keyBuf.get()));
    }

    // -----------------------------------------------------------------------
    // Write certificate to PEM  (heap-allocated — 2 KB on stack is fatal)
    // In mbedTLS 4.x, mbedtls_x509write_crt_pem takes only 3 arguments —
    // the RNG callback was removed; PSA handles randomness internally.
    // -----------------------------------------------------------------------
    {
        auto crtBuf = std::make_unique<uint8_t[]>(2048);
        rc = mbedtls_x509write_crt_pem(&crt, crtBuf.get(), 2048);
        if (rc != 0) { log.error("x509write_crt_pem: -0x%04x", -rc); goto cleanup; }
        cert_ = std::string(reinterpret_cast<char *>(crtBuf.get()));
    }

    log.info("Certificate generated (%zu bytes cert, %zu bytes key)",
             cert_.size(), key_.size());

cleanup:
    mbedtls_x509write_crt_free(&crt);
    mbedtls_pk_free(&pk);
    if (keyId != PSA_KEY_ID_NULL) {
        psa_destroy_key(keyId);
    }

    if (rc != 0) {
        cert_.clear();
        key_.clear();
        return ESP_FAIL;
    }

    // Persist for future boots
    esp_err_t storeErr = storeToNvs();
    if (storeErr != ESP_OK) {
        log.warn("Failed to store cert in NVS (%s) — will regenerate next boot",
                 esp_err_to_name(storeErr));
    } else {
        log.info("Cert stored in NVS");
    }

    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

common::Result EspDeviceCert::ensure(const std::string &hostname) {
    if (loadFromNvs() == ESP_OK) {
        log.info("Loaded cert from NVS (hostname in cert may differ if renamed)");
        return common::Result::Ok;
    }
    return toResult(generateAndStore(hostname));
}

common::Result EspDeviceCert::regenerate(const std::string &hostname) {
    cert_.clear();
    key_.clear();
    return toResult(generateAndStore(hostname));
}

} // namespace esp_platform
