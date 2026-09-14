"""Import a user-provided Renesas compiler once, then build from its local files."""

from __future__ import annotations

import argparse
import hashlib
import io
import os
import re
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path

from . import installshield
from .common import ROOT, InputError, read_json, write_json

# The C phases, H8/300H normal-mode runtime packs, and headers used by this build.
REQUIRED_FILES = {
    "bin/ch38.exe",
    "bin/c38pep.exe",
    "bin/c38frnt.exe",
    "bin/c38mid.exe",
    "bin/c38cgn.exe",
    "bin/c38asm.exe",
    "bin/asm38.exe",
    "bin/optlnk.exe",
    "bin/lbg38.exe",
    "bin/lib3hn.pak",
    "bin/libinc.pak",
    "bin/libsrc.pak",
    "include/machine.h",
    "include/stddef.h",
    "include/limits.h",
}
LOCAL_TOOLCHAINS = ROOT / ".local/toolchains"


def validate_name(name: str) -> str:
    if (
        not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,63}", name)
        or name.endswith(".")
        or name.split(".")[0].casefold() in installshield.WINDOWS_DEVICES
    ):
        raise InputError(
            "Toolchain names must start with a letter or digit and contain only "
            "letters, digits, dots, hyphens or underscores (up to 64 characters)."
        )
    return name


def catalog_files(catalog: dict) -> dict[str, str]:
    return {name.casefold(): name for suite in catalog.values() for name in suite["files"]}


def walk_files(directory: Path):
    count = 0
    for parent, directories, files in os.walk(directory, followlinks=False):
        directories[:] = sorted(name for name in directories if not name.startswith("."))
        for name in sorted(files):
            count += 1
            if count > installshield.MAX_MEMBERS:
                raise InputError(
                    "Too many input files. Supply the compiler's bin/include directory."
                )
            yield Path(parent) / name


def installation_root(source: Path) -> Path:
    if source.is_file() and source.name.casefold() == "ch38.exe":
        source = source.parent
    if source.name.casefold() == "bin":
        return source.parent
    direct = [
        child for child in source.iterdir() if child.is_dir() and child.name.casefold() == "bin"
    ]
    if any(
        path.is_file() and path.name.casefold() == "ch38.exe"
        for child in direct
        for path in child.iterdir()
    ):
        return source
    roots = {
        path.parent.parent
        for path in walk_files(source)
        if path.name.casefold() == "ch38.exe" and path.parent.name.casefold() == "bin"
    }
    if not roots and direct:
        return source
    if len(roots) != 1:
        raise InputError(
            "Supply a single compiler suite directory, its bin directory, or ch38.exe."
        )
    return roots.pop()


def installation_contents(directory: Path, catalog: dict) -> dict[str, bytes]:
    canonical = catalog_files(catalog)
    contents = {}
    total = 0
    children = sorted(
        child
        for child in directory.iterdir()
        if child.is_dir() and child.name.casefold() in {"bin", "include"}
    )
    for child in children:
        for path in walk_files(child):
            relative = path.relative_to(directory).as_posix()
            key = relative.casefold()
            name = canonical.get(
                key, child.name.casefold() + "/" + path.relative_to(child).as_posix()
            )
            name = installshield.relative_path(name)
            if key in contents:
                raise InputError(f"Compiler input has conflicting filename spellings: {relative}.")
            size = path.stat().st_size
            total += size
            if size > installshield.MAX_FILE_BYTES or total > installshield.MAX_EXPANDED_BYTES:
                raise InputError("Compiler files exceed the import size limit.")
            data = path.read_bytes()
            if len(data) != size:
                raise InputError("Compiler input changed during import. Repeat the command.")
            contents[key] = (name, data)
    return dict(contents.values())


def inspect_contents(contents: dict[str, bytes], catalog: dict) -> dict:
    inventory = {name: hashlib.sha256(data).hexdigest() for name, data in sorted(contents.items())}
    folded = {name.casefold(): checksum for name, checksum in inventory.items()}
    missing = sorted(REQUIRED_FILES - folded.keys())
    if missing:
        raise InputError(
            "Compiler suite is incomplete; missing "
            + ", ".join(missing)
            + ". Import the full installation or updater bundle."
        )
    empty = [
        name for name, data in contents.items() if name.casefold() in REQUIRED_FILES and not data
    ]
    if empty:
        raise InputError("Compiler suite contains empty required files: " + ", ".join(empty) + ".")
    versions = [
        version
        for version, suite in catalog.items()
        if folded == {name.casefold(): checksum for name, checksum in suite["files"].items()}
    ]
    qualified = versions[0] if len(versions) == 1 else None
    compiler = next(data for name, data in contents.items() if name.casefold() == "bin/ch38.exe")
    banner = re.search(rb"C(?:/C\+\+)? Compiler V\.?([0-9]+\.[0-9]{2}\.[0-9]{2})", compiler)
    version = banner.group(1).decode("ascii") if banner else qualified
    warnings = []
    if qualified is None:
        detail = f"CH38 reports {version}; these files" if version else "These compiler files"
        warnings.append(
            f"{detail} differ from the qualified 6.02.01/6.02.02 suites. "
            "The build is allowed; output may differ from retail."
        )
    return {
        "files": inventory,
        "qualified_version": qualified,
        "compiler_version": version,
        "warnings": warnings,
    }


def describe(directory: Path, catalog: dict | None = None) -> dict:
    """Hash the files currently installed, including suites imported by older tools."""
    if not directory.is_dir():
        raise InputError(
            f"Compiler directory is missing: {directory}. Run uv run -m tools.toolchain import PATH."
        )
    if catalog is None:
        catalog = read_json(ROOT / "config/toolchains.json")
    return inspect_contents(installation_contents(directory, catalog), catalog)


