#pragma once

#include <string>

namespace wifi_manager {

/**
 * Abstract mDNS interface.
 *
 * Advertises the device as <hostname>.local on the local network.
 * The concrete ESP-IDF implementation is EspMdnsManager in the
 * esp_platform component.  This header contains no ESP-IDF includes so
 * that wifi_manager remains portable and unit-testable.
 */
class MdnsInterface {
  public:
    virtual ~MdnsInterface() = default;

    /**
     * Initialise mDNS, set the hostname, and register HTTP/HTTPS service
     * records.  Safe to call multiple times — a running instance is stopped
     * first.
     */
    virtual void start(const std::string &hostname) = 0;

    /** Tear down mDNS.  No-op if not running. */
    virtual void stop() = 0;

    virtual bool isRunning() const = 0;
    virtual const std::string &hostname() const = 0;
};

} // namespace wifi_manager
