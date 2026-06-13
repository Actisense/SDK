# Unit Tests

# Error handling tests
add_sdk_test(test_error_category test_error_category.cpp)

# Logging tests
add_sdk_test(test_logging test_logging.cpp)

# Metrics tests
add_sdk_test(test_metrics test_metrics.cpp)

# Utility tests
add_sdk_test(test_message_ring_buffer test_message_ring_buffer.cpp)

# Transport tests
add_sdk_test(test_loopback_transport test_loopback_transport.cpp)

# Protocol tests
add_sdk_test(test_bdtp_protocol test_bdtp_protocol.cpp)
add_sdk_test(test_bdtp_frame_assembler test_bdtp_frame_assembler.cpp)
add_sdk_test(test_bst_frame test_bst_frame.cpp)

# BEM Command tests
add_sdk_test(test_operating_mode test_operating_mode.cpp)
add_sdk_test(test_port_baudrate test_port_baudrate.cpp)
add_sdk_test(test_port_pcode test_port_pcode.cpp)
add_sdk_test(test_rx_pgn_enable test_rx_pgn_enable.cpp)
add_sdk_test(test_tx_pgn_enable test_tx_pgn_enable.cpp)

# Phase 2a/2b BEM Command tests
add_sdk_test(test_device_control test_device_control.cpp)
add_sdk_test(test_session_device_control test_session_device_control.cpp)
add_sdk_test(test_total_time test_total_time.cpp)
add_sdk_test(test_echo test_echo.cpp)
add_sdk_test(test_unsolicited_messages test_unsolicited_messages.cpp)
add_sdk_test(test_session_unsolicited test_session_unsolicited.cpp)

# Phase 2c NMEA 2000 Product Info tests
add_sdk_test(test_nmea2000_product_info test_nmea2000_product_info.cpp)

# PGN 126720 BEM wrap/unwrap helpers (GIT-88)
add_sdk_test(test_bem_wrap_126720 test_bem_wrap_126720.cpp)

# BEM correlator srcAddr keying (GIT-88)
add_sdk_test(test_bem_correlation_src_addr test_bem_correlation_src_addr.cpp)

# Public PGN encoder tests (GIT-14)
add_sdk_test(test_pgn_encoders test_pgn_encoders.cpp)

# Session::sendPgn on-wire framing regression test (GIT-81 follow-up)
add_sdk_test(test_session_send_pgn test_session_send_pgn.cpp)

# Phase 2d PGN List Management tests
add_sdk_test(test_pgn_list_management test_pgn_list_management.cpp)
add_sdk_test(test_session_pgn_list_management test_session_pgn_list_management.cpp)

# API entry point tests
add_sdk_test(test_api_open test_api_open.cpp)
add_sdk_test(test_api_open_with_transport test_api_open_with_transport.cpp)

# Wire-trace (hex dump) tests (GIT-63)
add_sdk_test(test_wire_trace test_wire_trace.cpp)

# EBL writer / wire-trace EBL mode tests (GIT-80)
add_sdk_test(test_ebl_writer test_ebl_writer.cpp)

# macOS enumerator composition tests (GIT-70) — pure C++, runs on all platforms
add_sdk_test(test_enumerate_serial_devices_macos test_enumerate_serial_devices_macos.cpp)
