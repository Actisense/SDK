# CompilerFlags.cmake - Cross-platform compiler configuration

# ============================================================================
# MSVC (Windows)
# ============================================================================
if(MSVC)
    # Warning level 4, treat warnings as errors for core lib
    add_compile_options(/W4)
    
    # Enable standard-compliant behavior
    add_compile_options(/permissive-)
    
    # Disable security warnings for POSIX functions (strcpy, etc.)
    # Note: Use safe alternatives (_s) on Windows when possible
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    
    # Runtime library: use appropriate runtime for build type
    # MultiThreaded$<$<CONFIG:Debug>:Debug>DLL expands to:
    #   - MultiThreadedDebugDLL for Debug builds
    #   - MultiThreadedDLL for Release builds
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# ============================================================================
# GCC / Clang (Linux, macOS, MinGW)
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Standard warnings
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wformat=2
        -Wunused
        -Wcast-align
        -Wshadow
        -Wwrite-strings
    )
    
    # Treat warnings as errors in CI or strict mode
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Less strict in Release to avoid third-party warnings
    else()
        add_compile_options(-Werror)
    endif()
    
    # Visibility: hide symbols by default (library best practice)
    add_compile_options(-fvisibility=hidden)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
endif()

# ============================================================================
# Clang-specific
# ============================================================================
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
    )
endif()

# ============================================================================
# Debug vs Release
# ============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-g -O0)
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()
