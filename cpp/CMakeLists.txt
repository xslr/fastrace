cmake_minimum_required(VERSION 3.20)
project(fastrace_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find dependencies
# Note: For prototype, assuming libraries are installed system-wide or via package manager.
# CLI11 is header-only. libpqxx is for PostgreSQL.
# Include directories
include_directories(include)

# Executable
add_executable(cpp_parser src/main.cpp)

# Libraries
# find_package(libpqxx REQUIRED)
# target_link_libraries(cpp_parser PRIVATE libpqxx::pqxx)
find_package(ZLIB)
target_link_libraries(cpp_parser PRIVATE ZLIB::ZLIB)

# CLI11 (assuming it's available or we provide it in include/)
# For now, just setup the structure.
