// SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
// SPDX-License-Identifier: MIT

#pragma once

namespace logger {

enum class LogLevel {
	Verbose = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4
};

} // namespace logger