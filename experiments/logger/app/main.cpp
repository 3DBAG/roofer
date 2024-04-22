#include <iostream>

#include <fmt/format.h>

#include "config.hpp"
#include "logger.h"

int main() {
    std::cout << "Hello world!" << "\n";
    const auto welcome_message = fmt::format("Welcome to {} v{}\n", project_name, project_version);
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

    return 0;
}
