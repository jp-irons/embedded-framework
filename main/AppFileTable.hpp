#pragma once

#include "embedded_files/EmbeddedFileTable.hpp"

/**
 * Concrete EmbeddedFileTable for the application's own static files.
 *
 * Mirror of FrameworkFileTable: each file listed here has a corresponding
 * entry in AppFileTable.cpp that declares the EMBED_FILES linker symbols.
 * Add new app files to main/files/ and update AppFileTable.cpp to match.
 */
class AppFileTable : public embedded_files::EmbeddedFileTable {
public:
    const embedded_files::EmbeddedFile* find(std::string_view path) const override;
    const uint8_t* find(const char* path, size_t& outSize) const override;
};
