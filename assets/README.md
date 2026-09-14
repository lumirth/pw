# Artwork inputs

The build reads six NCG images and a BMP font sheet. Each file comes from
`assets/local/` when present, with original artwork in `assets/placeholders/`
filling any missing entry. The build reports the placeholders it uses.

Extract the resident artwork and font once:

```sh
uv run python -m tools.assets extract /path/to/retail.bin
```

Extraction checks the ROM's size and SHA-256, then saves the seven images and an
editor palette in the ignored `assets/local/` directory. Future builds read those
files, so the ROM can be moved or removed. Existing files are protected; use
`--force` to replace the extracted images and palette. Other files in the local
directory are preserved. Keep a copy of edited artwork before extracting again.

| File | Size | Contents |
| --- | --- | --- |
| `walker.ncg` | 32 × 32 | Device image used by the connection screens |
| `face-neutral.ncg` | 16 × 8 | Waiting expression |
| `face-smile.ncg` | 16 × 8 | Successful connection expression |
| `face-frown.ncg` | 16 × 8 | Failed connection expression |
| `button-arrow.ncg` | 8 × 8 | Button prompt |
| `ir-signal.ncg` | 8 × 8 | Infrared activity indicator |
| `font.bmp` | 108 × 8 | 36 adjacent 3 × 8 glyphs: `0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ` |
| `grayscale.ncl` | 16 palette entries | Editor palette; the first four entries show the firmware shades |

## Editing

To create custom artwork without a ROM, copy the desired placeholder files and
`grayscale.ncl` into `assets/local/` and edit them there.

NCG and NCL are Nitro intermediate graphics formats supported by
[NitroPaint](https://github.com/Garhoogin/NitroPaint). Keep four-bit tiles,
palette zero, and pixel indices 0 through 3. Those indices select white, light
gray, dark gray, and black. The supplied NCL palette displays this mapping in
an editor.

For an ordinary bitmap editor, convert an image to BMP and back:

```sh
uv run python -m tools.assets convert assets/local/walker.ncg walker.bmp
# Edit walker.bmp, preserving its dimensions and grayscale colors.
uv run python -m tools.assets convert walker.bmp assets/local/walker.ncg --force
```

Use opaque grayscale values **255, 170, 85, and 0**. The converter also accepts
**173 and 82**, the neighboring display values produced by the NCL palette's
five-bit channels. The font uses opaque **255 and 0**. Disable antialiasing while
drawing, and use nearest-neighbor scaling when viewing pixel art. Unsupported
colors produce an error identifying the file and pixel to fix.

The font sheet can be edited directly as a BMP. Every three consecutive columns
form one glyph, and every row of each cell is preserved. The firmware supplies
the blank separator column when drawing text.

### Walker pixels and rest timing

The sound engine's 42 note-period constants are defined in
`src/application/pw_builtin.c`, immediately before the walker image. Rest code
`0x7D` indexes beyond that table into the image and uses packed byte `0x53` as
its duration divisor. That byte is **2** in the retail image. Zero creates a
zero divisor; other values change the duration of rests.

All 256 walker bytes describe image pixels. Byte `0x53` is the low bit plane of
column **9**, rows **8–15**, in the generated firmware packing. To preserve it,
use light gray or black at **(9, 9)** and white or dark gray at **(9, 8)** and
**(9, 10–15)**. Coordinates start at zero.

The placeholder includes a light-gray pixel at (9, 9) for this purpose.
`config/artwork.json` records the required byte, and conversion checks it for
both local and placeholder images. An incompatible edit stops conversion with
the filename, packed offset, and expected value.

## File conversion

[`config/artwork.json`](../config/artwork.json) records each resource's ROM range,
dimensions, firmware packing, and shared audio-byte requirements.
[`tools/assets.py`](../tools/assets.py) performs
extraction and conversion. BMP handling uses
[Pillow](https://pillow.readthedocs.io/en/stable/handbook/image-file-formats.html#bmp).

The sprite files use the `NCCG` signature and a `CHAR` block containing dimensions
in tiles, four-bit color depth, and 8 × 8 tiles. Tiles appear from left to right
and top to bottom; each byte holds the left pixel in its low nibble. The public
[NitroPaint NCG reader](https://github.com/Garhoogin/NitroPaint/blob/be4d73c4843350665e0547590d7449996c6c33e8/NitroPaint/object/NitroCharacter.c#L642)
and [NCL writer](https://github.com/Garhoogin/NitroPaint/blob/be4d73c4843350665e0547590d7449996c6c33e8/NitroPaint/object/NitroPalette.c#L513)
describe these containers.

Extraction reconstructs the sprite pixels from the ROM. Its NCG writer supplies
fresh palette-zero tile attributes and a link to `grayscale.ncl`. That palette is
an editor aid chosen for this project. The BMP font sheet is also a project
representation of the surviving glyph bytes. Original artwork comments, palette
filenames, and the original font authoring format remain unknown.

The generated firmware images consist of eight-row pages, read from top to
bottom. Each page contains columns from left to right. A column stores two bytes:
the high bit plane, then the low bit plane. Bit zero describes the upper pixel.
This is the layout consumed by `DisplayBlit` and `RasterColumn`.

The font stores one byte per column, with bit zero at the top row and a set bit
for black. `DisplayText` reads the 108 column bytes as 36 adjacent glyphs. All
eight rows survive extraction and conversion, including pixels outside the
usual letter height.

Conversion produces the aggregate initializer included by
`src/application/pw_builtin.c`.
The font and all six images retain their byte extents and order. Artwork sent
from the console into serial EEPROM is handled by the firmware's communication
path and remains separate from these resident images.

## Placeholders

The bundled artwork shows a simple rectangular device, facial expressions, a
button arrow, a radio symbol, and small alphanumeric glyphs. Its drawing source
is [`placeholders/draw.py`](placeholders/draw.py). Regenerate the files with:

```sh
uv run python -m assets.placeholders.draw
```

The source and images are [available under CC0](placeholders/LICENSE.md).
Placeholder builds retain the resident image dimensions and glyph cells while
producing a different firmware hash. The firmware continues to obtain its other
artwork through console-provided EEPROM resources.
