#!/usr/bin/env python3
import os
import pathlib
import shutil
import stat
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from test_synthetic_pe import add_executable_pair, add_xnvse_files  # noqa: E402


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_unwritable_directory.py PATH_TO_NATIVE_PATCHER")
    source_exe = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as temp_name:
        root = pathlib.Path(temp_name)
        game = root / "readonly-game"
        outside = root / "outside"
        game.mkdir()
        outside.mkdir()
        add_executable_pair(game)
        add_xnvse_files(game)
        local_exe = game / source_exe.name
        shutil.copy2(source_exe, local_exe)
        local_exe.chmod(local_exe.stat().st_mode | stat.S_IXUSR)
        primary_before = (game / "FalloutNV.exe").read_bytes()
        cache_before = (game / "FalloutNV.exe.unpacked.exe").read_bytes()

        preexec_fn = None
        if hasattr(os, "geteuid") and os.geteuid() == 0:
            import pwd
            nobody = pwd.getpwnam("nobody")
            root.chmod(0o755)
            outside.chmod(0o777)
            preexec_fn = lambda: (os.setgid(nobody.pw_gid), os.setuid(nobody.pw_uid))

        game.chmod(0o555)
        try:
            result = subprocess.run(
                [str(local_exe)],
                cwd=outside,
                text=True,
                capture_output=True,
                preexec_fn=preexec_fn,
            )
        finally:
            game.chmod(0o755)
        output = result.stdout + result.stderr
        if result.returncode == 0:
            raise AssertionError("unwritable-directory run unexpectedly succeeded")
        if (game / "FalloutNV.exe").read_bytes() != primary_before:
            raise AssertionError("primary executable changed in unwritable-directory refusal")
        if (game / "FalloutNV.exe.unpacked.exe").read_bytes() != cache_before:
            raise AssertionError("cache executable changed in unwritable-directory refusal")
        if "ERROR:" not in output or "Final exit status: 1" not in output:
            raise AssertionError(f"controlled refusal was not reported:\n{output}")
    print("Unwritable patcher directory is refused without changing either executable.")


if __name__ == "__main__":
    main()
