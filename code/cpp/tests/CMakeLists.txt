# tests/CMakeLists.txt - Unit and integration tests

# Store the source directories before we descend into subdirectories
set(SDK_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")
set(SDK_PUBLIC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/public")

# ============================================================================
# Test Helper Function
# ============================================================================
function(add_sdk_test test_name test_file)
    add_executable(${test_name} ${test_file})
    
    target_link_libraries(${test_name}
        PRIVATE
            actisense_sdk
            GTest::gtest_main
    )
    
    target_include_directories(${test_name}
        PRIVATE
            ${SDK_SRC_DIR}
            ${SDK_PUBLIC_DIR}
    )
    
    target_compile_features(${test_name} PRIVATE cxx_std_20)
    
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# ============================================================================
# Test Subdirectories
# ============================================================================
add_subdirectory(unit)
add_subdirectory(integration)

message(STATUS "Tests configured (add test files in tests/unit/ and tests/integration/)")
