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
    Logger::info("Hello from Logger::info");
    return 0;
}
