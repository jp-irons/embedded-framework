#pragma once

#include "esp_err.h"
#include <string>

namespace device_cert {

/**
 * Per-device TLS certificate manager.
 *
 * On first boot, generates an ECDSA P-256 self-signed certificate with SANs:
 *   DNS:<hostname>.local
 *   DNS:<hostname>
 *   IP:192.168.4.1   (AP-mode captive portal address)
 *
 * The cert and private key are stored in NVS so subsequent boots load them
 * instantly without regenerating.  The cert is stable for the lifetime of the
 * device (10 years validity), so browsers only need to accept it once.
 *
 * Usage:
 *   DeviceCert dc;
 *   dc.ensure("esp32-a1b2c3");   // idempotent — generates once, loads thereafter
 *   server.setCert(dc.certPem(), dc.keyPem());
 */
class DeviceCert {
  public:
    DeviceCert() = default;

    /**
     * Ensure a valid cert exists for the given hostname.
     * Loads from NVS if present; otherwise generates and stores.
     * Returns ESP_OK on success.
     */
    esp_err_t ensure(const std::string &hostname);

    /** Force regeneration even if NVS already has a cert. */
    esp_err_t regenerate(const std::string &hostname);

    const std::string &certPem() const { return cert_; }
    const std::string &keyPem()  const { return key_; }

    bool isLoaded() const { return !cert_.empty(); }

  private:
    std::string cert_;
    std::string key_;

    esp_err_t loadFromNvs();
    esp_err_t generateAndStore(const std::string &hostname);
    esp_err_t storeToNvs() const;
};

} // namespace device_cert
