/**
 * Logger library for roofer.
 *
 * Implements a generic logger facade that can use different logging backends.
 * The logging backend is selected compile time with the USE_LOGGER_* option.
 * Currently available backends:
 *  - internal (default)
 *  - spdlog (enable with USE_LOGGER_SPDLOG)
 *
 * Each implementation is thread-safe.
 *
 * References:
 * - https://hnrck.io/post/singleton-design-pattern/
 * -
 * https://github.com/PacktPublishing/Multi-Paradigm-Programming-with-Modern-Cpp-daytime/blob/master/src/helpers/logger.h
 * */
#pragma once

#include <format>
#include <memory>
#include <string>
#include <cstdint>
#include <string_view>

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
    ~Logger() = default;

    // Copy is cheap, because of the shared implementation.
    Logger(const Logger &) = default;
    Logger &operator=(const Logger &) = default;

    // Move would leave the object in a default state which does not make sense
    // here.
    Logger(Logger &&) noexcept = delete;
    Logger &operator=(Logger &&) noexcept = delete;

    /**
     * @brief Set the minimum level for the logger implementation. Messages
     * with a level below will be ignored.
     */
    void set_level(LogLevel level);

    /** @brief Returns a reference to the single logger instance. */
    static Logger &get_logger();

    template <typename... Args>
    void debug(std::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::DEBUG, std::vformat(fmt.get(), std::make_format_args(args...)));
    }

    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::INFO, std::vformat(fmt.get(), std::make_format_args(args...)));
    }

    template <typename... Args>
    void warning(std::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::WARNING, std::vformat(fmt.get(), std::make_format_args(args...)));
    }

    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::ERROR, std::vformat(fmt.get(), std::make_format_args(args...)));
    }

    template <typename... Args>
    void critical(std::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::CRITICAL, std::vformat(fmt.get(), std::make_format_args(args...)));
    }

   private:
    inline static const std::string logfile_path_{"roofer.log"};

    Logger() = default;

    struct logger_impl;
    std::shared_ptr<logger_impl> impl_;

    void log(LogLevel level, std::string_view message);

  };

}  // namespace roofer::logger
