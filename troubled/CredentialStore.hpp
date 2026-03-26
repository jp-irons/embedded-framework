#pragma once

#include <vector>
#include <string>
#include <stdint.h>
#include "nvs.h"
#include "nvs_flash.h"

// Forward declarations only
namespace wifi_manager {
    struct WiFiEntry;
    enum class WiFiState;
}

class CredentialStore {
public:
    CredentialStore(const char* nvsNamespace = "wifi");

    bool begin();

    bool saveEntries(const std::vector<wifi_manager::WiFiEntry>& entries);
    bool loadEntries(std::vector<wifi_manager::WiFiEntry>& outEntries);

    bool saveState(wifi_manager::WiFiState state);
    bool loadState(wifi_manager::WiFiState& outState);

    bool clearAll();
    bool erase(const std::string& ssid);

private:
    bool encodeEntries(const std::vector<wifi_manager::WiFiEntry>& entries, std::vector<uint8_t>& out);
    bool decodeEntries(const uint8_t* data, size_t len, std::vector<wifi_manager::WiFiEntry>& out);

private:
    const char* ns;
    nvs_handle_t handle = 0;
};