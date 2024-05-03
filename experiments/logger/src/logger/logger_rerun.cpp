/*
 * Copyright (c) 2024 Balázs Dukai.
 */
#include "config.hpp"

#ifdef USE_LOGGER_RERUN

#include <rerun.hpp>

#include "logger.h"

namespace Logger {
  Logger &Logger::get_logger() {
    static auto &&logger = Logger();
    return logger;
  }

  void Logger::debug(std::string_view message) {}

  void Logger::info(std::string_view message) {}

  void Logger::warning(std::string_view message) {}

  void Logger::error(std::string_view message) {}

  void Logger::critical(std::string_view message) {}
}  // namespace Logger
#endif
