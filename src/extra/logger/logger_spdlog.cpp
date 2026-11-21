// Copyright (c) 2018-2026 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
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
 * spdlog logging backend implementation.
 * Logs messages to stdout, stderr, file.
 * The log file is a JSON file.
 */
#ifdef RF_USE_LOGGER_SPDLOG

#include <roofer/logger/logger.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cassert>

namespace roofer::logger {
  spdlog::level::level_enum cast_level(LogLevel level) {
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

  struct Logger::logger_impl {
    std::atomic<bool> stderr_only{false};
    LogLevel level = LogLevel::default_level;

    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> stdout_sink =
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    std::shared_ptr<spdlog::sinks::stderr_color_sink_mt> stderr_sink =
        std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    std::shared_ptr<spdlog::sinks::basic_file_sink_mt> file_sink;
    spdlog::logger logger_stdout{"stdout", stdout_sink};
    spdlog::logger logger_stderr{"stderr", stderr_sink};

    logger_impl() {
      // Stdout and its pipe can be writable even in a read-only directory.
      try {
        file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            logfile_path_, true);
      } catch (const spdlog::spdlog_ex& e) {
        logger_stderr.warn("File logging unavailable: {}", e.what());
      }
      if (file_sink) {
        file_sink->set_pattern(
            R"({"time": "%Y-%m-%dT%H:%M:%S.%f%z", "name": "%n", "level": "%^%l%$", "process": %P, "thread": %t, "message": "%v"},)");
        logger_stdout.sinks().push_back(file_sink);
        logger_stderr.sinks().push_back(file_sink);
      }
      set_level(level);
    }

    void set_level(LogLevel new_level) {
      // We set the "level" member too, for the sake of completeness, althought
      // we only need to set the levels on the sinks for spdlog.
      level = new_level;
      auto spdlog_level = cast_level(new_level);
      stdout_sink->set_level(spdlog_level);
      stderr_sink->set_level(spdlog_level);
      if (file_sink) file_sink->set_level(spdlog_level);
      logger_stdout.set_level(spdlog_level);
      logger_stderr.set_level(spdlog_level);
    }
  };

  void Logger::set_console_to_stderr(bool enabled) {
    impl_->stderr_only = enabled;
  }

  void Logger::set_level(LogLevel level) {
    assert(impl_);
    impl_->set_level(level);
  }

  Logger& Logger::get_logger() {
    static Logger singleton;
    if (!singleton.impl_) {
      auto impl = std::make_shared<Logger::logger_impl>();
      singleton.impl_ = impl;
    }
    return singleton;
  }

  void Logger::log(LogLevel level, std::string_view message) {
    assert(impl_);
    if (level == LogLevel::off) return;
    auto& logger = impl_->stderr_only || level >= LogLevel::error
                       ? impl_->logger_stderr
                       : impl_->logger_stdout;
    logger.log(cast_level(level), message);
  }

}  // namespace roofer::logger
#endif
