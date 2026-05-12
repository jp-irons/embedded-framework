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
