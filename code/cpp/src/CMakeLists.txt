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
    public/actisense/version.hpp
    public/actisense/error.hpp
    public/actisense/config.hpp
    public/actisense/events.hpp
    public/actisense/session.hpp
    public/actisense/sdk.hpp
)

# ============================================================================
# Source Files
# ============================================================================
set(SDK_SOURCES
    # Core
    core/error_category.cpp

    # Transport layer (internal)
    transport/loopback/loopback_transport.cpp

    # Protocols
    protocols/bdtp/bdtp_protocol.cpp
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
