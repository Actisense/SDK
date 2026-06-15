# ============================================================================
# check_public_headers_guard.cmake
#
# Static guard for GIT-112: no header under src/public/ may #include an
# internal "protocols/" header. The public API surface (anything an external
# consumer can include) must depend only on other public headers, so that
# internal protocol detail never leaks across the API boundary.
#
# Run via ctest: cmake -DPUBLIC_DIR=<src/public> -P check_public_headers_guard.cmake
# ============================================================================

if(NOT DEFINED PUBLIC_DIR)
    message(FATAL_ERROR "check_public_headers_guard.cmake requires -DPUBLIC_DIR=<path>")
endif()

file(GLOB_RECURSE public_headers "${PUBLIC_DIR}/*.hpp")

set(offenders "")
foreach(hdr ${public_headers})
    file(READ "${hdr}" contents)
    # Matches: #include "protocols/...  (with optional whitespace after #)
    if(contents MATCHES "#[ \t]*include[ \t]+\"protocols/")
        list(APPEND offenders "${hdr}")
    endif()
endforeach()

if(offenders)
    string(REPLACE ";" "\n  " offender_list "${offenders}")
    message(FATAL_ERROR
        "Public SDK headers must not #include internal \"protocols/\" headers "
        "(GIT-112). Offending files:\n  ${offender_list}")
endif()

message(STATUS "Public header guard OK: no \"protocols/\" includes under ${PUBLIC_DIR}")
