#!/usr/bin/env python3
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile


def u16(buf, off, value):
    struct.pack_into('<H', buf, off, value)


def u32(buf, off, value):
    struct.pack_into('<I', buf, off, value)


def make_fixture(path: pathlib.Path):
    data = bytearray(0xA00)
    data[0:2] = b'MZ'
    u32(data, 0x3C, 0x80)

    pe = 0x80
    data[pe:pe+4] = b'PE\0\0'
    coff = pe + 4
    u16(data, coff + 0, 0x014C)  # I386
    u16(data, coff + 2, 2)       # sections
    u16(data, coff + 16, 0xE0)   # optional header size
    u16(data, coff + 18, 0x0102) # executable + 32-bit

    opt = coff + 20
    u16(data, opt + 0, 0x010B)   # PE32
    u32(data, opt + 4, 0x200)    # SizeOfCode
    u32(data, opt + 8, 0x400)    # SizeOfInitializedData
    u32(data, opt + 16, 0x1000)  # entry point
    u32(data, opt + 20, 0x1000)  # base of code
    u32(data, opt + 24, 0x2000)  # base of data
    u32(data, opt + 28, 0x400000)
    u32(data, opt + 32, 0x1000)  # section alignment
    u32(data, opt + 36, 0x200)   # file alignment
    u16(data, opt + 40, 4)
    u16(data, opt + 48, 4)
    u32(data, opt + 56, 0x3000)  # size of image
    u32(data, opt + 60, 0x400)   # size of headers
    u16(data, opt + 68, 3)       # console subsystem
    u16(data, opt + 70, 0)       # no dynamic base
    u32(data, opt + 72, 0x100000)
    u32(data, opt + 76, 0x1000)
    u32(data, opt + 80, 0x100000)
    u32(data, opt + 84, 0x1000)
    u32(data, opt + 92, 16)
    # Import directory index 1
    u32(data, opt + 96 + 8, 0x2000)
    u32(data, opt + 96 + 12, 0x40)

    section_table = opt + 0xE0
    # .text
    data[section_table:section_table+8] = b'.text\0\0\0'
    u32(data, section_table + 8, 0x100)
    u32(data, section_table + 12, 0x1000)
    u32(data, section_table + 16, 0x200)
    u32(data, section_table + 20, 0x400)
    u32(data, section_table + 36, 0x60000020)
    # .rdata
    s2 = section_table + 40
    data[s2:s2+8] = b'.rdata\0\0'
    u32(data, s2 + 8, 0x300)
    u32(data, s2 + 12, 0x2000)
    u32(data, s2 + 16, 0x400)
    u32(data, s2 + 20, 0x600)
    u32(data, s2 + 36, 0x40000040)

    data[0x400] = 0xC3  # RET, never executed in the test

    # Import descriptor at RVA 0x2000 / file 0x600
    u32(data, 0x600 + 0, 0x2040)  # OriginalFirstThunk
    u32(data, 0x600 + 12, 0x2080) # Name
    u32(data, 0x600 + 16, 0x2050) # FirstThunk
    # Null descriptor follows.
    u32(data, 0x640, 0x2090)
    u32(data, 0x644, 0)
    u32(data, 0x650, 0x2090)
    u32(data, 0x654, 0)
    data[0x680:0x680+13] = b'KERNEL32.dll\0'
    data[0x690:0x692] = b'\0\0'
    data[0x692:0x692+13] = b'LoadLibraryA\0'
    data[0x6C0:0x6C0+24] = b'FalloutNV Fallout: New Vegas'
    path.write_bytes(data)


def run(exe, cwd, *args):
    result = subprocess.run([str(exe), *args], cwd=cwd, text=True, capture_output=True)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    if result.returncode != 0:
        raise RuntimeError(f'command failed: {exe} {args}')
    return result.stdout


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: test_synthetic_pe.py PATH_TO_PATCHER')
    exe = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as temp_name:
        temp = pathlib.Path(temp_name)
        make_fixture(temp / 'FalloutNV.exe')
        (temp / 'nvse_steam_loader.dll').write_bytes(b'test')
        (temp / 'nvse_1_4.dll').write_bytes(b'test')

        baseline = run(exe, temp, '--verify')
        assert 'Large Address Aware: no' in baseline
        assert 'GameNative xNVSE patch marker: no' in baseline

        patched = run(exe, temp)
        assert 'Patched FalloutNV.exe successfully.' in patched
        backup = temp / 'FalloutNV.exe.gn4gb-backup'
        assert backup.exists()
        assert not backup.name.lower().endswith('.exe')

        report = run(exe, temp, '--verify')
        assert 'Large Address Aware: yes' in report
        assert 'GameNative xNVSE patch marker: yes' in report

        repeat = run(exe, temp)
        assert 'already patched' in repeat

        restored = run(exe, temp, '--restore')
        assert 'Restored FalloutNV.exe' in restored
        final_report = run(exe, temp, '--verify')
        assert 'Large Address Aware: no' in final_report
        assert 'GameNative xNVSE patch marker: no' in final_report

    print('Synthetic PE patch/verify/restore test passed.')


if __name__ == '__main__':
    main()
