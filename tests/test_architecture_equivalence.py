#!/usr/bin/env python3
import pathlib
import shutil
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from test_synthetic_pe import add_executable_pair, add_xnvse_files, run_from  # noqa: E402


def normalized(output: str, game: pathlib.Path, exe: pathlib.Path) -> str:
    ignored = (
        "Patcher architecture:",
        "Executable path:",
        "Selected target directory:",
        "Persistent log:",
    )
    normalized_output = output.replace(str(game), "<GAME_DIR>").replace(str(exe), "<PATCHER>")
    normalized_output = normalized_output.replace(str(game).replace("\\", "/"), "<GAME_DIR>")
    normalized_output = normalized_output.replace(str(exe).replace("\\", "/"), "<PATCHER>")
    return "\n".join(
        line for line in normalized_output.splitlines()
        if not line.strip().startswith(ignored)
    )


def exercise(exe: pathlib.Path, root: pathlib.Path):
    root.mkdir(parents=True, exist_ok=True)
    game = root / exe.stem
    game.mkdir()
    add_executable_pair(game)
    add_xnvse_files(game)

    original_primary = (game / "FalloutNV.exe").read_bytes()
    original_cache = (game / "FalloutNV.exe.unpacked.exe").read_bytes()

    baseline = run_from(exe, game, root, "--verify")
    patched_output = run_from(exe, game, root, "--patch")
    patched_primary = (game / "FalloutNV.exe").read_bytes()
    patched_cache = (game / "FalloutNV.exe.unpacked.exe").read_bytes()
    primary_backup = (game / "FalloutNV.exe.gn4gb-backup").read_bytes()
    cache_backup = (game / "FalloutNV.exe.unpacked.exe.gn4gb-backup").read_bytes()
    verify_output = run_from(exe, game, root, "--verify")

    before_repeat = patched_primary, patched_cache
    repeat_output = run_from(exe, game, root, "--patch")
    after_repeat = (
        (game / "FalloutNV.exe").read_bytes(),
        (game / "FalloutNV.exe.unpacked.exe").read_bytes(),
    )
    if before_repeat != after_repeat:
        raise AssertionError(f"{exe.name}: repeat run changed patched files")

    restore_output = run_from(exe, game, root, "--restore")
    restored = (
        (game / "FalloutNV.exe").read_bytes(),
        (game / "FalloutNV.exe.unpacked.exe").read_bytes(),
    )
    if restored != (original_primary, original_cache):
        raise AssertionError(f"{exe.name}: restore did not return exact original bytes")

    if not (b".gnvse" in patched_primary and b".gnvse" in patched_cache):
        raise AssertionError(f"{exe.name}: expected .gnvse marker is missing")
    if primary_backup != original_primary or cache_backup != original_cache:
        raise AssertionError(f"{exe.name}: backups are not exact originals")

    return {
        "patched_primary": patched_primary,
        "patched_cache": patched_cache,
        "primary_backup": primary_backup,
        "cache_backup": cache_backup,
        "baseline": normalized(baseline, game, exe),
        "patch": normalized(patched_output, game, exe),
        "verify": normalized(verify_output, game, exe),
        "repeat": normalized(repeat_output, game, exe),
        "restore": normalized(restore_output, game, exe),
    }


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_architecture_equivalence.py X86_EXE X64_EXE")
    x86 = pathlib.Path(sys.argv[1]).resolve()
    x64 = pathlib.Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory() as temp_name:
        root = pathlib.Path(temp_name)
        left = exercise(x86, root / "x86")
        right = exercise(x64, root / "x64")
        for key in left:
            if left[key] != right[key]:
                raise AssertionError(f"x86/x64 mismatch for {key}")
    print("Windows x86 and x64 hosts produced identical PE32 transformations and verification results.")


if __name__ == "__main__":
    main()
