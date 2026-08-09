#!/usr/bin/env python3
import pathlib
import struct
import sys

EXPECTED = {
    "x86": (0x014C, 0x010B, "PE32 / Intel 80386"),
    "x64": (0x8664, 0x020B, "PE32+ / AMD64"),
}


def inspect(path: pathlib.Path):
    data = path.read_bytes()
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise SystemExit(f"{path}: missing DOS header")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 26 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise SystemExit(f"{path}: missing PE signature")
    machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
    optional_magic = struct.unpack_from("<H", data, pe_offset + 24)[0]
    return machine, optional_magic


def main():
    if len(sys.argv) != 3 or sys.argv[2] not in EXPECTED:
        raise SystemExit("usage: inspect_pe_machine.py PATH (x86|x64)")
    path = pathlib.Path(sys.argv[1])
    machine, magic = inspect(path)
    expected_machine, expected_magic, description = EXPECTED[sys.argv[2]]
    if (machine, magic) != (expected_machine, expected_magic):
        raise SystemExit(
            f"{path}: expected machine=0x{expected_machine:04x}, magic=0x{expected_magic:04x}; "
            f"found machine=0x{machine:04x}, magic=0x{magic:04x}"
        )
    print(f"{path}: {description} confirmed")


if __name__ == "__main__":
    main()
