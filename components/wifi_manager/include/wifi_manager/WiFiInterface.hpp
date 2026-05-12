#pragma once

#include "common/Result.hpp"
#include "network_store/WiFiNetwork.hpp"
#include "wifi_manager/WiFiTypes.hpp"
#include <vector>

namespace wifi_manager {

/**
 * Abstract WiFi driver interface.
 *
 * Only framework types appear on the boundary (ApConfig, WiFiNetwork,
 * IpAddress, WiFiStatus, Result, WiFiAp).  No ESP-IDF headers are included
 * here; all ESP-IDF types and includes are confined to EspWiFiInterface.
 *
 * WiFiManager holds a WiFiInterface* so it remains portable and unit-testable
 * without a real WiFi driver.  The ESP-IDF concrete implementation is
 * EspWiFiInterface.
 */
class WiFiInterface {
  public:
    virtual ~WiFiInterface() = default;

    virtual common::Result startDriver() = 0;
    virtual common::Result stopDriver()  = 0;

    virtual common::Result startAp(const ApConfig& cfg) = 0;
    virtual common::Result stopAp()                      = 0;

    virtual WiFiStatus     connectSta(const network_store::WiFiNetwork& cred) = 0;
    virtual common::Result disconnectSta()                                     = 0;

    virtual common::Result scan(std::vector<WiFiAp>& results) = 0;

    virtual IpAddress getApIp()  const = 0;
    virtual IpAddress getStaIp() const = 0;
};

} // namespace wifi_manager
