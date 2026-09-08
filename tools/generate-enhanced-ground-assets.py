"""Generate Enhanced-only OCS ground-target presentation assets.

The three objects remain one CPC gameplay cell wide, but Enhanced presents
them as two vertically stacked masked 8x8 cells. Runtime data is ordered as
RADAR top/bottom, LAUNCHER top/bottom, GUN top/bottom. Each scanline contains
four colour-plane bytes and one opacity-mask byte, exactly matching
drawGameScrollTileMasked().
"""

import argparse
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = ROOT / "amiga" / "assets" / "enhanced"

PEN_BY_CHAR = {
    ".": 0,   # transparent
    "L": 2,   # light grey
    "M": 3,   # mid grey
    "D": 4,   # dark grey
    "G": 5,   # green
    "Y": 6,   # yellow
    "g": 8,   # dark green
    "R": 9,   # red
    "K": 10,  # black outline
    "O": 12,  # orange
    "B": 13,  # bright green
}

OBJECTS = {
    "radar": (
        "........",
        ".LLLLL..",
        "..LLLK..",
        "...LKK..",
        "...KK...",
        "...K....",
        "...K....",
        "...K....",
        "...K....",
        "...K....",
        "..KDK...",
        ".KDDDK..",
        ".KLLLK..",
        ".KDDDK..",
        ".KDDDK..",
        "..KKK...",
    ),
    "launcher": (
        "......Y.",
        ".....OLK",
        "....OLLK",
        "...OLKK.",
        "..OLKK..",
        "...K....",
        "...K....",
        "..KKK...",
        "........",
        "........",
        "...KK...",
        "..KLLK..",
        ".KDLLDK.",
        "KDDDDDDK",
        "KLLLLLLK",
        ".KKKKKK.",
    ),
    "gun": (
        "...K..K.",
        "....K..K",
        "....K.K.",
        ".....K..",
        "....KK..",
        "...KDK..",
        "..KDDK..",
        "...KK...",
        "........",
        "........",
        "..KDDK..",
        ".KDLLDK.",
        "KDDDDDDK",
        "KDLLLLDK",
        "KDDDDDDK",
        ".KKKKKK.",
    ),
}


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


def masked_vertical_tiles(rows: tuple[str, ...]) -> bytes:
    output = bytearray()
    for tile_y in range(2):
        for local_y in range(8):
            pens = [PEN_BY_CHAR[char] for char in rows[tile_y * 8 + local_y]]
            for plane in range(4):
                value = 0
                for x, pen in enumerate(pens):
                    if pen & (1 << plane):
                        value |= 0x80 >> x
                output.append(value)
            mask = 0
            for x, pen in enumerate(pens):
                if pen:
                    mask |= 0x80 >> x
            output.append(mask)
    return bytes(output)


def generate(force: bool = False) -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_paths = [OUTPUT_DIR / f"{name}_8x16.png" for name in OBJECTS]
    existing = [path for path in output_paths if path.exists()]
    if existing and not force:
        names = ", ".join(path.name for path in existing)
        raise FileExistsError(
            f"Refusing to overwrite edited masters: {names}. Use --force to reset them."
        )
    palette = game_palette()
    combined = bytearray()
    contact = Image.new("P", (8 * len(OBJECTS), 16))
    contact.putpalette(palette)
    contact.info["transparency"] = 0

    for object_index, (name, rows) in enumerate(OBJECTS.items()):
        if len(rows) != 16 or any(len(row) != 8 for row in rows):
            raise ValueError(f"{name} must be exactly 8x16")
        pixels = [PEN_BY_CHAR[char] for row in rows for char in row]
        image = Image.new("P", (8, 16))
        image.putpalette(palette)
        image.putdata(pixels)
        image.info["transparency"] = 0
        image.save(OUTPUT_DIR / f"{name}_8x16.png", transparency=0,
                   optimize=False)
        contact.paste(image, (object_index * 8, 0))
        combined.extend(masked_vertical_tiles(rows))

    if len(combined) != 240:
        raise ValueError(f"Expected 240 masked bytes, got {len(combined)}")
    (OUTPUT_DIR / "ground_targets_8x16_masked.bpl").write_bytes(combined)
    contact.save(OUTPUT_DIR / "ground_targets_contact.png", transparency=0,
                 optimize=False)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true",
                        help="replace editable PNG masters with the built-in seed art")
    generate(force=parser.parse_args().force)
