# src/CMakeLists.txt - Core SDK library (unified: core, transports, protocols)

# ============================================================================
# Library Definition
# ============================================================================
add_library(actisense_sdk STATIC)

# ============================================================================
# Public-only staged include tree (GIT-131)
# ============================================================================
# Downstream consumers (the examples, the public-header self-containment sweep,
# and the api.hpp umbrella TU) must be able to reach public/ headers but NOT the
# internal dirs (core/, protocols/, transport/, util/, platform/). src/ contains
# both, so exposing src/ downstream would let an example #include an internal
# header. Instead we stage a build-tree tree that holds ONLY public/*.hpp and
# expose THAT as the library's downstream (PUBLIC) build-interface include. The
# library compiles its own TUs against src/ (PRIVATE), where the internal headers
# live. In-tree tests that legitimately use internals add src/ explicitly (see
# tests/CMakeLists.txt). The install interface is unchanged (<prefix>/include,
# with headers installed under include/public/ — see the top-level CMakeLists).
set(SDK_PUBLIC_INCLUDE_STAGE "${CMAKE_BINARY_DIR}/public_include"
    CACHE INTERNAL "Public-only staged include root for downstream consumers")
file(MAKE_DIRECTORY "${SDK_PUBLIC_INCLUDE_STAGE}/public")

# Refresh the staged copy on every build so header edits propagate
# (copy_if_different keeps it incremental); CONFIGURE_DEPENDS re-globs when a
# public header is added or removed.
file(GLOB_RECURSE _public_header_sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/public/*.hpp")
set(_staged_public_headers "")
foreach(_hdr ${_public_header_sources})
    file(RELATIVE_PATH _rel "${CMAKE_CURRENT_SOURCE_DIR}/public" "${_hdr}")
    set(_staged "${SDK_PUBLIC_INCLUDE_STAGE}/public/${_rel}")
    add_custom_command(
        OUTPUT  "${_staged}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_hdr}" "${_staged}"
        DEPENDS "${_hdr}"
        COMMENT "Staging public header ${_rel}"
        VERBATIM)
    list(APPEND _staged_public_headers "${_staged}")
endforeach()
add_custom_target(stage_public_headers DEPENDS ${_staged_public_headers})
add_dependencies(actisense_sdk stage_public_headers)

# Public headers reach downstream via the public-only stage; internal src/ is
# PRIVATE to the library's own TUs.
target_include_directories(actisense_sdk
    PUBLIC
        $<BUILD_INTERFACE:${SDK_PUBLIC_INCLUDE_STAGE}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# ============================================================================
# Public Header Files
# ============================================================================
set(PUBLIC_HEADERS
    public/version.hpp
    public/error.hpp
    public/config.hpp
    public/events.hpp
    public/session.hpp
    public/remote_device.hpp
    public/api.hpp
    public/logging.hpp
    public/metrics.hpp
    public/wire_trace.hpp
    public/ebl_writer.hpp
    public/operating_mode.hpp
    public/hardware_info.hpp
    public/pgn_encoders.hpp
    public/received_frame.hpp
)

# ============================================================================
# Source Files
# ============================================================================
set(SDK_SOURCES
    # Core
    core/error_category.cpp
    core/session.cpp
    core/session_impl.cpp
    core/session_commands.cpp
    core/session_remote.cpp
    core/session_receive.cpp
    core/supported_pgn_list_walk.cpp
    core/session_wire_trace.cpp
    core/remote_device.cpp
    core/remote_device_impl.cpp
    core/logger.cpp
    core/metrics_collector.cpp

    # Transport layer (internal)
    transport/loopback/loopback_transport.cpp
    transport/serial/serial_transport.cpp
    # Linux custom-baud (BOTHER/TCSETS2) helper. Built on every platform; the
    # body is gated to __linux__ and compiles to a no-op elsewhere. Kept in its
    # own TU so <asm/termbits.h> never co-includes <termios.h>.
    transport/serial/posix_custom_baud.cpp

    # Protocols
    protocols/bdtp/bdtp_protocol.cpp
    protocols/bst/bst_decoder.cpp
    protocols/bst/bst_frame.cpp
    protocols/bem/bem_protocol.cpp
    protocols/bem/bem_wrap_126720.cpp
    protocols/bem/bem_commands/operating_mode.cpp

    # Utilities
    util/bdtp_frame_assembler.cpp
    util/debug_log.cpp
    util/stderr_logger.cpp

    # Public API implementation
    public/api.cpp
    public/api_session.cpp
    public/wire_trace.cpp
    public/ebl_writer.cpp
    public/hardware_info.cpp
    public/pgn_encoders.cpp
    public/received_frame.cpp

    # Platform-specific
    $<$<PLATFORM_ID:Windows>:platform/windows/enumerate_serial_devices_windows.cpp>
    $<$<PLATFORM_ID:Linux>:platform/linux/enumerate_serial_devices_linux.cpp>
    # macOS file is built on every platform so BuildSerialDeviceInfoList()
    # (the pure-C++ composition helper) can be unit-tested without IOKit.
    # The IOKit-using EnumerateSerialDevices() inside is gated to __APPLE__.
    platform/macos/enumerate_serial_devices_macos.cpp
)

target_sources(actisense_sdk PRIVATE ${SDK_SOURCES})

# ============================================================================
# Link Dependencies
# ============================================================================
if(ACTISENSE_ENABLE_FMT)
    target_link_libraries(actisense_sdk PUBLIC fmt::fmt)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(actisense_sdk PRIVATE ws2_32)
endif()

if(APPLE)
    target_link_libraries(actisense_sdk PRIVATE "-framework IOKit" "-framework CoreFoundation")
endif()

# ============================================================================
# Compiler Features
# ============================================================================
target_compile_features(actisense_sdk PUBLIC cxx_std_20)
