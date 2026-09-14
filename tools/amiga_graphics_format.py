"""Shared format rules for the editable Enhanced Amiga graphics.

The PNG files are authoritative, indexed masters.  Runtime banks use one
opacity-mask byte after the four OCS bitplane bytes for every eight pixels.
Keeping these rules in one module prevents the editor and the build packer
from quietly disagreeing about dimensions, palette indices or tile order.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable
import re

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "amiga" / "assets" / "enhanced"
PALETTE_PATH = ROOT / "amiga" / "assets" / "game_palette.pal"


@dataclass(frozen=True)
class AssetSpec:
    key: str
    label: str
    filename: str
    width: int
    height: int

    @property
    def path(self) -> Path:
        return ASSET_DIR / self.filename


GROUND_ASSETS = (
    AssetSpec("tank", "Tank", "tank_16x8.png", 16, 8),
    AssetSpec("radar", "Radar", "radar_8x8.png", 8, 8),
    AssetSpec("launcher", "Launcher / car", "launcher_8x8.png", 8, 8),
    AssetSpec("gun", "Flak / ground gun", "gun_8x8.png", 8, 8),
    AssetSpec("fuel_depot", "Fuel depot F (+20%)", "fuel_depot_8x8.png", 8, 8),
)


def load_town_layouts() -> tuple[tuple[int, ...], ...]:
    source = (ROOT / "amiga/assets/promoted_assets.h").read_text()
    layouts = []
    for index in range(8):
        match = re.search(rf"harCpcTownBlock{index}Tiles\[\d+\] = \{{(.*?)\}};", source, re.S)
        if not match:
            raise ValueError(f"Missing town block {index}")
        cells = tuple(map(int, re.findall(r"\d+", match[1])))
        if len(cells) % 5 or any(cells[x] != 1 for x in range(len(cells)) if x % 5 >= 3):
            raise ValueError("Town layout no longer has three graphic rows above terrain")
        layouts.append(tuple(cells[x] for x in range(len(cells)) if x % 5 < 3))
    return tuple(layouts)


TOWN_LAYOUTS = load_town_layouts()
TOWN_ASSETS = tuple(
    AssetSpec(f"town_{i}", f"City {i}: {label}", f"town_{i}.png", len(layout) // 3 * 8, 24)
    for i, (layout, label) in enumerate(zip(TOWN_LAYOUTS, (
        "Installation", "Truck", "House", "Radar buildings",
        "Building complex", "Tank", "Tower", "Armed buildings",
    )))
)
MISSILE_TANK = AssetSpec("missile_tank", "Missile Tank", "missile_tank_16x8.png", 16, 8)
# Tile order is shared with enhancedProjectileIndex() in the runtime.
PROJECTILE_TILES = (53, 54, 55, 56, 98, 99, 100, 101, 53, 54, 55, 98, 56)
PROJECTILE_ASSETS = tuple(
    AssetSpec(f"projectile_{i}", label, f"projectile_{i}_8x8.png", 8, 8)
    for i, label in enumerate((
        "Maverick up-left", "Maverick down-left", "Maverick left",
        "Rocket / Maverick right", "Maverick up-right", "Maverick down-right",
        "Maverick down", "Maverick up", "Enemy missile up",
        "Enemy missile down", "Enemy missile level",
        "Missile Tank rising", "Missile Tank cruising",
    ))
)
BOMB_ASSETS = tuple(AssetSpec(f"bomb_{i}", label, f"bomb_{i}_4x3.png", 4, 3)
                    for i, label in enumerate(("Bomb launch", "Bomb falling")))
ENCOUNTER_ASSETS = (
    AssetSpec("silo_closed", "Missile Silo closed", "silo_closed_16x8.png", 16, 8),
    AssetSpec("silo_open", "Missile Silo open", "silo_open_16x8.png", 16, 8),
    AssetSpec("helicopter_0", "Helicopter rotor A", "helicopter_0_16x8.png", 16, 8),
    AssetSpec("helicopter_1", "Helicopter rotor B", "helicopter_1_16x8.png", 16, 8),
)
ASSETS = GROUND_ASSETS + (MISSILE_TANK,) + ENCOUNTER_ASSETS + PROJECTILE_ASSETS + BOMB_ASSETS + TOWN_ASSETS


def pixel_is_editable(spec: AssetSpec, x: int, y: int) -> bool:
    if not spec.key.startswith("town_"):
        return True
    layout = TOWN_LAYOUTS[int(spec.key.split("_")[1])]
    return layout[(x // 8) * 3 + y // 8] not in (0, 1)

ASSET_BY_KEY = {asset.key: asset for asset in ASSETS}


def load_palette_words(path: Path = PALETTE_PATH) -> list[int]:
    raw = path.read_bytes()
    if len(raw) < 32 or len(raw) % 2:
        raise ValueError(f"{path} must contain at least 16 big-endian colours")
    # The complete game palette contains additional sprite colours.  These
    # masked playfield assets are four-plane data and therefore use only the
    # first 16 colour registers, matching the existing asset generators.
    return [int.from_bytes(raw[offset : offset + 2], "big") & 0x0FFF
            for offset in range(0, 32, 2)]


def rgb12_to_rgb24(value: int) -> tuple[int, int, int]:
    return tuple(((value >> shift) & 0xF) * 17 for shift in (8, 4, 0))


def project_palette_rgb() -> list[tuple[int, int, int]]:
    return [rgb12_to_rgb24(value) for value in load_palette_words()]


def editor_palette_words(lighting: str) -> list[int]:
    """Game colours above raster 112, where editable ground/city art lives.

    Read the runtime constants rather than silently maintaining another palette.
    PNG masters always retain the base palette and their original indices.
    """
    words = load_palette_words()
    if lighting == "PNG":
        return words
    source = (ROOT / "amiga/main.c").read_text()
    def colour(name):
        match = re.search(rf"^#define {name}\s+(0x[0-9a-fA-F]+)\s*$", source, re.M)
        if not match:
            raise ValueError(f"Missing runtime palette constant: {name}")
        return int(match[1], 16)
    phase = lighting.upper()
    suffix = "" if lighting == "Day" else f"_{phase}"
    words[0] = colour(f"GAME_SKY_LOW{suffix}_RGB")
    words[5] = colour(f"GAME_LAND_{phase}_RGB")
    words[15] = colour("GAME_SKY_TOP_CLOUD_RGB" if lighting == "Day" else f"GAME_SKY_TOP_CLOUD_{phase}_RGB")
    for index, name in ((6, "HEALTH"), (9, "WINGMAN"), (8, "BOMBS"), (14, "ROCKETS")):
        words[index] = colour(f"GAME_POWERUP_{name}_RGB")
    return words


def pillow_palette() -> list[int]:
    flattened = [component for colour in project_palette_rgb()
                 for component in colour]
    return flattened + [0] * (768 - len(flattened))


def load_and_validate(spec: AssetSpec) -> Image.Image:
    if not spec.path.is_file():
        raise ValueError(f"Missing Enhanced graphics master: {spec.path}")

    image = Image.open(spec.path)
    image.load()
    if image.mode != "P":
        raise ValueError(f"{spec.filename}: expected indexed PNG mode P, got {image.mode}")
    if image.size != (spec.width, spec.height):
        raise ValueError(
            f"{spec.filename}: expected {spec.width}x{spec.height}, got "
            f"{image.width}x{image.height}"
        )

    actual_palette = image.getpalette()
    expected_palette = pillow_palette()
    if actual_palette is None or actual_palette[:48] != expected_palette[:48]:
        raise ValueError(
            f"{spec.filename}: first 16 colours do not match game_palette.pal"
        )

    pixels = list(image.getdata())
    invalid = sorted({int(pixel) for pixel in pixels if int(pixel) > 15})
    if invalid:
        raise ValueError(
            f"{spec.filename}: palette indices outside OCS 0..15: {invalid}"
        )
    if image.info.get("transparency") != 0:
        raise ValueError(f"{spec.filename}: palette index 0 must be transparent")
    if any(pen and not pixel_is_editable(spec, i % spec.width, i // spec.width)
           for i, pen in enumerate(pixels)):
        raise ValueError(f"{spec.filename}: empty gameplay cells must remain transparent")
    return image


def encode_masked_tile(image: Image.Image, origin_x: int, origin_y: int) -> bytes:
    if origin_x % 8 or origin_y % 8:
        raise ValueError("Masked tile origins must be aligned to 8 pixels")
    if origin_x + 8 > image.width or origin_y + 8 > image.height:
        raise ValueError("Masked tile lies outside its source image")

    output = bytearray()
    pixels = image.load()
    for local_y in range(8):
        row = [int(pixels[origin_x + x, origin_y + local_y]) for x in range(8)]
        for plane in range(4):
            value = 0
            for x, pen in enumerate(row):
                if pen & (1 << plane):
                    value |= 0x80 >> x
            output.append(value)
        mask = 0
        for x, pen in enumerate(row):
            if pen != 0:
                mask |= 0x80 >> x
        output.append(mask)
    return bytes(output)


def encode_tiles(image: Image.Image, origins: Iterable[tuple[int, int]]) -> bytes:
    return b"".join(encode_masked_tile(image, x, y) for x, y in origins)


def build_runtime_banks() -> dict[Path, bytes]:
    images = {spec.key: load_and_validate(spec) for spec in ASSETS}
    # Rightward runtime heading; keep indexed editing masters intact.
    for key in ("helicopter_0", "helicopter_1"):
        images[key] = images[key].transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    tank = encode_tiles(images["tank"], ((0, 0), (8, 0)))
    ground = b"".join(
        encode_tiles(images[key], ((0, 0),))
        for key in ("radar", "launcher", "gun", "fuel_depot")
    )
    if len(tank) != 80 or len(ground) != 160:
        raise AssertionError("Internal Enhanced graphics bank-size error")
    # Town cells use the ordinary five-plane format. Pen zero is sky; the
    # fifth plane remains zero. Order matches the original column-major map.
    town = bytearray()
    offsets = []
    for spec in TOWN_ASSETS:
        offsets.append(len(town) // 40)
        im = images[spec.key]
        for x in range(0, spec.width, 8):
            for y in range(0, 24, 8):
                masked = encode_masked_tile(im, x, y)
                for row in range(8):
                    town.extend(masked[row * 5:row * 5 + 4])
                    town.append(0)
    header = (
        "/* Generated by pack-enhanced-graphics.py; do not edit. */\n"
        f"#define ENHANCED_TOWN_TILE_COUNT {len(town) // 40}\n"
        "static const UBYTE enhancedTownTileOffsets[8] = {"
        + ", ".join(map(str, offsets)) + "};\n"
    ).encode("ascii")
    weapons = bytearray()
    compatible = []
    for spec in PROJECTILE_ASSETS + BOMB_ASSETS:
        im = images[spec.key]
        compatible.append(int(set(im.getdata()) <= {0, 6, 10}))
        # One five-byte masked row, with narrow bombs aligned to the high bits.
        for y in range(spec.height):
            pens = [im.getpixel((x, y)) for x in range(spec.width)]
            for plane in range(4):
                weapons.append(sum((0x80 >> x) for x, pen in enumerate(pens) if pen & (1 << plane)))
            weapons.append(sum((0x80 >> x) for x, pen in enumerate(pens) if pen))
    original_bombs = []
    for i, spec in enumerate(BOMB_ASSETS):
        xs = (0, 2, 3) if i == 0 else (1, 1, 1)
        original_bombs.append(int(list(images[spec.key].getdata()) ==
            [10 if x == xs[y] else 0 for y in range(3) for x in range(4)]))
    weapon_header = ("/* Generated by pack-enhanced-graphics.py; do not edit. */\n"
        "static const UBYTE enhancedProjectileHardwareCompatible[13] = {"
        + ", ".join(map(str, compatible[:13])) + "};\n"
        + "static const UBYTE enhancedBombOriginal[2] = {"
        + ", ".join(map(str, original_bombs)) + "};\n").encode("ascii")
    helicopter_shifted = bytearray()
    for key in ("helicopter_0", "helicopter_1"):
        im = images[key]
        for shift in range(8):
            for y in range(8):
                pens = [im.getpixel((x, y)) for x in range(16)]
                masks = [sum(1 << (23 - shift - x) for x, pen in enumerate(pens) if pen)]
                masks += [sum(1 << (23 - shift - x) for x, pen in enumerate(pens) if pen & (1 << plane)) for plane in range(4)]
                for bits in masks:
                    helicopter_shifted.extend(bits.to_bytes(3, "big"))
    return {
        ASSET_DIR / "helicopter_shifted.bpl": bytes(helicopter_shifted),
        ASSET_DIR / "encounters_masked.bpl": b"".join(encode_tiles(images[spec.key], ((0, 0), (8, 0))) for spec in ENCOUNTER_ASSETS),
        ASSET_DIR / "missile_tank_16x8_masked.bpl": encode_tiles(images["missile_tank"], ((0, 0), (8, 0))),
        ASSET_DIR / "weapons_masked.bpl": bytes(weapons),
        ASSET_DIR / "weapons_graphics.h": weapon_header,
        ASSET_DIR / "tank_16x8_masked.bpl": tank,
        ASSET_DIR / "ground_targets_8x8_masked.bpl": ground,
        ASSET_DIR / "town_tiles.bpl": bytes(town),
        ASSET_DIR / "town_graphics.h": header,
    }


def save_indexed_master(path: Path, size: tuple[int, int], pixels: list[int]) -> None:
    if len(pixels) != size[0] * size[1]:
        raise ValueError("Pixel count does not match image dimensions")
    if any(pixel < 0 or pixel > 15 for pixel in pixels):
        raise ValueError("Enhanced masters may use only palette indices 0..15")
    image = Image.new("P", size)
    image.putpalette(pillow_palette())
    image.putdata(pixels)
    image.info["transparency"] = 0
    temporary = path.with_suffix(path.suffix + ".tmp")
    image.save(temporary, format="PNG", transparency=0, optimize=False)
    temporary.replace(path)
