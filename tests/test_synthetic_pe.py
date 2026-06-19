#!/usr/bin/env python3
import pathlib
import struct
import subprocess
import sys
import tempfile


def u16(buf, off, value):
    struct.pack_into('<H', buf, off, value)


def u32(buf, off, value):
    struct.pack_into('<I', buf, off, value)


def read_u32(buf, off):
    return struct.unpack_from('<I', buf, off)[0]


def make_fixture(
    path: pathlib.Path,
    *,
    second_section_name=b'.rdata',
    security_offset=0,
    security_size=0,
    include_identity=True,
):
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

    # Import directory index 1.
    u32(data, opt + 96 + 8, 0x2000)
    u32(data, opt + 96 + 12, 0x40)

    # Security directory index 4. Unlike most directories, this uses a file offset.
    security_entry = opt + 96 + 4 * 8
    u32(data, security_entry, security_offset)
    u32(data, security_entry + 4, security_size)

    section_table = opt + 0xE0
    # .text
    data[section_table:section_table+8] = b'.text\0\0\0'
    u32(data, section_table + 8, 0x100)
    u32(data, section_table + 12, 0x1000)
    u32(data, section_table + 16, 0x200)
    u32(data, section_table + 20, 0x400)
    u32(data, section_table + 36, 0x60000020)

    # Second file-backed section, optionally named .bind for the packed-state test.
    s2 = section_table + 40
    name = second_section_name[:8].ljust(8, b'\0')
    data[s2:s2+8] = name
    u32(data, s2 + 8, 0x300)
    u32(data, s2 + 12, 0x2000)
    u32(data, s2 + 16, 0x400)
    u32(data, s2 + 20, 0x600)
    u32(data, s2 + 36, 0x40000040)

    data[0x400] = 0xC3  # RET, never executed in the test.

    # Import descriptor at RVA 0x2000 / file 0x600.
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
    if include_identity:
        identity = b'FalloutNV Fallout: New Vegas'
        data[0x6C0:0x6C0+len(identity)] = identity

    if security_offset and security_size and security_offset + security_size <= len(data):
        # Minimal in-bounds WIN_CERTIFICATE-like data. The patcher only needs to
        # distinguish actual in-file data from a stale out-of-bounds pointer.
        u32(data, security_offset, security_size)
        u16(data, security_offset + 4, 0x0200)
        u16(data, security_offset + 6, 0x0002)
        data[security_offset + 8:security_offset + security_size] = b'X' * (security_size - 8)

    path.write_bytes(data)


def add_xnvse_files(directory: pathlib.Path):
    (directory / 'nvse_steam_loader.dll').write_bytes(b'test')
    (directory / 'nvse_1_4.dll').write_bytes(b'test')


def run(exe, cwd, *args, expected_returncode=0):
    result = subprocess.run([str(exe), *args], cwd=cwd, text=True, capture_output=True)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    if result.returncode != expected_returncode:
        raise RuntimeError(
            f'command returned {result.returncode}, expected {expected_returncode}: '
            f'{exe} {args}'
        )
    return result.stdout + result.stderr


def assert_no_generated_files(directory: pathlib.Path):
    assert not (directory / 'FalloutNV.exe.gn4gb-backup').exists()
    assert not (directory / 'FalloutNV.exe.gn4gb-temp').exists()
    assert not (directory / 'FalloutNV.exe.unpacked.exe.gn4gb-backup').exists()
    assert not (directory / 'FalloutNV.exe.unpacked.exe.gn4gb-temp').exists()


def add_executable_pair(directory: pathlib.Path, **fixture_kwargs):
    primary = directory / 'FalloutNV.exe'
    cache = directory / 'FalloutNV.exe.unpacked.exe'
    make_fixture(primary, **fixture_kwargs)
    make_fixture(cache, **fixture_kwargs)
    return primary, cache


