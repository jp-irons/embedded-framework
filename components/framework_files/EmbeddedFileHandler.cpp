#include "framework_files/EmbeddedFileHandler.hpp"

#include "framework_files/EmbeddedFileTable.hpp"
#include "logger/Logger.hpp"

namespace framework_files {

using namespace http;

static logger::Logger log{EmbeddedFileHandler::TAG};

EmbeddedFileHandler::EmbeddedFileHandler(std::string basePath,
                                         std::string defaultFile,
                                         EmbeddedFileTable &table)
    : base(std::move(basePath))
    , defaultFile(std::move(defaultFile))
    , table(table) {
    log.debug("constructor '%s'", base.c_str());
}

common::Result EmbeddedFileHandler::handle(http::HttpRequest &request,
                                            http::HttpResponse &response) {
    std::string path = request.path();
    log.debug("handle '%s' base '%s'", path.c_str(), base.c_str());

    // Strip the base prefix so the table can use bare filenames (e.g. "/index.html")
    // regardless of what URL prefix the files are served from.
    if (!base.empty() && path.rfind(base, 0) == 0) {
        path = path.substr(base.size());
    }
    // Any directory-like path resolves to the default file:
    //   empty ("")      — base was stripped and nothing remained → prepend "/"
    //   exactly "/"     — root after stripping              → prepend (reuse back()=='/')
    //   trailing "/"    — e.g. "/app/ui/" with no stripping → append only
    if (path.empty()) {
        path = "/" + defaultFile;
    } else if (path.back() == '/') {
        path += defaultFile;
    }

    const EmbeddedFile *file = table.find(path);
    if (!file) {
        log.debug("not found: %s", path.c_str());
        return common::Result::NotFound;
    }

    const char *type = contentTypeForPath(path);
    return response.send(file->data, file->size, type);
}

const char *EmbeddedFileHandler::contentTypeForPath(const std::string &path) {
    if (path.ends_with(".html")) return "text/html";
    if (path.ends_with(".js"))   return "application/javascript";
    if (path.ends_with(".css"))  return "text/css";
    if (path.ends_with(".png"))  return "image/png";
    if (path.ends_with(".jpg"))  return "image/jpeg";
    if (path.ends_with(".svg"))  return "image/svg+xml";
    if (path.ends_with(".ico"))  return "image/x-icon";

    return "application/octet-stream";
}

} // namespace framework_files
