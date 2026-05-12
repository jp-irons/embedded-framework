#pragma once

#include "common/Result.hpp"
#include "http/HttpHandler.hpp"
#include <string>

#include "framework_files/EmbeddedFileTable.hpp"

namespace framework_files {

/**
 * HTTP handler that serves files from an embedded file table.
 *
 * The table is supplied by the caller (non-owning reference) so that both the
 * framework and the app can create their own handler instances backed by
 * different file tables (FrameworkFileTable vs AppFileTable).
 *
 * URL stripping: if basePath is non-empty it is stripped from the front of the
 * request path before the table lookup.  Pass "" to skip stripping and use full
 * URL paths as table keys (e.g. "/app/ui/index.html", "/favicon.ico").
 *
 * Directory paths (empty or trailing "/") are resolved to defaultFile.
 *
 * Returns NotFound (without sending a response) when no entry matches, so the
 * caller can fall through to another handler or send its own 404.
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

} // namespace framework_files
