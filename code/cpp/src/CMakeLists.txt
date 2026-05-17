# src/CMakeLists.txt - Core SDK library (unified: core, transports, protocols)

# ============================================================================
# Library Definition
# ============================================================================
add_library(actisense_sdk STATIC)

# Public headers (relative to src/)
target_include_directories(actisense_sdk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        # No additional private directories needed
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
)

# ============================================================================
# Source Files
# ============================================================================
set(SDK_SOURCES
    # Core
    core/error_category.cpp
    core/session_impl.cpp
    core/remote_device_impl.cpp
    core/logger.cpp
    core/metrics_collector.cpp

    # Transport layer (internal)
    transport/loopback/loopback_transport.cpp
    transport/serial/serial_transport.cpp
    transport/transport_error.cpp

    # Protocols
    protocols/bdtp/bdtp_protocol.cpp
    protocols/bst/bst_decoder.cpp
    protocols/bst/bst_frame.cpp
    protocols/bem/bem_protocol.cpp
    protocols/bem/bem_wrap_126720.cpp
    protocols/bem/bem_commands/operating_mode.cpp
    protocols/protocol_error.cpp

    # Utilities
    util/bdtp_frame_assembler.cpp
    util/stderr_logger.cpp

    # Public API implementation
    public/api.cpp
    public/api_session.cpp
    public/wire_trace.cpp
    public/ebl_writer.cpp
    public/hardware_info.cpp
    public/pgn_encoders.cpp

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
