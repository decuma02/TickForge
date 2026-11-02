cmake_minimum_required(VERSION 3.20)
project(TickForge
    VERSION 1.0.0
    DESCRIPTION "High-Performance Containerized C++17 Project"
    LANGUAGES CXX
)

# ------------------------------------------------------------------------------
# C++ Standard: C++17
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Build Types and Optimization Flags
# ------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# Global Warning Flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wconversion
    )
endif()

# Release flags (optimized for profiling and benchmarking)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include Sanitizer Module
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Sanitizers)

# Options
option(BUILD_TESTING "Build the test suite (Google Test)" ON)
option(BUILD_BENCHMARKS "Build the benchmarks (Google Benchmark)" ON)
option(ENABLE_SANITIZERS "Explicitly force ASan & UBSan regardless of build type" OFF)

# ------------------------------------------------------------------------------
# Core Library Target
# ------------------------------------------------------------------------------
add_library(tickforge_core
    src/engine.cpp
)

target_include_directories(tickforge_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Attach sanitizers if Debug or explicitly enabled
configure_target_sanitizers(tickforge_core)

# ------------------------------------------------------------------------------
# Main Application Target
# ------------------------------------------------------------------------------
add_executable(tickforge_app
    src/main.cpp
)

target_link_libraries(tickforge_app
    PRIVATE
        tickforge_core
)

configure_target_sanitizers(tickforge_app)

# ------------------------------------------------------------------------------
# Testing (Google Test)
# ------------------------------------------------------------------------------
if(BUILD_TESTING)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()

# ------------------------------------------------------------------------------
# Benchmarking (Google Benchmark)
# ------------------------------------------------------------------------------
if(BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
    add_subdirectory(benchmarks)
endif()
