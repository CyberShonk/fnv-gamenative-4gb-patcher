# Technical Overview

## Purpose

GameNative's Steam workflow uses Steamless to create `FalloutNV.exe.unpacked.exe`, then copies that file over the normal `FalloutNV.exe` launch target. A patch applied only to the normal filename can therefore be lost when GameNative uses its cached unpacked result again.

Version `0.1.2-alpha` treats the two files as one managed executable pair:

```text
FalloutNV.exe.unpacked.exe  -> GameNative cache and overwrite source
FalloutNV.exe               -> normal launch target
```

Both receive the same structural PE transformation. GameNative's `FalloutNV.exe.original.exe` safety copy is not modified.

## Launch chain

```text
GameNative launches FalloutNV.exe
        ↓
Windows/Wine begins at the patched PE entry point
        ↓
The .gnvse loader calls LoadLibraryA("nvse_steam_loader.dll")
        ↓
xNVSE initializes its early hooks
        ↓
The loader restores CPU state
        ↓
Execution jumps to the original FalloutNV.exe entry point
```

## Pair preflight

Before writing either executable, the patcher:

1. Requires both managed files to exist.
2. Reads and parses both complete files.
3. Classifies each executable condition.
4. Confirms their clean source bytes match when both clean references are available.
5. Constructs every required patched image in memory.
6. Verifies LAA, the `.gnvse` marker, and normalized Authenticode state.
7. Validates any existing backup against its corresponding clean target.

Any failure occurs before an executable is changed.

## Installation order and rollback

The write sequence is deliberately cache-first:

1. Create any missing non-`.exe` backups.
2. Write and read back both temporary patched files.
3. Replace `FalloutNV.exe.unpacked.exe`.
4. Replace `FalloutNV.exe`.
5. Verify both installed results.

Securing the cache first closes the known overwrite path before the normal launch target is replaced. If a later installation step fails, already installed files are restored from their backups. Temporary files are removed, and newly created backups are removed after a complete rollback.

## Backups

```text
FalloutNV.exe.unpacked.exe.gn4gb-backup
FalloutNV.exe.gn4gb-backup
```

Neither filename ends in `.exe`, reducing the chance that GameNative treats a backup as another executable to scan or unpack.

## Executable classification

The supported conditions are:

- unpacked and ready to patch;
- unpacked with the tested stale Authenticode metadata;
- still Steam-wrapped because `.bind` is present;
- already patched;
- actual in-file Authenticode certificate data remains;
- malformed Authenticode metadata;
- unsupported Fallout identity or PE layout.

Only the first two conditions are newly patchable. An already-patched member can be retained while the other member is upgraded, which supports migration from `0.1.1-alpha`.

## PE transformation

The implementation validates the DOS header, PE signature, COFF header, PE32 optional header, section table, imports, security directory, entry point, and alignment values.

It then:

- clears only the tested stale out-of-bounds Authenticode directory entry;
- finds the existing `LoadLibraryA` Import Address Table entry;
- appends an executable/readable `.gnvse` section;
- stores the loader, DLL name, patch marker, and original entry point;
- redirects the PE entry point to the loader;
- enables `IMAGE_FILE_LARGE_ADDRESS_AWARE` with bitwise OR;
- updates section count, `SizeOfCode`, and `SizeOfImage`;
- clears the obsolete PE checksum field.

## Loader behavior

```text
save flags
save general-purpose registers
push address of "nvse_steam_loader.dll"
call the existing LoadLibraryA import
restore registers
restore flags
jump to the original entry point
```

The loader uses the executable's existing resolved import rather than assuming a fixed Windows function address.

## Authenticode handling

The patcher distinguishes:

- **None** — offset and size are both zero.
- **Stale out-of-bounds metadata** — structurally valid fields reference removed certificate data beyond the file; this tested GameNative/Steamless state is cleared.
- **Real in-bounds certificate data** — refused.
- **Malformed metadata** — refused.

The exact original bytes remain in the corresponding backup.

## Repeat safety and persistence reporting

The `.gnvse` section contains the marker `FNVGN4GB-V1`. The patcher uses the section and marker together to identify its own work.

`--verify` reports both file states and four persistence facts:

- whether `FalloutNV.exe` is patched;
- whether the cached unpacked executable exists;
- whether the cache is patched;
- whether both files have persistent cache coverage.

## ASLR boundary

The current x86 payload uses addresses derived from the preferred image base. Files marked `DYNAMIC_BASE` are refused until a position-independent or relocation-aware implementation is justified and tested.

## Independence

The project is an independently written structural implementation. It does not include source expressions, patch arrays, fixed offsets, binaries, assets, or game files from the established FNV 4GB Patcher.
