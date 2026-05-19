# Integration Tests

# BDTP over loopback transport
add_sdk_test(test_bdtp_loopback test_bdtp_loopback.cpp)
# BEM commands over real serial device (requires ACTISENSE_TEST_PORT env var)
add_sdk_test(test_bem_device test_bem_device.cpp)
# Remote BEM via PGN 126720 wrapping over a fake-gateway loopback (GIT-88)
add_sdk_test(test_remote_bem_loopback test_remote_bem_loopback.cpp)
# Remote BEM sweep against a real remote device on the N2K bus (GIT-92)
# Requires ACTISENSE_TEST_PORT (local gateway) and optionally
# ACTISENSE_TEST_REMOTE_ADDR (defaults to 200).
add_sdk_test(test_bem_remote_device test_bem_remote_device.cpp)
# Two-device Tx PGN filtering sweep, single-frame standard PGNs (GIT-89).
# Requires ACTISENSE_TEST_PORT (DUT) AND ACTISENSE_TEST_RX_PORT (receiver).
add_sdk_test(test_tx_pgn_blocking_singleframe test_tx_pgn_blocking_singleframe.cpp)
# Two-device Tx PGN filtering sweep, single-frame proprietary PGNs (GIT-97).
# Same rig as the standard sweep; exercises the NGXSW-3329 DP0 bitmap mechanic.
add_sdk_test(test_tx_pgn_blocking_singleframe_proprietary test_tx_pgn_blocking_singleframe_proprietary.cpp)

# Add integration test files here as they are developed

# Example (uncomment when implementing Phase 6+):
# add_sdk_test(test_loopback_nmea test_loopback_nmea.cpp)
# add_sdk_test(test_request_response test_request_response.cpp)
