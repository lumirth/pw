"""Extract editable artwork files and convert them into firmware arrays."""

from __future__ import annotations

import argparse
import io
import shutil
import struct
import tempfile
from pathlib import Path

from PIL import Image

from .common import ROOT, InputError, read_json, validate_rom, write_bytes

PAGE_HEIGHT = 8
SHADES = (255, 170, 85, 0)
# NCL editors expand five-bit channels to these neighboring display values.
NCG_SHADES = {255: 0, 170: 1, 173: 1, 85: 2, 82: 2, 0: 3}


def layout(asset: dict) -> tuple[int, int, int]:
    """Check that the image dimensions account for every stored bit."""
    planes = {"column-2bpp": 2, "font-1bpp": 1}.get(asset["format"])
    if planes is None:
        raise InputError(f"{asset['name']}: unknown artwork format {asset['format']}.")
    width, height = asset["width"], asset["height"]
    if width <= 0 or height <= 0 or height % PAGE_HEIGHT:
        raise InputError(f"{asset['name']}: artwork must contain complete eight-row pages.")
    if width * height * planes // 8 != asset["size"]:
        raise InputError(f"{asset['name']}: dimensions disagree with the stored size.")
    return width, height, planes


def decode(data: bytes, asset: dict) -> Image.Image:
    """Expand the firmware's vertical bit columns into grayscale pixels."""
    width, height, planes = layout(asset)
    if len(data) != asset["size"]:
        raise InputError(f"{asset['name']}: incomplete artwork data.")
    pixels = Image.new("L", (width, height), SHADES[0])
    position = 0
    for page_y in range(0, height, PAGE_HEIGHT):
        for x in range(width):
            column = data[position : position + planes]
            position += planes
            for bit in range(PAGE_HEIGHT):
                value = 0
                for plane in column:
                    value = (value << 1) | ((plane >> bit) & 1)
                shade = SHADES[value] if planes == 2 else (255, 0)[value]
                pixels.putpixel((x, page_y + bit), shade)
    return pixels


def pixel_values(image: Image.Image, planes: int) -> list[int]:
    """Require colors that have an exact firmware representation."""
    pixels = image.convert("RGBA")
    shades = NCG_SHADES if planes == 2 else {255: 0, 0: 1}
    values = []
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue, alpha = pixels.getpixel((x, y))
            if alpha != 255 or red != green or red != blue or red not in shades:
                allowed = ", ".join(str(shade) for shade in shades)
                raise InputError(
                    f"Pixel ({x}, {y}) must be opaque grayscale with value "
                    f"{allowed}. Disable antialiasing when editing."
                )
            values.append(shades[red])
    return values


def container(magic: bytes, blocks: list[tuple[bytes, bytes]]) -> bytes:
    """Write a little-endian Nitro intermediate file with aligned blocks."""
    body = bytearray()
    for tag, payload in blocks:
        payload += bytes(-len(payload) % 4)
        body.extend(struct.pack("<4sI", tag, len(payload) + 8))
        body.extend(payload)
    header = struct.pack("<4sHHIHH", magic, 0xFEFF, 0x0100, len(body) + 16, 16, len(blocks))
    return header + body


def blocks(data: bytes, magic: bytes) -> dict[bytes, bytes]:
    if len(data) < 16:
        raise InputError("Incomplete NCG file header.")
    signature, bom, version, size, position, count = struct.unpack_from("<4sHHIHH", data)
    if signature != magic or bom != 0xFEFF or version != 0x0100 or position != 16:
        raise InputError("Expected an NCCG version 1.0 file with little-endian blocks.")
    if size != len(data):
        raise InputError("NCG file length disagrees with its header.")
    result = {}
    for _ in range(count):
        if position + 8 > len(data):
            raise InputError("Incomplete NCG block header.")
        tag, size = struct.unpack_from("<4sI", data, position)
        if size < 8 or size % 4 or position + size > len(data):
            raise InputError("NCG block has an invalid length.")
        if tag in result:
            raise InputError(f"NCG file contains a duplicate {tag!r} block.")
        result[tag] = data[position + 8 : position + size]
        position += size
    if position != len(data):
        raise InputError("NCG file has data outside its declared blocks.")
    return result


