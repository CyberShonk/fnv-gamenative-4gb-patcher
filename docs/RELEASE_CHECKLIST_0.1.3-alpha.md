# v0.1.3-alpha x86/x64 Release Checklist

## Repository state

- [ ] Record `git branch --show-current` and `git rev-parse HEAD` after the release commit is created.
- [ ] Confirm the working tree contains only reviewed release changes.
- [ ] Confirm no game executable, backup, private path, generated log, or Python cache file is tracked.
- [ ] Confirm `git diff --check` passes before commit.

## Build

```bash
mkdir -p dist

i686-w64-mingw32-g++ \
  -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  -static -static-libgcc -static-libstdc++ \
  src/main.cpp src/patcher.cpp \
  -o dist/FNVGameNativePatcher-x86.exe

x86_64-w64-mingw32-g++ \
  -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  -static -static-libgcc -static-libstdc++ \
  -mwindows \
  src/windows_gui.cpp src/patcher.cpp \
  -o dist/FNVGameNativePatcher.exe
```

## PE identity proof

```bash
file dist/FNVGameNativePatcher-x86.exe dist/FNVGameNativePatcher.exe
python3 tests/inspect_pe_machine.py dist/FNVGameNativePatcher-x86.exe x86
python3 tests/inspect_pe_machine.py dist/FNVGameNativePatcher.exe x64
python3 tests/inspect_pe_subsystem.py dist/FNVGameNativePatcher-x86.exe console
python3 tests/inspect_pe_subsystem.py dist/FNVGameNativePatcher.exe gui
```

Expected:

- x86: PE32 / Intel 80386 / Windows CUI.
- x64: PE32+ / AMD64 / Windows GUI.

## Automated tests

- [x] Native synthetic PE safety suite passes on the development candidate.
- [x] Unwritable-directory refusal passes on the development candidate.
- [x] Windows x86 runtime fixture suite passes on the development candidate.
- [x] Windows x64 GUI runtime fixture suite passes on the development candidate.
- [x] x86/x64 transformation-equivalence test passes on the development candidate.
- [ ] GitHub Actions passes from the reviewed release commit.

## Controlled failures

The development candidate has exercised controlled refusal for:

- [x] missing primary executable;
- [x] missing cached executable;
- [x] missing xNVSE files;
- [x] unsupported or malformed executable;
- [x] mismatched executable pair;
- [x] incompatible pre-existing backup;
- [x] unwritable patcher directory;
- [x] already-patched input remains repeat-safe.

## GameNative device acceptance

Validated on an AYN Thor with GameNative 1.1.1:

- [x] Copy `FNVGameNativePatcher.exe` beside the FNV executable pair and xNVSE files.
- [x] Temporarily set the container **Executable Path** to `FNVGameNativePatcher.exe`.
- [x] Confirm the canonical `FNVGameNativePatcher.exe` x64 GUI launches in the foreground.
- [x] Confirm the target directory is resolved from the patcher's executable location.
- [x] Confirm a no-argument launch performs read-only Verify.
- [x] Confirm `FNVGameNativePatcher.log` is written beside the patcher.
- [x] Confirm both managed executables report LAA and the GameNative xNVSE patch marker when already patched.
- [x] Confirm persistent cache coverage is reported.
- [x] Restore the GameNative **Executable Path** to `FalloutNV.exe`.
- [x] Confirm Fallout New Vegas launches successfully.

Known limitation:

- GameNative 1.1.1 Open Container starts the x64 patcher process but does not foreground its window in the tested environment. This release uses the temporary Executable Path workflow rather than executable-name impersonation.

## Publication

- [ ] Review the complete staged diff.
- [ ] Build release artifacts from the reviewed commit.
- [ ] Record and publish SHA-256 hashes for the distributed patcher executables.
- [ ] Perform antivirus/reputation checks on release artifacts.
- [ ] Confirm release notes match implemented and validated behavior.
- [ ] Tag only after the release commit and CI result are approved.
