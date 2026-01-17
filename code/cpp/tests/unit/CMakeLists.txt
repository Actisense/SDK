# Unit Tests

# Error handling tests
add_sdk_test(test_error_category test_error_category.cpp)

# Utility tests
add_sdk_test(test_ring_buffer test_ring_buffer.cpp)
add_sdk_test(test_message_ring_buffer test_message_ring_buffer.cpp)

# Transport tests
add_sdk_test(test_loopback_transport test_loopback_transport.cpp)

# Protocol tests
add_sdk_test(test_bdtp_protocol test_bdtp_protocol.cpp)