def test_clean_patch_restore(exe: pathlib.Path, root: pathlib.Path):
    temp = root / 'clean'
    temp.mkdir()
    target, cache = add_executable_pair(temp)
    add_xnvse_files(temp)
    original = target.read_bytes()
    cache_original = cache.read_bytes()

    baseline = run(exe, temp, '--verify')
    assert baseline.count('Condition: unpacked and ready to patch') == 2
    assert 'Persistent cache coverage: no' in baseline

    patched_output = run(exe, temp)
    assert "Patched GameNative's Fallout: New Vegas executable pair successfully." in patched_output
    assert 'FalloutNV.exe.unpacked.exe: patched' in patched_output
    assert 'FalloutNV.exe: patched' in patched_output
    assert 'Persistent cache coverage: yes' in patched_output

    backup = temp / 'FalloutNV.exe.gn4gb-backup'
    cache_backup = temp / 'FalloutNV.exe.unpacked.exe.gn4gb-backup'
    assert backup.exists()
    assert cache_backup.exists()
    assert backup.read_bytes() == original
    assert cache_backup.read_bytes() == cache_original
    assert not backup.name.lower().endswith('.exe')
    assert not cache_backup.name.lower().endswith('.exe')

    report = run(exe, temp, '--verify')
    assert report.count('Condition: already patched') == 2
    assert 'Persistent cache coverage: yes' in report

    before_repeat = target.read_bytes()
    cache_before_repeat = cache.read_bytes()
    repeat = run(exe, temp)
    assert 'Both GameNative launch copies are already patched.' in repeat
    assert target.read_bytes() == before_repeat
    assert cache.read_bytes() == cache_before_repeat

    # Simulate GameNative copying its cached executable over the normal launch target.
    target.write_bytes(cache.read_bytes())
    copied_report = run(exe, temp, '--verify')
    assert copied_report.count('Condition: already patched') == 2
    assert 'Persistent cache coverage: yes' in copied_report

    restored = run(exe, temp, '--restore')
    assert 'Restored FalloutNV.exe.unpacked.exe' in restored
    assert 'Restored FalloutNV.exe' in restored
    assert target.read_bytes() == original
    assert cache.read_bytes() == cache_original

def test_stale_authenticode_repair(exe: pathlib.Path, root: pathlib.Path):
    temp = root / 'stale-authenticode'
    temp.mkdir()
    target, cache = add_executable_pair(
        temp,
        security_offset=0x2000,
        security_size=0x20,
    )
    add_xnvse_files(temp)
    original = target.read_bytes()
    cache_original = cache.read_bytes()

    baseline = run(exe, temp, '--verify')
    assert 'Condition: unpacked and patchable after stale Authenticode repair' in baseline
    assert 'Authenticode state: stale out-of-bounds metadata' in baseline
    assert 'Patchable: yes' in baseline

    patched_output = run(exe, temp)
    assert patched_output.count('Stale GameNative Authenticode metadata cleared') == 2
    assert (temp / 'FalloutNV.exe.gn4gb-backup').read_bytes() == original
    assert (temp / 'FalloutNV.exe.unpacked.exe.gn4gb-backup').read_bytes() == cache_original

    patched = target.read_bytes()
    opt = 0x80 + 4 + 20
    security_entry = opt + 96 + 4 * 8
    assert read_u32(patched, security_entry) == 0
    assert read_u32(patched, security_entry + 4) == 0

    report = run(exe, temp, '--verify')
    assert 'Condition: already patched' in report
    assert 'Authenticode state: none' in report

    run(exe, temp, '--restore')
    assert target.read_bytes() == original
    assert cache.read_bytes() == cache_original
    restored_report = run(exe, temp, '--verify')
    assert restored_report.count('Authenticode state: stale out-of-bounds metadata') == 2


def test_refusal_cases(exe: pathlib.Path, root: pathlib.Path):
    cases = [
        (
            'packed',
            {'second_section_name': b'.bind'},
            'still Steam-wrapped',
            "Run GameNative's Unpack Files operation again",
        ),
        (
            'malformed-authenticode',
            {'security_offset': 0, 'security_size': 0x20},
            'malformed Authenticode metadata',
            'Restore a clean copy of FalloutNV.exe',
        ),
        (
            'real-authenticode',
            {'security_offset': 0x900, 'security_size': 0x20},
            'actual Authenticode certificate data is still present',
            'Do not modify this file',
        ),
        (
            'wrong-identity',
            {'include_identity': False},
            'not recognized as Fallout: New Vegas',
            'Confirm that this is the Steam FalloutNV.exe',
        ),
    ]

    for name, kwargs, condition, action in cases:
        temp = root / name
        temp.mkdir()
        target, cache = add_executable_pair(temp, **kwargs)
        add_xnvse_files(temp)
        original = target.read_bytes()
        cache_original = cache.read_bytes()

        report = run(exe, temp, '--verify')
        assert f'Condition: {condition}' in report
        assert 'Patchable: no' in report
        assert action in report

        failure = run(exe, temp, expected_returncode=1)
        assert condition in failure
        assert 'No executable files were changed.' in failure
        assert 'Next action:' in failure
        assert target.read_bytes() == original
        assert cache.read_bytes() == cache_original
        assert_no_generated_files(temp)


