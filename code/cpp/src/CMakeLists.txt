# src/CMakeLists.txt - Core SDK library (unified: core, transports, protocols)

# ============================================================================
# Library Definition
# ============================================================================
add_library(actisense_sdk STATIC)

# Public headers (relative to src/public/)
target_include_directories(actisense_sdk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/public>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
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
    public/api.hpp
)

# ============================================================================
# Source Files
# ============================================================================
set(SDK_SOURCES
    # Core
    core/error_category.cpp
    core/session_impl.cpp

    # Transport layer (internal)
    transport/loopback/loopback_transport.cpp
    transport/serial/serial_transport.cpp

    # Protocols
    protocols/bdtp/bdtp_protocol.cpp
    protocols/bst/bst_decoder.cpp
    protocols/bem/bem_protocol.cpp

    # Public API implementation
    public/api.cpp

    # Platform-specific
    $<$<PLATFORM_ID:Windows>:platform/windows/EnumerateSerialDevices_windows.cpp>
    $<$<PLATFORM_ID:Linux>:platform/linux/EnumerateSerialDevices_linux.cpp>
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

# ============================================================================
# Compiler Features
# ============================================================================
target_compile_features(actisense_sdk PUBLIC cxx_std_20)
