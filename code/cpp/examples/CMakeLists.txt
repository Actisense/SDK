# examples/CMakeLists.txt - Example applications

# ============================================================================
# Example Helper Function
# ============================================================================
function(add_sdk_example example_name example_file)
    # Extra source files (e.g. a multi-file example) may follow example_file.
    add_executable(${example_name} ${example_file} ${ARGN})

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
add_sdk_example(pgn_transmitter pgn_transmitter.cpp)
add_sdk_example(nmea_reader_console
    nmea_reader/nmea_reader_console.cpp
    nmea_reader/pgn_table_model.cpp)

# Future examples (uncomment when files are created)
# add_sdk_example(udp_sender udp_sender.cpp)
# add_sdk_example(raw_transport_usage raw_transport_usage.cpp)

message(STATUS "Examples configured: actisense_console, pgn_transmitter, nmea_reader_console")
