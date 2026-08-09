# Testing and Validation

## Automated coverage

The synthetic PE suite verifies:

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
12. a `0.1.1-alpha` primary-only installation upgrades by patching only the missing cache member;
13. launching from a different working directory still targets the patcher's own directory;
14. missing required files and incompatible backups fail safely;
15. an unwritable patcher directory is refused without changing either managed executable.

Run the native suite with:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
python3 tests/test_synthetic_pe.py build/FNVGameNativePatcher
python3 tests/test_unwritable_directory.py build/FNVGameNativePatcher
```

## Windows architecture and subsystem validation

CI builds and retains:

- `FNVGameNativePatcher-x86.exe` — machine `0x014c`, PE32 / Intel 80386, Windows CUI.
- `FNVGameNativePatcher.exe` — canonical GameNative build; machine `0x8664`, PE32+ / AMD64, Windows GUI.

The build verifies both machine type and PE subsystem. Each executable runs the complete synthetic PE32 suite. A separate equivalence test executes both hosts against independently generated copies of the same fixtures and compares exit behavior, patched primary/cache bytes, backups, normalized verification output, repeat-run stability, and exact restoration.

The x64 GUI build accepts explicit command arguments so the Windows runtime suite can exercise patch, verify, restore, and refusal behavior without UI automation.

## Current validation status

The `0.1.3-alpha` development candidate has passed:

- native synthetic safety tests;
- unwritable-directory refusal;
- Windows x86 runtime fixtures;
- Windows x64 GUI runtime fixtures;
- exact x86/x64 PE32 transformation and restoration equivalence;
- x86 CUI and x64 GUI subsystem assertions.

### Real GameNative device validation

Validated on an AYN Thor with GameNative 1.1.1:

- the canonical `FNVGameNativePatcher.exe` x64 patcher launches in the foreground when selected as the GameNative container Executable Path;
- the GUI resolves the real Fallout New Vegas directory from its own executable path;
- a no-argument launch performs read-only Verify;
- the managed `FalloutNV.exe` and `FalloutNV.exe.unpacked.exe` pair is recognized as already patched with LAA and the GameNative xNVSE marker present;
- the required xNVSE DLLs are detected;
- persistent cache coverage is reported;
- `FNVGameNativePatcher.log` is written beside the patcher;
- after restoring the GameNative Executable Path to `FalloutNV.exe`, Fallout New Vegas launches successfully.

### Open Container limitation

In the tested GameNative 1.1.1 environment, launching the x64 patcher from Open Container starts the patcher process but does not foreground its window. The supported device workflow uses the temporary Executable Path method instead.

## Regression checks for release candidates

Before publishing a release candidate:

- rerun native and Windows automated suites from the release commit;
- confirm x86/x64 architecture equivalence;
- confirm x86 CUI and x64 GUI subsystem identity;
- confirm no target bytes change for controlled refusal cases;
- confirm the x64 GameNative GUI still launches through the configured Executable Path;
- confirm Verify remains read-only;
- restore the GameNative Executable Path to `FalloutNV.exe` and confirm the game launches normally;
- record SHA-256 values for distributed patcher artifacts.

Private game executables, backups, and third-party binaries must not be committed or distributed as test artifacts.

## Address-space verification

Inspect the PE LAA flag and separately confirm that the selected GameNative/Proton environment provides the intended larger 32-bit user address space. The flag alone is not runtime proof.

## Antivirus and reproducibility

Before release:

- build from a tagged commit with GitHub Actions;
- publish SHA-256 hashes;
- scan the Windows artifacts with multiple engines;
- investigate detections;
- record compiler and build commands;
- compare a second build where practical.

## Bug reports

Reports should include environment details, patcher output, hashes, and relevant xNVSE log excerpts. They must never include game executables or third-party binaries.
