#pragma once

#include "common/Result.hpp"
#include "embedded_files/EmbeddedFileTable.hpp"
#include "http/HttpHandler.hpp"
#include <string>

namespace embedded_files {

/**
 * HTTP handler that serves files from an embedded file table.
 *
 * The table is supplied by the caller (non-owning reference) so that both the
 * framework and the app can create their own handler instances backed by
 * different file tables (FrameworkFileTable vs AppFileTable).
 *
 * URL stripping: EmbeddedFileHandler strips `basePath` from the front of each
 * request path before looking it up in the table, so the table entries only
 * need bare names like "/index.html" regardless of what URL prefix the files
 * are served from.
 */
class EmbeddedFileHandler : public http::HttpHandler {
  public:
    /**
     * @param basePath    URL prefix stripped before table lookup (e.g. "/app/ui").
     * @param defaultFile Filename served when the path is empty after stripping.
     * @param table       File table to look up assets in.  Must outlive this handler.
     */
    EmbeddedFileHandler(std::string basePath, std::string defaultFile, EmbeddedFileTable &table);

    common::Result handle(http::HttpRequest &request, http::HttpResponse &response) override;

  private:
    std::string       base;
    std::string       defaultFile;
    EmbeddedFileTable &table;   // non-owning reference

    static const char *contentTypeForPath(const std::string &path);
};

} // namespace embedded_files
