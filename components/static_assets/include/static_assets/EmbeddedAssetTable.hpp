#pragma once
#include <cstddef>
#include <cstdint>

namespace static_assets {

class EmbeddedAssetTable {
public:
    static const uint8_t* find(const char* path, size_t& outSize);
};

}