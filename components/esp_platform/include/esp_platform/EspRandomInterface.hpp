#pragma once

#include "device/RandomInterface.hpp"

namespace esp_platform {

/**
 * ESP-IDF concrete implementation of RandomInterface.
 *
 * Uses esp_fill_random / esp_random (hardware RNG).
 * All ESP-IDF types are confined to EspRandomInterface.cpp.
 */
class EspRandomInterface : public device::RandomInterface {
  public:
    void     fillRandom(uint8_t* buf, size_t len) override;
    uint32_t random32()                           override;
};

} // namespace esp_platform
