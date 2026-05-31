// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once
#include "LogLevel.hpp"

#include <string_view>

namespace logger {

class LogSink {
  public:
    virtual ~LogSink() = default;

    virtual void write(LogLevel level, std::string_view tag, 
		std::string_view message) = 0;
    virtual void onTagLevelChanged(std::string_view tag, LogLevel level) {}
};

} // namespace logger