# Technical Overview

## Purpose

GameNative may unpack the Steam-protected `FalloutNV.exe` before launching it in its Android Windows-container environment. Unpacking changes the executable's bytes and therefore its hash. The established FNV 4GB Patcher recognizes specific known executable builds, so the changed file may be rejected even though it originated from a legitimate supported game installation.

This project uses a structural Portable Executable transformation instead of selecting a fixed patch solely from the executable hash.

## High-level transformation

The intended launch chain is:

```text
GameNative launches FalloutNV.exe
        ↓
Windows/Wine begins at the patched PE entry point
        ↓
The .gnvse loader calls LoadLibraryA("nvse_steam_loader.dll")
        ↓
xNVSE's loader initializes its early hooks
        ↓
The loader restores CPU state
        ↓
Execution jumps to the original FalloutNV.exe entry point
```

The original game code remains in place. The patcher adds an early loading step and then resumes the original startup path.

## PE structures examined

The prototype reads and validates:

- DOS `MZ` header;
- PE signature;
- COFF header;
- PE32 optional header;
- executable characteristics;
- section table;
- import directory;
- import lookup and address tables;
- security directory;
- original entry-point RVA;
- image, file, and section alignment values.

Fixed-width integer types are used because PE fields have defined binary widths.

## Executable classification and no-write preflight

Before creating a backup or temporary file, the patcher reads the complete target, parses its PE structures, and classifies its condition. The supported conditions are:

- unpacked and ready to patch;
- unpacked with the tested stale Authenticode metadata;
- still Steam-wrapped because `.bind` is present;
- already patched;
- actual in-file Authenticode certificate data remains;
- malformed Authenticode metadata;
- unsupported Fallout identity or PE layout.

Only the first two conditions are patchable. The complete transformed image is constructed and verified in memory before the backup is created. Packed, malformed, already-patched, and otherwise unsupported targets are not modified. `--verify` reports the condition and a recommended next action without attempting a patch.

## Authenticode handling

The PE security directory is unusual because its address is a file offset rather than an RVA. The tested GameNative/Steamless output removes the certificate data while leaving the original security-directory offset and size in the optional header. Those fields then point beyond the end of the unpacked file.

The patcher distinguishes four states:

- **None:** offset and size are both zero.
- **Stale out-of-bounds metadata:** both fields are structurally valid, but the referenced range is outside the file. This specific state is cleared before patching.
- **Real in-bounds certificate data:** the referenced range exists inside the file. The alpha refuses to modify it.
- **Malformed metadata:** only one field is zero, alignment is invalid, the size is too small, or the entry is otherwise inconsistent. The alpha refuses to modify it.

Clearing the stale state sets only the security-directory offset and size to zero. The original unpacked executable, including the stale fields, remains preserved in the backup for exact restoration.

## Large Address Aware

The patcher enables the `IMAGE_FILE_LARGE_ADDRESS_AWARE` characteristic using bitwise OR. This preserves every unrelated existing characteristic while ensuring the LAA bit is set.

The flag allows a compatible 32-bit process to use addresses above the traditional 2 GB boundary when the host environment provides the larger address space. It does not add physical memory or guarantee that the game will allocate 4 GB.

## Locating `LoadLibraryA`

The loader does not assume a fixed Windows function address. The patcher parses the executable's import directory and locates the existing `LoadLibraryA` entry in the Import Address Table.

At runtime, Windows or Wine resolves that IAT entry to the loaded implementation. The injected code calls through the resolved entry.

If the expected import cannot be found, the prototype refuses to patch the executable.

## The `.gnvse` section

The patcher appends a new executable and readable PE section named `.gnvse` when a safe unused section-header slot is available.

The section stores:

- a short x86 loader payload;
- the string `nvse_steam_loader.dll`;
- a patch marker;
- the original entry-point RVA for diagnostics and continuation.

The PE section count, entry point, `SizeOfCode`, and `SizeOfImage` are updated using the executable's own alignment values.

## Loader behavior

The loader payload performs the following conceptual operations:

```text
save flags
save general-purpose registers
push address of "nvse_steam_loader.dll"
call the existing LoadLibraryA import
restore registers
restore flags
jump to the original entry point
```

Preserving CPU state reduces interference with the original startup environment.

## Why ASLR is currently rejected

The current alpha payload uses absolute virtual addresses derived from the executable's preferred image base. An executable marked `DYNAMIC_BASE` may be relocated by ASLR, making those assumptions unsafe.

The alpha therefore fails closed when `DYNAMIC_BASE` is present. A later implementation could use fully position-independent code or relocation entries, but that should not be added without a concrete compatibility need and test coverage.

## Backup design

The clean GameNative-unpacked executable is stored as:

```text
FalloutNV.exe.gn4gb-backup
```

The backup deliberately does not end with `.exe`, reducing the chance that GameNative's executable-unpacking scan treats it as another launchable executable.

The entire transformed image is first constructed and verified in memory. Only then is the backup created, followed by a temporary file that is read back and verified before replacing the target. The installed result is verified again.

## Repeat safety

The `.gnvse` section includes the marker:

```text
FNVGN4GB-V1
```

The patcher uses the section and marker together to detect its own prior transformation. Re-running the patcher should report that the target is already patched rather than adding another section.

## Structural validation versus hashes

Hashes remain useful for diagnostics and identifying tested builds. They should not be the only validation mechanism because GameNative's unpacking is the reason the original known hash no longer applies.

A release build should combine:

- known tested hashes;
- PE structure validation;
- expected architecture and format;
- version-resource inspection where available;
- expected sections and imports;
- Fallout-specific identity checks;
- conservative failure for unknown layouts.

## Independence from the established patcher

The established FNV 4GB Patcher source was examined to understand the rejection and required outcome. This project uses an independently written structural implementation.

It does not include the original project's:

- source expressions;
- fixed patch arrays;
- hard-coded patch offsets;
- binaries;
- assets;
- redistributed game executable.

This should be described publicly as an independent implementation, not as a formal clean-room implementation.
