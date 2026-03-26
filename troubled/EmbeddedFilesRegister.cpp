#include "EmbeddedFiles.hpp"

extern const uint8_t _binary_favicon_ico_start[];
extern const uint8_t _binary_favicon_ico_end[];

extern const uint8_t _binary_index_html_start[];
extern const uint8_t _binary_index_html_end[];

extern const uint8_t _binary_provision_js_start[];
extern const uint8_t _binary_provision_js_end[];

extern const uint8_t _binary_tailwind_css_start[];
extern const uint8_t _binary_tailwind_css_end[];

const EmbeddedFile embeddedFiles[] = {
    { "/favicon.ico",
      _binary_favicon_ico_start,
      size_t(_binary_favicon_ico_end - _binary_favicon_ico_start),
      "image/x-icon" },

    { "/provision/index.html",
      _binary_index_html_start,
      size_t(_binary_index_html_end - _binary_index_html_start),
      "text/html" },

    { "/provision/provision.js",
      _binary_provision_js_start,
      size_t(_binary_provision_js_end - _binary_provision_js_start),
      "application/javascript" },

    { "/provision/tailwind.css",
      _binary_tailwind_css_start,
      size_t(_binary_tailwind_css_end - _binary_tailwind_css_start),
      "text/css" },
};

const size_t embeddedFilesCount = sizeof(embeddedFiles) / sizeof(embeddedFiles[0]);