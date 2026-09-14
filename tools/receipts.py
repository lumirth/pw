"""Bind generated artifacts to their source, producer and output identities."""

from __future__ import annotations

import hashlib
from pathlib import Path

from . import assets, modules
from .common import InputError, digest, read_json

SCHEMA = 3


def outputs(action: str, module: str | None = None) -> list[str]:
    if action == "artwork":
        return ["rom_assets.h"]
    if action == "runtime":
        return ["runtime.lib"]
    if action == "compile":
        return [f"{module}.obj"]
    return ["pw.bin", "pw.map", "link.sub"]


def stem(action: str, module: str | None = None) -> str:
    return f"{module}" if action == "compile" else action


def compile_options(config: dict, module: str) -> list[str]:
    includes = [*config["include_dirs"], "compiler/include"]
    return [
        *config["compiler_flags"],
        "-include=" + ",".join(includes),
        *config["modules"][module]["flags"],
    ]


def link_commands(config: dict) -> list[str]:
    objects = [f"{name}.obj" for name in config["modules"]]
    return [
        *(f"input out\\{name}" for name in objects),
        "library out\\runtime.lib",
        "form binary",
        *config["linker_options"],
        "output out\\pw.bin=0-BFFF",
        "space FF",
        "show symbol",
        "list out\\pw.map",
        "exit",
        "",
    ]


def recipe(action: str, config: dict, module: str | None = None) -> dict:
    if action == "artwork":
        return {"operation": "Convert resident artwork files to compiler arrays"}
    if action == "runtime":
        return {
            "tool": "lbg38.exe",
            "arguments": ["-output=out\\runtime.lib", "-head=Runtime", "-cpu=300HN"],
        }
    if action == "compile":
        return {
            "tool": "ch38.exe",
            "arguments": [
                *compile_options(config, module),
                f"-object=out\\{module}.obj",
                ("src/" + config["modules"][module]["file"]).replace("/", "\\"),
            ],
        }
    return {
        "tool": "optlnk.exe",
        "arguments": ["-subcommand=out\\link.sub"],
        "subcommand": link_commands(config),
    }


def link_inputs(config: dict) -> list[str]:
    names = ["rom_assets.h", "artwork.json", "runtime.lib", "runtime.json"]
    for module in config["modules"]:
        names.extend([f"{module}.obj", f"{module}.json"])
    return names


def dependencies(
    root: Path,
    output: Path,
    source: dict,
    settings_hash: str,
    action: str,
    config: dict,
    module: str | None = None,
) -> dict:
    # Each object's dependencies include shared build code, configuration,
    # its source file, and every staged header.
    result = {
        name: value for name, value in source.items() if not name.startswith(("src/", "include/"))
    }
    result[".local/config.json"] = settings_hash
    if action == "artwork":
        manifest = read_json(root / "config/artwork.json")
        paths = assets.resolved_inputs(root, manifest).values()
        result.update((p.relative_to(root).as_posix(), digest(p)) for p in paths)
    elif action == "compile":
        name = "src/" + config["modules"][module]["file"]
        result[name] = source[name]
        result.update(
            (name, value) for name, value in source.items() if name.startswith("include/")
        )
        if module == modules.ARTWORK_MODULE:
            name = (output / "rom_assets.h").relative_to(root).as_posix()
            result[name] = digest(root / name)
    elif action == "link":
        result.update(source)
        for name in link_inputs(config):
            path = output / name
            result[path.relative_to(root).as_posix()] = digest(path)
    return result


def identity(
    version: str,
    action: str,
    inputs: dict,
    config: dict,
    inventory: dict,
    launcher: dict,
    module: str | None = None,
) -> dict:
    return {
        "schema": SCHEMA,
        "toolchain": version,
        "action": action,
        "inputs": inputs,
        "recipe": recipe(action, config, module),
        "tool_files": inventory,
        "launcher": launcher,
    }


def validate(record: dict, expected: dict, directory: Path, names: list[str], label: str) -> None:
    for key, value in expected.items():
        if record.get(key) != value:
            raise InputError(
                f"Stale {label} receipt ({key} changed). Run uv run configure.py, "
                "then uv run ninja -t clean and uv run ninja."
            )
    found = record.get("outputs", {})
    if set(found) != set(names):
        raise InputError(f"Incomplete {label} receipt. Rebuild with Ninja.")
    for name in names:
        path = directory / name
        if not path.is_file() or digest(path) != found[name]:
            raise InputError(f"{label} output changed: {name}. Rebuild with Ninja.")


def copy_checked(source: Path, target: Path, expected: str) -> None:
    # Validate the bytes that will reach the producer, catching source changes
    # since the earlier inventory check. The producer reads the staged copy.
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise InputError(f"Input changed while staging: {source}. Repeat the build.")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)


def validate_prerequisites(
    root: Path,
    output: Path,
    source: dict,
    settings_hash: str,
    version: str,
    config: dict,
    inventory: dict,
    launcher: dict,
) -> list[dict]:
    calls = []
    steps = [("artwork", None), ("runtime", None)]
    steps.extend(("compile", name) for name in config["modules"])
    for action, module in steps:
        label = stem(action, module)
        path = output / f"{label}.json"
        if not path.is_file():
            raise InputError(f"Missing {label} receipt. Rebuild with Ninja.")
        record = read_json(path)
        inputs = dependencies(root, output, source, settings_hash, action, config, module)
        expected = identity(version, action, inputs, config, inventory, launcher, module)
        validate(record, expected, output, outputs(action, module), label)
        if action != "artwork":
            calls.append(record)
    return calls
