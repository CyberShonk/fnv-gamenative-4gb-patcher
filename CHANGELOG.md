# Changelog

All notable changes to this project will be documented here.

The project is still pre-release. Version numbers describe development milestones and do not imply verified GameNative compatibility unless stated explicitly.

## 0.1.3-alpha

### Added

- Canonical Windows x86-64 GUI artifact `FNVGameNativePatcher.exe`, used as the primary GameNative build.
- Explicit retained x86 console artifact `FNVGameNativePatcher-x86.exe`.
- Verify, Patch, Restore, and Close controls in the x64 GUI.
- Executable-directory resolution through `GetModuleFileNameW()` on Windows.
- Persistent `FNVGameNativePatcher.log` diagnostics beside the patcher.
- PE machine-type and subsystem assertions.
- x86/x64 transformation-equivalence tests.
- Controlled tests for different launch working directories, missing required files, incompatible backups, and unwritable patcher directories.
- Shared patcher engine separated from the CLI and Windows GUI frontends.

### Changed

- The x64 GameNative artifact now uses the Windows GUI subsystem instead of the console subsystem.
- A no-argument x64 launch opens the GUI and performs read-only Verify instead of immediately patching.
- Explicit `--patch`, `--verify`, `--restore`, and `--help` arguments remain available for direct execution and automated validation.
- Release and CI packaging use unambiguous architecture names and verify x86 CUI versus x64 GUI subsystem identity.
- The documented GameNative workflow temporarily selects `FNVGameNativePatcher.exe` as the container Executable Path, then restores `FalloutNV.exe` after patching.

### Validated

- Native synthetic safety and unwritable-directory tests pass.
- Windows x86 and x64 runtime fixture suites pass.
- Windows x86 and x64 hosts produce identical PE32 transformations, backups, normalized verification results, repeat-run behavior, and exact restoration.
- The canonical `FNVGameNativePatcher.exe` x64 GUI filename launches in the foreground when selected as the Executable Path in GameNative 1.1.1 on an AYN Thor.
- The x64 GUI resolves the real Fallout New Vegas directory, verifies the managed executable pair, and writes the persistent log.
- Fallout New Vegas launches successfully after restoring GameNative's Executable Path to `FalloutNV.exe`.

### Known limitation

- Launching the x64 patcher from GameNative's Open Container file manager starts the process but does not foreground its window in the tested GameNative 1.1.1 environment. Use the temporary Executable Path workflow instead.

### Unchanged

- The patch remains a coordinated, fail-closed PE32 transformation of `FalloutNV.exe` and `FalloutNV.exe.unpacked.exe`.
- Backup compatibility checks, in-memory preflight, temporary-file verification, cache-first installation, rollback, verification-only mode, repeat-run behavior, and restoration remain in place.

## 0.1.2-alpha

### Added

- Managed support for GameNative's persistent `FalloutNV.exe.unpacked.exe` cache.
- Separate non-`.exe` backups and temporary files for both the normal launch executable and cached unpacked executable.
- Pair validation that refuses mismatched clean executable bytes before writing.
- `--verify` reporting for both executable copies and explicit persistent-cache coverage.
- Upgrade handling for the `0.1.1-alpha` state where only `FalloutNV.exe` was patched.
- Synthetic tests that simulate GameNative copying the cached executable back over `FalloutNV.exe`.

### Changed

- The default patch operation now requires GameNative to have completed Unpack Files once.
- The cached executable is installed first, followed by the normal launch executable, so the overwrite source is secured before the launch target.
- Patch preparation, backup validation, temporary-file verification, installation, and rollback now operate across the executable pair.
- `--restore` restores both managed backups when present.

### Validation status

- Synthetic pair patching, repeat safety, simulated cache overwrite, restoration, mismatched-pair refusal, missing-cache refusal, and `0.1.1-alpha` upgrade behavior pass locally.

## 0.1.1-alpha

### Added

- Executable-condition classification with recommended next actions in `--verify` and patch failures.
- Authenticode state classification for absent, stale out-of-bounds, real in-bounds, and malformed entries.
- Safe repair of the stale security-directory pointer left by the tested GameNative/Steamless unpacking output.
- No-write refusal tests for `.bind`, malformed metadata, real certificate data, unsupported identity, and malformed PE input.
- Offline patch, verification, and byte-for-byte restoration validation against a real GameNative-unpacked Steam executable copy.
- Initial public repository documentation.
- Technical explanation of the PE transformation and xNVSE loading path.
- Real-device testing requirements and issue-reporting guidance.
- Security guidance for executable modification and antivirus reports.

### Changed

- The complete patch is now constructed and verified in memory before any backup or temporary file is created.
- Already-patched targets now produce a full condition report and no file changes.
- Authenticode is no longer rejected solely because its directory fields are nonzero.

### Validated

- Confirmed on an AYN Thor using GameNative.
- Successfully patched a GameNative/Steamless-unpacked `FalloutNV.exe`.
- Confirmed that stale Authenticode metadata was repaired safely.
- Confirmed that the game launches successfully after patching.
- Confirmed that xNVSE logs recognize the executable as patched and xNVSE loads through the normal `FalloutNV.exe` launch path.

## 0.1.0-alpha

### Added

- PE32/x86 parser and structural validation.
- Detection of the still-packed Steam `.bind` section.
- Large Address Aware flag application.
- `.gnvse` executable section and early xNVSE loader shim.
- Discovery of the existing `LoadLibraryA` import.
- Non-`.exe` backup naming for GameNative compatibility.
- Repeat-safe patch detection.
- `--verify`, `--restore`, and `--help` commands.
- Synthetic PE patch, verification, repeat-run, and restoration test.
- Linux test build and static 32-bit Windows MinGW build workflows.

### Known limitations

- Steam is the only intended storefront for the first verified release.
- Executables using ASLR, actual in-bounds Authenticode certificate data, malformed security metadata, or unsupported layouts are intentionally rejected.
