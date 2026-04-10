#include "static_assets/StaticFileHandler.hpp"

#include "logger/Logger.hpp"

namespace static_assets {

using namespace common;

static logger::Logger log{"StaticFileHandler"};

StaticFileHandler::StaticFileHandler(std::string basePath, std::string defaultFile)
    : base(std::move(basePath))
    , defaultFile(std::move(defaultFile))
    , table() // EmbeddedAssetTable has no state, so default construct
{}

Result StaticFileHandler::handle(http::HttpRequest &request, http::HttpResponse &response) {
    log.debug("handle '%s'", request.path());
	
	// TODO review logging - probably over the top.
    // Convert string_view → string safely
    std::string resolved = resolvePath(request.uri());

	log.debug("resolved path %s", resolved.c_str());

    const EmbeddedAsset *asset = table.find(resolved);
    if (!asset) {
        log.warn("Asset not found: %s", resolved.c_str());
        // TODO fix this       response.setStatus(404);
		response.sendNotFound404("Asset not found");
        response.send("Not found");
        return Result::NotFound;
    }

    const char *type = contentTypeForPath(resolved);
    response.setType(type);
    response.send(asset->data, asset->size);
	return Result::Ok;
}

std::string StaticFileHandler::resolvePath(std::string_view uri) const {
    // Convert string_view → string
    std::string path(uri.data(), uri.size());
//	log.debug("base='%s' (len=%zu)", base.c_str(), base.size());
//	log.debug("path='%s'", path.c_str());

    // Must start with base
    if (!path.starts_with(base)) {
        return ""; // will 404
    }

    // Strip base prefix
    std::string sub = path.substr(base.size());

	// Must start with base
	if (!sub.starts_with("/")) {
	    sub = "/" + sub;
	}

    // If empty or "/", serve default file
    if (sub.empty() || sub == "/") {
        return "/" + defaultFile;
    }

    return sub;
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