# Integration Tests

# BDTP over loopback transport
add_sdk_test(test_bdtp_loopback test_bdtp_loopback.cpp)
# BEM commands over real serial device (requires ACTISENSE_TEST_PORT env var)
add_sdk_test(test_bem_device test_bem_device.cpp)

# Add integration test files here as they are developed

# Example (uncomment when implementing Phase 6+):
# add_sdk_test(test_loopback_nmea test_loopback_nmea.cpp)
# add_sdk_test(test_request_response test_request_response.cpp)
