// EmbeddedFiles.cpp
#include "EmbeddedFiles.hpp"
#include <span>

std::span<const EmbeddedFile> getAllEmbeddedFiles() {
    return std::span<const EmbeddedFile>(
        &embeddedFiles[0],
        embeddedFilesCount
    );
}

const EmbeddedFile* getEmbeddedFile(std::string_view path) {
    for (size_t i = 0; i < embeddedFilesCount; i++) {
        if (path == embeddedFiles[i].path) {
            return &embeddedFiles[i];
        }
    }
    return nullptr;
}

