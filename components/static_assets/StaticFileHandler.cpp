#include "static_assets/StaticFileHandler.hpp"

#include "logger/Logger.hpp"

namespace static_assets {

using namespace common;

static logger::Logger log{"StaticFileHandler"};

StaticFileHandler::StaticFileHandler(std::string basePath, std::string defaultFile)
    : base(std::move(basePath))
    , defaultFile(std::move(defaultFile))
    , table() {
		log.debug("constructor '%s'", base.c_str());
}

Result StaticFileHandler::handle(http::HttpRequest &request, http::HttpResponse &response) {
	const char * path = request.path();
    log.debug("handle '%s' base '%s'", path, base.c_str());
    const EmbeddedAsset *asset = table.find(path);
    if (!asset) {
        log.warn("Asset not found: %s", path);
        response.sendNotFound404("Asset not found");
        return Result::NotFound;
    }

    const char *type = contentTypeForPath(path);
    response.setType(type);
    response.send(asset->data, asset->size);
    return Result::Ok;
}

const char *StaticFileHandler::contentTypeForPath(const std::string &path) {
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

} // namespace static_assets