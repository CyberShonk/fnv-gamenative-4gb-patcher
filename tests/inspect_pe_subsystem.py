#!/usr/bin/env python3
import pathlib
import struct
import sys

SUBSYSTEMS = {
    "gui": 2,
    "console": 3,
}


def read_u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def read_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def main() -> None:
    if len(sys.argv) != 3 or sys.argv[2] not in SUBSYSTEMS:
        raise SystemExit("usage: inspect_pe_subsystem.py EXE gui|console")

    path = pathlib.Path(sys.argv[1])
    data = path.read_bytes()

    if len(data) < 0x40 or data[:2] != b"MZ":
        raise SystemExit(f"{path}: not a DOS/PE executable")

    pe_offset = read_u32(data, 0x3C)
    if pe_offset + 24 + 70 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise SystemExit(f"{path}: invalid PE header")

    optional = pe_offset + 24
    magic = read_u16(data, optional)
    if magic not in (0x10B, 0x20B):
        raise SystemExit(f"{path}: unsupported optional-header magic 0x{magic:04x}")

    subsystem = read_u16(data, optional + 68)
    expected = SUBSYSTEMS[sys.argv[2]]
    if subsystem != expected:
        raise SystemExit(
            f"{path}: subsystem={subsystem}, expected {expected} ({sys.argv[2]})"
        )

    print(f"{path}: Windows {sys.argv[2]} subsystem confirmed")


if __name__ == "__main__":
    main()
