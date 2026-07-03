#!/usr/bin/env python3
"""Round-trip and spec-vector tests for the CAN ASCII (CAN Direct ASCII) codec."""

from _harness import TestRunner
import candirect as cd


def roundtrip(pgn, priority, source, dest, data, direction="T"):
    line = cd.encode_canascii(pgn, priority, source, dest, bytes(data),
                              direction=direction)
    assert line.endswith("\r\n"), "line must be CRLF terminated"
    return cd.decode_canascii(line)


def main() -> None:
    t = TestRunner("CAN ASCII CAN Direct codec")

    # --- Round-trip: PDU2 broadcast (depth default packet) -------------------
    depth_data = bytes([0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    d = roundtrip(128267, 3, 0, 255, depth_data)
    t.equal("depth PGN round-trips", d["pgn"], 128267)
    t.equal("depth priority round-trips", d["priority"], 3)
    t.equal("depth source round-trips", d["source"], 0)
    t.equal("depth dest is broadcast (PDU2)", d["destination"], 255)
    t.equal("depth data round-trips", d["data"], depth_data)
    t.equal("send direction is T", d["direction"], "T")

    # --- Round-trip: PDU1 addressed -----------------------------------------
    d = roundtrip(126208, 6, 22, 35, bytes([0xAA, 0xBB]))
    t.equal("PDU1 PGN round-trips", d["pgn"], 126208)
    t.equal("PDU1 destination preserved", d["destination"], 35)
    t.equal("PDU1 source preserved", d["source"], 22)
    t.equal("PDU1 data round-trips", d["data"], bytes([0xAA, 0xBB]))

    # --- Fixed decode vector from can-frame-ascii-A.md -----------------------
    # Example line: 17:33:21.107 R 19F51323 01 2F 30 70 00 2F 30 70
    line = "17:33:21.107 R 19F51323 01 2F 30 70 00 2F 30 70\r\n"
    d = cd.decode_canascii(line)
    t.equal("spec line PGN", d["pgn"], 128275)   # 0x1F513
    t.equal("spec line priority", d["priority"], 6)
    t.equal("spec line source", d["source"], 35)  # 0x23
    t.equal("spec line destination broadcast", d["destination"], 255)
    t.equal("spec line direction R", d["direction"], "R")
    t.equal("spec line data length", d["length"], 8)
    t.equal("spec line timestamp preserved", d["timestamp"], "17:33:21.107")

    # --- 29-bit identifier composition is exact ------------------------------
    can_id = cd.encode_can_id(128275, 6, 35, 255)
    t.equal("29-bit id matches spec example", can_id, 0x19F51323)
    line = cd.encode_canascii(128275, 6, 35, 255, bytes([0x01, 0x2F, 0x30, 0x70]))
    t.check("encoded line carries the id", f"{can_id:08X}" in line)

    # --- Robustness: a non-CAN-ASCII line is rejected, not crashed -----------
    for junk in ("", "garbage", "12:00:00 X 1F801 00", "not hex here"):
        try:
            cd.decode_canascii(junk)
            t.check(f"junk line rejected: {junk!r}", False)
        except ValueError:
            t.check(f"junk line rejected: {junk!r}", True)

    t.finish()


if __name__ == "__main__":
    main()
