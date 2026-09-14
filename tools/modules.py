"""Resolve firmware modules and their alphabetical object order."""

from __future__ import annotations

import re
from pathlib import PurePosixPath

from .common import InputError

ARTWORK_MODULE = "pw_builtin"
RESERVED_NAMES = {"artwork", "runtime", "link", "build", "status", "verified"}


def resolve(config: dict, source_files) -> dict:
    """Derive module identities from the captured source inventory."""
    paths = [
        PurePosixPath(name)
        for name in source_files
        if name.startswith("src/") and PurePosixPath(name).suffix.lower() == ".c"
    ]
    if not paths:
        raise InputError("No C source files found under src/.")
    overrides = config.get("source_flags", {})
    modules = {}
    for path in sorted(paths, key=lambda path: path.name):
        if not re.fullmatch(r"[a-z][a-z0-9_]*\.c", path.name):
            raise InputError(f"Use a lowercase C module basename with letters, digits or _: {path}")
        if path.stem in RESERVED_NAMES:
            raise InputError(f"Module name is reserved for a build artifact: {path.stem}")
        if path.stem in modules:
            other = modules[path.stem]["file"]
            raise InputError(f"Duplicate module basename: src/{other} and {path}")
        modules[path.stem] = {
            "file": path.relative_to("src").as_posix(),
            "flags": overrides.get(path.name, []),
        }
    missing = set(overrides) - {path.name for path in paths}
    if missing:
        raise InputError("Source flags refer to missing modules: " + ", ".join(sorted(missing)))
    return config | {"modules": modules}
