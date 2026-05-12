#pragma once

// esp_err_t <-> common::Result conversions.
// HTTP method conversions live in http/private/EspHttpAdapter.hpp (http-internal).

#include "common/Result.hpp"
#include "esp_err.h"

namespace esp_platform {

inline esp_err_t toEspError(common::Result r) {
    using common::Result;

    switch (r) {
        case Result::Ok:            return ESP_OK;
        case Result::NotFound:      return ESP_ERR_NOT_FOUND;
        case Result::BadRequest:    return ESP_ERR_INVALID_ARG;
        case Result::Forbidden:     return ESP_ERR_INVALID_STATE;
        case Result::InternalError: return ESP_FAIL;
        case Result::Unsupported:   return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_FAIL;
}

inline common::Result toResult(esp_err_t err) {
    using common::Result;

    switch (err) {
        case ESP_OK:                return Result::Ok;
        case ESP_ERR_NOT_FOUND:     return Result::NotFound;
        case ESP_ERR_INVALID_ARG:   return Result::BadRequest;
        case ESP_ERR_INVALID_STATE: return Result::Forbidden;
        case ESP_ERR_NOT_SUPPORTED: return Result::Unsupported;
        case ESP_FAIL:
        default:                    return Result::InternalError;
    }
}

} // namespace esp_platform
