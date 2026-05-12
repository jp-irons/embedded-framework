#pragma once

#include <string>

namespace wifi_manager {

/**
 * Thin wrapper around the ESP-IDF mDNS component.
 *
 * Advertises the device as <hostname>.local on the local network, with
 * _http._tcp (port 80) and _https._tcp (port 443) service records so
 * that browsers can find it by name regardless of its DHCP-assigned IP.
 *
 * Lifecycle: call start() once the active interface is up — either when STA
 * obtains an IP, or immediately after the soft-AP is started (the AP netif
 * has a static IP and is ready at that point).  stop() on disconnect/idle.
 * Calling start() again while already running is safe — it restarts cleanly.
 */
class MdnsManager {
  public:
    MdnsManager() = default;
    ~MdnsManager();

    /**
     * Initialise mDNS, set the hostname, and register HTTP/HTTPS service
     * records.  Safe to call multiple times — a running instance is stopped
     * first.
     */
    void start(const std::string &hostname);

    /** Tear down mDNS.  No-op if not running. */
    void stop();

    bool isRunning() const { return running_; }
    const std::string &hostname() const { return hostname_; }

  private:
    bool running_  = false;
    std::string hostname_;
};

} // namespace wifi_manager
