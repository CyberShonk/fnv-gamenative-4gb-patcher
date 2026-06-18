# Changelog

All notable changes to this project will be documented here.

The project is still pre-release. Version numbers describe development milestones and do not imply verified GameNative compatibility unless stated explicitly.

## Unreleased

### Added

- Initial public repository documentation.
- Technical explanation of the PE transformation and xNVSE loading path.
- Real-device testing requirements and issue-reporting guidance.
- Security guidance for executable modification and antivirus reports.

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

- A real GameNative-unpacked Steam executable has not yet been validated.
- Steam is the only intended storefront for the first verified release.
- The command-line interface is still provisional.
- Executables using ASLR, Authenticode security data, or unsupported layouts are intentionally rejected.
