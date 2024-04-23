/*
 * Copyright (c) 2024 Balázs Dukai.
 */
#include "config.hpp"

#if !defined(USE_LOGGER_SPDLOG) && !defined(USE_LOGGER_RERUN)

#include <cassert>
#include <chrono>
#include <iostream>
#include <syncstream>

#include "logger.h"

namespace {
  /** @brief Convert log level into string */
  std::string string_from_log_level(Logger::LogLevel level) {
    std::array<std::string, 6> names = {"OFF",     "DEBUG", "INFO",
                                        "WARNING", "ERROR", "CRITICAL"};
    auto ilevel = static_cast<int>(level);
    return names[ilevel];
  }

  /** @brief Get current time, for printing into the log */
  std::string get_now_string() {
    using system_clock = std::chrono::system_clock;
    auto now = system_clock::now();
    auto cnow = system_clock::to_time_t(now);
    auto result = std::string{std::ctime(&cnow)};
    // ctime result always ends with '\n', we don't want it in our logs
    if (!result.empty()) result.pop_back();
    return result;
  }
}  // namespace

namespace Logger {
  struct Logger::logger_impl {
    LogLevel level = LogLevel::default_level;
    std::osyncstream stream{std::cout};

    logger_impl() = default;
    ~logger_impl() = default;

    void write_message(LogLevel log_level, std::string_view message) {
      stream << "[" << get_now_string() << "]\t"
             << string_from_log_level(log_level) << '\t' << message
             << '\n';
      stream.emit();
    }
  };

  void Logger::set_level(LogLevel level) {
    assert(impl_);
    impl_->level = level;
  }

  Logger &Logger::get_logger() {
    static Logger singleton;
    if (!singleton.impl_) {
      auto impl = std::make_shared<Logger::logger_impl>();
      singleton.impl_ = impl;
    }
    return singleton;
  }

  void Logger::debug(std::string_view message) {
    assert(impl_);
    if (impl_->level > LogLevel::DEBUG) {
      return;
    }
    impl_->write_message(LogLevel::DEBUG, message);
  }

  void Logger::info(std::string_view message) {
    assert(impl_);
    if (impl_->level > LogLevel::INFO) {
      return;
    }
    impl_->write_message(LogLevel::INFO, message);
  }

  void Logger::warning(std::string_view message) {
    assert(impl_);
    if (impl_->level > LogLevel::WARNING) {
      return;
    }
    impl_->write_message(LogLevel::WARNING, message);
  }

  void Logger::error(std::string_view message) {
    assert(impl_);
    if (impl_->level > LogLevel::ERROR) {
      return;
    }
    impl_->write_message(LogLevel::ERROR, message);
  }

  void Logger::critical(std::string_view message) {
    assert(impl_);
    if (impl_->level > LogLevel::CRITICAL) {
      return;
    }
    impl_->write_message(LogLevel::CRITICAL, message);
  }
}  // namespace Logger
#endif