def read_package(source: Path) -> bytes:
    if source.stat().st_size > installshield.MAX_PACKAGE_BYTES:
        raise InputError(
            "Compiler package exceeds the 512 MiB import limit. Import its installed directory."
        )
    data = source.read_bytes()
    if len(data) > installshield.MAX_PACKAGE_BYTES:
        raise InputError("Compiler package grew during import. Repeat the command.")
    return data


def zip_updater(data: bytes, catalog: dict) -> bytes:
    """Select the H8 updater explicitly; a ZIP may also contain other setup programs."""
    with zipfile.ZipFile(io.BytesIO(data)) as bundle:
        entries = bundle.infolist()
        if len(entries) > installshield.MAX_MEMBERS:
            raise InputError("Compiler ZIP contains too many files.")
        known = {
            suite["archive"]["member"].casefold()
            for suite in catalog.values()
            if "archive" in suite
        }
        candidates = [
            entry
            for entry in entries
            if not entry.is_dir()
            and (
                entry.filename.casefold() in known
                or re.fullmatch(
                    r"h8v[0-9]+[a-z]*\.exe",
                    Path(entry.filename.replace("\\", "/")).name,
                    flags=re.IGNORECASE,
                )
            )
        ]
        if len(candidates) != 1:
            raise InputError(
                "ZIP must contain one H8 updater executable (for example h8v6202u.exe). "
                "Extract the intended updater and import that file directly."
            )
        member = candidates[0]
        installshield.relative_path(member.filename)
        if member.flag_bits & 1 or member.file_size > installshield.MAX_PACKAGE_BYTES:
            raise InputError(
                "Compiler ZIP member is encrypted or exceeds the extraction size limit."
            )
        with bundle.open(member) as stream:
            data = stream.read(installshield.MAX_PACKAGE_BYTES + 1)
        if len(data) != member.file_size:
            raise InputError("Compiler ZIP member has the wrong size.")
        return data


def replace_installation(staged: Path, destination: Path, backup: Path) -> None:
    """Keep the previous import available until the replacement is in place."""
    try:
        if destination.exists():
            destination.replace(backup)
        staged.replace(destination)
    except BaseException:
        if backup.exists() and not destination.exists():
            backup.replace(destination)
        raise


def import_compiler(source: Path, catalog: dict, destination: Path, name: str | None = None) -> str:
    source = source.expanduser().resolve()
    if name is not None:
        validate_name(name)
    if not source.exists():
        raise InputError(f"Compiler input does not exist: {source}.")
    package_warnings = []
    if source.is_dir() or source.name.casefold() == "ch38.exe":
        contents = installation_contents(installation_root(source), catalog)
    else:
        package = read_package(source)
        checksum = hashlib.sha256(package).hexdigest()
        hashes = {suite.get("package_sha256") for suite in catalog.values()}
        hashes.update(
            suite["archive"]["sha256"] for suite in catalog.values() if "archive" in suite
        )
        if checksum not in hashes:
            package_warnings.append(
                "Compiler package hash is unfamiliar. Importing its files for qualification."
            )
        data = zip_updater(package, catalog) if zipfile.is_zipfile(io.BytesIO(package)) else package
        contents = installshield.compiler_members(data, catalog)
    description = inspect_contents(contents, catalog)
    version = description["qualified_version"] or description["compiler_version"]
    if name is None and version is None:
        raise InputError(
            "Compiler version could not be identified. Repeat the import with --name LABEL."
        )
    label = validate_name(name or version)
    destination.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".import-", dir=destination))
    backup = stage / "previous"
    target = destination / label
    try:
        suite = stage / "suite"
        for relative, data in contents.items():
            output = suite / relative
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(data)
        write_json(suite / "toolchain.json", {"schema": 1, **description})
        replace_installation(suite, target, backup)
    finally:
        if backup.exists() and not target.exists():
            print(
                f"Previous compiler files remain at {backup}; restore that directory to {target}.",
                file=sys.stderr,
            )
        else:
            shutil.rmtree(stage)
    for warning in package_warnings + description["warnings"]:
        print(f"Warning: {warning}", file=sys.stderr)
    return label


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    importer = commands.add_parser(
        "import", help="copy files from an installation, updater EXE or ZIP"
    )
    importer.add_argument("source", type=Path)
    importer.add_argument("--name", help="local name for this compiler suite")
    commands.add_parser("list", help="show imported compiler suites")
    args = parser.parse_args(argv)
    try:
        catalog = read_json(ROOT / "config/toolchains.json")
        if args.command == "import":
            label = import_compiler(args.source, catalog, LOCAL_TOOLCHAINS, args.name)
            print(f"Imported {label}. Configure with: uv run configure.py --toolchain {label}")
            return 0
        suites = sorted(
            path
            for path in LOCAL_TOOLCHAINS.glob("*")
            if path.is_dir() and not path.name.startswith(".")
        )
        if not suites:
            print("No compiler imported. Run: uv run -m tools.toolchain import PATH")
        for suite in suites:
            try:
                description = describe(suite, catalog)
                qualified = description["qualified_version"]
                state = f"qualified {qualified}" if qualified else "unqualified"
            except InputError as error:
                state = str(error)
            print(f"{suite.name}: {state}")
    except (InputError, OSError, ValueError, zipfile.BadZipFile, RuntimeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Compiler import interrupted. Repeat the command to try again.", file=sys.stderr)
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
