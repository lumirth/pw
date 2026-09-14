"""Convert assets, run the H8 tools, and report the resulting image identity."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from . import assets, host, modules, receipts, toolchain
from .common import (
    ROOT,
    InputError,
    digest,
    image_status,
    read_json,
    source_inventory,
    validate_toolchain,
    windows_path,
    write_bytes,
    write_json,
)


def invoke(
    stage: Path, tool: str, arguments: list[str], cwd: Path, launcher: dict, output: Path, stem: str
) -> dict:
    temp = stage / "tmp"
    temp.mkdir()
    env = dict(os.environ)
    env["PATH"] = str(stage / "compiler/bin") + os.pathsep + env.get("PATH", "")
    # A trailing separator is required by this driver's include-path parser.
    env["CH38"] = windows_path(stage / "compiler/include") + ";"
    env["CH38TMP"] = (
        os.path.relpath(temp, cwd).replace("/", "\\") if tool == "ch38.exe" else windows_path(temp)
    )
    for name in ("TMP", "TEMP", "TMPDIR"):
        env[name] = str(temp)
    command = [*host.command(launcher), str(stage / "compiler/bin" / tool), *arguments]
    start = time.monotonic()
    result = subprocess.run(
        command, cwd=cwd, env=env, capture_output=True, timeout=600, text=True, errors="replace"
    )
    text = result.stdout + result.stderr
    write_bytes(output / f"{stem}.log", text.encode("utf-8"))
    call = {
        "tool": tool,
        "arguments": arguments,
        "exit_code": result.returncode,
        "seconds": round(time.monotonic() - start, 3),
        "log": f"{stem}.log",
    }
    if result.returncode or re.search(r"\b[ACFL]\d{4}\s*\((?:E|F)\)", text):
        raise InputError(f"{tool} failed; see {output / (stem + '.log')}\n{text[-4000:]}")
    return call


def source_json(name: str, source: dict) -> dict:
    data = (ROOT / name).read_bytes()
    if hashlib.sha256(data).hexdigest() != source[name]:
        raise InputError(f"Build input changed while reading: {name}. Repeat the build.")
    return json.loads(data)


def load_state(version: str) -> dict:
    source = source_inventory()
    data = (ROOT / ".local/config.json").read_bytes()
    settings = json.loads(data)
    if "tool_files" not in settings:
        raise InputError("Build configuration needs refreshing. Run uv run configure.py.")
    if settings["toolchain"] != version:
        raise InputError("Build graph and selected compiler disagree; rerun configure.py.")
    config = modules.resolve(source_json("config/build.json", source), source)
    if any(name not in {"include", "generated"} for name in config["include_dirs"]):
        raise InputError(
            "Compiler include directories must use the staged include/ or generated/ tree."
        )
    return {
        "source": source,
        "settings": settings,
        "settings_hash": hashlib.sha256(data).hexdigest(),
        "config": config,
        "inventory": settings["tool_files"],
    }


def expected(state: dict, version: str, action: str, module: str | None = None) -> dict:
    output = ROOT / "build" / version
    inputs = receipts.dependencies(
        ROOT, output, state["source"], state["settings_hash"], action, state["config"], module
    )
    return receipts.identity(
        version,
        action,
        inputs,
        state["config"],
        state["inventory"],
        state["settings"]["launcher"],
        module,
    )


def prerequisites(state: dict, version: str) -> list[dict]:
    return receipts.validate_prerequisites(
        ROOT,
        ROOT / "build" / version,
        state["source"],
        state["settings_hash"],
        version,
        state["config"],
        state["inventory"],
        state["settings"]["launcher"],
    )


def validate_artwork(state: dict, version: str) -> None:
    output = ROOT / "build" / version
    path = output / "artwork.json"
    if not path.is_file():
        raise InputError("Missing artwork receipt. Rebuild with Ninja.")
    receipts.validate(
        read_json(path),
        expected(state, version, "artwork"),
        output,
        receipts.outputs("artwork"),
        "artwork",
    )


def check_image(state: dict, version: str, strict: bool) -> None:
    output = ROOT / "build" / version
    build_data = (output / "build.json").read_bytes()
    build_hash = hashlib.sha256(build_data).hexdigest()
    record = json.loads(build_data)
    if record.get("schema") != receipts.SCHEMA or record.get("source_files") != state["source"]:
        raise InputError(
            "Build receipt does not describe the current source. Rebuild with Ninja; "
            "use ninja -t clean if input timestamps were preserved."
        )
    calls = prerequisites(state, version)
    link_data = (output / "link.json").read_bytes()
    link_hash = hashlib.sha256(link_data).hexdigest()
    link = json.loads(link_data)
    wanted = expected(state, version, "link")
    receipts.validate(link, wanted, output, receipts.outputs("link"), "link")
    if (
        record.get("calls") != [*calls, link]
        or record.get("link_receipt_sha256") != link_hash
        or record.get("toolchain") != version
        or record.get("tool_files") != state["inventory"]
        or record.get("launcher") != state["settings"]["launcher"]
    ):
        raise InputError("Build and producer receipts disagree. Rebuild with Ninja.")
    image = (output / "pw.bin").read_bytes()
    checksum = hashlib.sha256(image).hexdigest()
    if checksum != link["outputs"]["pw.bin"] or checksum != record.get("image_sha256"):
        raise InputError("Linked image changed after its receipt was written. Rebuild with Ninja.")
    status = image_status(image, state["config"]["target"])
    if expected(load_state(version), version, "link") != wanted:
        raise InputError("Build inputs changed during verification. Repeat the build.")
    if digest(output / "build.json") != build_hash or digest(output / "link.json") != link_hash:
        raise InputError("Build receipts changed during verification. Repeat the build.")
    receipts.validate(link, wanted, output, receipts.outputs("link"), "link")
    validate_artwork(state, version)
    validate_toolchain(ROOT / ".local/toolchains" / version, state["inventory"])
    host.command(state["settings"]["launcher"])
    artwork_record = read_json(output / "artwork.json")
    placeholders = artwork_record.get("placeholders", [])
    if placeholders:
        print("Warning: original placeholder artwork: " + ", ".join(placeholders), file=sys.stderr)
    if state["settings"].get("qualified_version") is None:
        print("Warning: this compiler suite has an unfamiliar fingerprint.", file=sys.stderr)
    report = {
        **status,
        "placeholders": placeholders,
        "source_manifest_sha256": hashlib.sha256(
            json.dumps(state["source"], sort_keys=True).encode()
        ).hexdigest(),
        "build_receipt_sha256": build_hash,
    }
    write_json(output / "status.json", report)
    if status["matches_retail"]:
        if strict:
            write_json(output / "verified.json", report)
        print(f"MATCH: {len(image):,} bytes; retail SHA-256 {checksum}")
        return
    message = f"Output differs from retail: {len(image):,} bytes; SHA-256 {checksum}"
    if strict:
        raise InputError(message)
    print(f"Warning: {message}. The firmware was built successfully.", file=sys.stderr)


def prepare_command(
    action: str, module: str | None, stage: Path, output: Path, config: dict, identity: dict
) -> tuple[str, list[str], Path]:
    """Stage the inputs needed by one compiler phase and return its command."""
    if action == "runtime":
        return (
            "lbg38.exe",
            [
                f"-output={windows_path(stage / 'out/runtime.lib')}",
                "-head=Runtime",
                "-cpu=300HN",
            ],
            stage / "compiler/bin",
        )
    if action == "compile":
        source = config["modules"][module]
        for name, checksum in identity["inputs"].items():
            if not name.startswith(("src/", "include/")):
                continue
            receipts.copy_checked(ROOT / name, stage / name, checksum)
        if module == modules.ARTWORK_MODULE:
            name = (output / "rom_assets.h").relative_to(ROOT).as_posix()
            receipts.copy_checked(
                ROOT / name, stage / "generated/rom_assets.h", identity["inputs"][name]
            )
        return (
            "ch38.exe",
            [
                *receipts.compile_options(config, module),
                f"-object={windows_path(stage / 'out' / f'{module}.obj')}",
                windows_path(stage / "src" / source["file"]),
            ],
            stage,
        )
    for name in receipts.link_inputs(config):
        key = (output / name).relative_to(ROOT).as_posix()
        receipts.copy_checked(output / name, stage / "out" / name, identity["inputs"][key])
    lines = receipts.link_commands(config)
    (stage / "out/link.sub").write_bytes("\r\n".join(lines).encode("ascii"))
    return "optlnk.exe", ["-subcommand=out\\link.sub"], stage


def step(
    version: str, action: str, module: str | None = None, source_path: str | None = None
) -> None:
    output = ROOT / "build" / version
    (output / "verified.json").unlink(missing_ok=True)
    (output / "status.json").unlink(missing_ok=True)
    state = load_state(version)
    settings, config, inventory = state["settings"], state["config"], state["inventory"]
    if action == "compile" and module not in config["modules"]:
        raise InputError("Compilation requires a discovered source module name.")
    if source_path is not None and (
        action != "compile" or source_path != config["modules"][module]["file"]
    ):
        raise InputError("Source path and discovered module disagree; rerun configure.py.")
    suite = ROOT / ".local/toolchains" / version
    validate_toolchain(suite, inventory)
    host.command(settings["launcher"])
    if action in {"status", "verify"}:
        check_image(state, version, strict=action == "verify")
        return

    if action == "link":
        prerequisites(state, version)
    elif action == "compile" and module == modules.ARTWORK_MODULE:
        validate_artwork(state, version)
    identity = expected(state, version, action, module)
    if action == "artwork":
        manifest = source_json("config/artwork.json", state["source"])
        data, placeholders = assets.generate(ROOT, manifest)
        if expected(load_state(version), version, action) != identity:
            raise InputError("Artwork inputs changed during conversion. Repeat the build.")
        write_bytes(output / "rom_assets.h", data)
        write_json(
            output / "artwork.json",
            {
                **identity,
                "placeholders": placeholders,
                "outputs": {"rom_assets.h": hashlib.sha256(data).hexdigest()},
            },
        )
        if placeholders:
            print("Using original placeholders: " + ", ".join(placeholders))
        return

    with tempfile.TemporaryDirectory(prefix="pw-", dir=settings["work_dir"]) as temporary:
        stage = Path(temporary)
        # Stage the imported files recorded when this build was configured.
        for name, checksum in inventory.items():
            receipts.copy_checked(suite / name, stage / "compiler" / name, checksum)
        validate_toolchain(stage / "compiler", inventory)
        (stage / "out").mkdir()
        stem = receipts.stem(action, module)
        results = receipts.outputs(action, module)
        tool, arguments, cwd = prepare_command(action, module, stage, output, config, identity)
        call = invoke(stage, tool, arguments, cwd, settings["launcher"], output, stem)
        if expected(load_state(version), version, action, module) != identity:
            raise InputError("Producer inputs changed while it ran; repeat the build.")
        validate_toolchain(suite, inventory)
        host.command(settings["launcher"])
        if action == "compile" and module == modules.ARTWORK_MODULE:
            validate_artwork(state, version)
        hashes = {}
        for name in results:
            path = stage / "out" / name
            if not path.is_file():
                raise InputError(f"{tool} did not create {name}. See {output / (stem + '.log')}.")
            data = path.read_bytes()
            hashes[name] = hashlib.sha256(data).hexdigest()
            write_bytes(output / name, data)
        call = {**call, **identity, "outputs": hashes}
        write_json(output / f"{stem}.json", call)
        if action == "link":
            calls = prerequisites(state, version)
            calls.append(call)
            write_json(
                output / "build.json",
                {
                    "schema": receipts.SCHEMA,
                    "toolchain": version,
                    "host": platform.platform(),
                    "architecture": platform.machine(),
                    "launcher": settings["launcher"],
                    "source_files": state["source"],
                    "tool_files": inventory,
                    "calls": calls,
                    "image_sha256": digest(output / "pw.bin"),
                    "link_receipt_sha256": digest(output / "link.json"),
                },
            )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    parser.add_argument(
        "action", choices=["artwork", "runtime", "compile", "link", "status", "verify"]
    )
    parser.add_argument("module", nargs="?", help="source module basename without .c")
    parser.add_argument("--source", dest="source_path", help="source path recorded by Ninja")
    args = parser.parse_args()
    try:
        toolchain.validate_name(args.version)
        if args.action == "compile" and args.module is None:
            raise InputError("Compilation requires a source module.")
        step(args.version, args.action, args.module, args.source_path)
    except (InputError, OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
