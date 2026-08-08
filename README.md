# FNV GameNative 4GB + xNVSE Patcher

A standalone compatibility patcher for a legitimate Steam copy of **Fallout: New Vegas** after GameNative has completed its executable-unpacking process.

> **Development status:** `0.1.3-alpha`. This revision adds a native Windows x86-64 GUI build for GameNative, retains the x86 console build, resolves targets relative to the patcher executable, and writes persistent diagnostics beside the patcher. The target transformation remains PE32-specific.

## Why this project exists

GameNative uses Steamless to unpack the Steam executable. Current GameNative behavior leaves two relevant files:

- `FalloutNV.exe` — the normal launch target.
- `FalloutNV.exe.unpacked.exe` — GameNative's cached unpacked result, which it can copy over the normal executable.

Patching only `FalloutNV.exe` is therefore not persistent. A later GameNative copy can restore the unpatched cached bytes.

This patcher independently examines the unpacked PE files, enables Large Address Aware support, and adds an early loader for the user's separately installed `nvse_steam_loader.dll`. It does not include Fallout: New Vegas, xNVSE, Steamless, GameNative files, or code and binaries from the existing FNV 4GB Patcher.

## What 0.1.3-alpha does

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

## Windows builds

Release packages contain two static executables:

- `FNVGameNativePatcher.exe` — the primary GameNative build. This is a PE32+ x86-64 **Windows GUI** application.
- `FNVGameNativePatcher-x86.exe` — retained for conventional 32-bit Windows and Wine environments. This remains a PE32 x86 **console** application.

The patcher process architecture is independent of the target executable architecture. Both builds parse and modify the same PE32/x86 `FalloutNV.exe` and `FalloutNV.exe.unpacked.exe` bytes using fixed-width fields; the x64 patcher does not convert the game executables to PE32+.

Both frontends share the same patching engine. The patcher resolves the target folder from its own module path with `GetModuleFileNameW()`, so a different launch working directory does not redirect the operation.

Every invocation appends diagnostics to `FNVGameNativePatcher.log` beside the patcher. The log records version and architecture, executable and target paths, validation and PE-state output, backup and patch decisions, verification output, controlled refusals, exceptions, and final exit status. It does not contain executable contents.

## GameNative workflow

1. Install a legitimate Steam copy of Fallout: New Vegas through GameNative.
2. Enable **Unpack Files**, launch once, and allow GameNative's DRM handling to finish.
3. Close the game.
4. Install the current xNVSE release into the folder containing `FalloutNV.exe`.
5. Copy `FNVGameNativePatcher.exe` into that same folder.
6. In the GameNative container settings, temporarily change **Executable Path** from `FalloutNV.exe` to `FNVGameNativePatcher.exe`.
7. Launch the container. The patcher GUI opens and performs a read-only Verify on startup.
8. Use **Patch** if verification reports that both managed executables are ready. Patch and Restore require confirmation.
9. Close the patcher and restore **Executable Path** to `FalloutNV.exe`.
10. Launch Fallout: New Vegas normally through GameNative.

If `FalloutNV.exe.unpacked.exe` is missing, the patcher refuses to make a non-persistent change and explains how to let GameNative create the pair first.

### Open Container limitation

Launching the x64 patcher from GameNative's **Open Container** file manager starts the process, but GameNative 1.1.1 does not foreground the patcher window in the tested environment. Use the temporary **Executable Path** method above. The patcher does not impersonate `FalloutNV.exe` or `wfm.exe` to work around this behavior.

## Commands

The x64 GUI opens when launched without arguments and automatically runs read-only Verify. Explicit arguments execute directly and are also used by automated tests:

```text
FNVGameNativePatcher.exe --verify
FNVGameNativePatcher.exe --patch
FNVGameNativePatcher.exe --restore
FNVGameNativePatcher.exe --help
```

The x86 console build retains its command-line behavior. With no argument, the x86 build performs the patch operation.

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

Native development tests:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
python3 tests/test_synthetic_pe.py build/FNVGameNativePatcher
python3 tests/test_unwritable_directory.py build/FNVGameNativePatcher
```

Static Windows builds:

```bash
i686-w64-mingw32-g++ \
  -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  -static -static-libgcc -static-libstdc++ \
  src/main.cpp src/patcher.cpp \
  -o FNVGameNativePatcher-x86.exe

x86_64-w64-mingw32-g++ \
  -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  -static -static-libgcc -static-libstdc++ \
  -mwindows \
  src/windows_gui.cpp src/patcher.cpp \
  -o FNVGameNativePatcher.exe
```

GitHub Actions builds both files, checks PE machine type and subsystem, runs the synthetic PE32 corpus through both Windows hosts, and compares patched bytes, backups, verification output, repeat-run behavior, and restoration results.

## Validation status

`0.1.3-alpha` has passed:

- native synthetic safety tests and unwritable-directory refusal;
- Windows x86 and x64 runtime fixture suites;
- exact x86/x64 transformation and restoration equivalence;
- PE architecture and subsystem checks;
- real-device launch of the canonical `FNVGameNativePatcher.exe` x64 GUI through GameNative 1.1.1 on an AYN Thor;
- read-only verification against the real managed executable pair;
- persistent log creation beside the patcher;
- successful Fallout New Vegas launch after restoring GameNative's Executable Path to `FalloutNV.exe`.

The tested Open Container foreground limitation is described above.

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
