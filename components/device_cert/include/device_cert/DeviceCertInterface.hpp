#pragma once

#include "common/Result.hpp"
#include <string>

namespace device_cert {

/**
 * Abstract interface for per-device TLS certificate management.
 *
 * No ESP-IDF types appear here.  The concrete implementation (EspDeviceCert)
 * lives in the esp_platform component and is constructed in FrameworkContext.
 */
class DeviceCertInterface {
  public:
    virtual ~DeviceCertInterface() = default;

    /**
     * Ensure a valid cert exists for the given hostname.
     * Loads from NVS if present; otherwise generates and stores.
     * Returns Ok on success.
     */
    virtual common::Result ensure(const std::string &hostname) = 0;

    /** Force regeneration even if NVS already has a cert. */
    virtual common::Result regenerate(const std::string &hostname) = 0;

    virtual const std::string &certPem() const = 0;
    virtual const std::string &keyPem()  const = 0;

    virtual bool isLoaded() const = 0;
};

} // namespace device_cert
