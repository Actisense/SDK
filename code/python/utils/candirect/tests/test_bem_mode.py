#!/usr/bin/env python3
"""Tests for the BEM Get/Set operating-mode codec (BST id 0xA1 / 0xA0, BEM 0x11)."""

from _harness import TestRunner, hexstr
import candirect as cd


def datagram_on_wire(wire: bytes) -> bytes:
    """Recover the BST datagram (incl checksum) from BDTP-framed wire bytes."""
    frames = cd.BDTPDecoder().feed(wire)
    assert len(frames) == 1, f"expected 1 frame, got {len(frames)}"
    return frames[0]


def fake_mode_response(mode: int) -> bytes:
    """Build the BDTP-recovered datagram a device would send for a mode read."""
    datagram = bytes([cd.BEM_RSP_ID, 3, cd.BEM_OPMODE, mode & 0xFF, (mode >> 8) & 0xFF])
    return datagram + bytes([cd.bst_checksum(datagram)])


def main() -> None:
    t = TestRunner("BEM operating-mode codec")

    # --- Set-mode vector from bem-detail/operating-mode.md -------------------
    # Setting OM = 2 must produce datagram A1 03 11 02 00 (+ zero-sum checksum).
    dg = datagram_on_wire(cd.encode_bem_set_mode(2))
    t.equal("Set-mode-2 datagram", dg, bytes([0xA1, 0x03, 0x11, 0x02, 0x00, 0x49]))
    t.check("Set-mode-2 datagram is zero-sum", cd.verify_zero_sum(dg))

    # Setting the three modes the tool uses.
    for mode in (cd.MODE_NORMAL, cd.MODE_CAN_DIRECT, cd.MODE_CAN_ASCII):
        dg = datagram_on_wire(cd.encode_bem_set_mode(mode))
        t.equal(f"Set-mode-{mode} header", dg[:3], bytes([0xA1, 0x03, 0x11]))
        t.equal(f"Set-mode-{mode} low byte", dg[3], mode)
        t.equal(f"Set-mode-{mode} high byte", dg[4], 0)
        t.check(f"Set-mode-{mode} zero-sum", cd.verify_zero_sum(dg))

    # --- Get-mode command ----------------------------------------------------
    dg = datagram_on_wire(cd.encode_bem_get_mode())
    t.equal("Get-mode datagram", dg, bytes([0xA1, 0x01, 0x11, 0x4D]))
    t.check("Get-mode datagram is zero-sum", cd.verify_zero_sum(dg))

    # --- Response decode round-trip -----------------------------------------
    for mode in (cd.MODE_NORMAL, cd.MODE_CAN_DIRECT, cd.MODE_CAN_ASCII):
        got = cd.decode_bem_mode_response(fake_mode_response(mode))
        t.equal(f"decode mode response {mode}", got, mode)

    # --- Negative cases: non-mode frames must return None, not raise ---------
    not_mode = cd.decode_bem_mode_response(datagram_on_wire(cd.encode_bem_get_mode()))
    t.equal("command frame is not a mode response", not_mode, None)
    bst95 = cd.BDTPDecoder().feed(cd.encode_bst95(128267, 3, 0, 255, bytes(8)))[0]
    t.equal("BST-95 frame is not a mode response", cd.decode_bem_mode_response(bst95), None)
    corrupt = bytearray(fake_mode_response(cd.MODE_CAN_DIRECT))
    corrupt[-1] ^= 0xFF
    t.equal("corrupt response rejected", cd.decode_bem_mode_response(bytes(corrupt)), None)

    print(f"\nSet-mode-5 wire: {hexstr(cd.encode_bem_set_mode(cd.MODE_CAN_DIRECT))}")
    t.finish()


if __name__ == "__main__":
    main()
