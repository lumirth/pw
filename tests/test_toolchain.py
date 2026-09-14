"""Exercise compiler imports with small, authored installations and archives."""

import hashlib
import io
import struct
import tempfile
import unittest
import zipfile
import zlib
from contextlib import redirect_stderr
from pathlib import Path
from unittest.mock import patch

from tools import installshield, toolchain
from tools.common import InputError, read_json


def fixture_files():
    files = {
        name: f"Authored fixture: {name}\n".encode("ascii") for name in toolchain.REQUIRED_FILES
    }
    files["bin/ch38.exe"] = b"Authored C/C++ Compiler V.6.02.02.000 fixture"
    return files


def fixture_catalog(files):
    return {
        "6.02.02": {
            "files": {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}
        }
    }


def updater_fixture(files):
    """Wrap raw fixture files in the subset of InstallShield used by the importer."""
    directories = list(dict.fromkeys(name.rsplit("/", 1)[0] for name, _ in files))
    strings = bytearray(4 * len(directories))
    directory_offsets = {}
    for index, directory in enumerate(directories):
        struct.pack_into("<I", strings, index * 4, len(strings))
        directory_offsets[directory] = index
        strings.extend(directory.encode("ascii") + b"\0")
    names = []
    for name, _ in files:
        names.append(len(strings))
        strings.extend(name.rsplit("/", 1)[-1].encode("ascii") + b"\0")
    base = 32
    strings_offset = 128
    records_offset = strings_offset + len(strings)
    header = bytearray(records_offset + len(files) * 87)
    header[:4] = b"ISc("
    struct.pack_into("<I", header, 12, base)
    struct.pack_into("<I", header, base + 12, strings_offset - base)
    struct.pack_into("<I", header, base + 40, len(files))
    struct.pack_into("<I", header, base + 44, len(strings))
    header[strings_offset:records_offset] = strings
    payload = bytearray(b"\0")
    for index, (name, data) in enumerate(files):
        descriptor = records_offset + index * 87
        struct.pack_into("<QQQ", header, descriptor + 2, len(data), len(data), len(payload))
        header[descriptor + 26 : descriptor + 42] = hashlib.md5(
            data, usedforsecurity=False
        ).digest()
        struct.pack_into("<I", header, descriptor + 58, names[index])
        struct.pack_into("<H", header, descriptor + 62, directory_offsets[name.rsplit("/", 1)[0]])
        struct.pack_into("<H", header, descriptor + 85, 2)
        payload.extend(data)
    executable = bytearray(512)
    executable[:2] = b"MZ"
    struct.pack_into("<I", executable, 0x3C, 64)
    executable[64:68] = b"PE\0\0"
    struct.pack_into("<H", executable, 70, 1)
    struct.pack_into("<II", executable, 64 + 24 + 16, 512, 0)
    for name, data in (("data1.cab", b"fixture"), ("data1.hdr", header), ("data2.cab", payload)):
        executable.extend(name.encode("ascii") + b"\0fixture\0fixture\0")
        executable.extend(str(len(data)).encode("ascii") + b"\0" + data)
    return bytes(executable)


class ToolchainTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.files = fixture_files()
        self.catalog = fixture_catalog(self.files)
        self.installed = self.root / "HEW/H8/current"
        for name, data in self.files.items():
            path = self.installed / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

    def import_from(self, source, name=None):
        warnings = io.StringIO()
        with redirect_stderr(warnings):
            label = toolchain.import_compiler(source, self.catalog, self.root / "imports", name)
        return label, warnings.getvalue()

    def test_installed_root_bin_executable_and_enclosing_directory(self):
        for source in (
            self.installed,
            self.installed / "bin",
            self.installed / "bin/ch38.exe",
            self.root / "HEW",
        ):
            with self.subTest(source=source):
                label, warnings = self.import_from(source)
                self.assertEqual(label, "6.02.02")
                self.assertEqual(warnings, "")
                imported = self.root / "imports" / label
                self.assertEqual(
                    toolchain.describe(imported, self.catalog)["files"],
                    self.catalog[label]["files"],
                )
                self.assertEqual(read_json(imported / "toolchain.json")["qualified_version"], label)

    def test_different_hash_is_a_warning_and_actual_bytes_are_recorded(self):
        changed = self.installed / "bin/c38mid.exe"
        changed.write_bytes(b"another compiler revision")
        label, warnings = self.import_from(self.installed)
        self.assertEqual(label, "6.02.02")
        self.assertIn("Warning:", warnings)
        imported = self.root / "imports" / label
        description = toolchain.describe(imported, self.catalog)
        self.assertIsNone(description["qualified_version"])
        self.assertEqual(
            description["files"]["bin/c38mid.exe"], hashlib.sha256(changed.read_bytes()).hexdigest()
        )
        changed_import = imported / "bin/c38mid.exe"
        changed_import.write_bytes(b"updated again")
        self.assertNotEqual(
            toolchain.describe(imported, self.catalog)["files"], description["files"]
        )

    def test_extra_compiler_file_warns_and_remains_available(self):
        (self.installed / "bin/extra.dll").write_bytes(b"additional runtime dependency")
        label, warnings = self.import_from(self.installed)
        self.assertIn("Warning:", warnings)
        installed = self.root / "imports" / label
        self.assertIsNone(toolchain.describe(installed, self.catalog)["qualified_version"])
        self.assertTrue((installed / "bin/extra.dll").exists())

    def test_extra_files_are_retained_and_unused_catalog_headers_are_optional(self):
        self.catalog["6.02.02"]["files"]["include/iostream"] = "0" * 64
        extra = self.installed / "bin/optional.pak"
        extra.write_bytes(b"optional pack")
        label, warnings = self.import_from(self.installed)
        self.assertIn("Warning:", warnings)
        self.assertEqual(
            (self.root / "imports" / label / "bin/optional.pak").read_bytes(), b"optional pack"
        )

    def test_incomplete_replacement_preserves_previous_installation(self):
        label, _ = self.import_from(self.installed)
        previous = self.root / "imports" / label
        recorded = (previous / "toolchain.json").read_bytes()
        (self.installed / "bin/c38mid.exe").unlink()
        with self.assertRaisesRegex(InputError, "missing bin/c38mid.exe"):
            self.import_from(self.installed)
        self.assertEqual((previous / "toolchain.json").read_bytes(), recorded)
        self.assertEqual(sorted(path.name for path in previous.parent.iterdir()), [label])

    def test_failed_rename_restores_previous_installation(self):
        label, _ = self.import_from(self.installed)
        previous = self.root / "imports" / label
        recorded = (previous / "toolchain.json").read_bytes()
        real_replace = Path.replace

        def fail_commit(path, destination):
            if path.name == "suite":
                raise OSError("fixture rename failure")
            return real_replace(path, destination)

        with patch.object(Path, "replace", fail_commit):
            with self.assertRaisesRegex(OSError, "fixture rename failure"):
                self.import_from(self.installed)
        self.assertEqual((previous / "toolchain.json").read_bytes(), recorded)
        self.assertEqual(sorted(path.name for path in previous.parent.iterdir()), [label])

    def test_failed_rollback_keeps_a_recoverable_backup(self):
        label, _ = self.import_from(self.installed)
        previous = self.root / "imports" / label
        recorded = (previous / "toolchain.json").read_bytes()
        real_replace = Path.replace

        def fail_commit_and_restore(path, destination):
            if path.name in {"suite", "previous"}:
                raise OSError("fixture rename failure")
            return real_replace(path, destination)

        warnings = io.StringIO()
        with patch.object(Path, "replace", fail_commit_and_restore), redirect_stderr(warnings):
            with self.assertRaises(OSError):
                toolchain.import_compiler(self.installed, self.catalog, previous.parent)
        backups = list(previous.parent.glob(".import-*/previous/toolchain.json"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), recorded)
        self.assertIn("Previous compiler files remain", warnings.getvalue())

    def test_suite_selection_does_not_mix_two_installations(self):
        other = self.installed.parent / "other/bin"
        other.mkdir(parents=True)
        (other / "ch38.exe").write_bytes(b"another installation")
        with self.assertRaisesRegex(InputError, "single compiler suite"):
            self.import_from(self.root / "HEW")
        self.assertEqual(self.import_from(self.installed)[0], "6.02.02")

    def test_custom_name_and_case_insensitive_input(self):
        (self.installed / "bin/ch38.exe").write_bytes(b"authored compiler without a version banner")
        (self.installed / "bin").rename(self.installed / "BIN")
        (self.installed / "BIN/ch38.exe").rename(self.installed / "BIN/CH38.EXE")
        with self.assertRaisesRegex(InputError, "--name"):
            self.import_from(self.installed)
        label, warnings = self.import_from(self.installed, "custom-build")
        self.assertEqual(label, "custom-build")
        self.assertIn("Warning:", warnings)
        self.assertTrue((self.root / "imports/custom-build/bin/ch38.exe").is_file())

    def test_label_validation_is_portable(self):
        for value in (
            "../outside",
            "x/y",
            "x\\y",
            "a:b",
            ".",
            "..",
            "CON",
            "nul.txt",
            "a.",
            "a" * 65,
        ):
            with self.subTest(value=value), self.assertRaises(InputError):
                self.import_from(self.installed, value)
        self.assertFalse((self.root / "imports").exists())

    def test_unfamiliar_updater_hash_is_allowed(self):
        entries = [("H8/current/" + name, data) for name, data in self.files.items()]
        updater = self.root / "h8v6202u.exe"
        updater.write_bytes(updater_fixture(entries))
        label, warnings = self.import_from(updater)
        self.assertEqual(label, "6.02.02")
        self.assertIn("package hash is unfamiliar", warnings)
        self.assertEqual(
            toolchain.describe(self.root / "imports" / label, self.catalog)["qualified_version"],
            label,
        )

    def test_zip_selects_h8_updater_and_does_not_choose_setup_exe(self):
        entries = [("H8/current/" + name, data) for name, data in self.files.items()]
        bundle_path = self.root / "compiler.zip"
        with zipfile.ZipFile(bundle_path, "w") as bundle:
            bundle.writestr("setup.exe", b"authored unrelated executable")
            bundle.writestr("bundle/h8v6202u.exe", updater_fixture(entries))
        self.assertEqual(self.import_from(bundle_path)[0], "6.02.02")
        with zipfile.ZipFile(bundle_path, "w") as bundle:
            bundle.writestr("setup.exe", b"authored unrelated executable")
        with self.assertRaisesRegex(InputError, "one H8 updater"):
            self.import_from(bundle_path)

    def test_unknown_duplicate_copies_are_reported(self):
        entries = [("H8/current/" + name, data) for name, data in self.files.items()]
        entries.append(("H8/current/bin/optlnk.exe", b"alternate linker"))
        data = updater_fixture(entries)
        self.assertEqual(
            installshield.compiler_members(data, self.catalog)["bin/optlnk.exe"],
            self.files["bin/optlnk.exe"],
        )
        self.catalog["6.02.02"]["files"]["bin/optlnk.exe"] = "0" * 64
        with self.assertRaisesRegex(InputError, "conflicting copies"):
            installshield.compiler_members(data, self.catalog)

    def test_archive_ranges_paths_and_expansion_limits(self):
        entries = [("H8/current/" + name, data) for name, data in self.files.items()]
        good = updater_fixture(entries)
        for data in (b"invalid", good[:100], good[:-3]):
            with self.subTest(size=len(data)), self.assertRaises(InputError):
                installshield.compiler_members(data, self.catalog)
        entries.append(("H8/current/bin/../../outside", b"bad path"))
        with self.assertRaisesRegex(InputError, "archive path"):
            installshield.compiler_members(updater_fixture(entries), self.catalog)
        with patch.object(installshield, "MAX_EXPANDED_BYTES", 8):
            with self.assertRaisesRegex(InputError, "size limit"):
                installshield.compiler_members(good, self.catalog)

    def test_cabinet_integrity_is_checked_independently_of_qualification(self):
        entries = [("H8/current/" + name, data) for name, data in self.files.items()]
        data = updater_fixture(entries)
        damaged = data[:-1] + bytes([data[-1] ^ 1])
        with self.assertRaisesRegex(InputError, "Cabinet checksum failed"):
            installshield.compiler_members(damaged, self.catalog)

    def test_compressed_blocks_require_exact_declared_size(self):
        expected = b"authored compressed fixture" * 30
        encoder = zlib.compressobj(wbits=-15)
        compressed = encoder.compress(expected) + encoder.flush()
        stored = len(compressed).to_bytes(2, "little") + compressed
        self.assertEqual(installshield.decompress_blocks(stored, len(expected)), expected)
        for data, size in (
            (stored, len(expected) - 1),
            (stored, len(expected) + 1),
            (stored[:-2], len(expected)),
            (b"\0\0", 1),
            (b"\1", 1),
        ):
            with self.subTest(size=size), self.assertRaises(InputError):
                installshield.decompress_blocks(data, size)


if __name__ == "__main__":
    unittest.main()
