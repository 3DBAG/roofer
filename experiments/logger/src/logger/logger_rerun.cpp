/*
 * Copyright (c) 2024 Balázs Dukai.
 */
#include "config.hpp"

#ifdef USE_LOGGER_RERUN

#include <rerun.hpp>
#include "logger.h"

namespace Logger {
    void debug(std::string_view message) {
    }

    void info(std::string_view message) {
    }

    void warning(std::string_view message) {
    }

    void error(std::string_view message) {
    }

    void critical(std::string_view message) {
    }
} // Logger
#endif
