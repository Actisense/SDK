# FindOrFetchDependencies.cmake - Dependency management (find or fetch)

include(FetchContent)

# ============================================================================
# fmt (optional formatting library)
# ============================================================================
if(ACTISENSE_ENABLE_FMT)
    message(STATUS "Looking for fmt library...")
    find_package(fmt 9.0 QUIET)
    
    if(NOT fmt_FOUND)
        message(STATUS "fmt not found locally, fetching from GitHub...")
        FetchContent_Declare(fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG 10.1.1
        )
        FetchContent_MakeAvailable(fmt)
    else()
        message(STATUS "fmt found: ${fmt_DIR}")
    endif()
endif()

# ============================================================================
# nlohmann/json (optional JSON library for protocol adapters)
# ============================================================================
# Not fetched by default; user can enable via find_package or manual inclusion
# Example for future use:
#   find_package(nlohmann_json QUIET)
#   if(NOT nlohmann_json_FOUND)
#     FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
#     FetchContent_MakeAvailable(json)
#   endif()

# ============================================================================
# GTest (for testing)
# ============================================================================
if(ACTISENSE_BUILD_TESTS)
    message(STATUS "Looking for GTest...")
    find_package(GTest QUIET)
    
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found locally, fetching from GitHub...")
        FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        FetchContent_MakeAvailable(googletest)
    else()
        message(STATUS "GTest found: ${GTest_DIR}")
    endif()
endif()

message(STATUS "Dependencies resolved")
