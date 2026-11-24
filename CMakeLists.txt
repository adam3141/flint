cmake_minimum_required(VERSION 3.28)

project(flint VERSION 0.1.0 DESCRIPTION "Silica foundation library" LANGUAGES CXX)

set(FLINT_IS_TOP_LEVEL ${PROJECT_IS_TOP_LEVEL})

option(FLINT_BUILD_TESTS "Build flint unit tests" ${FLINT_IS_TOP_LEVEL})
option(FLINT_BUILD_EXAMPLES "Build flint examples" ${FLINT_IS_TOP_LEVEL})
option(FLINT_BUILD_TOOLS "Build flint command-line tools" ON)
option(FLINT_ENABLE_TRACY "Compile Tracy profiler zones into flint and its users" OFF)
set(FLINT_SANITIZERS "" CACHE STRING "Semicolon-separated sanitizers, e.g. address;undefined")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FlintCompilerOptions)
include(FlintDependencies)

add_library(flint)
add_library(flint::flint ALIAS flint)

target_sources(flint
  PRIVATE
    src/bench/benchmark.cpp
    src/bench/frame_stats.cpp
    src/bench/statistics.cpp
    src/json/parser.cpp
    src/json/value.cpp
    src/json/writer.cpp
    src/log/category.cpp
    src/log/config.cpp
    src/log/console_sink.cpp
    src/log/default_logger.cpp
    src/log/file_sink.cpp
    src/log/filter.cpp
    src/log/json_file_sink.cpp
    src/log/json_record.cpp
    src/log/level.cpp
    src/log/logger.cpp
    src/log/network_sink.cpp
    src/log/pattern_formatter.cpp
    src/log/record.cpp
    src/log/rotating_file.cpp
    src/log/tracy_sink.cpp
    src/net/socket_posix.cpp
    src/net/tcp_client.cpp
    src/net/tcp_listener.cpp
    src/platform/process.cpp
    src/platform/terminal.cpp
    src/platform/thread.cpp
    src/platform/uptime.cpp
)

target_include_directories(flint
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(flint PUBLIC cxx_std_26)
flint_target_options(flint)
flint_enable_reflection(flint)

find_package(Threads REQUIRED)
target_link_libraries(flint PUBLIC Threads::Threads)

if(FLINT_ENABLE_TRACY)
  target_link_libraries(flint PUBLIC Tracy::TracyClient)
  target_compile_definitions(flint PUBLIC FLINT_ENABLE_TRACY=1)
endif()

if(FLINT_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(FLINT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(FLINT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
