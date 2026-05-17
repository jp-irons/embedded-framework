#pragma once

#include "device_cert/DeviceCertInterface.hpp"
#include "esp_err.h"

#include <string>

namespace esp_platform {

/**
 * ESP-IDF concrete implementation of device_cert::DeviceCertInterface.
 *
 * On first boot, generates an ECDSA P-256 self-signed certificate with SANs:
 *   DNS:<hostname>.local
 *   DNS:<hostname>
 *   IP:192.168.4.1   (AP-mode captive portal address)
 *
 * The cert, private key, and hostname are stored in NVS.  On subsequent boots
 * the cert is loaded instantly.  If the hostname has changed since the cert was
 * generated (e.g. the app prefix was renamed), ensure() detects the mismatch
 * and regenerates automatically.  The cert is otherwise stable for 10 years,
 * so browsers only need to accept it once per hostname.
 *
 * All ESP-IDF types are confined to this header and its .cpp.
 * Nothing outside esp_platform needs to include this file — consumers
 * depend only on DeviceCertInterface.hpp.
 */
class EspDeviceCert : public device_cert::DeviceCertInterface {
  public:
    EspDeviceCert() = default;

    common::Result ensure(const std::string &hostname) override;
    common::Result regenerate(const std::string &hostname) override;

    const std::string &certPem() const override { return cert_; }
    const std::string &keyPem()  const override { return key_; }

    bool isLoaded() const override { return !cert_.empty(); }

  private:
    std::string cert_;
    std::string key_;
    std::string storedHostname_; // hostname recorded in NVS when the cert was generated

    esp_err_t loadFromNvs();
    esp_err_t generateAndStore(const std::string &hostname);
    esp_err_t storeToNvs() const;
};

} // namespace esp_platform
