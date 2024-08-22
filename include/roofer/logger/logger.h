// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of geoflow-roofer (https://github.com/3DBAG/geoflow-roofer)

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
 * Implements a generic logger facade that can use different logging backends.
 * The logging backend is selected compile time with the RF_USE_LOGGER_* option.
 * Currently available backends:
 *  - internal (default)
 *  - spdlog (enable with RF_USE_LOGGER_SPDLOG)
 *
 * Each implementation is thread-safe.
 * The spdlog backend writes the messages to a JSON file in addition to logging
 * to the console. If you enable tracing, it is recommended to use the spdlog
 * backend, because the JSON log allows efficient log processing.
 *
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

#include "fmt/format.h"

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
    void set_level(LogLevel level);

    /** @brief Returns a reference to the single logger instance. */
    static Logger &get_logger();

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

    struct logger_impl;
    std::shared_ptr<logger_impl> impl_;

    void log(LogLevel level, std::string_view message);
  };

}  // namespace roofer::logger
