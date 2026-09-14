"""Exercise artwork ordering, editable inputs, and extraction recovery."""

import hashlib
import re
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from PIL import Image

from assets.placeholders import draw as placeholder_artwork
from tools import assets
from tools.common import InputError, read_json, write_bytes


class AssetTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def asset(self, name="image", width=16, height=16, offset=0, font=False):
        return {
            "name": name,
            "file": name + (".bmp" if font else ".ncg"),
            "width": width,
            "height": height,
            "offset": offset,
            "size": width * height // (8 if font else 4),
            "format": "font-1bpp" if font else "column-2bpp",
        }

    def rom(self, data):
        path = self.root / "retail.bin"
        path.write_bytes(data)
        return path, {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}

    def save(self, directory, asset, image):
        path = self.root / directory / asset["file"]
        write_bytes(path, assets.image_bytes(image, path.suffix))
        return path

    def test_asymmetric_ncg_tiles_pack_into_vertical_firmware_pages(self):
        # Four tiles distinguish horizontal nibble order and both tile axes.
        tile_bytes = bytearray(128)
        tile_bytes[0] = 0x12
        tile_bytes[32] = 0x03
        tile_bytes[64] = 0x01
        tile_bytes[124] = 0x20
        character = struct.pack("<III", 2, 2, 0) + tile_bytes
        data = assets.container(b"NCCG", [(b"CHAR", character)])
        path = self.root / "asymmetric.ncg"
        path.write_bytes(data)
        image = assets.read_image(path)
        self.assertEqual(image.getpixel((0, 0)), 85)
        self.assertEqual(image.getpixel((1, 0)), 170)
        self.assertEqual(image.getpixel((8, 0)), 0)
        self.assertEqual(image.getpixel((0, 8)), 170)
        self.assertEqual(image.getpixel((9, 15)), 85)
        self.assertEqual(image.getpixel((15, 9)), 255)

        expected = bytearray(64)
        expected[0], expected[3] = 1, 1
        expected[16], expected[17] = 1, 1
        expected[33], expected[50] = 1, 128
        self.assertEqual(assets.encode(path, self.asset()), expected)
        rewritten = assets.blocks(assets.write_ncg(image), b"NCCG")
        self.assertEqual(rewritten[b"CHAR"], character)

    def test_font_preserves_glyph_order_and_all_eight_rows(self):
        asset = self.asset("font", 108, 8, font=True)
        data = bytearray(108)
        data[:6] = bytes((1, 128, 85, 3, 64, 170))
        data[-3:] = bytes((255, 129, 24))
        image = assets.decode(data, asset)
        self.assertEqual(image.getpixel((0, 0)), 0)
        self.assertEqual(image.getpixel((1, 7)), 0)
        self.assertEqual(image.getpixel((3, 1)), 0)
        self.assertEqual(image.getpixel((4, 6)), 0)
        self.assertEqual(image.getpixel((106, 7)), 0)
        self.assertEqual(image.getpixel((107, 3)), 0)
        self.assertEqual(image.getpixel((107, 0)), 255)
        path = self.save("", asset, image)
        self.assertEqual(struct.unpack_from("<H", path.read_bytes(), 28)[0], 1)
        self.assertEqual(assets.encode(path, asset), data)

    def test_generated_arrays_use_individual_fallbacks_and_keep_manifest_order(self):
        first = self.asset("first", 3, 8, font=True)
        second = self.asset("second", 3, 8, font=True)
        self.save("assets/placeholders", first, assets.decode(b"\x01\x02\x04", first))
        self.save("assets/placeholders", second, assets.decode(b"\x08\x10\x20", second))
        self.save("assets/local", first, assets.decode(b"\x80\x40\x20", first))
        output, fallback = assets.generate(self.root, [first, second])
        self.assertEqual(fallback, ["second"])
        packed = bytes(int(value, 16) for value in re.findall(rb"0x([0-9A-F]{2})", output))
        self.assertEqual(packed, b"\x80\x40\x20\x08\x10\x20")
        (self.root / "assets/local/first.bmp").write_bytes(b"broken image")
        with self.assertRaisesRegex(InputError, "first.bmp"):
            assets.generate(self.root, [first, second])

    def test_extract_once_then_build_without_the_rom(self):
        sprite = self.asset("sprite", 8, 8, offset=4)
        font = self.asset("font", 3, 8, offset=20, font=True)
        data = bytes(range(24))
        path, target = self.rom(data)
        outputs = assets.extract(path, self.root, [sprite, font], target)
        self.assertEqual(len(outputs), 3)
        path.unlink()
        output, fallback = assets.generate(self.root, [sprite, font])
        packed = bytes(int(value, 16) for value in re.findall(rb"0x([0-9A-F]{2})", output))
        self.assertEqual(packed, data[4:23])
        self.assertEqual(fallback, [])

    def test_force_replaces_only_expected_files_and_protects_edits_by_default(self):
        asset = self.asset("font", 3, 8, font=True)
        path, target = self.rom(b"\x01\x02\x04")
        assets.extract(path, self.root, [asset], target)
        local = self.root / "assets/local"
        (local / "font.bmp").write_bytes(b"edited artwork")
        (local / "notes.txt").write_text("keep my notes")
        with self.assertRaisesRegex(InputError, "--force"):
            assets.extract(path, self.root, [asset], target)
        self.assertEqual((local / "font.bmp").read_bytes(), b"edited artwork")
        assets.extract(path, self.root, [asset], target, force=True)
        self.assertEqual(assets.encode(local / "font.bmp", asset), b"\x01\x02\x04")
        self.assertEqual((local / "notes.txt").read_text(), "keep my notes")

    def test_extraction_failure_restores_existing_directory(self):
        asset = self.asset("font", 3, 8, font=True)
        path, target = self.rom(b"\x01\x02\x04")
        assets.extract(path, self.root, [asset], target)
        local = self.root / "assets/local"
        (local / "font.bmp").write_bytes(b"edited artwork")
        rename = Path.rename

        def fail_install(source, destination):
            if source.name == "local" and source.parent.name.startswith(".extract-"):
                raise OSError("authored installation failure")
            return rename(source, destination)

        with patch.object(Path, "rename", fail_install):
            with self.assertRaisesRegex(OSError, "installation failure"):
                assets.extract(path, self.root, [asset], target, force=True)
        self.assertEqual((local / "font.bmp").read_bytes(), b"edited artwork")
        self.assertEqual(list(local.parent.glob(".extract-*")), [])

    def test_invalid_input_does_not_create_assets(self):
        asset = self.asset("font", 3, 8, font=True)
        path, target = self.rom(b"\x01\x02\x04")
        path.write_bytes(b"\x01\x02\x05")
        with self.assertRaises(InputError):
            assets.extract(path, self.root, [asset], target)
        self.assertFalse((self.root / "assets").exists())

    def test_failed_restore_keeps_previous_artwork_for_recovery(self):
        asset = self.asset("font", 3, 8, font=True)
        path, target = self.rom(b"\x01\x02\x04")
        assets.extract(path, self.root, [asset], target)
        local = self.root / "assets/local"
        (local / "font.bmp").write_bytes(b"edited artwork")
        rename = Path.rename

        def fail_install_and_restore(source, destination):
            if source.parent.name.startswith(".extract-"):
                raise OSError("authored file lock")
            return rename(source, destination)

        with patch.object(Path, "rename", fail_install_and_restore):
            with self.assertRaisesRegex(InputError, "previous artwork is preserved"):
                assets.extract(path, self.root, [asset], target, force=True)
        backups = list(local.parent.glob(".extract-*/previous/font.bmp"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), b"edited artwork")

    def test_bitmap_errors_identify_the_file_and_pixel(self):
        asset = self.asset("font", 3, 8, font=True)
        path = self.root / "font.bmp"
        image = Image.new("RGB", (3, 8), "white")
        image.putpixel((1, 6), (255, 0, 0))
        image.save(path)
        with self.assertRaisesRegex(InputError, r"font.bmp: Pixel \(1, 6\)"):
            assets.encode(path, asset)
        Image.new("1", (4, 8)).save(path)
        with self.assertRaisesRegex(InputError, "expected 3 x 8"):
            assets.encode(path, asset)

    def test_ncg_rejects_truncation_depth_and_unrepresentable_indices(self):
        original = assets.write_ncg(Image.new("L", (8, 8), 255))
        for damaged in (original[:8], original[:-1], original + b"\0"):
            with self.assertRaises(InputError):
                assets.read_ncg(damaged)
        character = bytearray(assets.blocks(original, b"NCCG")[b"CHAR"])
        character[8] = 1
        with self.assertRaisesRegex(InputError, "4bpp"):
            assets.read_ncg(assets.container(b"NCCG", [(b"CHAR", character)]))
        character[8], character[12] = 0, 0x40
        with self.assertRaisesRegex(InputError, r"pixel \(1, 0\).*index 4"):
            assets.read_ncg(assets.container(b"NCCG", [(b"CHAR", character)]))

    def test_ncg_bmp_editing_round_trip_and_overwrite_guard(self):
        source = self.root / "source.ncg"
        bitmap = self.root / "edit.bmp"
        destination = self.root / "edited.ncg"
        image = Image.new("L", (8, 8), 255)
        for x, value in enumerate((0, 85, 170, 255)):
            image.putpixel((x, 7), value)
        source.write_bytes(assets.write_ncg(image))
        assets.convert(source, bitmap)
        assets.convert(bitmap, destination)
        self.assertEqual(assets.read_image(destination).tobytes(), image.tobytes())
        with self.assertRaisesRegex(InputError, "--force"):
            assets.convert(bitmap, destination)
        self.assertTrue((self.root / "grayscale.ncl").is_file())

    def test_bundled_placeholders_satisfy_every_firmware_extent(self):
        manifest = read_json(assets.ROOT / "config/artwork.json")
        for asset in manifest:
            path = assets.ROOT / "assets/placeholders" / asset["file"]
            self.assertEqual(len(assets.encode(path, asset)), asset["size"], asset["file"])

    def test_bundled_and_regenerated_walker_preserve_the_rest_note_divisor(self):
        manifest = read_json(assets.ROOT / "config/artwork.json")
        walker = next(asset for asset in manifest if asset["name"] == "walkerImage")
        with patch.object(placeholder_artwork, "DIRECTORY", self.root):
            placeholder_artwork.draw()
        for directory in (assets.ROOT / "assets/placeholders", self.root):
            with self.subTest(directory=directory):
                data = assets.encode(directory / walker["file"], walker)
                # NOTE_REST indexes byte 125 of the resident object, after
                # 42 pitch entries. The resulting divisor controls rest timing.
                self.assertEqual(data[125 - 42], 2)

    def test_required_bytes_protect_audio_timing_in_local_and_fallback_artwork(self):
        asset = self.asset("sprite", 8, 8)
        asset["required_bytes"] = [
            {"offset": 1, "value": 2, "purpose": "rest-note duration divisor"}
        ]
        for directory in ("assets/placeholders", "assets/local"):
            for value in (0, 3):
                with self.subTest(directory=directory, value=value):
                    data = bytes((0, value)) + bytes(14)
                    path = self.save(directory, asset, assets.decode(data, asset))
                    with self.assertRaisesRegex(InputError, "0x01.*0x02.*rest-note"):
                        assets.generate(self.root, [asset])
            data = b"\x80\x02\x04" + bytes(13)
            path = self.save(directory, asset, assets.decode(data, asset))
            output, _ = assets.generate(self.root, [asset])
            packed = bytes(int(value, 16) for value in re.findall(rb"0x([0-9A-F]{2})", output))
            self.assertEqual(packed, data)
            path.unlink()

    def test_required_bytes_must_fit_the_declared_asset(self):
        asset = self.asset("sprite", 8, 8)
        self.save("assets/local", asset, assets.decode(bytes(16), asset))
        for offset, value in ((-1, 2), (16, 2), (1, 256)):
            with self.subTest(offset=offset, value=value):
                asset["required_bytes"] = [
                    {"offset": offset, "value": value, "purpose": "timing fixture"}
                ]
                with self.assertRaisesRegex(InputError, "invalid required byte"):
                    assets.generate(self.root, [asset])


if __name__ == "__main__":
    unittest.main()
