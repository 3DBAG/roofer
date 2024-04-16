/*
 * Copyright (c) 2024 Balázs Dukai.
 */
#include "config.hpp"

#if !defined(USE_LOGGER_SPDLOG) && !defined(USE_LOGGER_RERUN)

#include <iostream>
#include "logger.h"

namespace Logger {
    void debug(std::string_view message) {
        std::cout << "DEBUG:" << " " << message << "\n";
    }

    void info(std::string_view message) {
        std::cout << "INFO:" << " " << message << "\n";
    }

    void warning(std::string_view message) {
        std::cout << "WARNING:" << " " << message << "\n";
    }

    void error(std::string_view message) {
        std::cout << "ERROR:" << " " << message << "\n";
    }

    void critical(std::string_view message) {
        std::cout << "CRITICAL:" << " " << message << "\n";
    }
} // Logger
#endif