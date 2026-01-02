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
# Source Files (add as modules develop)
# ============================================================================
# Placeholder source to satisfy STATIC library requirement
# Will be replaced with actual sources in Phase 1+
set(SDK_SOURCES
    # core/version.cpp
    # core/error_category.cpp
    # transport/loopback_transport.cpp
    # protocols/protocol_registry.cpp
)

# For now, create a minimal source to allow library creation
# This will be replaced when Phase 1 implementation begins
if(NOT SDK_SOURCES)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/placeholder.cpp" 
         "// Placeholder - will be replaced with actual sources\n")
    list(APPEND SDK_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/placeholder.cpp")
endif()

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
target_compile_features(actisense_sdk PUBLIC cxx_std_17)
