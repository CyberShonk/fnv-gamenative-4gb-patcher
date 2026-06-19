# Changelog

All notable changes to this project will be documented here.

The project is still pre-release. Version numbers describe development milestones and do not imply verified GameNative compatibility unless stated explicitly.


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

- Android runtime xNVSE initialization has not yet been validated with this revision.
- Steam is the only intended storefront for the first verified release.
- The command-line interface is still provisional.
- Executables using ASLR, actual in-bounds Authenticode certificate data, malformed security metadata, or unsupported layouts are intentionally rejected.
