// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

#include "network_store/WiFiNetwork.hpp"
#include "wifi_manager/WiFiTypes.hpp"
#include "esp_wifi_types_generic.h"

namespace esp_platform {

wifi_auth_mode_t        toEspAuth(wifi_manager::WiFiAuthMode mode);

// Translates the app-facing WiFiPowerSaveMode into ESP-IDF's wifi_ps_type_t.
// Callers must reject WiFiPowerSaveMode::Unset before calling this (see
// EspWiFiInterface::startDriver()) -- Unset falls back to WIFI_PS_NONE here
// only as a last-resort safety net, not as a supported default.
wifi_ps_type_t          toEspPs(wifi_manager::WiFiPowerSaveMode mode);

wifi_config_t           makeStaConfig(const network_store::WiFiNetwork& cred);

wifi_config_t           makeApConfig(const wifi_manager::ApConfig& cfg);

wifi_manager::WiFiError toWiFiError(uint8_t reason);

std::string             toString(uint8_t reason);

} // namespace esp_platform
