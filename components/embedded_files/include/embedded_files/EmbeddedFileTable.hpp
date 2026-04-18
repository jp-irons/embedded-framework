#pragma once

#include <string_view>
#include <cstddef>
#include <cstdint>

namespace embedded_files {

struct EmbeddedFile {
    const uint8_t* data;
    size_t size;
};

class EmbeddedFileTable {
public:
    const EmbeddedFile* find(std::string_view path) const;
    const uint8_t* find(const char* path, size_t& outSize) const;
};

} // namespace embedded_files