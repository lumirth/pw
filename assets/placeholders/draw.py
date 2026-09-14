"""Draw the original placeholder images from simple shapes and tiny glyphs.

Run from the repository root with: uv run python -m assets.placeholders.draw
The generated NCG and BMP files are ordinary inputs used by the build.
"""

from pathlib import Path

from PIL import Image, ImageDraw

from tools.assets import image_bytes, palette

DIRECTORY = Path(__file__).resolve().parent

# Each slash starts a new row in a three-column glyph. Remaining rows stay blank.
GLYPHS = {
    "0": "###/#.#/#.#/#.#/###",
    "1": ".#./##./.#./.#./###",
    "2": "##./..#/.#./#../###",
    "3": "##./..#/.#./..#/##.",
    "4": "#.#/#.#/###/..#/..#",
    "5": "###/#../##./..#/##.",
    "6": ".##/#../###/#.#/###",
    "7": "###/..#/.#./.#./.#.",
    "8": "###/#.#/###/#.#/###",
    "9": "###/#.#/###/..#/##.",
    "A": ".#./#.#/###/#.#/#.#",
    "B": "##./#.#/##./#.#/##.",
    "C": ".##/#../#../#../.##",
    "D": "##./#.#/#.#/#.#/##.",
    "E": "###/#../##./#../###",
    "F": "###/#../##./#../#..",
    "G": ".##/#../#.#/#.#/.##",
    "H": "#.#/#.#/###/#.#/#.#",
    "I": "###/.#./.#./.#./###",
    "J": "..#/..#/..#/#.#/.#.",
    "K": "#.#/#.#/##./#.#/#.#",
    "L": "#../#../#../#../###",
    "M": "#.#/###/###/#.#/#.#",
    "N": "#.#/###/###/###/#.#",
    "O": ".#./#.#/#.#/#.#/.#.",
    "P": "##./#.#/##./#../#..",
    "Q": ".#./#.#/#.#/.#./..#",
    "R": "##./#.#/##./#.#/#.#",
    "S": ".##/#../.#./..#/##.",
    "T": "###/.#./.#./.#./.#.",
    "U": "#.#/#.#/#.#/#.#/.#.",
    "V": "#.#/#.#/#.#/.#./.#.",
    "W": "#.#/#.#/###/###/#.#",
    "X": "#.#/#.#/.#./#.#/#.#",
    "Y": "#.#/#.#/.#./.#./.#.",
    "Z": "###/..#/.#./#../###",
}


def save(image: Image.Image, filename: str) -> None:
    path = DIRECTORY / filename
    path.write_bytes(image_bytes(image, path.suffix))


def rows_image(rows: tuple[str, ...]) -> Image.Image:
    image = Image.new("L", (len(rows[0]), len(rows)), 255)
    for y, row in enumerate(rows):
        for x, pixel in enumerate(row):
            if pixel == "#":
                image.putpixel((x, y), 0)
    return image


def draw() -> None:
    device = Image.new("L", (32, 32), 255)
    pen = ImageDraw.Draw(device)
    pen.rectangle((4, 2, 27, 29), fill=170, outline=0)
    pen.rectangle((7, 6, 24, 17), fill=255, outline=0)
    # Walker byte 0x53 also supplies the rest-note divisor. This pixel keeps it at 2.
    pen.point((9, 9), fill=170)
    for x in (8, 14, 20):
        pen.rectangle((x, 21, x + 3, 24), fill=85, outline=0)
    save(device, "walker.ncg")

    for expression in ("neutral", "smile", "frown"):
        face = Image.new("L", (16, 8), 255)
        pen = ImageDraw.Draw(face)
        pen.line((4, 1, 4, 2), fill=0)
        pen.line((11, 1, 11, 2), fill=0)
        mouths = {
            "neutral": ((5, 5), (10, 5)),
            "smile": ((4, 4), (6, 6), (9, 6), (11, 4)),
            "frown": ((4, 6), (6, 4), (9, 4), (11, 6)),
        }
        pen.line(mouths[expression], fill=0)
        save(face, f"face-{expression}.ncg")

    save(
        rows_image(
            (
                "...##...",
                "..####..",
                ".######.",
                "########",
                "...##...",
                "...##...",
                "...##...",
                "........",
            )
        ),
        "button-arrow.ncg",
    )
    save(
        rows_image(
            (
                "........",
                ".######.",
                "#......#",
                "..####..",
                ".#....#.",
                "...##...",
                "........",
                "...##...",
            )
        ),
        "ir-signal.ncg",
    )

    font = Image.new("L", (36 * 3, 8), 255)
    for cell, character in enumerate("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"):
        glyph = rows_image(tuple(GLYPHS[character].split("/")))
        font.paste(glyph, (cell * 3, 1))
    save(font, "font.bmp")
    (DIRECTORY / "grayscale.ncl").write_bytes(palette())


if __name__ == "__main__":
    draw()
