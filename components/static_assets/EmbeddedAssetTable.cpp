#include "static_assets/EmbeddedAssetTable.hpp"

#include <cstring>

extern const uint8_t _binary_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t _binary_index_html_end[] asm("_binary_index_html_end");

extern const uint8_t _binary_app_js_start[] asm("_binary_app_js_start");
extern const uint8_t _binary_app_js_end[] asm("_binary_app_js_end");

extern const uint8_t _binary_style_css_start[] asm("_binary_style_css_start");
extern const uint8_t _binary_style_css_end[] asm("_binary_style_css_end");

using namespace static_assets;

namespace static_assets {

static const char *TAG = "EmbeddedAssetTable";
struct AssetEntry {
    const char *path;
    const uint8_t *start;
    const uint8_t *end;
};

static const AssetEntry assets[] = {
    {"/index.html", _binary_index_html_start, _binary_index_html_end},
    {"/app.js", _binary_app_js_start, _binary_app_js_end},
    {"/style.css", _binary_style_css_start, _binary_style_css_end},
};

const uint8_t *EmbeddedAssetTable::find(const char *path, size_t &outSize) {
    for (auto &a : assets) {
        if (strcmp(a.path, path) == 0) {
            outSize = a.end - a.start;
            return a.start;
        }
    }
    return nullptr;
}
} // namespace static_assets