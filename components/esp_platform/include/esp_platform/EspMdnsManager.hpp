#pragma once

#include "wifi_manager/MdnsInterface.hpp"

namespace wifi_manager {

/**
 * ESP-IDF concrete implementation of MdnsInterface.
 *
 * All ESP-IDF mDNS includes (mdns.h) are confined to EspMdnsManager.cpp.
 * Nothing outside the esp_platform component needs to include this file —
 * consumers depend only on MdnsInterface.hpp.
 *
 * Advertises the device as <hostname>.local with _http._tcp (port 80) and
 * _https._tcp (port 443) service records so browsers can find the device
 * by name regardless of its DHCP-assigned IP.
 *
 * Lifecycle: call start() once the active interface is up — either when STA
 * obtains an IP, or immediately after the soft-AP is started.  stop() on
 * disconnect/idle.  Calling start() again while already running is safe —
 * it restarts cleanly.
 */
class EspMdnsManager : public MdnsInterface {
  public:
    EspMdnsManager() = default;
    ~EspMdnsManager() override;

    void start(const std::string &hostname) override;
    void stop() override;

    bool isRunning() const override { return running_; }
    const std::string &hostname() const override { return hostname_; }

  private:
    bool running_  = false;
    std::string hostname_;
};

} // namespace wifi_manager
