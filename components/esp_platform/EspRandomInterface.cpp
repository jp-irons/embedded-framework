// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#include "esp_platform/EspRandomInterface.hpp"

#include "esp_random.h"

namespace esp_platform {

void EspRandomInterface::fillRandom(uint8_t* buf, size_t len) {
    esp_fill_random(buf, len);
}

uint32_t EspRandomInterface::random32() {
    return esp_random();
}

} // namespace esp_platform
