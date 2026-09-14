"""Check source formatting, target-aware static analysis, and utility tests."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

from .common import ROOT, InputError


def executable(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise InputError(f"{name} is not on PATH; see docs/build.md for developer checks.")
    return path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--format", action="store_true", help="apply formatting and exit")
    parser.add_argument("--clang-format", default="clang-format", help="formatter executable")
    args = parser.parse_args()
    try:
        os.chdir(ROOT)
        ruff = executable("ruff")
        if args.format:
            subprocess.run([ruff, "check", "--fix", "."], check=True)
            subprocess.run([ruff, "format", "."], check=True)
        else:
            subprocess.run([ruff, "check", "."], check=True)
            subprocess.run([ruff, "format", "--check", "."], check=True)
        formatter = executable(args.clang_format)
        banner = subprocess.check_output([formatter, "--version"], text=True)
        if "version 21.1.8" not in banner:
            raise InputError("Use clang-format 21.1.8 for the project's formatting profile.")
        os.chdir(ROOT)
        files = [
            str(p)
            for folder in ("src", "include")
            for p in sorted(Path(folder).rglob("*"))
            if p.is_file() and p.suffix in {".c", ".h"} and p != Path("include/startup/iodefine.h")
        ]
        options = ["-i"] if args.format else ["--dry-run", "--Werror"]
        subprocess.run([formatter, *options, *files], check=True)
        if args.format:
            return 0
        subprocess.run(
            [
                executable("cppcheck"),
                "--std=c89",
                "--platform=config/h8.xml",
                "--enable=warning,performance,portability",
                "--error-exitcode=1",
                "--suppressions-list=config/lint-suppressions.txt",
                "--template=gcc",
                "--quiet",
                "-D__HITACHI__",
                "-Iinclude",
                "src",
            ],
            check=True,
        )
        subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests"], check=True)
        print("Formatting, static analysis, and utility tests passed.")
    except (InputError, OSError, subprocess.SubprocessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
