#pragma once

#include "common/Result.hpp"
#include "http/HttpHandler.hpp"
#include <string>

#include "embedded_files/EmbeddedFileTable.hpp"

namespace embedded_files {

class EmbeddedFileHandler : public http::HttpHandler {
  public:
    EmbeddedFileHandler(std::string basePath, std::string defaultFile);

    common::Result handle(http::HttpRequest &request, http::HttpResponse &response) override;

  private:
    std::string base;
    std::string defaultFile;
    EmbeddedFileTable table; // owns its own table

    static const char *contentTypeForPath(const std::string &path);
};

} // namespace embedded_files