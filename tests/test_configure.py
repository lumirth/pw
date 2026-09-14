"""Exercise configuration recovery independently of the installed host tools."""

import argparse
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import configure
from tools.common import InputError, read_json, write_bytes, write_json


class ConfigureTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.settings = self.root / ".local/config.json"
        self.graph = self.root / "build.ninja"
        write_json(self.settings, {"toolchain": "6.02.02", "wibo": "/former/mac/wibo"})
        write_json(self.root / "config/build.json", {})
        self.graph.write_bytes(b"previous graph")
        self.previous = self.settings.read_bytes()
        self.args = argparse.Namespace(
            toolchain=None, wibo=None, managed_wibo=False, offline=True, work_dir=self.root
        )
        for name, value in (
            ("configure.ROOT", self.root),
            ("configure.os.chdir", lambda path: None),
            ("configure.host.select", lambda override, offline: {"name": "native"}),
            ("configure.host.qualified_host", lambda: True),
            (
                "configure.toolchain.describe",
                lambda directory: {"files": {}, "qualified_version": "6.02.02", "warnings": []},
            ),
        ):
            context = patch(name, value)
            context.start()
            self.addCleanup(context.stop)

    def test_windows_configuration_drops_a_previous_wibo_override(self):
        with patch("configure.generate", return_value=b"new graph"):
            configure.configure(self.args)
        settings = read_json(self.settings)
        self.assertEqual(settings["launcher"], {"name": "native"})
        self.assertNotIn("wibo", settings)

    def test_invalid_graph_preserves_previous_configuration(self):
        with patch("configure.generate", side_effect=InputError("bad manifest")):
            with self.assertRaisesRegex(InputError, "bad manifest"):
                configure.configure(self.args)
        self.assertEqual(self.settings.read_bytes(), self.previous)
        self.assertEqual(self.graph.read_bytes(), b"previous graph")

    def test_failed_graph_write_restores_previous_settings(self):
        write = configure.write_bytes

        def fail_graph(path, data):
            if path == self.graph:
                raise OSError("authored write failure")
            write(path, data)

        with patch("configure.generate", return_value=b"new graph"):
            with patch("configure.write_bytes", side_effect=fail_graph):
                with self.assertRaises(OSError):
                    configure.configure(self.args)
        self.assertEqual(self.settings.read_bytes(), self.previous)
        self.assertEqual(self.graph.read_bytes(), b"previous graph")


class BuildGraphTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        previous = Path.cwd()
        os.chdir(self.root)
        self.addCleanup(os.chdir, previous)
        for target in ("configure.ROOT", "tools.common.ROOT"):
            context = patch(target, self.root)
            context.start()
            self.addCleanup(context.stop)
        for name in (
            "src/support/a.c",
            "src/application/b.c",
            "src/application/pw_builtin.c",
            "include/support/fixture.h",
            "configure.py",
            "tools/driver.py",
            ".local/toolchains/6.02.01/bin/ch38.exe",
        ):
            write_bytes(self.root / name, name.encode())
        (self.root / "assets/local").mkdir(parents=True)
        (self.root / "assets/placeholders").mkdir()
        self.config = {
            "include_dirs": ["include", "generated"],
            "compiler_flags": [],
            "linker_options": [],
            "source_flags": {"b.c": ["-speed"]},
        }
        self.settings = {
            "toolchain": "6.02.01",
            "tool_files": {"bin/ch38.exe": "authored checksum"},
            "launcher": {"name": "native"},
        }
        write_json(self.root / "config/build.json", self.config)
        write_json(self.root / "config/artwork.json", [])
        write_json(self.root / ".local/config.json", self.settings)

    def generate(self):
        return configure.generate(self.settings, self.config).decode().replace("$\n", "")

    def edge(self, graph, target):
        return next(
            line
            for line in graph.splitlines()
            if line.startswith("build ") and target in line.split(":", 1)[0].split()[1:]
        )

    def linked_objects(self, graph):
        line = self.edge(graph, "build/6.02.01/pw.bin")
        return [word for word in line.split(":", 1)[1].split() if word.endswith(".obj")]

    def test_nested_paths_supply_dependencies_and_modules_link_alphabetically(self):
        graph = self.generate()
        reconfigure = self.edge(graph, "build.ninja").split()
        for path in (
            "src/support/a.c",
            "src/application/b.c",
            "include/support/fixture.h",
            "src/support",
            "src/application",
            "include/support",
        ):
            self.assertIn(path, reconfigure)
        headers = self.edge(graph, "headers").split()
        self.assertIn("include/support/fixture.h", headers)
        self.assertIn("include/support", headers)
        source = self.edge(graph, "build/6.02.01/a.obj").split()
        self.assertIn("src/support/a.c", source)
        self.assertIn("headers", source)
        artwork = self.edge(graph, "build/6.02.01/pw_builtin.obj").split()
        self.assertIn("build/6.02.01/rom_assets.h", artwork)
        self.assertIn("build/6.02.01/artwork.json", artwork)
        self.assertNotIn("build/6.02.01/rom_assets.h", source)
        self.assertEqual(
            self.linked_objects(graph),
            [f"build/6.02.01/{module}.obj" for module in ("a", "b", "pw_builtin")],
        )
        self.assertIn("  action = compile a", graph)

    def test_source_addition_and_removal_update_the_graph(self):
        self.generate()
        write_bytes(self.root / "src/startup/h8_added.c", b"new authored unit")
        added = self.generate()
        self.assertEqual(
            self.linked_objects(added),
            [f"build/6.02.01/{module}.obj" for module in ("a", "b", "h8_added", "pw_builtin")],
        )
        self.assertIn("src/startup", self.edge(added, "build.ninja").split())
        (self.root / "src/support/a.c").unlink()
        removed = self.generate()
        self.assertNotIn("build/6.02.01/a.obj", removed)
        self.assertNotIn("src/support/a.c", removed)
        self.assertIn("src/support", self.edge(removed, "build.ninja").split())

    def test_directory_move_changes_source_dependencies_and_preserves_object_order(self):
        before = self.generate()
        (self.root / "src/startup").mkdir()
        (self.root / "src/support/a.c").rename(self.root / "src/startup/a.c")
        after = self.generate()
        self.assertEqual(self.linked_objects(before), self.linked_objects(after))
        source = self.edge(after, "build/6.02.01/a.obj").split()
        self.assertIn("src/startup/a.c", source)
        self.assertNotIn("src/support/a.c", after)

    def test_removing_a_source_with_flags_requires_removing_its_override(self):
        (self.root / "src/application/b.c").unlink()
        with self.assertRaises(InputError):
            self.generate()
