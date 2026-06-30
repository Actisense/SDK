# examples/CMakeLists.txt - Example applications

# ============================================================================
# Example Helper Function
# ============================================================================
function(add_sdk_example example_name example_file)
    # Extra source files (e.g. a multi-file example) may follow example_file.
    add_executable(${example_name} ${example_file} ${ARGN})

    # Examples link only the public SDK target. They deliberately do NOT add
    # the src/ include root, so internal headers (core/, protocols/, transport/,
    # util/, platform/) are physically unreachable from an example — the
    # actisense_sdk target exposes a public-only include path (see
    # src/CMakeLists.txt). An example that reaches into an internal header fails
    # to compile, and is independently caught by the examples_no_internal_include
    # static guard (see tests/CMakeLists.txt). This mirrors what an external
    # consumer sees: only the installed public/ headers.
    target_link_libraries(${example_name}
        PRIVATE
            actisense_sdk
    )

    target_compile_features(${example_name} PRIVATE cxx_std_20)
endfunction()

# ============================================================================
# Example Definitions
# ============================================================================
add_sdk_example(pgn_transmitter pgn_transmitter.cpp)
add_sdk_example(nmea_reader_console
    nmea_reader/nmea_reader_console.cpp
    nmea_reader/pgn_table_model.cpp)

# Future examples (uncomment when files are created)
# add_sdk_example(udp_sender udp_sender.cpp)
# add_sdk_example(raw_transport_usage raw_transport_usage.cpp)

message(STATUS "Examples configured: pgn_transmitter, nmea_reader_console")
