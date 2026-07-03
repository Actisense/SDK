#!/usr/bin/env python3
"""Tests for the BEM Get/Set operating-mode codec (BST id 0xA1 / 0xA0, BEM 0x11)."""

from _harness import TestRunner, hexstr
import candirect as cd


def datagram_on_wire(wire: bytes) -> bytes:
    """Recover the BST datagram (incl checksum) from BDTP-framed wire bytes."""
    frames = cd.BDTPDecoder().feed(wire)
    assert len(frames) == 1, f"expected 1 frame, got {len(frames)}"
    return frames[0]


def fake_mode_response(mode: int, *, seq: int = 1, model: int = 0x000E,
                       serial: int = 0x12345678, error: int = 0) -> bytes:
    """Build the BDTP-recovered datagram a device would send for a mode read.

    Mirrors the real BEM response wire layout (bst-bem-response.md): a data block
    of BEM Id, Sequence Id, Model Id (2, LE), Serial Number (4, LE), Error Code
    (4, LE) then the 2-byte little-endian operating mode, plus the trailing
    zero-sum checksum. The mode therefore sits behind the 14-byte header, not
    immediately after the BEM Id.
    """
    data_block = (
        bytes([cd.BEM_OPMODE, seq & 0xFF])
        + (model & 0xFFFF).to_bytes(2, "little")
        + (serial & 0xFFFFFFFF).to_bytes(4, "little")
        + (error & 0xFFFFFFFF).to_bytes(4, "little")
        + bytes([mode & 0xFF, (mode >> 8) & 0xFF])
    )
    datagram = bytes([cd.BEM_RSP_ID, len(data_block)]) + data_block
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

    # Decode the worked example from bst-bem-response.md (mode 0x0203).
    doc_example = bytes([0xA0, 0x0E, 0x11, 0x05, 0x00, 0x0E, 0x78, 0x56, 0x34, 0x12,
                         0x00, 0x00, 0x00, 0x00, 0x03, 0x02])
    doc_example += bytes([cd.bst_checksum(doc_example)])
    t.equal("decode documented example", cd.decode_bem_mode_response(doc_example), 0x0203)

    # Regression: the mode must be read from behind the 14-byte response header.
    # An earlier decoder read the Sequence Id and Model-Id low byte instead, so a
    # device genuinely in mode 5 was reported as 15105 (0x3B01 - seq 0x01 as the
    # low byte, model-id low byte 0x3B as the high byte).
    misread = fake_mode_response(cd.MODE_CAN_DIRECT, seq=0x01, model=0x003B)
    t.equal("mode 5 not misread as 15105",
            cd.decode_bem_mode_response(misread), cd.MODE_CAN_DIRECT)

    # --- Negative cases: non-mode frames must return None, not raise ---------
    not_mode = cd.decode_bem_mode_response(datagram_on_wire(cd.encode_bem_get_mode()))
    t.equal("command frame is not a mode response", not_mode, None)
    bst95 = cd.BDTPDecoder().feed(cd.encode_bst95(128267, 3, 0, 255, bytes(8)))[0]
    t.equal("BST-95 frame is not a mode response", cd.decode_bem_mode_response(bst95), None)
    corrupt = bytearray(fake_mode_response(cd.MODE_CAN_DIRECT))
    corrupt[-1] ^= 0xFF
    t.equal("corrupt response rejected", cd.decode_bem_mode_response(bytes(corrupt)), None)
    err_rsp = fake_mode_response(cd.MODE_NORMAL, error=0x00000019)
    t.equal("error-code response rejected", cd.decode_bem_mode_response(err_rsp), None)

    print(f"\nSet-mode-5 wire: {hexstr(cd.encode_bem_set_mode(cd.MODE_CAN_DIRECT))}")
    t.finish()


if __name__ == "__main__":
    main()
