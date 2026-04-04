#include "static_assets/ContentType.hpp"

#include <cstring>

using namespace static_assets;

namespace static_assets {

static const char *TAG = "ContentType";

const char *ContentType::fromPath(const char *path) {
    if (strstr(path, ".html"))
        return "text/html";
    if (strstr(path, ".js"))
        return "application/javascript";
    if (strstr(path, ".css"))
        return "text/css";
    if (strstr(path, ".png"))
        return "image/png";
    if (strstr(path, ".svg"))
        return "image/svg+xml";
    return "application/octet-stream";
}
} // namespace static_assets