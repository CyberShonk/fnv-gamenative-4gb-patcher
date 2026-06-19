# FNV GameNative 4GB + xNVSE Patcher

A standalone compatibility patcher for a legitimate Steam copy of **Fallout: New Vegas** after GameNative has completed its executable-unpacking process.

> **Development status:** `0.1.2-alpha`. The previous `0.1.1-alpha` transformation and xNVSE launch path were validated on an AYN Thor. This revision adds persistent handling for GameNative's cached `FalloutNV.exe.unpacked.exe`; that paired-file workflow still requires real-device validation before public release.

## Why this project exists

GameNative uses Steamless to unpack the Steam executable. Current GameNative behavior leaves two relevant files:

- `FalloutNV.exe` — the normal launch target.
- `FalloutNV.exe.unpacked.exe` — GameNative's cached unpacked result, which it can copy over the normal executable.

Patching only `FalloutNV.exe` is therefore not persistent. A later GameNative copy can restore the unpatched cached bytes.

This patcher independently examines the unpacked PE files, enables Large Address Aware support, and adds an early loader for the user's separately installed `nvse_steam_loader.dll`. It does not include Fallout: New Vegas, xNVSE, Steamless, GameNative files, or code and binaries from the existing FNV 4GB Patcher.

## What 0.1.2-alpha does

- Requires both GameNative unpacked executable copies to be present.
- Validates that both are supported PE32/x86 Fallout: New Vegas executables.
- Refuses mismatched clean executable-pair bytes.
- Repairs the tested stale Authenticode pointer left by GameNative/Steamless.
- Enables `IMAGE_FILE_LARGE_ADDRESS_AWARE` without replacing unrelated flags.
- Adds a `.gnvse` section that loads `nvse_steam_loader.dll` before the original entry point.
- Patches `FalloutNV.exe.unpacked.exe` first, then `FalloutNV.exe`.
- Saves separate backups that do not end in `.exe`:
  - `FalloutNV.exe.unpacked.exe.gn4gb-backup`
  - `FalloutNV.exe.gn4gb-backup`
- Detects previous patching and upgrades the `0.1.1-alpha` state where only the normal executable was patched.
- Reports whether both launch copies have persistent cache coverage.
- Restores both managed executable copies when backups are available.

The patcher does **not** modify GameNative's `FalloutNV.exe.original.exe` safety copy.

## Intended user workflow

1. Install a legitimate Steam copy of Fallout: New Vegas through GameNative.
2. Ensure **Unpack Files** is enabled.
3. Launch the game once and allow GameNative's DRM handling to finish.
4. Close the game.
5. Install the current xNVSE release into the folder containing `FalloutNV.exe`.
6. Copy `FNVGameNativePatcher.exe` into that folder and run it once.
7. Launch the normal `FalloutNV.exe` entry through GameNative.
8. Confirm xNVSE initialized using `nvse_steam_loader.log` and `nvse.log`.

No executable renaming should be required. After both files are patched, the user should not need to disable **Unpack Files** for ordinary launches.

If `FalloutNV.exe.unpacked.exe` is missing, the patcher refuses to make a non-persistent change and explains how to let GameNative create the pair first.

## Commands

```text
FNVGameNativePatcher.exe
FNVGameNativePatcher.exe --verify
FNVGameNativePatcher.exe --restore
FNVGameNativePatcher.exe --help
```

## Current safety boundaries

The alpha build intentionally refuses to patch when:

- either managed executable is missing;
- the clean executable pair does not match byte for byte;
- a target is not PE32 x86;
- the Steam `.bind` wrapper is still present;
- `nvse_steam_loader.dll` or `nvse_1_4.dll` is missing;
- a target uses `DYNAMIC_BASE`/ASLR;
- actual Authenticode certificate data remains inside the file;
- Authenticode metadata is malformed;
- no safe empty PE section-header slot is available;
- `LoadLibraryA` cannot be located;
- expected Fallout: New Vegas identity strings are absent;
- a pre-existing backup conflicts with the current clean target.

All transformed bytes are constructed and verified in memory before backups or temporary executables are created. Both temporary files are verified before installation. The cache is installed first so GameNative's overwrite source is protected before the normal launch target is replaced.

See [Technical overview](docs/TECHNICAL_OVERVIEW.md) and [Testing](docs/TESTING.md).

## Building

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
python3 tests/test_synthetic_pe.py build/FNVGameNativePatcher
```

GitHub Actions cross-compiles a static 32-bit Windows executable using MinGW. Development artifacts are not public releases.

## Validation status

`0.1.1-alpha` was validated on an AYN Thor with a real GameNative-unpacked Steam executable: patching succeeded, the game launched, and xNVSE initialized through the normal `FalloutNV.exe` path.

The `0.1.2-alpha` synthetic test suite additionally covers:

- paired patching and exact restoration;
- simulated copying of `FalloutNV.exe.unpacked.exe` over `FalloutNV.exe`;
- missing-cache refusal;
- mismatched-pair refusal;
- repeat-run safety;
- stale Authenticode repair;
- upgrade from the prior primary-only patch state.

Real-device validation of the paired persistence workflow remains required.

## Independence and credits

This is an independent implementation based on the documented Microsoft PE format and xNVSE's loader architecture. The existing FNV 4GB Patcher source was inspected to understand the compatibility failure, but this repository does not include its source expressions, patch arrays, fixed offsets, binaries, or assets.

Credits:

- **Vault 13 Dweller** — reported the GameNative incompatibility that prompted the project.
- **Roy Batty and LuthienAnarion** — creators of the established FNV 4GB Patcher and direct-launch workflow.
- **The xNVSE team** — xNVSE and `nvse_steam_loader.dll`.
- **Utkarsh Dalal and GameNative contributors** — GameNative.
- **atom0s** — Steamless.
- **CyberShonk** — independent implementation and maintenance.

Crediting a person or project does not imply endorsement.

## Relationship to Droid Mod Loader

This patcher remains a standalone utility. The validated capability may later be integrated into a broader GameNative helper for Droid Mod Loader, with automatic detection, diagnostics, managed backups, restoration, and support for other verified 32-bit Bethesda games. Those are future plans, not current features.

## Legal and affiliation notice

This is an unofficial fan-made compatibility utility. It is not affiliated with or endorsed by Bethesda Softworks, Bethesda Game Studios, Obsidian Entertainment, Microsoft, Valve, the xNVSE team, GameNative, Steamless, or the authors of the existing FNV 4GB Patcher.

Users must provide their own legitimate game installation and install xNVSE separately. Do not upload game executables when reporting issues.

## License

Project source is available under the [MIT License](LICENSE). Third-party projects and game files remain under their respective licenses and terms.
