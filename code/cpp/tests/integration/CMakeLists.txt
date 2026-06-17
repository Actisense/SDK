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
# Two-device Tx PGN filtering sweep, fast-packet standard PGNs (GIT-98).
# Same rig as the standard sweep; exercises multi-frame (fast-packet) host-Tx.
add_sdk_test(test_tx_pgn_blocking_fastpacket test_tx_pgn_blocking_fastpacket.cpp)
# Two-device Tx PGN filtering sweep, fast-packet proprietary PGNs (GIT-99).
# Combines the GIT-97 proprietary header with the GIT-98 multi-frame payload;
# exercises the NGXSW-3329 DP1 bitmap mechanic across varied fast-packet lengths.
add_sdk_test(test_tx_pgn_blocking_fastpacket_proprietary test_tx_pgn_blocking_fastpacket_proprietary.cpp)
# GIT-106 diagnostic: NGT vs NGX divergence on PGN 59904 (ISO Request) host-Tx.
# Requires ACTISENSE_TEST_NGT_PORT (NGT-1) and ACTISENSE_TEST_NGX_PORT (NGX-1).
add_sdk_test(test_iso_request_git106 test_iso_request_git106.cpp)
# Black-box NGX CAN Packet routing — CanPacket / CanPacketAscii (NGXSW-4207).
# Requires ACTISENSE_TEST_PORT (a real NGX on a live N2K bus); skips otherwise.
add_sdk_test(test_can_packet_mode test_can_packet_mode.cpp)
# Negative Ack (0xF4) cancels the in-flight BEM request — hermetic loopback,
# no hardware required (GIT-100).
add_sdk_test(test_negative_ack_loopback test_negative_ack_loopback.cpp)
# getRxPgnEnable device-error reporting — hermetic loopback, no hardware (GIT-127).
add_sdk_test(test_rx_pgn_enable_device_error_loopback test_rx_pgn_enable_device_error_loopback.cpp)
# GIT-107 characterisation: NGX Rx-All silently drops PGN 59904 (ISO Request).
# Two-gateway live bus; requires ACTISENSE_TEST_NGT_PORT + ACTISENSE_TEST_NGX_PORT.
add_sdk_test(test_rxall_pgn_filter_git107 test_rxall_pgn_filter_git107.cpp)

# Add integration test files here as they are developed

# Example (uncomment when implementing Phase 6+):
# add_sdk_test(test_loopback_nmea test_loopback_nmea.cpp)
# add_sdk_test(test_request_response test_request_response.cpp)
