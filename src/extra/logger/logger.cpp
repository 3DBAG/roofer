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
 * Internal logging backend implementation.
 * Logs all messages to stdout.
 *
 * Uses a separate writer-thread to serialize the messages to the output stream.
 * Message writer thread implementation taken from
 * https://github.com/PacktPublishing/Multi-Paradigm-Programming-with-Modern-Cpp-daytime
 * .
 */
#if !defined(RF_USE_LOGGER_SPDLOG)

#include <fmt/chrono.h>
#include <roofer/logger/logger.h>

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>

namespace roofer::logger {
  /** @brief Convert the log level into string */
  std::string string_from_log_level(roofer::logger::LogLevel level) {
    std::array<std::string, 7> names = {"OFF",     "TRACE", "DEBUG",   "INFO",
                                        "WARNING", "ERROR", "CRITICAL"};
    auto ilevel = static_cast<int>(level);
    return names[ilevel];
  }

  /** @brief Get current time, for printing into the log */
  std::string get_now_string() {
    using system_clock = std::chrono::system_clock;
    auto now = system_clock::now();
    return fmt::format("{:%F %T}", now);
  }

  struct Logger::logger_impl {
    LogLevel level = LogLevel::default_level;

    std::mutex messages_mutex;  // protects the messages queue
    struct Message {
      std::string text;
      bool to_stderr;
    };
    std::queue<Message> messages;

    std::atomic<bool> stderr_only{false};
    bool active = true;
    std::condition_variable message_pending;
    std::thread writer_thread;

    logger_impl() : writer_thread([this]() { write_messages(); }) {}

    ~logger_impl() {
      {
        std::lock_guard lock{messages_mutex};
        active = false;
      }
      message_pending.notify_one();
      writer_thread.join();
    }

    void write_messages() {
      std::unique_lock lock{messages_mutex};
      for (;;) {
        message_pending.wait(lock,
                             [&] { return !messages.empty() || !active; });
        if (messages.empty()) return;
        std::queue<Message> pending;
        pending.swap(messages);
        lock.unlock();
        while (!pending.empty()) {
          const auto& message = pending.front();
          auto& output = message.to_stderr ? std::cerr : std::cout;
          output << message.text << '\n';
          pending.pop();
        }
        lock.lock();
      }
    }

    void push_message(LogLevel log_level, std::string_view message) {
      std::stringstream stream;
      stream << "[" << get_now_string() << "]\t"
             << string_from_log_level(log_level) << '\t' << message;

      {
        std::scoped_lock lock{messages_mutex};
        messages.push({stream.str(), stderr_only.load()});
      }
      message_pending.notify_one();
    }
  };

  void Logger::set_console_to_stderr(bool enabled) {
    impl_->stderr_only = enabled;
  }

  void Logger::set_level(LogLevel level) {
    assert(impl_);
    impl_->level = level;
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
    if (impl_->level > level) {
      return;
    }
    impl_->push_message(level, message);
  }

}  // namespace roofer::logger
#endif
