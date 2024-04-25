/*
 * Copyright (c) 2024 Balázs Dukai.
 */
#pragma once

/**
 * Logger module for roofer.
 * */

#include <string_view>
#include <memory>

namespace roofer::logger {

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
    inline static const std::string logfile_path_{"roofer.log"};

    ~Logger() = default;

    // Copy is cheap, because of the shared implementation.
    Logger(const Logger &) = default;
    Logger& operator=(const Logger &) = default;

    // Move would leave the object in a default state which does not make sense
    // here.
    Logger(Logger &&) noexcept = delete;
    Logger &operator=(Logger &&) noexcept = delete;

    /**
     * @brief Set the minimum level for the logger implementation. Messages with a
     * below will be ignored.
     */
    void set_level(LogLevel level);

    /** @brief Returns a reference to the single logger instance. */
    static Logger &get_logger();

    void debug(std::string_view message);

    void info(std::string_view message);

    void warning(std::string_view message);

    void error(std::string_view message);

    void critical(std::string_view message);

   private:
    Logger() = default;

    struct logger_impl;
    std::shared_ptr<logger_impl> impl_;
  };

}  // namespace Logger
