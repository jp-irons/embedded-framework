// EmbeddedFiles.hpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <span>

struct EmbeddedFile {
    const char* path;
    const uint8_t* start;
    size_t size;
    const char* mime;
};

extern const EmbeddedFile embeddedFiles[];
extern const size_t embeddedFilesCount;

std::span<const EmbeddedFile> getAllEmbeddedFiles();
const EmbeddedFile* getEmbeddedFile(std::string_view path);