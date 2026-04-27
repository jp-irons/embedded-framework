#include "embedded_files/EmbeddedFileHandler.hpp"
#include "embedded_files/EmbeddedFileTable.hpp"

#include "logger/Logger.hpp"

namespace embedded_files {

using namespace http;

static logger::Logger log{"EmbeddedFileHandler"};

EmbeddedFileHandler::EmbeddedFileHandler(std::string basePath, std::string defaultFile)
    : base(std::move(basePath))
    , defaultFile(std::move(defaultFile))
    , table() {
		log.debug("constructor '%s'", base.c_str());
}

common::Result EmbeddedFileHandler::handle(http::HttpRequest &request, http::HttpResponse &response) {
	const char * path = request.path();
    log.debug("handle '%s' base '%s'", path, base.c_str());
    const EmbeddedFile *file = table.find(path);
    if (!file) {
        log.warn("File not found: %s", path);
        return response.sendJsonError(404, "File '" + std::string(path) + "' not found");
    }

    const char *type = contentTypeForPath(path);
	return response.send(file->data, file->size, type);
}

const char *EmbeddedFileHandler::contentTypeForPath(const std::string &path) {
    if (path.ends_with(".html"))
        return "text/html";
    if (path.ends_with(".js"))
        return "application/javascript";
    if (path.ends_with(".css"))
        return "text/css";
    if (path.ends_with(".png"))
        return "image/png";
    if (path.ends_with(".jpg"))
        return "image/jpeg";
    if (path.ends_with(".svg"))
        return "image/svg+xml";

    return "application/octet-stream";
}

} // namespace embedded_files