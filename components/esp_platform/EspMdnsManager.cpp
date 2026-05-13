#include "esp_platform/EspMdnsManager.hpp"

#include "logger/Logger.hpp"
#include "mdns.h"

namespace esp_platform {

static logger::Logger log{"EspMdnsManager"};

EspMdnsManager::~EspMdnsManager() {
    stop();
}

void EspMdnsManager::start(const std::string &hostname) {
    if (running_) {
        log.debug("already running — stopping before restart");
        stop();
    }

    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        log.error("mdns_init failed: %s", esp_err_to_name(err));
        return;
    }

    err = mdns_hostname_set(hostname.c_str());
    if (err != ESP_OK) {
        log.error("mdns_hostname_set failed: %s", esp_err_to_name(err));
        mdns_free();
        return;
    }

    // Human-readable instance name shown in service browsers
    mdns_instance_name_set(hostname.c_str());

    // Register _http._tcp so plain-HTTP clients / redirector can be found
    err = mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
    if (err != ESP_OK) {
        log.warn("mdns_service_add _http failed: %s", esp_err_to_name(err));
    }

    // Register _https._tcp for the TLS server
    err = mdns_service_add(nullptr, "_https", "_tcp", 443, nullptr, 0);
    if (err != ESP_OK) {
        log.warn("mdns_service_add _https failed: %s", esp_err_to_name(err));
    }

    hostname_ = hostname;
    running_  = true;
    log.info("mDNS started - hostname: %s.local", hostname_.c_str());
}

void EspMdnsManager::stop() {
    if (!running_) {
        return;
    }
    mdns_free();
    running_ = false;
    log.info("mDNS stopped");
}

} // namespace esp_platform
