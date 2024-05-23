/**
 * spdlog logging backend implementation.
 * Logs messages to stdout, stderr, file.
 */
#ifdef RF_USE_LOGGER_SPDLOG

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cassert>

#include <roofer/logger/logger.h">

namespace roofer::logger {
  spdlog::level::level_enum cast_level(LogLevel level) {
    switch (level) {
      case LogLevel::OFF:
        return spdlog::level::off;
      case LogLevel::DEBUG:
        return spdlog::level::debug;
      case LogLevel::INFO:
        return spdlog::level::info;
      case LogLevel::WARNING:
        return spdlog::level::warn;
      case LogLevel::ERROR:
        return spdlog::level::err;
      case LogLevel::CRITICAL:
        return spdlog::level::critical;
    }
    return spdlog::level::off;
  }

  struct Logger::logger_impl {
    LogLevel level = LogLevel::default_level;

    std::shared_ptr<
        spdlog::sinks::ansicolor_stdout_sink<spdlog::details::console_mutex>>
        stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    std::shared_ptr<
        spdlog::sinks::ansicolor_stderr_sink<spdlog::details::console_mutex>>
        stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    std::shared_ptr<spdlog::sinks::basic_file_sink<std::mutex>> file_sink =
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile_path_,
                                                            true);

    spdlog::logger logger_stdout =
        spdlog::logger("stdout", {stdout_sink, file_sink});
    spdlog::logger logger_stderr =
        spdlog::logger("stderr", {stderr_sink, file_sink});

    logger_impl() {
      stdout_sink->set_level(cast_level(level));
      stderr_sink->set_level(cast_level(level));
      file_sink->set_level(cast_level(level));
    }

    ~logger_impl() {
      spdlog::drop("stdout");
      spdlog::drop("stderr");
    }

    void set_level(LogLevel new_level) {
      // We set the "level" member too, for the sake of completeness, althought
      // we only need to set the levels on the sinks for spdlog.
      level = new_level;
      stdout_sink->set_level(cast_level(new_level));
      stderr_sink->set_level(cast_level(new_level));
      file_sink->set_level(cast_level(new_level));
    }
  };

  void Logger::set_level(LogLevel level) {
    assert(impl_);
    impl_->set_level(level);
  }

  Logger &Logger::get_logger() {
    static Logger singleton;
    if (!singleton.impl_) {
      auto impl = std::make_shared<Logger::logger_impl>();
      singleton.impl_ = impl;
    }
    return singleton;
  }

  void Logger::log(LogLevel level, std::string_view message) {
    assert(impl_);
    switch (level) {
      case LogLevel::OFF:
        return;
      case LogLevel::DEBUG:
        impl_->logger_stdout.debug(message);
        return;
      case LogLevel::INFO:
        impl_->logger_stdout.info(message);
        return;
      case LogLevel::WARNING:
        impl_->logger_stdout.warn(message);
        return;
      case LogLevel::ERROR:
        impl_->logger_stderr.error(message);
        return;
      case LogLevel::CRITICAL:
        impl_->logger_stderr.critical(message);
        return;
    }
  }

}  // namespace roofer::logger
#endif