def test_malformed_pe_report(exe: pathlib.Path, root: pathlib.Path):
    temp = root / 'malformed-pe'
    temp.mkdir()
    target = temp / 'FalloutNV.exe'
    cache = temp / 'FalloutNV.exe.unpacked.exe'
    target.write_bytes(b'not a PE file')
    make_fixture(cache)
    add_xnvse_files(temp)
    original = target.read_bytes()

    report = run(exe, temp, '--verify')
    assert 'Condition: malformed or unsupported PE data' in report
    assert 'Patchable: no' in report
    assert 'Files changed: no' in report

    failure = run(exe, temp, expected_returncode=1)
    assert 'malformed or unsupported PE data' in failure
    assert 'No executable files were changed.' in failure
    assert target.read_bytes() == original
    assert_no_generated_files(temp)




def test_mismatched_pair_refusal(exe: pathlib.Path, root: pathlib.Path):
    temp = root / 'mismatched-pair'
    temp.mkdir()
    target, cache = add_executable_pair(temp)
    cache_data = bytearray(cache.read_bytes())
    cache_data[0x700] ^= 0x01
    cache.write_bytes(cache_data)
    add_xnvse_files(temp)
    primary_original = target.read_bytes()
    cache_original = cache.read_bytes()

    failure = run(exe, temp, expected_returncode=1)
    assert 'do not originate from the same unpacked executable bytes' in failure
    assert 'Let GameNative recreate a matching executable pair' in failure
    assert target.read_bytes() == primary_original
    assert cache.read_bytes() == cache_original
    assert_no_generated_files(temp)


def test_missing_cache_refusal(exe: pathlib.Path, root: pathlib.Path):
    temp = root / 'missing-cache'
    temp.mkdir()
    target = temp / 'FalloutNV.exe'
    make_fixture(target)
    add_xnvse_files(temp)
    original = target.read_bytes()

    report = run(exe, temp, '--verify')
    assert 'FalloutNV.exe.unpacked.exe present: no' in report
    assert 'Persistent cache coverage: no' in report

    failure = run(exe, temp, expected_returncode=1)
    assert 'FalloutNV.exe.unpacked.exe was not found' in failure
    assert 'Make GameNative rerun Unpack Files' in failure
    assert 'Leave both executable names unchanged' in failure
    assert target.read_bytes() == original
    assert_no_generated_files(temp)


def test_upgrade_from_primary_only_patch(exe: pathlib.Path, root: pathlib.Path):
    temp = root / 'primary-only-upgrade'
    temp.mkdir()
    target, cache = add_executable_pair(temp)
    add_xnvse_files(temp)
    original = target.read_bytes()
    cache_original = cache.read_bytes()

    run(exe, temp)
    patched_primary = target.read_bytes()
    run(exe, temp, '--restore')

    # Recreate the 0.1.1-alpha state: only FalloutNV.exe is patched and only
    # its original backup exists.
    target.write_bytes(patched_primary)
    (temp / 'FalloutNV.exe.unpacked.exe.gn4gb-backup').unlink()

    output = run(exe, temp)
    assert 'FalloutNV.exe.unpacked.exe: patched' in output
    assert 'FalloutNV.exe: already patched' in output
    assert 'Persistent cache coverage: yes' in output
    assert (temp / 'FalloutNV.exe.gn4gb-backup').read_bytes() == original
    assert (temp / 'FalloutNV.exe.unpacked.exe.gn4gb-backup').read_bytes() == cache_original

    report = run(exe, temp, '--verify')
    assert report.count('Condition: already patched') == 2


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: test_synthetic_pe.py PATH_TO_PATCHER')
    exe = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as temp_name:
        root = pathlib.Path(temp_name)
        test_clean_patch_restore(exe, root)
        test_stale_authenticode_repair(exe, root)
        test_refusal_cases(exe, root)
        test_malformed_pe_report(exe, root)
        test_mismatched_pair_refusal(exe, root)
        test_missing_cache_refusal(exe, root)
        test_upgrade_from_primary_only_patch(exe, root)

    print('Synthetic PE architecture and safety tests passed.')


if __name__ == '__main__':
    main()
