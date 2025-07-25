// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

// Author(s):
// Balazs Dukai

/**
 * Logger library for roofer.
 *
 * Implements a logger using the spdlog library as the backend.
 * The logger is thread-safe and writes messages to a JSON file in addition to 
 * logging to the console. The JSON log allows efficient log processing.
 *
 * References:
 * - https://hnrck.io/post/singleton-design-pattern/
 * -
 * https://github.com/PacktPublishing/Multi-Paradigm-Programming-with-Modern-Cpp-daytime/blob/master/src/helpers/logger.h
 * */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <mutex>

#include "fmt/format.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace roofer::logger {

  enum class LogLevel : std::uint8_t {
    off = 0,
    trace,
    debug,
    info,
    default_level = info,
    warning,
    error,
    critical,
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
    void set_level(LogLevel level) {
      if (impl_) {
        impl_->set_level(level);
      }
    }

    /** @brief Returns a reference to the single logger instance. */
    static Logger &get_logger() {
      static Logger singleton;
      if (!singleton.impl_) {
        auto impl = std::make_shared<Logger::logger_impl>();
        singleton.impl_ = impl;
      }
      return singleton;
    }

    /**
     * @brief Trace is used for logging counts on a process, eg. the number of
     * reconstructed buildings.
     *
     * The trace message is a string that contains a valid JSON object with the
     * "name" and "count" members.
     *
     * @param name Name of the process, eg. "reconstruct".
     * @param count Current count, eg. the number of reconstructed buildings.
     */
    void trace(std::string_view name, size_t count) {
      log(LogLevel::trace,
          fmt::format(R"({{\"name\":\"{}\",\"count\":{}}})", name, count));
    }

    template <typename... Args>
    void debug(fmt::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::debug, fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

    template <typename... Args>
    void info(fmt::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::info, fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

    template <typename... Args>
    void warning(fmt::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::warning, fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

    template <typename... Args>
    void error(fmt::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::error, fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

    template <typename... Args>
    void critical(fmt::format_string<Args...> fmt, Args &&...args) {
      log(LogLevel::critical,
          fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

   private:
    inline static const std::string logfile_path_{"roofer.log.json"};

    Logger() = default;

    struct logger_impl {
      LogLevel level = LogLevel::default_level;

      std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> stdout_sink =
          std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      std::shared_ptr<spdlog::sinks::stderr_color_sink_mt> stderr_sink =
          std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
      std::shared_ptr<spdlog::sinks::basic_file_sink<std::mutex>> file_sink =
          std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile_path_,
                                                              true);

      spdlog::logger logger_stdout =
          spdlog::logger("stdout", {stdout_sink, file_sink});
      spdlog::logger logger_stderr =
          spdlog::logger("stderr", {stderr_sink, file_sink});

      logger_impl() {
        auto spdlog_level = cast_level(level);
        stdout_sink->set_level(spdlog_level);
        stderr_sink->set_level(spdlog_level);
        file_sink->set_level(spdlog_level);
        // Set up json logging in the logfile.
        file_sink->set_pattern("{\n \"log\": [");
        std::string jsonpattern = {
            R"({"time": "%Y-%m-%dT%H:%M:%S.%f%z", "name": "%n", "level": "%^%l%$", "process": %P, "thread": %t, "message": "%v"},)"};
        file_sink->set_pattern(jsonpattern);
      }

      ~logger_impl() {
        // Finalize the json logfile.
        std::string jsonlastlogpattern = {
            R"({"time": "%Y-%m-%dT%H:%M:%S.%f%z", "name": "%n", "level": "%^%l%$", "process": %P, "thread": %t, "message": "%v"})"};
        file_sink->set_pattern(jsonlastlogpattern);
        file_sink->set_pattern("]\n}");
        spdlog::drop_all();
      }

      void set_level(LogLevel new_level) {
        level = new_level;
        auto spdlog_level = cast_level(new_level);
        stdout_sink->set_level(spdlog_level);
        stderr_sink->set_level(spdlog_level);
        file_sink->set_level(spdlog_level);
        logger_stdout.set_level(spdlog_level);
        logger_stderr.set_level(spdlog_level);
      }

      static spdlog::level::level_enum cast_level(LogLevel level) {
        switch (level) {
          case LogLevel::off:
            return spdlog::level::off;
          case LogLevel::trace:
            return spdlog::level::trace;
          case LogLevel::debug:
            return spdlog::level::debug;
          case LogLevel::info:
            return spdlog::level::info;
          case LogLevel::warning:
            return spdlog::level::warn;
          case LogLevel::error:
            return spdlog::level::err;
          case LogLevel::critical:
            return spdlog::level::critical;
        }
        return spdlog::level::off;
      }
    };
    std::shared_ptr<logger_impl> impl_;

    void log(LogLevel level, std::string_view message) {
      if (!impl_) return;
      switch (level) {
        case LogLevel::off:
          return;
        case LogLevel::trace:
          impl_->logger_stdout.trace(message);
          return;
        case LogLevel::debug:
          impl_->logger_stdout.debug(message);
          return;
        case LogLevel::info:
          impl_->logger_stdout.info(message);
          return;
        case LogLevel::warning:
          impl_->logger_stdout.warn(message);
          return;
        case LogLevel::error:
          impl_->logger_stderr.error(message);
          return;
        case LogLevel::critical:
          impl_->logger_stderr.critical(message);
          return;
      }
    }
  };

}  // namespace roofer::logger
