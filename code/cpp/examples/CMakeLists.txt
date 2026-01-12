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
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    
    target_compile_features(${example_name} PRIVATE cxx_std_20)
endfunction()

# ============================================================================
# Example Definitions
# ============================================================================
add_sdk_example(actisense_console actisense_console.cpp)

# Future examples (uncomment when files are created)
# add_sdk_example(serial_nmea_receiver serial_nmea_receiver.cpp)
# add_sdk_example(udp_sender udp_sender.cpp)
# add_sdk_example(raw_transport_usage raw_transport_usage.cpp)

message(STATUS "Examples configured: actisense_console")
