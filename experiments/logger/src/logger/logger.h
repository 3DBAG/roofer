/*
 * Copyright (c) 2024 Balázs Dukai.
 */
#pragma once

#include <string_view>

namespace Logger {

    void debug(std::string_view message);

    void info(std::string_view message);

    void warning(std::string_view message);

    void error(std::string_view message);

    void critical(std::string_view message);

} // Logger

