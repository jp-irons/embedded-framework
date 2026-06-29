#pragma once

#include "persistent_log/PersistentLogSink.hpp"

void setupLogging();

/** Process-wide persistent log sink, for FrameworkContext::setLogSink(). */
persistent_log::PersistentLogSink& persistentLogSink();
