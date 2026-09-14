"""Check module discovery and the filename rules that determine link order."""

import unittest

from tools import modules, receipts
from tools.common import InputError


class ModuleTests(unittest.TestCase):
    def test_discovery_sorts_basenames_and_applies_only_named_overrides(self):
        config = {"source_flags": {"pw_pedometer.c": ["-speed"]}, "compiler_flags": []}
        paths = [
            "src/application/pw_pedometer.c",
            "include/globals.h",
            "src/support/ir.c",
            "src/globals.c",
            "tools/example.c",
            "src/application/pw_fourier.c",
        ]
        expected = {
            "globals": {"file": "globals.c", "flags": []},
            "ir": {"file": "support/ir.c", "flags": []},
            "pw_fourier": {"file": "application/pw_fourier.c", "flags": []},
            "pw_pedometer": {"file": "application/pw_pedometer.c", "flags": ["-speed"]},
        }
        for inputs in (paths, reversed(paths), dict.fromkeys(paths, "authored checksum")):
            result = modules.resolve(config, inputs)
            self.assertEqual(result["modules"], expected)
            self.assertEqual(list(result["modules"]), list(expected))
        self.assertNotIn("modules", config)

    def test_directory_moves_preserve_module_identity_and_link_order(self):
        config = {"source_flags": {}, "linker_options": []}
        before = modules.resolve(config, ["src/a/z.c", "src/z/a.c"])
        after = modules.resolve(config, ["src/startup/a.c", "src/support/z.c"])
        self.assertEqual(list(before["modules"]), ["a", "z"])
        self.assertEqual(list(after["modules"]), ["a", "z"])
        self.assertEqual(receipts.link_commands(before), receipts.link_commands(after))

    def test_duplicate_basenames_are_rejected_across_directories(self):
        with self.assertRaises(InputError):
            modules.resolve({"source_flags": {}}, ["src/support/a.c", "src/application/a.c"])

    def test_module_names_are_lowercase_ascii_c_identifiers(self):
        for name in ("A", "naïve", "two-words", "2fast", "two.parts"):
            with self.subTest(name=name), self.assertRaises(InputError):
                modules.resolve({"source_flags": {}}, [f"src/{name}.c"])

    def test_module_names_cannot_replace_other_build_artifacts(self):
        for name in ("artwork", "runtime", "link", "build", "status", "verified"):
            with self.subTest(name=name), self.assertRaises(InputError):
                modules.resolve({"source_flags": {}}, [f"src/{name}.c"])

    def test_flag_overrides_must_name_an_existing_source_basename(self):
        for name in ("missing.c", "a", "support/a.c"):
            with self.subTest(name=name), self.assertRaises(InputError):
                modules.resolve({"source_flags": {name: ["-speed"]}}, ["src/support/a.c"])


if __name__ == "__main__":
    unittest.main()
