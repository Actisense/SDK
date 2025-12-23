# tests/CMakeLists.txt - Unit and integration tests

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
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    
    target_compile_features(${test_name} PRIVATE cxx_std_17)
    
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# ============================================================================
# Test Definitions (placeholder - add as tests are written)
# ============================================================================
# add_sdk_test(test_error_category unit/test_error_category.cpp)
# add_sdk_test(test_protocol_registry unit/test_protocol_registry.cpp)
# add_sdk_test(test_loopback_session integration/test_loopback_session.cpp)

message(STATUS "Tests configured (add test files in tests/ subdirectories)")
