/*
 * Copyright (c) 2024 Balázs Dukai.
 */
#include "config.hpp"

#ifdef USE_LOGGER_SPDLOG

#include <spdlog/spdlog.h>
#include "logger.h"

namespace Logger {
    void debug(std::string_view message) {
        spdlog::debug(message);
    }

    void info(std::string_view message) {
        spdlog::info(message);
    }

    void warning(std::string_view message) {
        spdlog::warn(message);
    }

    void error(std::string_view message) {
        spdlog::error(message);
    }

    void critical(std::string_view message) {
        spdlog::critical(message);
    }
} // Logger
#endif
