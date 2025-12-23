# src/CMakeLists.txt - Core SDK library

# ============================================================================
# Library Definition
# ============================================================================
add_library(actisense_sdk STATIC)

# Public headers (relative to src/)
target_include_directories(actisense_sdk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================================
# Source Files (placeholder - add as modules develop)
# ============================================================================
# target_sources(actisense_sdk PRIVATE
#     core/error_category.cpp
#     transport/transport_factory.cpp
#     ...
# )

# ============================================================================
# Link Dependencies
# ============================================================================
if(ACTISENSE_ENABLE_FMT)
    target_link_libraries(actisense_sdk PUBLIC fmt::fmt)
endif()

# ============================================================================
# Compiler Features
# ============================================================================
target_compile_features(actisense_sdk PUBLIC cxx_std_17)
