#pragma once

namespace common {

enum class Result {
    Ok,            // Operation succeeded
    NotFound,      // Resource not found
    BadRequest,    // Invalid input or malformed request
    Forbidden,     // Access denied
    InternalError, // Unexpected failure
    Unsupported    // Operation not supported
};

inline const char* toString(Result res) {
    switch (res) {
        case Result::Ok:            return "Ok";
        case Result::NotFound:      return "NotFound";
        case Result::BadRequest:    return "BadRequest";
        case Result::Forbidden:     return "Forbidden";
        case Result::InternalError: return "InternalError";
        case Result::Unsupported:   return "Unsupported";
    }
    return "Unknown";
}

}
