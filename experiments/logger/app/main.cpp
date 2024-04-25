#include <fmt/format.h>

#include <iostream>
#include <thread>
#include <vector>

#include "config.hpp"
#include "logger.h"

static void test_function(int i) {
  auto &logger = Logger::Logger::get_logger();

  const auto message =
      std::string("this is the message n° " + std::to_string(i) + ".");

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  logger.info(message);
}

int main() {
  std::cout << "Hello world!" << "\n";
  const auto welcome_message =
      fmt::format("Welcome to {} v{}\n", project_name, project_version);
  std::cout << welcome_message;
  if (use_spdlog) {
    std::cout << "use_spdlog cmake config variable is set" << "\n";
  }

  auto &logger = Logger::Logger::get_logger();
  auto &logger2 = Logger::Logger::get_logger();
  auto &logger3 = Logger::Logger::get_logger();

  std::cout << "setting log level to info" << "\n";
  logger.set_level(Logger::LogLevel::INFO);
  logger.debug("debug message");
  logger2.info("Hello from Logger::info");
  logger.warning("warning message");
  logger3.error("error message");
  logger3.critical("critical message");

  std::cout << "setting log level to error" << "\n";
  logger.set_level(Logger::LogLevel::ERROR);
  logger.debug("debug message");
  logger2.info("Hello from Logger::info");
  logger.warning("warning message");
  logger3.error("error message");
  logger3.critical("critical message");

  std::cout << "setting log level to debug" << "\n";
  logger.set_level(Logger::LogLevel::DEBUG);

  auto nr_threads = 10;

  // Using auto-join on the threads
  std::vector<std::jthread> threads_auto_join;
  for (int i = 1; i <= 10; ++i) {
    threads_auto_join.emplace_back(
        [](const int id) {
          auto &logger = Logger::Logger::get_logger();
          const auto message =
              "I am thread [" + std::to_string(id) + "]";
          logger.info(message);
        },
        i);
  }

  // Explicitly joining the threads
  auto threads = std::vector<std::thread>(nr_threads);
  for (auto i = 0U; i < nr_threads; ++i) {
    threads[i] = std::thread(test_function, i);
  }
  for (auto &thread : threads) {
    thread.join();
  }

  return 0;
}
