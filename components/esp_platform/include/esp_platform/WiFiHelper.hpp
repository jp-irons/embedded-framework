#pragma once

#include "network_store/WiFiNetwork.hpp"
#include "wifi_manager/WiFiTypes.hpp"
#include "esp_wifi_types_generic.h"

namespace wifi_manager {

wifi_auth_mode_t toEspAuth(wifi_manager::WiFiAuthMode mode);

wifi_config_t makeStaConfig(const network_store::WiFiNetwork& cred);

wifi_config_t makeApConfig(const wifi_manager::ApConfig& cfg);

wifi_manager::WiFiError toWiFiError(uint8_t reason);

std::string toString(uint8_t reason);

} // namespace wifi_manager
