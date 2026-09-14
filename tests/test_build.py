"""Exercise source/artifact association with authored inputs and fake producers."""

import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

from PIL import Image

import configure
from tools import assets, build, common, receipts
from tools.common import InputError, digest, read_json, write_bytes, write_json


class BuildFixture(unittest.TestCase):
    ROM = b"authored complete image"

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.version = "6.02.01"
        self.output = self.root / "build" / self.version
        self.rom = self.ROM
        self.sources = {
            "a": "support/a.c",
            "b": "application/b.c",
            "pw_builtin": "application/pw_builtin.c",
        }
        for name in (
            *("src/" + path for path in self.sources.values()),
            "include/support/fixture.h",
            "tools/driver.py",
            "configure.py",
        ):
            write_bytes(self.root / name, name.encode())
        self.suite = self.root / ".local/toolchains" / self.version
        files = {}
        for name in ("bin/ch38.exe", "bin/lbg38.exe", "bin/optlnk.exe"):
            write_bytes(self.suite / name, b"authored producer fixture")
            files[name] = digest(self.suite / name)
        write_bytes(self.suite / "include/not-qualified.h", b"must not be staged")
        write_json(
            self.root / ".local/config.json",
            {
                "toolchain": self.version,
                "tool_files": files,
                "qualified_version": self.version,
                "launcher": {"name": "native"},
                "work_dir": str(self.root),
            },
        )
        write_json(
            self.root / "config/build.json",
            {
                "target": {"size": len(self.rom), "sha256": hashlib.sha256(self.rom).hexdigest()},
                "include_dirs": ["include", "generated"],
                "compiler_flags": [],
                "linker_options": [],
                "source_flags": {"b.c": ["-speed"]},
            },
        )
        write_json(self.root / "config/toolchains.json", {self.version: {"files": files}})
        write_json(
            self.root / "config/artwork.json",
            [
                {
                    "name": "fixture",
                    "file": "fixture.ncg",
                    "format": "column-2bpp",
                    "width": 8,
                    "height": 8,
                    "offset": 0,
                    "size": 16,
                }
            ],
        )
        picture = assets.write_ncg(Image.new("L", (8, 8), 170))
        write_bytes(self.root / "assets/local/fixture.ncg", picture)
        write_bytes(self.root / "assets/placeholders/fixture.ncg", picture)
        for target, value in (
            ("tools.build.ROOT", self.root),
            ("tools.common.ROOT", self.root),
            ("tools.host.command", lambda launcher: []),
        ):
            context = patch(target, value)
            context.start()
            self.addCleanup(context.stop)
        self.invocation = patch("tools.build.invoke", side_effect=self.produce)
        self.invocation.start()
        self.addCleanup(self.invocation.stop)
        self.rebuild()

    def produce(self, stage, tool, arguments, cwd, launcher, output, stem):
        self.assertFalse((stage / "compiler/include/not-qualified.h").exists())
        if tool == "ch38.exe":
            sources = list((stage / "src").rglob("*.c"))
            self.assertEqual(len(sources), 1)
            self.assertEqual(sources[0].name, stem + ".c")
            self.assertIn(common.windows_path(sources[0]), arguments)
            self.assertEqual(
                (stage / "include/support/fixture.h").read_bytes(),
                (self.root / "include/support/fixture.h").read_bytes(),
            )
            self.assertEqual((stage / "generated/rom_assets.h").is_file(), stem == "pw_builtin")
            payload = b""
            for folder in ("src", "include", "generated"):
                for path in sorted((stage / folder).rglob("*")):
                    if path.is_file():
                        payload += path.read_bytes()
            write_bytes(stage / "out" / (stem + ".obj"), hashlib.sha256(payload).digest())
        elif tool == "lbg38.exe":
            write_bytes(stage / "out/runtime.lib", b"authored runtime")
        else:
            # Deliberately always matches, even for obsolete objects: the
            # association checks must reject stale source before trusting it.
            write_bytes(stage / "out/pw.bin", self.rom)
            write_bytes(stage / "out/pw.map", b"authored map")
        write_bytes(output / f"{stem}.log", b"authored producer log")
        return {
            "tool": tool,
            "arguments": arguments,
            "exit_code": 0,
            "seconds": 0,
            "log": f"{stem}.log",
        }

    def step(self, action, module=None):
        with redirect_stdout(io.StringIO()):
            build.step(self.version, action, module)

    def rebuild(self):
        self.step("artwork")
        self.step("runtime")
        for module in self.sources:
            self.step("compile", module)
        self.step("link")
        self.step("verify")


