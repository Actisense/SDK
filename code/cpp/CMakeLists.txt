cmake_minimum_required(VERSION 3.20)

project(ActisenseSDK
    VERSION 0.1.0
    DESCRIPTION "Modern C++17 SDK for Actisense device communication"
    LANGUAGES CXX
)

# ============================================================================
# C++ Standard & Compiler Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable position-independent code (PIE) for security
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# Build Options
# ============================================================================
option(ACTISENSE_BUILD_TESTS      "Build unit and integration tests" ON)
option(ACTISENSE_BUILD_EXAMPLES   "Build example applications" ON)
option(ACTISENSE_ENABLE_SANITIZERS "Enable ASan/UBSan on Clang/GCC" OFF)
option(ACTISENSE_ENABLE_FMT       "Enable fmt library for formatting" OFF)
option(ACTISENSE_ENABLE_COROUTINES "Enable C++20 coroutine wrappers (requires C++20)" OFF)

# ============================================================================
# CMake Helper Modules
# ============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerFlags)
include(FindOrFetchDependencies)

# ============================================================================
# Sanitizers (optional, for development)
# ============================================================================
if(ACTISENSE_ENABLE_SANITIZERS AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
    message(STATUS "Enabling ASan/UBSan sanitizers")
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# ============================================================================
# Source Directories
# ============================================================================
add_subdirectory(src)

if(ACTISENSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ACTISENSE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================================
# Install Targets
# ============================================================================
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/public/
    DESTINATION include/actisense
    FILES_MATCHING PATTERN "*.hpp"
)

# ============================================================================
# Project Summary
# ============================================================================
message(STATUS "")
message(STATUS "ActisenseSDK Configuration Summary:")
message(STATUS "  Version:                ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:           ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Tests:            ${ACTISENSE_BUILD_TESTS}")
message(STATUS "  Build Examples:         ${ACTISENSE_BUILD_EXAMPLES}")
message(STATUS "  Sanitizers:             ${ACTISENSE_ENABLE_SANITIZERS}")
message(STATUS "  fmt Support:            ${ACTISENSE_ENABLE_FMT}")
message(STATUS "  Coroutines (C++20):     ${ACTISENSE_ENABLE_COROUTINES}")
message(STATUS "  Generator:              ${CMAKE_GENERATOR}")
message(STATUS "  Compiler:               ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
