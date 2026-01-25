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
