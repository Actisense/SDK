# examples/CMakeLists.txt - Example applications

# ============================================================================
# Example Helper Function
# ============================================================================
function(add_sdk_example example_name example_file)
    add_executable(${example_name} ${example_file})
    
    target_link_libraries(${example_name}
        PRIVATE
            actisense_sdk
    )
    
    target_include_directories(${example_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/public
    )
    
    target_compile_features(${example_name} PRIVATE cxx_std_20)
endfunction()

# ============================================================================
# Example Definitions (placeholder - add as examples are created)
# ============================================================================
# add_sdk_example(serial_nmea_receiver serial_nmea_receiver.cpp)
# add_sdk_example(udp_sender udp_sender.cpp)
# add_sdk_example(raw_transport_usage raw_transport_usage.cpp)

message(STATUS "Examples configured (add example files in examples/ directory)")
