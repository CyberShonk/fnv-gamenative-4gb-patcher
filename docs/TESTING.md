# Testing and Validation

## Current automated coverage

The synthetic PE test verifies:

1. both clean GameNative executable copies are recognized and patched;
2. the cache is installed before the normal launch target;
3. separate non-`.exe` backups preserve each original exactly;
4. stale out-of-bounds Authenticode metadata is repaired in both patched copies;
5. `.bind`, malformed Authenticode data, real certificate data, unsupported identity, and malformed PE input fail without executable writes;
6. mismatched clean pair bytes are refused;
7. a missing `FalloutNV.exe.unpacked.exe` cache is refused with a GameNative-specific next action;
8. LAA and the `.gnvse` marker are present in both files;
9. a second patch attempt changes nothing;
10. copying the patched cache over `FalloutNV.exe` leaves a valid patched launch target;
11. `--restore` returns both original byte sequences exactly;
12. a `0.1.1-alpha` primary-only installation upgrades by patching only the missing cache member.

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
python3 tests/test_synthetic_pe.py build/FNVGameNativePatcher
```

## Established 0.1.1-alpha validation

The prior primary-only revision was validated on an AYN Thor using a real GameNative-unpacked Steam executable. The game launched through the normal `FalloutNV.exe` entry and xNVSE initialized successfully.

That result validates the PE transformation and xNVSE loader path. It does not by itself validate the new paired-cache persistence behavior.

## Required 0.1.2-alpha real-device validation

Record:

### Environment

- Android device model and version
- SoC/GPU
- GameNative version or commit
- Proton version
- graphics driver
- Fallout: New Vegas storefront and displayed version
- xNVSE version
- patcher commit/version

### Pair state

Record SHA-256 values without uploading executable files:

- `FalloutNV.exe` before patching
- `FalloutNV.exe.unpacked.exe` before patching
- both files after patching
- both backup files
- both files after restoration

Also record:

- `.bind` state
- Authenticode state
- LAA state
- `.gnvse` marker state
- whether the two clean executable files match
- whether the two patched executable files match

### Functional checks

- Let GameNative complete Unpack Files once.
- Confirm both managed executable files exist.
- Run the patcher once without renaming files.
- Confirm `--verify` reports persistent cache coverage.
- Launch the normal `FalloutNV.exe` entry.
- Reach the main menu and load a save or new game.
- Confirm `nvse_steam_loader.log` and `nvse.log` are created.
- Confirm `GetNVSEVersion` returns the expected value.
- Confirm at least one known xNVSE plugin loads.
- Exit and relaunch several times.
- Confirm neither managed executable loses the marker or LAA flag.
- Trigger a GameNative state that copies the cached executable over the primary, where safely reproducible, and confirm the resulting primary remains patched.
- Run the patcher twice and confirm the second run changes nothing.
- Test upgrade from an existing `0.1.1-alpha` primary-only installation.
- Restore both files, confirm byte-for-byte recovery, launch, then repatch.

### Failure-path checks

- cache missing;
- xNVSE files missing;
- normal executable missing;
- mismatched pair bytes;
- packed `.bind` state;
- real or malformed Authenticode data;
- wrong architecture;
- conflicting backup;
- read-only folder or denied storage permission;
- interrupted replacement of the second file after cache installation;
- patched file with a missing backup during restoration.

## Address-space verification

Inspect the PE LAA flag and separately confirm that the selected GameNative/Proton environment provides the intended larger 32-bit user address space. The flag alone is not runtime proof.

## Antivirus and reproducibility

Before release:

- build from a tagged commit with GitHub Actions;
- publish SHA-256 hashes;
- scan the Windows artifact with multiple engines;
- investigate detections;
- record compiler and build commands;
- compare a second build where practical.

## Bug reports

Reports should include environment details, patcher output, hashes, and relevant xNVSE log excerpts. They must never include game executables or third-party binaries.
