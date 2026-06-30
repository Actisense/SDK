# ============================================================================
# check_api_umbrella.cmake
#
# Static guard (GIT-131): public/api.hpp must aggregate the entire public API.
#
# Verifies that every top-level public header (src/public/*.hpp, non-recursive)
# is #included by public/api.hpp. The bem_responses/ sub-headers are intentionally
# excluded — they are pulled in transitively via public/bem_callbacks.hpp, which
# api.hpp does include. This makes the umbrella property self-maintaining: add a
# new public header and forget to aggregate it, and this test fails.
#
# The companion compile-time proof is tests/unit/test_api_umbrella.cpp, which
# includes only public/api.hpp and names a symbol from each public header.
#
# Parameters:
#   PUBLIC_DIR  (required)  Path to src/public.
#
# Run via ctest:
#   cmake -DPUBLIC_DIR=<src/public> -P check_api_umbrella.cmake
# ============================================================================

if(NOT DEFINED PUBLIC_DIR)
    message(FATAL_ERROR "check_api_umbrella.cmake requires -DPUBLIC_DIR=<path>")
endif()

set(api_header "${PUBLIC_DIR}/api.hpp")
if(NOT EXISTS "${api_header}")
    message(FATAL_ERROR "check_api_umbrella.cmake: ${api_header} does not exist")
endif()

file(READ "${api_header}" api_contents)

# Top-level public headers only (non-recursive).
file(GLOB public_headers RELATIVE "${PUBLIC_DIR}" "${PUBLIC_DIR}/*.hpp")

set(missing "")
foreach(hdr ${public_headers})
    if(hdr STREQUAL "api.hpp")
        continue()
    endif()
    # Look for: #include "public/<hdr>"  (tolerate whitespace after #)
    string(REPLACE "." "\\." hdr_escaped "${hdr}")
    if(NOT api_contents MATCHES "#[ \t]*include[ \t]+\"public/${hdr_escaped}\"")
        list(APPEND missing "${hdr}")
    endif()
endforeach()

if(missing)
    string(REPLACE ";" "\n  " missing_list "${missing}")
    message(FATAL_ERROR
        "public/api.hpp is not a complete umbrella (GIT-131): it must "
        "#include every top-level public header. Missing:\n  ${missing_list}\n"
        "Add the include(s) to src/public/api.hpp.")
endif()

message(STATUS "api.hpp umbrella OK: all top-level public headers are aggregated")
