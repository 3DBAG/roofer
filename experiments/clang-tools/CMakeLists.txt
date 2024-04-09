cmake_minimum_required(VERSION 3.22)

project(ClangTools VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Generates complile_commands.json which is required by clang-tidy

option(ENABLE_CLANG_TIDY "Enable to add clang tidy." OFF)
option(ENABLE_CLANG_FORMAT "Enable to add clang-format." OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")
include(Tools)

add_clang_format_target()

add_subdirectory(src)
