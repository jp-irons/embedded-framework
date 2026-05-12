#pragma once

#include <cstddef>
#include <cstdint>

namespace device {

/**
 * Abstract hardware random-number generator.
 *
 * Provides raw entropy suitable for token and key generation.
 * Concrete implementation: esp_platform::EspRandomInterface.
 */
class RandomInterface {
  public:
    virtual ~RandomInterface() = default;

    /**
     * Fill buf with len cryptographically-random bytes.
     */
    virtual void fillRandom(uint8_t* buf, size_t len) = 0;

    /**
     * Return a single 32-bit random value.
     */
    virtual uint32_t random32() = 0;
};

} // namespace device