def read_ncg(data: bytes) -> Image.Image:
    """Read four-bit 8x8 tiles, with the left pixel in each low nibble."""
    sections = blocks(data, b"NCCG")
    characters = sections.get(b"CHAR", b"")
    if len(characters) < 12:
        raise InputError("NCG file needs a CHAR block with tile dimensions.")
    columns, rows, depth = struct.unpack_from("<III", characters)
    if depth != 0:
        raise InputError("Use 4bpp NCG tiles and grayscale palette indices 0 through 3.")
    if columns == 0 or rows == 0 or len(characters) != 12 + columns * rows * 32:
        raise InputError("NCG tile dimensions disagree with its pixel data.")
    attributes = sections.get(b"ATTR")
    if attributes is not None:
        if len(attributes) < 8 + columns * rows:
            raise InputError("Incomplete NCG tile attributes.")
        if struct.unpack_from("<II", attributes) != (columns, rows):
            raise InputError("NCG attribute dimensions disagree with the image.")
        if any(attributes[8 : 8 + columns * rows]):
            raise InputError("Use palette zero for every NCG tile.")
    image = Image.new("L", (columns * 8, rows * 8), 255)
    tiles = characters[12:]
    for tile in range(columns * rows):
        tile_x, tile_y = (tile % columns) * 8, (tile // columns) * 8
        for y in range(8):
            for x in range(8):
                packed = tiles[tile * 32 + y * 4 + x // 2]
                value = (packed >> ((x % 2) * 4)) & 15
                if value > 3:
                    raise InputError(
                        f"NCG pixel ({tile_x + x}, {tile_y + y}) uses palette "
                        f"index {value}; use indices 0 through 3."
                    )
                image.putpixel((tile_x + x, tile_y + y), SHADES[value])
    return image


def write_ncg(image: Image.Image) -> bytes:
    """Create NCG artwork with fresh palette-zero attributes and a palette link."""
    width, height = image.size
    if width == 0 or height == 0 or width % 8 or height % 8:
        raise InputError("NCG dimensions must be positive multiples of eight pixels.")
    values = pixel_values(image, 2)
    tiles = bytearray()
    for tile_y in range(0, height, 8):
        for tile_x in range(0, width, 8):
            for y in range(8):
                for x in range(0, 8, 2):
                    position = (tile_y + y) * width + tile_x + x
                    tiles.append(values[position] | (values[position + 1] << 4))
    columns, rows = width // 8, height // 8
    return container(
        b"NCCG",
        [
            (b"CHAR", struct.pack("<III", columns, rows, 0) + tiles),
            (b"ATTR", struct.pack("<II", columns, rows) + bytes(columns * rows)),
            (b"LINK", b"grayscale.ncl\0"),
        ],
    )


def palette() -> bytes:
    """Provide a four-shade editor palette in the first four 16-color slots."""
    colors = [0x7FFF, 0x56B5, 0x294A, 0x0000] + [0] * 12
    return container(b"NCCL", [(b"PALT", struct.pack("<II16H", 16, 1, *colors))])


def read_image(path: Path) -> Image.Image:
    try:
        if path.suffix.lower() == ".ncg":
            return read_ncg(path.read_bytes())
        if path.suffix.lower() != ".bmp":
            raise InputError("Use .ncg artwork or a .bmp bitmap.")
        with Image.open(path) as source:
            if source.format != "BMP":
                raise InputError("Save this image in BMP format.")
            return source.convert("RGBA")
    except (OSError, InputError) as error:
        raise InputError(f"{path}: {error}") from error


def image_bytes(image: Image.Image, suffix: str) -> bytes:
    if suffix.lower() == ".ncg":
        return write_ncg(image)
    if suffix.lower() != ".bmp":
        raise InputError("Use an .ncg or .bmp output filename.")
    stream = io.BytesIO()
    grayscale = image.convert("L")
    colors = {value for count, value in grayscale.getcolors() or []}
    bitmap = image.convert("RGB")
    if colors <= {0, 255}:
        bitmap = grayscale.convert("1", dither=Image.Dither.NONE)
    bitmap.save(stream, format="BMP")
    return stream.getvalue()


def encode(path: Path, asset: dict) -> bytes:
    """Pack an artwork file into the firmware's page and column order."""
    width, height, planes = layout(asset)
    image = read_image(path)
    if image.size != (width, height):
        raise InputError(
            f"{path}: expected {width} x {height} pixels; found {image.width} x {image.height}."
        )
    try:
        values = pixel_values(image, planes)
    except InputError as error:
        raise InputError(f"{path}: {error}") from error
    data = bytearray()
    for page_y in range(0, height, PAGE_HEIGHT):
        for x in range(width):
            for plane in range(planes - 1, -1, -1):
                column = 0
                for bit in range(PAGE_HEIGHT):
                    value = values[(page_y + bit) * width + x]
                    column |= ((value >> plane) & 1) << bit
                data.append(column)
    for requirement in asset.get("required_bytes", []):
        offset, value = requirement["offset"], requirement["value"]
        if not 0 <= offset < len(data) or not 0 <= value <= 255:
            raise InputError(f"{asset['name']}: invalid required byte in the artwork manifest.")
        if data[offset] != value:
            raise InputError(
                f"{path}: packed byte 0x{offset:02X} must be 0x{value:02X} "
                f"({requirement['purpose']}); found 0x{data[offset]:02X}. "
                "See assets/README.md for the affected pixels."
            )
    return bytes(data)


def resolved_inputs(root: Path, manifest: list[dict]) -> dict[str, Path]:
    """Choose each local image when present, otherwise its bundled placeholder."""
    inputs = {}
    for asset in manifest:
        filename = asset["file"]
        if Path(filename).name != filename or Path(filename).suffix not in (".ncg", ".bmp"):
            raise InputError(f"{asset['name']}: expected a plain NCG or BMP filename.")
        local = root / "assets/local" / filename
        if local.exists() or local.is_symlink():
            inputs[asset["name"]] = local
            continue
        inputs[asset["name"]] = root / "assets/placeholders" / filename
    return inputs


def generate(root: Path, manifest: list[dict]) -> tuple[bytes, list[str]]:
    """Return the C aggregate initializer and names using placeholder images."""
    inputs = resolved_inputs(root, manifest)
    lines = ["/* Generated from editable artwork files. */"]
    placeholders = []
    for asset in manifest:
        path = inputs[asset["name"]]
        if path.parent == root / "assets/placeholders":
            placeholders.append(asset["name"])
        data = encode(path, asset)
        lines.extend((f"/* {asset['name']} */", "{"))
        for offset in range(0, len(data), 12):
            lines.append(
                "  " + ", ".join(f"0x{value:02X}" for value in data[offset : offset + 12]) + ","
            )
        lines.append("},")
    return ("\n".join(lines) + "\n").encode("ascii"), placeholders


def extract(
    rom_path: Path, root: Path, manifest: list[dict], target: dict, *, force: bool = False
) -> list[Path]:
    """Validate the ROM once, then install the complete extracted artwork set."""
    rom = validate_rom(rom_path, target)
    resolved_inputs(root, manifest)
    directory = root / "assets/local"
    if directory.is_symlink() or (directory.exists() and not directory.is_dir()):
        raise InputError(f"{directory}: use an ordinary directory for local artwork.")
    outputs = [directory / asset["file"] for asset in manifest]
    outputs.append(directory / "grayscale.ncl")
    existing = [path.name for path in outputs if path.exists() or path.is_symlink()]
    if existing and not force:
        raise InputError(
            "Local artwork already exists: "
            + ", ".join(existing)
            + ". Use --force to replace it with the ROM artwork."
        )
    images = {"grayscale.ncl": palette()}
    for asset in manifest:
        start, size = asset["offset"], asset["size"]
        if start < 0 or size <= 0 or start + size > len(rom):
            raise InputError(f"{asset['name']}: artwork range lies outside the ROM.")
        image = decode(rom[start : start + size], asset)
        images[asset["file"]] = image_bytes(image, Path(asset["file"]).suffix)

    directory.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(dir=directory.parent, prefix=".extract-"))
    staged = temporary / "local"
    backup = temporary / "previous"
    installed = False
    try:
        if directory.exists():
            shutil.copytree(directory, staged, symlinks=True)
        for filename, data in images.items():
            write_bytes(staged / filename, data)
        if directory.exists():
            directory.rename(backup)
        staged.rename(directory)
        installed = True
    except (OSError, KeyboardInterrupt):
        if backup.exists():
            try:
                backup.rename(directory)
            except OSError as error:
                raise InputError(
                    f"Could not restore {directory}. Your previous artwork is "
                    f"preserved in {backup}; move it back before retrying."
                ) from error
        raise
    finally:
        if installed or not backup.exists():
            shutil.rmtree(temporary)
    return outputs


def convert(source: Path, destination: Path, *, force: bool = False) -> None:
    if (destination.exists() or destination.is_symlink()) and not force:
        raise InputError(f"{destination} already exists; use --force to replace it.")
    image = read_image(source)
    pixel_values(image, 2)
    write_bytes(destination, image_bytes(image, destination.suffix))
    if destination.suffix.lower() == ".ncg":
        palette_path = destination.parent / "grayscale.ncl"
        if not palette_path.exists():
            write_bytes(palette_path, palette())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    extraction = commands.add_parser("extract", help="save ROM artwork and font as editable files")
    extraction.add_argument("rom", type=Path)
    conversion = commands.add_parser("convert", help="convert between NCG and BMP artwork")
    conversion.add_argument("source", type=Path)
    conversion.add_argument("destination", type=Path)
    for command in (extraction, conversion):
        command.add_argument("--force", action="store_true", help="replace existing output files")
    arguments = parser.parse_args()
    try:
        if arguments.command == "convert":
            convert(
                arguments.source.expanduser(),
                arguments.destination.expanduser(),
                force=arguments.force,
            )
            print(f"Converted {arguments.destination}.")
            return
        manifest = read_json(ROOT / "config/artwork.json")
        target = read_json(ROOT / "config/build.json")["target"]
        extract(arguments.rom.expanduser(), ROOT, manifest, target, force=arguments.force)
    except (InputError, OSError) as error:
        parser.exit(1, f"error: {error}\n")
    except KeyboardInterrupt:
        parser.exit(130, "Interrupted.\n")
    print(
        f"Extracted {len(manifest)} artwork files and an editor palette to {ROOT / 'assets/local'}."
    )
    print("Future builds use these files; the ROM can be moved or removed.")


if __name__ == "__main__":
    main()
