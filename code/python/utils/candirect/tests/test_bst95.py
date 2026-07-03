#!/usr/bin/env python3
"""Round-trip and spec-vector tests for the BST-95 (CAN Direct binary) codec."""

from _harness import TestRunner, hexstr
import candirect as cd


def roundtrip(pgn, priority, source, dest, data, direction=cd.DIR_TX):
    """Encode -> on-the-wire BDTP -> decode, returning the decoded dict."""
    wire = cd.encode_bst95(pgn, priority, source, dest, bytes(data), direction=direction)
    frames = cd.BDTPDecoder().feed(wire)
    assert len(frames) == 1, f"expected 1 frame, got {len(frames)}"
    return cd.decode_bst95(frames[0])


def main() -> None:
    t = TestRunner("BST-95 CAN Direct codec")

    # --- Round-trip: PDU2 broadcast (depth default packet) -------------------
    depth_data = bytes([0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    d = roundtrip(128267, 3, 0, 255, depth_data)
    t.equal("depth PGN round-trips", d["pgn"], 128267)
    t.equal("depth priority round-trips", d["priority"], 3)
    t.equal("depth source round-trips", d["source"], 0)
    t.equal("depth dest is broadcast (PDU2)", d["destination"], 255)
    t.equal("depth data round-trips", d["data"], depth_data)
    t.equal("send direction flag is T", d["direction"], "T")

    # --- Round-trip: PDU1 addressed (dest carried in PDUS) -------------------
    # PGN 126208 (0x1ED00) is PDU1 (PDUF 0xED = 237 < 240).
    d = roundtrip(126208, 6, 22, 35, bytes([0x01, 0x02, 0x03]))
    t.equal("PDU1 PGN round-trips", d["pgn"], 126208)
    t.equal("PDU1 destination preserved", d["destination"], 35)
    t.equal("PDU1 source preserved", d["source"], 22)
    t.equal("PDU1 length is 3", d["length"], 3)

    # --- DLE stuffing: a 0x10 data byte must survive the wire ----------------
    stuffed = bytes([0x10, 0x10, 0xAA, 0x10])
    wire = cd.encode_bst95(129026, 2, 48, 255, stuffed)
    t.check("wire contains stuffed DLE pair", bytes([cd.DLE, cd.DLE]) in wire)
    d = cd.decode_bst95(cd.BDTPDecoder().feed(wire)[0])
    t.equal("DLE-stuffed data round-trips", d["data"], stuffed)

    # --- Fixed decode vector from BST-95-can-frame.md ------------------------
    # Spec example datagram (16 bytes, before the BDTP checksum is appended):
    spec = bytes.fromhex("95 0E 01 20 30 02 F8 09 FF FC 37 0A 00 10 FF FF".replace(" ", ""))
    frame = spec + bytes([cd.bst_checksum(spec)])
    t.equal("spec checksum byte", frame[-1], 0xBF)
    d = cd.decode_bst95(frame)
    t.equal("spec example PGN", d["pgn"], 129026)
    t.equal("spec example priority", d["priority"], 2)
    t.equal("spec example source", d["source"], 48)
    t.equal("spec example data page reflected in PGN", d["pgn"] >> 16, 1)
    t.equal("spec example receive direction", d["direction"], "R")

    # --- Store-length field matches the spec formula (L = 6 + n) -------------
    datagram = cd.encode_bst95_datagram(128267, 3, 0, 255, depth_data)
    t.equal("store length is 6 + data (14 for 8 bytes)", datagram[1], 14)

    # --- Structural guards ---------------------------------------------------
    bad = cd.encode_bst95(128267, 3, 0, 255, depth_data)
    corrupt = bytearray(cd.BDTPDecoder().feed(bad)[0])
    corrupt[-1] ^= 0xFF  # break the checksum
    try:
        cd.decode_bst95(bytes(corrupt))
        t.check("corrupt checksum rejected", False)
    except ValueError:
        t.check("corrupt checksum rejected", True)

    print(f"\nExample wire frame: {hexstr(wire)}")
    t.finish()


if __name__ == "__main__":
    main()
