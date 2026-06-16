# tests/CMakeLists.txt - Unit and integration tests

include(GoogleTest)

# Store the source directories before we descend into subdirectories
set(SDK_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")
set(SDK_PUBLIC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/public")

# ============================================================================
# Test Helper Function
# ============================================================================
function(add_sdk_test test_name test_file)
    # Extra source files (e.g. an isolated helper TU) may follow test_file.
    add_executable(${test_name} ${test_file} ${ARGN})

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

    gtest_discover_tests(${test_name}
        WORKING_DIRECTORY $<TARGET_FILE_DIR:${test_name}>
        PROPERTIES
            LABELS "unit"
        DISCOVERY_TIMEOUT 30
    )
endfunction()

# ============================================================================
# Test Subdirectories
# ============================================================================
add_subdirectory(unit)
add_subdirectory(integration)

# ============================================================================
# Static guard (GIT-112): no header under src/public/ may #include an internal
# "protocols/" header. Pure CMake script test — portable, needs no compiler.
# ============================================================================
add_test(
    NAME public_headers_no_protocols_include
    COMMAND ${CMAKE_COMMAND}
            -DPUBLIC_DIR=${SDK_PUBLIC_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_public_headers_guard.cmake
)
set_tests_properties(public_headers_no_protocols_include PROPERTIES LABELS "guard")

message(STATUS "Tests configured (add test files in tests/unit/ and tests/integration/)")
