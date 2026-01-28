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
add_sdk_test(test_bst_frame test_bst_frame.cpp)

# BEM Command tests
add_sdk_test(test_port_baudrate test_port_baudrate.cpp)
add_sdk_test(test_port_pcode test_port_pcode.cpp)
add_sdk_test(test_rx_pgn_enable test_rx_pgn_enable.cpp)
add_sdk_test(test_tx_pgn_enable test_tx_pgn_enable.cpp)

# Phase 2a/2b BEM Command tests
add_sdk_test(test_device_control test_device_control.cpp)
add_sdk_test(test_total_time test_total_time.cpp)
add_sdk_test(test_echo test_echo.cpp)
add_sdk_test(test_unsolicited_messages test_unsolicited_messages.cpp)

# Phase 2c NMEA 2000 Product Info tests
add_sdk_test(test_nmea2000_product_info test_nmea2000_product_info.cpp)

# Phase 2d PGN List Management tests
add_sdk_test(test_pgn_list_management test_pgn_list_management.cpp)
