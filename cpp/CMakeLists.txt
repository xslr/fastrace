cmake_minimum_required(VERSION 3.20)
project(fastrace_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to RelWithDebInfo build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build." FORCE)
endif()

# Dependencies
include(FetchContent)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.15.3
)
FetchContent_Declare(
  libdeflate
  GIT_REPOSITORY https://github.com/ebiggers/libdeflate.git
  GIT_TAG        v1.25
)
set(LIBDEFLATE_BUILD_GZIP OFF CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog libdeflate)

# Note: For prototype, assuming libraries are installed system-wide or via package manager.
# CLI11 is header-only. libpqxx is for PostgreSQL.
# Include directories
include_directories(include)

# Library for analyzer components so UI can link against it
add_library(fastrace_analyzer STATIC
  src/Analyzer.cpp
  src/MappedFile.cpp
  src/WorkQueue.cpp
)
target_include_directories(fastrace_analyzer PUBLIC include)
target_link_libraries(fastrace_analyzer PUBLIC libdeflate_static spdlog::spdlog)

# Executable
add_executable(cpp_parser
  src/main.cpp
)

# Libraries
# find_package(libpqxx REQUIRED)
# target_link_libraries(cpp_parser PRIVATE libpqxx::pqxx)
target_link_libraries(cpp_parser PRIVATE fastrace_analyzer)

# CLI11 (assuming it's available or we provide it in include/)
# For now, just setup the structure.
