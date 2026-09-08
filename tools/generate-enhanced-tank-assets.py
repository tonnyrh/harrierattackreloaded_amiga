"""Generate the first Enhanced-mode OCS tile asset.

The source art deliberately lives as readable 16x16 pixel rows below. PNG
palette indices are the game's existing four-bit world pens, so the preview
shows exactly which registers the runtime asset uses. The .bpl output is
four row-major 8x8 masked tiles. Each scanline contains four colour-plane
bytes followed by one opacity-mask byte, matching
drawGameScrollTileMasked().
"""

import argparse
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = ROOT / "amiga" / "assets" / "enhanced"

# Existing stable game pens from assets/game_palette.pal.
PEN_BY_CHAR = {
    ".": 0,   # transparent
    "L": 2,   # light grey
    "M": 3,   # mid grey
    "D": 4,   # dark grey
    "G": 5,   # green
    "Y": 6,   # yellow highlight
    "g": 8,   # dark green
    "K": 10,  # black outline
    "B": 13,  # bright green
}

PIXELS = (
    "................",
    "................",
    "...........Y....",
    "...........K....",
    ".........KKKK...",
    "...KKKKKKGGGK...",
    "......KGGGGGGK..",
    ".....KGGGBBGGK..",
    "...KKGGGGGGGGKK.",
    "..KGGGGGGGGGGGGK",
    ".KDDDDDDDDDDDDDK",
    "KDLLLLLLLLLLLLDK",
    "KDMDDMDDMDDMDDMK",
    ".KDDDDDDDDDDDDK.",
    "..KKKKKKKKKKKK..",
    "...K.K.K.K.K.K..",
)


def rgb12_to_rgb24(value: int) -> tuple[int, int, int]:
    return tuple(((value >> shift) & 0xF) * 17 for shift in (8, 4, 0))


def game_palette() -> list[int]:
    raw = (ROOT / "amiga" / "assets" / "game_palette.pal").read_bytes()
    values = [int.from_bytes(raw[i : i + 2], "big") for i in range(0, 32, 2)]
    palette: list[int] = []
    for value in values:
        palette.extend(rgb12_to_rgb24(value))
    palette.extend([0] * (768 - len(palette)))
    return palette


def generate(force: bool = False) -> None:
    if len(PIXELS) != 16 or any(len(row) != 16 for row in PIXELS):
        raise ValueError("Enhanced tank source must be exactly 16x16")
    pixels = [PEN_BY_CHAR[char] for row in PIXELS for char in row]
    used = set(pixels)
    allowed = {0, 2, 3, 4, 5, 6, 8, 10, 13}
    if not used <= allowed:
        raise ValueError(f"Unexpected game pens: {sorted(used - allowed)}")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_png = OUTPUT_DIR / "tank_16x16.png"
    if output_png.exists() and not force:
        raise FileExistsError(
            f"Refusing to overwrite edited master {output_png}. Use --force to reset it."
        )
    image = Image.new("P", (16, 16))
    image.putpalette(game_palette())
    image.putdata(pixels)
    image.info["transparency"] = 0
    image.save(output_png, transparency=0, optimize=False)

    masked_tiles = bytearray()
    for tile_y in range(2):
        for tile_x in range(2):
            for local_y in range(8):
                row = PIXELS[tile_y * 8 + local_y]
                pens = [PEN_BY_CHAR[row[tile_x * 8 + x]] for x in range(8)]
                for plane in range(4):
                    value = 0
                    for x, pen in enumerate(pens):
                        if pen & (1 << plane):
                            value |= 0x80 >> x
                    masked_tiles.append(value)
                mask = 0
                for x, pen in enumerate(pens):
                    if pen:
                        mask |= 0x80 >> x
                masked_tiles.append(mask)

    if len(masked_tiles) != 160:
        raise ValueError(f"Expected 160 masked bytes, got {len(masked_tiles)}")
    (OUTPUT_DIR / "tank_16x16_masked.bpl").write_bytes(masked_tiles)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true",
                        help="replace the editable PNG master with the built-in seed art")
    generate(force=parser.parse_args().force)
