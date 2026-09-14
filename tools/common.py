"""Shared input validation and file operations."""

from __future__ import annotations

import hashlib
import json
import os
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class InputError(Exception):
    """An input or environment problem that can be reported without a traceback."""


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def write_bytes(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
        temporary.replace(path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def write_json(path: Path, value) -> None:
    write_bytes(path, (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8"))


def validate_rom(path: Path, target: dict) -> bytes:
    if path.stat().st_size != target["size"]:
        raise InputError("Unsupported ROM; see docs/build.md for the retail image identity.")
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != target["sha256"]:
        raise InputError("Unsupported ROM; see docs/build.md for the retail image identity.")
    return data


def validate_toolchain(directory: Path, expected: dict) -> None:
    for name, checksum in expected.items():
        path = directory / name
        if not path.is_file() or digest(path) != checksum:
            raise InputError(
                f"Compiler input is missing or changed: {name}. Run configure.py again."
            )


def image_status(image: bytes, target: dict) -> dict:
    checksum = hashlib.sha256(image).hexdigest()
    return {
        "bytes": len(image),
        "sha256": checksum,
        "matches_retail": len(image) == target["size"] and checksum == target["sha256"],
    }


def windows_path(path: Path) -> str:
    absolute = str(path.resolve())
    return absolute if os.name == "nt" else "Z:" + absolute.replace("/", "\\")


def source_inventory() -> dict:
    paths = [
        ROOT / name
        for name in ("configure.py", "pyproject.toml", "uv.lock", ".python-version")
        if (ROOT / name).is_file()
    ]
    for name in ("src", "include", "config", "tools"):
        paths.extend(
            p
            for p in (ROOT / name).rglob("*")
            if p.is_file()
            and "__pycache__" not in p.parts
            and p.name != ".DS_Store"
            and p.suffix != ".md"
        )
    return {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(paths)}