class BuildReceiptTests(BuildFixture):
    def test_build_and_verification_work_without_the_rom(self):
        self.assertFalse((self.root / ".local/retail.bin").exists())
        self.assertTrue(read_json(self.output / "verified.json")["matches_retail"])

    def test_finder_metadata_preserves_the_source_inventory_and_receipts(self):
        previous = common.source_inventory()
        for directory in ("src/support", "include/support", "config", "tools"):
            write_bytes(self.root / directory / ".DS_Store", b"authored Finder metadata")
        self.assertEqual(common.source_inventory(), previous)
        self.step("verify")
        self.assertTrue(read_json(self.output / "verified.json")["matches_retail"])

    def test_module_names_and_flags_reach_the_compiler_and_linker(self):
        for module, source in self.sources.items():
            with self.subTest(module=module):
                record = read_json(self.output / f"{module}.json")
                self.assertEqual(set(record["outputs"]), {f"{module}.obj"})
                self.assertEqual(record["log"], f"{module}.log")
                arguments = record["recipe"]["arguments"]
                self.assertIn("src\\" + source.replace("/", "\\"), arguments)
                self.assertEqual("-speed" in arguments, module == "b")
        self.assertEqual(
            [
                line
                for line in (self.output / "link.sub").read_text().splitlines()
                if line.startswith("input ")
            ],
            ["input out\\a.obj", "input out\\b.obj", "input out\\pw_builtin.obj"],
        )

    def test_added_and_removed_sources_change_the_linked_modules(self):
        source = self.root / "src/support/c.c"
        write_bytes(source, b"new authored unit")
        with self.assertRaisesRegex(InputError, "Missing c receipt"):
            self.step("link")
        self.step("compile", "c")
        self.step("link")
        self.step("verify")
        self.assertIn("input out\\c.obj", (self.output / "link.sub").read_text())
        source.unlink()
        self.step("link")
        self.step("verify")
        self.assertNotIn("input out\\c.obj", (self.output / "link.sub").read_text())
        self.assertTrue((self.output / "c.obj").exists())
        self.assertEqual(
            read_json(self.output / "build.json")["source_files"], common.source_inventory()
        )

    def test_directory_move_preserves_link_order_and_requires_a_fresh_receipt(self):
        previous = (self.output / "link.sub").read_bytes()
        source = self.root / "src/support/a.c"
        destination = self.root / "src/startup/a.c"
        destination.parent.mkdir()
        source.rename(destination)
        with self.assertRaisesRegex(InputError, "Stale a receipt"):
            self.step("link")
        self.step("compile", "a")
        self.step("link")
        self.step("verify")
        self.assertEqual((self.output / "link.sub").read_bytes(), previous)
        self.assertIn(
            "src\\startup\\a.c",
            read_json(self.output / "a.json")["recipe"]["arguments"],
        )

    def test_compilation_requires_an_existing_module_identity(self):
        for module in (None, "missing", "../a", "a.c"):
            with self.subTest(module=module), self.assertRaises(InputError):
                self.step("compile", module)

    def test_compile_rejects_a_graph_with_an_outdated_source_path(self):
        with self.assertRaises(InputError):
            build.step(self.version, "compile", "a", source_path="former/a.c")

    def test_modified_build_warns_and_strict_verification_fails(self):
        self.rom = b"modified authored image"
        self.step("link")
        self.step("status")
        self.assertFalse(read_json(self.output / "status.json")["matches_retail"])
        with self.assertRaisesRegex(InputError, "differs from retail"):
            self.step("verify")
        self.assertFalse((self.output / "verified.json").exists())

    def test_missing_local_artwork_rebuilds_using_a_placeholder(self):
        (self.root / "assets/local/fixture.ncg").unlink()
        self.step("artwork")
        self.step("compile", "pw_builtin")
        self.step("link")
        self.step("status")
        self.assertEqual(read_json(self.output / "status.json")["placeholders"], ["fixture"])

    def test_incremental_unit_keeps_unrelated_objects_and_runtime_valid(self):
        untouched = {name: digest(self.output / name) for name in ("b.json", "runtime.json")}
        (self.root / "src/support/a.c").write_bytes(b"changed authored unit")
        self.step("compile", "a")
        self.step("link")
        self.step("verify")
        self.assertEqual(untouched, {name: digest(self.output / name) for name in untouched})
        self.assertEqual(
            read_json(self.output / "build.json")["source_files"], common.source_inventory()
        )

    def test_matching_image_cannot_qualify_source_edited_after_compilation(self):
        source = self.root / "src/support/a.c"
        times = source.stat()
        source.write_bytes(b"new source with preserved timestamps")
        os.utime(source, ns=(times.st_atime_ns, times.st_mtime_ns))
        self.assertEqual((self.output / "pw.bin").read_bytes(), self.rom)
        with self.assertRaisesRegex(InputError, "current source"):
            self.step("verify")
        self.assertFalse((self.output / "verified.json").exists())
        with self.assertRaisesRegex(InputError, "Stale a receipt"):
            self.step("link")

    def test_shared_include_change_addition_and_removal_invalidate_objects(self):
        header = self.root / "include/support/fixture.h"
        original = header.read_bytes()
        for change in ("modify", "add", "remove"):
            with self.subTest(change=change):
                if change == "modify":
                    header.write_bytes(b"changed header")
                elif change == "add":
                    write_bytes(self.root / "include/nested/data.inc", b"new include")
                else:
                    header.unlink()
                with self.assertRaisesRegex(InputError, "Stale a receipt"):
                    self.step("link")
                header.write_bytes(original)
                (self.root / "include/nested/data.inc").unlink(missing_ok=True)

    def test_replaced_outputs_are_rejected_even_when_the_image_still_matches(self):
        for name in ("a.obj", "runtime.lib", "rom_assets.h"):
            with self.subTest(output=name):
                path = self.output / name
                original = path.read_bytes()
                path.write_bytes(b"replaced artifact")
                with self.assertRaisesRegex(InputError, "output changed"):
                    self.step("verify")
                self.assertFalse((self.output / "verified.json").exists())
                path.write_bytes(original)

    def test_old_or_incomplete_unit_receipts_cannot_supply_their_own_key_set(self):
        path = self.output / "a.json"
        original = path.read_bytes()
        for change in ("schema", "dependency", "recipe"):
            with self.subTest(change=change):
                record = json.loads(original)
                if change == "schema":
                    record.pop("schema")
                elif change == "dependency":
                    record["inputs"].pop("include/support/fixture.h")
                else:
                    record["recipe"]["arguments"].append("-different")
                write_json(path, record)
                with self.assertRaisesRegex(InputError, "Stale a receipt"):
                    self.step("verify")
                path.write_bytes(original)
        path.unlink()
        with self.assertRaisesRegex(InputError, "Missing a receipt"):
            self.step("verify")

    def test_artwork_recipe_and_file_associations_are_checked(self):
        path = self.root / "assets/local/fixture.ncg"
        original = path.read_bytes()
        path.write_bytes(b"changed artwork")
        with self.assertRaisesRegex(InputError, "Stale artwork receipt"):
            self.step("link")
        path.write_bytes(original)
        config = self.root / "config/artwork.json"
        manifest = read_json(config)
        manifest[0]["width"] = 16
        write_json(config, manifest)
        with self.assertRaisesRegex(InputError, "Stale artwork receipt"):
            self.step("link")

    def test_changed_flags_launcher_or_tool_cannot_reuse_receipts(self):
        config_path = self.root / "config/build.json"
        settings_path = self.root / ".local/config.json"
        config_bytes, settings_bytes = config_path.read_bytes(), settings_path.read_bytes()
        for change in ("common flags", "source flags", "launcher"):
            with self.subTest(change=change):
                config = json.loads(config_bytes)
                if change == "common flags":
                    config["compiler_flags"].append("-changed")
                elif change == "source flags":
                    config["source_flags"]["a.c"] = ["-changed"]
                else:
                    settings = json.loads(settings_bytes)
                    settings["launcher"]["test_identity"] = "different"
                    write_json(settings_path, settings)
                write_json(config_path, config)
                with self.assertRaisesRegex(InputError, "Stale artwork receipt"):
                    self.step("link")
                config_path.write_bytes(config_bytes)
                settings_path.write_bytes(settings_bytes)
        (self.suite / "bin/ch38.exe").write_bytes(b"other tool")
        with self.assertRaisesRegex(InputError, "Compiler input is missing or changed"):
            self.step("verify")

    def test_copy_time_source_change_never_publishes_a_new_receipt(self):
        previous = (self.output / "a.json").read_bytes()
        copy = receipts.copy_checked

        def change_before_copy(source, target, expected):
            if source == self.root / "src/support/a.c":
                source.write_bytes(b"edit while staging")
            copy(source, target, expected)

        with patch("tools.receipts.copy_checked", side_effect=change_before_copy):
            with self.assertRaisesRegex(InputError, "Input changed while staging"):
                self.step("compile", "a")
        self.assertEqual((self.output / "a.json").read_bytes(), previous)
        self.assertFalse((self.output / "verified.json").exists())

    def test_source_edit_during_link_is_rejected_before_publication(self):
        previous = (self.output / "build.json").read_bytes()

        def change_after_producing(*args):
            result = self.produce(*args)
            (self.root / "src/support/a.c").write_bytes(b"edit during link")
            return result

        with patch("tools.build.invoke", side_effect=change_after_producing):
            with self.assertRaisesRegex(InputError, "Producer inputs changed"):
                self.step("link")
        self.assertEqual((self.output / "build.json").read_bytes(), previous)

    def test_verification_catches_artwork_and_compiler_changes_during_hashing(self):
        for path in (self.root / "assets/local/fixture.ncg", self.suite / "bin/ch38.exe"):
            with self.subTest(path=path):
                original = path.read_bytes()

                def change_after_hash(image, target):
                    result = common.image_status(image, target)
                    path.write_bytes(b"changed during hash check")
                    return result

                with patch("tools.build.image_status", side_effect=change_after_hash):
                    with self.assertRaises(InputError):
                        self.step("verify")
                self.assertFalse((self.output / "verified.json").exists())
                path.write_bytes(original)

    def test_verification_catches_fallback_switch_during_hashing(self):
        path = self.root / "assets/local/fixture.ncg"

        def remove_after_hash(image, target):
            result = common.image_status(image, target)
            path.unlink()
            return result

        with patch("tools.build.image_status", side_effect=remove_after_hash):
            with self.assertRaisesRegex(InputError, "Stale artwork"):
                self.step("verify")
        self.assertFalse((self.output / "verified.json").exists())

    def test_verify_checks_link_receipt_and_image_identity(self):
        path = self.output / "build.json"
        record = read_json(path)
        record["link_receipt_sha256"] = "0" * 64
        write_json(path, record)
        with self.assertRaisesRegex(InputError, "receipts disagree"):
            self.step("verify")

    def test_verify_does_not_attest_replacements_made_during_comparison(self):
        for name in ("build.json", "link.json", "pw.bin", "pw.map", "link.sub"):
            with self.subTest(replaced=name):
                path = self.output / name
                original = path.read_bytes()

                def replace_after_compare(image, target):
                    result = common.image_status(image, target)
                    path.write_bytes(b"unvalidated replacement")
                    return result

                with patch("tools.build.image_status", side_effect=replace_after_compare):
                    with self.assertRaisesRegex(InputError, "changed"):
                        self.step("verify")
                self.assertFalse((self.output / "verified.json").exists())
                path.write_bytes(original)


