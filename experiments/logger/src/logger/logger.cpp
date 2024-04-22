/*
 * Copyright (c) 2024 Balázs Dukai.
 */
#include "config.hpp"

#if !defined(USE_LOGGER_SPDLOG) && !defined(USE_LOGGER_RERUN)

#include <iostream>

#include "logger.h"

namespace Logger {

  Logger &Logger::get_logger() {
    static auto &&logger = Logger();
    return logger;
  }

  void Logger::debug(std::string_view message) {
    std::cout << "DEBUG:" << " " << message << "\n";
  }

  void Logger::info(std::string_view message) {
    std::cout << "INFO:" << " " << message << "\n";
  }

  void Logger::warning(std::string_view message) {
    std::cout << "WARNING:" << " " << message << "\n";
  }

  void Logger::error(std::string_view message) {
    std::cout << "ERROR:" << " " << message << "\n";
  }

  void Logger::critical(std::string_view message) {
    std::cout << "CRITICAL:" << " " << message << "\n";
  }
}  // namespace Logger
#endif
