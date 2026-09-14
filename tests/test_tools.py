"""Exercise utility contracts with authored input data."""

import hashlib
import io
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest.mock import patch

from tools.common import InputError, image_status, validate_rom, write_bytes
from tools.host import download
from tools.installshield import decompress_blocks


class UtilityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def test_rom_identity_includes_length_and_content(self):
        data = b"authored firmware fixture"
        path = self.root / "rom.bin"
        path.write_bytes(data)
        target = {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        self.assertEqual(validate_rom(path, target), data)
        for wrong in (data[:-1], data + b"\0", b"x" + data[1:]):
            path.write_bytes(wrong)
            with self.assertRaises(InputError):
                validate_rom(path, target)

    def test_image_status_uses_size_and_hash_without_a_rom_file(self):
        data = bytes(range(32))
        target = {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        self.assertTrue(image_status(data, target)["matches_retail"])
        for wrong in (data[:-1], data + b"\0", data[1:] + data[:1]):
            self.assertFalse(image_status(wrong, target)["matches_retail"])

    def test_installshield_blocks_require_exact_expanded_size(self):
        data = b"authored compressed fixture" * 30
        chunks = []
        for block in (data[:100], data[100:]):
            encoder = zlib.compressobj(wbits=-15)
            compressed = encoder.compress(block) + encoder.flush()
            chunks.append(len(compressed).to_bytes(2, "little") + compressed)
        stored = b"".join(chunks)
        self.assertEqual(decompress_blocks(stored, len(data)), data)
        for bad, size in (
            (stored, len(data) - 1),
            (stored, len(data) + 1),
            (stored[:-2], len(data)),
            (b"\0\0", 1),
            (b"\1", 1),
        ):
            with self.assertRaises(InputError):
                decompress_blocks(bad, size)

    def test_atomic_file_replacement(self):
        path = self.root / "with spaces/data.bin"
        write_bytes(path, b"first")
        write_bytes(path, b"second")
        self.assertEqual(path.read_bytes(), b"second")
        self.assertEqual(list(path.parent.iterdir()), [path])

    def test_download_hash_failure_preserves_existing_tool(self):
        path = self.root / "wibo"
        path.write_bytes(b"existing")
        with patch("tools.host.urllib.request.urlopen", return_value=io.BytesIO(b"wrong")):
            with self.assertRaises(InputError):
                download("https://example.invalid/tool", "0" * 64, path)
        self.assertEqual(path.read_bytes(), b"existing")
        self.assertEqual(list(self.root.iterdir()), [path])
        data = b"authored tool fixture"
        with patch("tools.host.urllib.request.urlopen", return_value=io.BytesIO(data)):
            download("https://example.invalid/tool", hashlib.sha256(data).hexdigest(), path)
        self.assertEqual(path.read_bytes(), data)


if __name__ == "__main__":
    unittest.main()
