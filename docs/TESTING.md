# Testing and Validation

## Current automated coverage

The synthetic test constructs controlled PE32/x86 fixtures and verifies:

1. a clean unpacked executable is recognized and patched;
2. stale out-of-bounds Authenticode metadata is recognized, cleared in the patched copy, and preserved in the backup;
3. `.bind` causes a no-write refusal with a next-action report;
4. malformed Authenticode metadata causes a no-write refusal;
5. actual in-bounds certificate data causes a no-write refusal;
6. an unsupported identity causes a no-write refusal;
7. malformed PE input is reported without creating a backup or temporary file;
8. Large Address Aware and the `.gnvse` marker are present after patching;
9. a second patch attempt changes nothing;
10. restoration returns the original bytes exactly.

Run it with:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
python3 tests/test_synthetic_pe.py build/FNVGameNativePatcher
```

The same implementation has completed an offline patch, verification, and byte-for-byte restoration cycle against a real GameNative-unpacked Steam executable copy exhibiting the stale Authenticode state. This validates the structural handling of that file, but it does not establish Android runtime behavior or successful xNVSE initialization.

## Required real-device validation

A public release requires testing on a legitimate Steam installation using an actual Android GameNative environment.

Record the following without committing game files or private paths:

### Environment

- Android device model:
- Android version:
- SoC/GPU:
- GameNative version or commit:
- selected Proton version:
- selected graphics driver:
- Fallout: New Vegas storefront:
- Fallout: New Vegas displayed version:
- xNVSE version:
- patcher commit/version:

### Executable state

- SHA-256 before GameNative unpacking:
- SHA-256 after GameNative unpacking:
- SHA-256 after patching:
- `.bind` section before unpacking:
- `.bind` section after unpacking:
- Authenticode state after unpacking (`none`, `stale`, `real`, or `malformed`):
- LAA before patching:
- LAA after patching:
- `.gnvse` section after patching:

Hashes are diagnostic metadata. Do not upload the executables.

### Functional checks

- GameNative launches the normal `FalloutNV.exe`.
- The game reaches the main menu.
- A new game or known save loads.
- `nvse_steam_loader.log` is created.
- `nvse.log` reports successful initialization.
- `GetNVSEVersion` returns the expected version in the console.
- At least one known xNVSE plugin loads successfully.
- The game can exit and launch again.
- Running the patcher twice does not alter the executable again.
- `--restore` returns the original unpacked executable.
- The restored executable launches normally without the injected section.
- Repatching after restoration succeeds.

### Failure-path checks

Confirm clear refusal or recovery when:

- xNVSE files are missing;
- `FalloutNV.exe` is missing;
- the packed Steam `.bind` section is still present and no backup is created;
- stale out-of-bounds Authenticode metadata is repaired only in the patched copy;
- actual in-bounds Authenticode data is refused without file writes;
- malformed Authenticode metadata is refused without file writes;
- the target is not a PE file;
- the target is the wrong architecture;
- an existing backup differs from the current unpatched target;
- the patch operation is interrupted;
- the folder is read-only or storage permission is denied;
- the executable has already been patched.

## Address-space verification

The PE LAA flag must be independently inspected after patching. Runtime testing should also establish that the selected GameNative/Proton environment actually provides the intended larger 32-bit user address space.

Do not describe the runtime as verified solely because the PE flag is present.

## Antivirus and reproducibility

Before release:

- build from a tagged commit using GitHub Actions;
- publish SHA-256 hashes for release artifacts;
- scan the artifact with multiple engines;
- investigate detections rather than assuming they are false positives;
- compare a second build where practical;
- document the compiler and exact build command.

## Bug reports

Reports should include environment details, patcher output, hashes, and relevant xNVSE log text. They must never include game executables or third-party binaries.
