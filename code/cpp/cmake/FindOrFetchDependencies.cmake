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
    message(STATUS "Looking for GTest (>= 1.17.0)...")
    find_package(GTest 1.17 QUIET)

    if(NOT GTest_FOUND)
        message(STATUS "GTest >= 1.17.0 not found locally, fetching from GitHub...")
        # MSVC 19.50 (VS 2026) has broken test registration with gtest < 1.17.
        # Match the host project's CRT to avoid LNK2038 on Windows.
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(googletest)
    else()
        message(STATUS "GTest found: ${GTest_DIR}")
    endif()
endif()

message(STATUS "Dependencies resolved")
