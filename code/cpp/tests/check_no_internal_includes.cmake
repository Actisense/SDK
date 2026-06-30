# ============================================================================
# check_no_internal_includes.cmake
#
# Generalized static boundary guard (GIT-131, generalizing the GIT-112 guard).
#
# Scans a directory tree of source files and fails if any of them #include an
# SDK-internal header — i.e. one prefixed with an internal subdirectory of src/
# (core/, transport/, platform/, util/, protocols/). Used to keep two distinct
# boundaries clean:
#   * src/public/ headers (*.hpp) — the public API surface an external consumer
#     can include must depend only on other public headers.
#   * examples/ (*.cpp, *.hpp)    — examples must model an external consumer and
#     reach the SDK only through public/ headers.
#
# Note: the public-header scan is restricted to *.hpp on purpose. The public API
# *implementation* TUs (src/public/*.cpp) legitimately include internal headers;
# only the headers themselves form the includable boundary.
#
# Parameters (all via -D on the cmake -P command line):
#   SCAN_DIR            (required)  Root directory to scan recursively.
#   SCAN_EXTENSIONS     (optional)  Semicolon list of extensions without the dot.
#                                   Default: "hpp".
#   FORBIDDEN_PREFIXES  (optional)  Semicolon list of internal include prefixes
#                                   to reject. Default:
#                                   "core;transport;platform;util;protocols".
#   GUARD_LABEL         (optional)  Human-readable label for messages.
#
# Run via ctest:
#   cmake -DSCAN_DIR=<dir> [-DSCAN_EXTENSIONS=hpp;cpp]
#         [-DFORBIDDEN_PREFIXES=core;transport;...] -P check_no_internal_includes.cmake
# ============================================================================

if(NOT DEFINED SCAN_DIR)
    message(FATAL_ERROR "check_no_internal_includes.cmake requires -DSCAN_DIR=<path>")
endif()

if(NOT DEFINED SCAN_EXTENSIONS)
    set(SCAN_EXTENSIONS "hpp")
endif()

if(NOT DEFINED FORBIDDEN_PREFIXES)
    set(FORBIDDEN_PREFIXES "core;transport;platform;util;protocols")
endif()

if(NOT DEFINED GUARD_LABEL)
    set(GUARD_LABEL "${SCAN_DIR}")
endif()

# Collect the files to scan across every requested extension.
set(scan_files "")
foreach(ext ${SCAN_EXTENSIONS})
    file(GLOB_RECURSE _matched "${SCAN_DIR}/*.${ext}")
    list(APPEND scan_files ${_matched})
endforeach()

# Build a single regex alternation of the forbidden prefixes, e.g.
#   #[ \t]*include[ \t]+"(core|transport|platform|util|protocols)/
string(REPLACE ";" "|" _prefix_alt "${FORBIDDEN_PREFIXES}")
set(forbidden_regex "#[ \t]*include[ \t]+\"(${_prefix_alt})/")

set(offenders "")
foreach(src ${scan_files})
    file(READ "${src}" contents)
    if(contents MATCHES "${forbidden_regex}")
        list(APPEND offenders "${src}")
    endif()
endforeach()

if(offenders)
    string(REPLACE ";" "\n  " offender_list "${offenders}")
    message(FATAL_ERROR
        "[${GUARD_LABEL}] Files must not #include internal SDK headers "
        "(${FORBIDDEN_PREFIXES}) — the public boundary leaks (GIT-131). "
        "Offending files:\n  ${offender_list}")
endif()

list(LENGTH scan_files _count)
message(STATUS
    "Boundary guard OK [${GUARD_LABEL}]: ${_count} file(s) scanned, "
    "no internal includes under ${SCAN_DIR}")
