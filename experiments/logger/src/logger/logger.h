/*
 * Copyright (c) 2024 Balázs Dukai.
 */
#pragma once

/**
 * Logger module for roofer.
 *
 * References:
 * - https://hnrck.io/post/singleton-design-pattern/
 * -
 * https://github.com/PacktPublishing/Multi-Paradigm-Programming-with-Modern-Cpp-daytime/blob/master/src/helpers/logger.h
 * */

#include <format>
#include <memory>
#include <string>
#include <string_view>

namespace Logger {

  enum class LogLevel : std::uint8_t {
    OFF = 0,
    DEBUG,
    INFO,
    default_level = INFO,
    WARNING,
    ERROR,
    CRITICAL,
  };

  class Logger final {
   public:
    ~Logger() = default;

    // Copy is cheap, because of the shared implementation.
    Logger(const Logger &) = default;
    Logger &operator=(const Logger &) = default;

    // Move would leave the object in a default state which does not make sense
    // here.
    Logger(Logger &&) noexcept = delete;
    Logger &operator=(Logger &&) noexcept = delete;

    /**
     * @brief Set the minimum level for the logger implementation. Messages with
     * a below will be ignored.
     */
    void set_level(LogLevel level);

    /** @brief Returns a reference to the single logger instance. */
    static Logger &get_logger();

    template <typename... Args>
    void debug(std::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::DEBUG, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::INFO, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void warning(std::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::WARNING, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::ERROR, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void critical(std::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::CRITICAL, std::format(fmt, std::forward<Args>(args)...));
    }

   private:
    inline static const std::string logfile_path_{"roofer.log"};

    Logger() = default;

    struct logger_impl;
    std::shared_ptr<logger_impl> impl_;

    void log(LogLevel level, std::string_view message);
  };

}  // namespace Logger
