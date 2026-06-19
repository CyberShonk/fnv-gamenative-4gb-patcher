# FNV GameNative 4GB + xNVSE Patcher

A standalone compatibility patcher for a legitimate Steam copy of **Fallout: New Vegas** after GameNative has unpacked `FalloutNV.exe`.

> **Development status:** `0.1.1-alpha`. The structural transformation has been validated against synthetic fixtures and a real GameNative-unpacked Steam executable copy. Android runtime validation of xNVSE initialization is still required before a public release.

## Why this project exists

This project began after Discord member **Vault 13 Dweller** reported that the established FNV 4GB Patcher rejected their `FalloutNV.exe`.

Investigation showed that GameNative’s executable-unpacking process changes the Steam executable. That changes the executable’s hash, so a patcher designed around known desktop executable versions no longer recognizes it.

FNV GameNative 4GB + xNVSE Patcher is an independent implementation designed specifically for that workflow. It structurally examines the unpacked executable, enables Large Address Aware support, and prepares the normal `FalloutNV.exe` launch path to load the user’s separately installed xNVSE files.

This repository solves one focused compatibility problem. The tested capability is also intended for later integration into a GameNative helper for [Droid Mod Loader](https://github.com/CyberShonk/DroidModLoader) as part of a broader effort to make established Bethesda modding workflows practical in Android Windows containers.

The project does not include Fallout: New Vegas, xNVSE, Steamless, or code and binaries from the existing FNV 4GB Patcher.

## What the patcher is intended to do

- Validate that `FalloutNV.exe` is a supported 32-bit x86 Portable Executable.
- Classify the executable before writing any files and report the recommended next action.
- Refuse to patch the still-packed Steam executable, malformed metadata, or an unsafe layout.
- Repair the specific stale Authenticode pointer left by the tested GameNative/Steamless unpacking workflow.
- Enable `IMAGE_FILE_LARGE_ADDRESS_AWARE` without replacing unrelated header flags.
- Add a small `.gnvse` section that loads `nvse_steam_loader.dll` before the original game entry point.
- Preserve the filename `FalloutNV.exe` so GameNative can launch it normally.
- Save the original unpacked executable as `FalloutNV.exe.gn4gb-backup`.
- Detect previous patching so repeat runs do not append duplicate changes.
- Provide verification and restoration commands.

See [Technical overview](docs/TECHNICAL_OVERVIEW.md) for the implementation design.

## Intended user workflow

The following workflow is provisional until real-device testing is complete:

1. Install a legitimate Steam copy of Fallout: New Vegas through GameNative.
2. Allow GameNative to complete its executable-unpacking process.
3. Install the current xNVSE release into the folder containing `FalloutNV.exe`.
4. Copy `FNVGameNativePatcher.exe` into that folder.
5. Run the patcher once.
6. Launch the normal `FalloutNV.exe` entry through GameNative.
7. Confirm xNVSE initialized by checking `nvse_steam_loader.log` and `nvse.log`.

## Commands

```text
FNVGameNativePatcher.exe
FNVGameNativePatcher.exe --verify
FNVGameNativePatcher.exe --restore
FNVGameNativePatcher.exe --help
```

## Current safety boundaries

The alpha build intentionally refuses to patch when:

- the executable is not PE32 x86;
- the Steam `.bind` wrapper is still present;
- `nvse_steam_loader.dll` or `nvse_1_4.dll` is missing;
- the executable uses `DYNAMIC_BASE`/ASLR;
- actual Authenticode certificate data remains inside the file;
- the Authenticode security-directory entry is malformed;
- no safe empty PE section-header slot is available;
- the executable does not import `LoadLibraryA`;
- the executable lacks expected Fallout: New Vegas identity strings.

A nonzero Authenticode entry is accepted only when it is the tested stale form: both fields are structurally valid, but the referenced certificate range is outside the unpacked file because the certificate data was removed. The patcher clears that dead pointer before applying its own changes.

Unknown layouts should fail with a clear condition report and recommended next action rather than be patched speculatively. Classification and the full patch transformation occur in memory before the backup or temporary file is created.

## Building

### Native development build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
python3 tests/test_synthetic_pe.py build/FNVGameNativePatcher
```

### Windows artifact

GitHub Actions cross-compiles a static 32-bit Windows executable using MinGW. Build artifacts from development branches are test artifacts, not public releases.

## Testing status

The automated test currently covers:

- clean unpacked verification and patching;
- stale Authenticode detection, repair, and restoration;
- refusal without file writes for `.bind`, malformed metadata, actual certificate data, an unsupported identity, and malformed PE input;
- Large Address Aware and patch-marker verification;
- repeat-run safety;
- byte-for-byte restoration.

The implementation has also completed an offline patch, verification, and byte-for-byte restoration cycle against a real GameNative-unpacked Steam executable copy. This does **not** establish Android runtime or xNVSE compatibility. See [Testing](docs/TESTING.md) for the release validation matrix.

## Independence and credits

This project is an independent implementation based on the documented Microsoft PE format and xNVSE’s loader architecture. The existing FNV 4GB Patcher source was inspected to understand the compatibility failure, but this repository does not include its source expressions, patch arrays, fixed offsets, binaries, or assets.

Credits:

- **Vault 13 Dweller**: reported the GameNative incompatibility that prompted the project.
- **Roy Batty and LuthienAnarion**: creators of the established FNV 4GB Patcher and direct-launch workflow.
- **The xNVSE team**: xNVSE and `nvse_steam_loader.dll`.
- **Utkarsh Dalal and GameNative contributors**: GameNative.
- **atom0s**: Steamless.
- **CyberShonk**: independent implementation and maintenance.

Crediting a person or project does not imply endorsement.

## Relationship to Droid Mod Loader

This patcher is intended to remain useful as a standalone tool. After the GameNative workflow is verified, the capability may also be integrated into a future GameNative helper for Droid Mod Loader, potentially adding automatic detection, diagnostics, managed backups, restoration, and support for other verified 32-bit Bethesda games.

Those are future plans, not features of the current patcher.

## Legal and affiliation notice

This is an unofficial fan-made compatibility utility. It is not affiliated with or endorsed by Bethesda Softworks, Bethesda Game Studios, Obsidian Entertainment, Microsoft, Valve, the xNVSE team, GameNative, Steamless, or the authors of the existing FNV 4GB Patcher.

Users must provide their own legitimate game installation and install xNVSE separately. Do not upload game executables when reporting issues.

## License

Project source is available under the [MIT License](LICENSE). Third-party projects and game files remain under their respective licenses and terms.
