"""Select a native Windows launcher or a cached, overridable Wibo executable."""

from __future__ import annotations

import os
import platform
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

from .common import ROOT, InputError, digest, read_json


def download(url: str, expected: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as stream:
            temporary = Path(stream.name)
            with urllib.request.urlopen(url, timeout=60) as response:
                shutil.copyfileobj(response, stream)
        if digest(temporary) != expected:
            raise InputError("Downloaded Wibo checksum differs from config/host-tools.json.")
        temporary.chmod(0o755)
        temporary.replace(destination)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def inspect(path: Path) -> dict:
    try:
        result = subprocess.run(
            [str(path), "--version"],
            check=True,
            capture_output=True,
            timeout=10,
            text=True,
            errors="replace",
        )
    except (OSError, subprocess.SubprocessError) as error:
        raise InputError(f"Cannot run Wibo at {path}: {error}. See docs/build.md.") from error
    return {
        "name": "wibo",
        "path": str(path),
        "version": result.stdout.strip(),
        "sha256": digest(path),
    }


def select(explicit: str | None, offline: bool) -> dict:
    if os.name == "nt":
        return {"name": "native"}
    if explicit:
        path = Path(explicit).expanduser()
        if not path.is_file():
            found = shutil.which(explicit)
            if found is None:
                raise InputError(f"Wibo executable not found: {explicit}")
            path = Path(found)
        return inspect(path.resolve())
    manifest = read_json(ROOT / "config/host-tools.json")["wibo"]
    asset = manifest.get(sys.platform)
    if asset is None:
        raise InputError("No managed Wibo build for this host; supply --wibo PATH.")
    path = ROOT / ".local/tools" / f"wibo-{manifest['version']}-{sys.platform}"
    if not path.is_file() or digest(path) != asset["sha256"]:
        if offline:
            raise InputError(
                "Managed Wibo is unavailable. Run configure.py online or supply --wibo PATH."
            )
        print(f"Downloading Wibo {manifest['version']}...", flush=True)
        download(asset["url"], asset["sha256"], path)
    return inspect(path)


def command(launcher: dict) -> list[str]:
    if launcher["name"] == "native":
        if os.name != "nt":
            raise InputError("Build configuration belongs to another host; rerun configure.py.")
        return []
    path = Path(launcher["path"])
    if digest(path) != launcher["sha256"]:
        raise InputError("Wibo changed after configuration; rerun configure.py.")
    return [str(path)]


def qualified_host() -> bool:
    machine = platform.machine().casefold()
    return (os.name == "nt" and machine in {"amd64", "x86_64"}) or (
        sys.platform == "darwin" and machine == "arm64"
    )