class NinjaIncrementalTests(BuildFixture):
    def rebuild(self):
        test_file = Path(__file__).resolve()
        for path, action in (("configure.py", "configure"), ("tools/build.py", "build")):
            wrapper = (
                "import runpy\nimport sys\n"
                f"sys.path.insert(0, {str(test_file.parents[1])!r})\n"
                "sys.modules.pop('tools', None)\n"
                f"sys.argv.insert(1, '--fixture-{action}')\n"
                f"runpy.run_path({str(test_file)!r}, run_name='__main__')\n"
            )
            write_bytes(self.root / path, wrapper.encode())
        # Create the wrapper cache directory before Ninja records input times.
        (self.root / "tools/__pycache__").mkdir()
        with patch("configure.ROOT", self.root):
            graph = configure.generate(
                read_json(self.root / ".local/config.json"),
                read_json(self.root / "config/build.json"),
            )
        write_bytes(self.root / "build.ninja", graph)
        self.run_ninja(self.sources)

    def run_ninja(self, expected_modules):
        executable = shutil.which("ninja")
        self.assertIsNotNone(executable, "Run with uv run so Ninja is available.")
        result = subprocess.run(
            [executable, "verify"], cwd=self.root, capture_output=True, text=True, timeout=60
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        compiled = [
            Path(line.split("CH38 ", 1)[1]).stem
            for line in result.stdout.splitlines()
            if "CH38 " in line
        ]
        self.assertCountEqual(compiled, expected_modules, result.stdout)
        self.assertTrue(read_json(self.output / "verified.json")["matches_retail"])

    def test_moving_a_source_preserves_mtime_and_recompiles_only_that_module(self):
        previous_order = (self.output / "link.sub").read_bytes()
        untouched = {name: digest(self.output / name) for name in ("b.json", "runtime.json")}
        source = self.root / "src/support/a.c"
        previous_mtime = source.stat().st_mtime_ns
        destination = self.root / "src/relocated files/a.c"
        destination.parent.mkdir()
        source.rename(destination)
        self.assertEqual(destination.stat().st_mtime_ns, previous_mtime)
        self.run_ninja(["a"])
        self.assertEqual((self.output / "link.sub").read_bytes(), previous_order)
        self.assertEqual(untouched, {name: digest(self.output / name) for name in untouched})
        self.assertIn("src/relocated files/a.c", read_json(self.output / "a.json")["inputs"])

    def test_source_addition_and_removal_update_the_build_automatically(self):
        source = self.root / "src/support/c.c"
        write_bytes(source, b"added authored source")
        self.run_ninja(["c"])
        self.assertIn("input out\\c.obj", (self.output / "link.sub").read_text())
        source.unlink()
        self.run_ninja([])
        self.assertNotIn("input out\\c.obj", (self.output / "link.sub").read_text())

    def test_nested_header_addition_and_removal_recompile_all_modules(self):
        untouched = {name: digest(self.output / name) for name in ("artwork.json", "runtime.json")}
        header = self.root / "include/nested/data.inc"
        write_bytes(header, b"added authored header")
        self.run_ninja(self.sources)
        header.unlink()
        self.run_ninja(self.sources)
        self.assertEqual(untouched, {name: digest(self.output / name) for name in untouched})

    def test_restored_artwork_directory_and_later_edits_rebuild_automatically(self):
        local = self.root / "assets/local"
        saved = self.root / "saved-artwork"
        previous_mtime = local.stat().st_mtime_ns
        local.rename(saved)
        local.mkdir()
        self.run_ninja(["pw_builtin"])
        self.assertEqual(read_json(self.output / "status.json")["placeholders"], ["fixture"])

        local.rmdir()
        saved.rename(local)
        self.assertEqual(local.stat().st_mtime_ns, previous_mtime)
        self.run_ninja(["pw_builtin"])
        self.assertEqual(read_json(self.output / "status.json")["placeholders"], [])
        original = digest(self.output / "rom_assets.h")

        picture = Image.new("L", (8, 8), 170)
        picture.putpixel((0, 0), 85)
        (local / "fixture.ncg").write_bytes(assets.write_ncg(picture))
        self.assertEqual(local.stat().st_mtime_ns, previous_mtime)
        self.run_ninja(["pw_builtin"])
        self.assertNotEqual(digest(self.output / "rom_assets.h"), original)
        self.run_ninja([])


def run_ninja_fixture(action):
    """Run the generated commands against authored producers in a child process."""
    root = Path.cwd()
    configure.ROOT = common.ROOT = build.ROOT = root
    if action == "--fixture-configure":
        graph = configure.generate(
            read_json(root / ".local/config.json"), read_json(root / "config/build.json")
        )
        write_bytes(root / "build.ninja", graph)
        return 0
    fixture = BuildFixture()
    fixture.root, fixture.rom = root, fixture.ROM
    build.invoke = fixture.produce
    build.host.command = lambda launcher: []
    return build.main()


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1].startswith("--fixture-"):
        raise SystemExit(run_ninja_fixture(sys.argv.pop(1)))
    unittest.main()
