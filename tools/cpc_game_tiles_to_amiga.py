#!/usr/bin/env python3
"""Convert CPC 8x8 Mode 1 game tiles to byte-aligned Amiga 8x8 tiles."""

from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

TILE_WIDTH = 8
TILE_HEIGHT = 8
AMIGA_PLANES = 5
SCENE_WIDTH_TILES = 40
SCENE_HEIGHT_TILES = 25
TILE_BYTES = TILE_HEIGHT * AMIGA_PLANES


def parse_int(token: str) -> int:
    token = token.strip()
    if "+" in token:
        return sum(parse_int(part) for part in token.split("+"))
    if token.startswith("&") or token.startswith("#"):
        return int(token[1:], 16)
    if token.startswith("%"):
        return int(token[1:], 2)
    return int(token, 0)


def strip_comment(line: str) -> str:
    return line.split(";", 1)[0].strip()


def parse_db_values(line: str) -> list[int]:
    match = re.search(r"\b(?:db|defb)\b(.+)$", line, re.IGNORECASE)
    if not match:
        return []

    values: list[int] = []
    for token in match.group(1).split(","):
        token = token.strip()
        if token:
            values.append(parse_int(token) & 0xFF)
    return values


def decode_mode1_byte(value: int) -> tuple[int, int, int, int]:
    """Decode one CPC Mode 1 byte into four 2-bit pen indices."""
    return (
        ((value >> 7) & 1) | (((value >> 3) & 1) << 1),
        ((value >> 6) & 1) | (((value >> 2) & 1) << 1),
        ((value >> 5) & 1) | (((value >> 1) & 1) << 1),
        ((value >> 4) & 1) | (((value >> 0) & 1) << 1),
    )


def cpc_plus_grb_to_amiga_rgb(value: int) -> int:
    green = (value >> 8) & 0x0F
    red = (value >> 4) & 0x0F
    blue = value & 0x0F
    return (red << 8) | (green << 4) | blue


def extract_palette_words(lines: list[str], label: str, count: int) -> list[int]:
    """Pulls `count` `defw &XXXX` colour words following `label:` in the CPC
    game-logic source. Used for the tables the game's own ASIC-palette
    interrupt routines (setasicpalettegame/fiftiethofasecondinterruptN,
    HarrierAttackSourceNew2_alt_CRTC_CART16.asm:9302-9339) actually write to
    hardware every frame, so these are the real in-game colours rather than
    hand-picked approximations."""
    values: list[int] = []
    in_block = False
    label_re = re.compile(rf"^{re.escape(label)}\s*:", re.IGNORECASE)
    for line in lines:
        clean = strip_comment(line)
        if not in_block:
            if label_re.match(clean):
                in_block = True
                rest = clean[clean.index(":") + 1 :]
                clean = rest
            else:
                continue
        for match in re.finditer(r"&([0-9A-Fa-f]{4})", clean):
            values.append(int(match.group(1), 16))
            if len(values) >= count:
                return values
    return values


def extract_sprite_table(lines: list[str]) -> list[str]:
    in_table = False
    labels: list[str] = []
    for line in lines:
        clean = strip_comment(line)
        if re.match(r"spritelookuptable\s*:", clean, re.IGNORECASE):
            in_table = True
            continue
        if in_table and re.match(r"spr\d+\s*:", clean, re.IGNORECASE):
            break
        if not in_table:
            continue

        match = re.search(r"\bdefw\b\s+([A-Za-z_][A-Za-z0-9_]*)", clean, re.IGNORECASE)
        if match:
            labels.append(match.group(1).lower())
    return labels


def extract_sprite_data(lines: list[str]) -> dict[str, list[int]]:
    sprites: dict[str, list[int]] = {}
    current: str | None = None
    in_sprite_data = False

    for line in lines:
        clean = strip_comment(line)
        if re.match(r"spr1\s*:", clean, re.IGNORECASE):
            in_sprite_data = True
        if in_sprite_data and re.match(r"\s*endif\b", clean, re.IGNORECASE):
            break
        if not in_sprite_data:
            continue

        label = re.match(r"\s*(spr\d+)\s*:", clean, re.IGNORECASE)
        if label:
            current = label.group(1).lower()
            sprites.setdefault(current, [])
            rest = clean[label.end() :]
            sprites[current].extend(parse_db_values(rest))
            continue

        if current:
            sprites[current].extend(parse_db_values(clean))

    return sprites


# Preserve the Amiga runtime's established logical colour slots while decoding
# the CPC's real four-pen Mode 1 bitmap.
MODE1_TO_GAME_COLOR = (0, 5, 10, 15)


def tile_to_amiga(data: list[int]) -> bytes:
    if len(data) < 16:
        data = data + [0] * (16 - len(data))

    out = bytearray(TILE_BYTES)
    for row in range(TILE_HEIGHT):
        pixels = [
            MODE1_TO_GAME_COLOR[pen]
            for pen in decode_mode1_byte(data[row * 2]) + decode_mode1_byte(data[row * 2 + 1])
        ]

        for plane in range(AMIGA_PLANES):
            value = 0
            for x, color in enumerate(pixels):
                if color & (1 << plane):
                    value |= 0x80 >> x
            out[row * AMIGA_PLANES + plane] = value
    return bytes(out)


def build_scene_map() -> bytes:
    # Keep Sprint 4 deliberately conservative. These are raw CPC object tiles
    # copied without the original game's palette/raster tricks or transparency
    # rules, so busy test compositions can look like bitplane corruption even
    # when the renderer is fine. A clean sky/sea scene gives us a stable base
    # before the scrolling sprint starts.
    scene = [0] * (SCENE_WIDTH_TILES * SCENE_HEIGHT_TILES)

    def set_tile(x: int, y: int, tile: int) -> None:
        if 0 <= x < SCENE_WIDTH_TILES and 0 <= y < SCENE_HEIGHT_TILES:
            scene[y * SCENE_WIDTH_TILES + x] = tile

    def put_row(x: int, y: int, tiles: list[int]) -> None:
        for index, tile in enumerate(tiles):
            set_tile(x + index, y, tile)

    sea_top = 15
    for y in range(sea_top, SCENE_HEIGHT_TILES):
        for x in range(SCENE_WIDTH_TILES):
            if y == sea_top:
                set_tile(x, y, 4 + ((x * 3) % 2))
            elif y == sea_top + 1 and x % 4 == 0:
                set_tile(x, y, 5)
            elif y > sea_top + 2 and (x + y) % 11 == 0:
                set_tile(x, y, 4)
            else:
                set_tile(x, y, 3)

    # Player Harrier in flight.
    put_row(8, 8, [75, 76])

    # A distant target ship, low contrast but useful for sprite/tile sanity.
    put_row(28, 14, [19, 20, 21, 22, 23])

    return bytes(scene)


def build_cpc_tile_palette(real_game_colors: list[int] | None) -> list[int]:
    """Return the Amiga palette with CPC Mode 1 pens in slots 0/5/10/15."""
    sky, land, black, sea = (real_game_colors or [0x0A7F, 0x0A60, 0x0000, 0x0009])[:4]
    cpc_colors = [
        sky, 0x0FFF, 0x0CCC, 0x0888, 0x0444,
        land, 0x0FF0, 0x0A69, 0x0640, 0x00F0,
        black, 0x0764, 0x0FF0, 0x0F80, 0x0335,
        sea,
    ]
    return cpc_colors


def write_palette(
    path: Path,
    real_game_colors: list[int] | None,
    real_instrument_colors: list[int] | None,
    real_sprite_colors: list[int] | None,
) -> None:
    cpc_colors = build_cpc_tile_palette(real_game_colors)
    amiga = [cpc_plus_grb_to_amiga_rgb(color) for color in cpc_colors]
    amiga += [0] * (32 - len(amiga))

    # The HUD renderer reproduces writecharf()'s three vertical colour bands
    # with indices 1/12/9. These are CPC instrument pens 1/0/3 respectively.
    # Index 12 is otherwise unused by world tiles; indices 1 and 9 are already
    # HUD-only. Keep 16-31 free for the attached hardware-sprite palette.
    if real_instrument_colors and len(real_instrument_colors) >= 4:
        amiga[1] = cpc_plus_grb_to_amiga_rgb(real_instrument_colors[1])
        amiga[12] = cpc_plus_grb_to_amiga_rgb(real_instrument_colors[0])
        amiga[9] = cpc_plus_grb_to_amiga_rgb(real_instrument_colors[3])

    # Colours 17-31: the CPC+ ASIC has exactly ONE 15-entry hardware sprite
    # palette shared by every sprite in the game (sprite_colours,
    # HarrierAttackSourceNew2_alt_CRTC_CART16.asm:9537-9553, copied once at
    # boot into ASIC registers starting at &6422 - "The sprites use a single
    # 15 entry sprite palette", source comment at :387). On Amiga, attaching
    # hardware sprite pair (0,1) for a 4-bitplane/15-colour player sprite has
    # the same constraint: the attached pixel value (0-15) indexes colours
    # 16-31 as one continuous block, which is therefore shared by every other
    # hardware sprite too (rocket/bomb/enemy/enemy missile) - matching the
    # real CPC+ hardware behaviour exactly rather than being a compromise.
    if real_sprite_colors and len(real_sprite_colors) >= 15:
        for offset, value in enumerate(real_sprite_colors[:15]):
            amiga[17 + offset] = cpc_plus_grb_to_amiga_rgb(value)
    else:
        amiga[17] = cpc_plus_grb_to_amiga_rgb(0x0000)  # sprite pen 1: black
        amiga[18] = cpc_plus_grb_to_amiga_rgb(0x0A60)  # sprite pen 2: green
        amiga[19] = cpc_plus_grb_to_amiga_rgb(0x0FFF)  # sprite pen 3: white
        amiga[21] = cpc_plus_grb_to_amiga_rgb(0x0000)  # sprite 2 pen 1: black
        amiga[22] = cpc_plus_grb_to_amiga_rgb(0x0FF0)  # sprite 2 pen 2: yellow
        amiga[23] = cpc_plus_grb_to_amiga_rgb(0x00F0)  # sprite 2 pen 3: red
        amiga[25] = cpc_plus_grb_to_amiga_rgb(0x0000)  # sprite 4 pen 1: black
        amiga[26] = cpc_plus_grb_to_amiga_rgb(0x0FF0)  # sprite 4 pen 2: yellow
        amiga[27] = cpc_plus_grb_to_amiga_rgb(0x00F0)  # sprite 4 pen 3: red

    path.write_bytes(b"".join(struct.pack(">H", color) for color in amiga))


def write_preview(path: Path, tiles: bytes, scene: bytes, palette_path: Path) -> None:
    width = SCENE_WIDTH_TILES * TILE_WIDTH
    height = SCENE_HEIGHT_TILES * TILE_HEIGHT
    palette_words = [
        struct.unpack(">H", palette_path.read_bytes()[i * 2 : i * 2 + 2])[0]
        for i in range(16)
    ]
    rgb = [
        (((color >> 8) & 0x0F) * 17, ((color >> 4) & 0x0F) * 17, (color & 0x0F) * 17)
        for color in palette_words
    ]

    pixels = [(0, 0, 0)] * (width * height)
    for tile_y in range(SCENE_HEIGHT_TILES):
        for tile_x in range(SCENE_WIDTH_TILES):
            tile_id = scene[tile_y * SCENE_WIDTH_TILES + tile_x]
            base = tile_id * TILE_BYTES
            for row in range(TILE_HEIGHT):
                plane_bytes = tiles[base + row * AMIGA_PLANES : base + row * AMIGA_PLANES + AMIGA_PLANES]
                for x in range(TILE_WIDTH):
                    color = 0
                    bit = 0x80 >> x
                    for plane, value in enumerate(plane_bytes):
                        if value & bit:
                            color |= 1 << plane
                    px = tile_x * TILE_WIDTH + x
                    py = tile_y * TILE_HEIGHT + row
                    pixels[py * width + px] = rgb[color & 0x0F]

    row_size = ((width * 3 + 3) // 4) * 4
    bitmap = bytearray()
    for y in range(height - 1, -1, -1):
        row = bytearray()
        for x in range(width):
            red, green, blue = pixels[y * width + x]
            row += bytes((blue, green, red))
        row += b"\x00" * (row_size - len(row))
        bitmap += row

    header_size = 14 + 40
    size = header_size + len(bitmap)
    with path.open("wb") as file:
        file.write(b"BM")
        file.write(struct.pack("<IHHI", size, 0, 0, header_size))
        file.write(struct.pack("<IIIHHIIIIII", 40, width, height, 1, 24, 0, len(bitmap), 2835, 2835, 0, 0))
        file.write(bitmap)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=Path("AMSTRADFONT3.asm"))
    parser.add_argument(
        "--palette-input",
        type=Path,
        default=Path("HarrierAttackSourceNew2_alt_CRTC_CART16.asm"),
        help="CPC game-logic source to read the real ASIC palette tables from.",
    )
    parser.add_argument("--out-tiles", type=Path, default=Path("amiga/assets/game_tiles.bpl"))
    parser.add_argument("--out-map", type=Path, default=Path("amiga/assets/game_scene.map"))
    parser.add_argument("--out-pal", type=Path, default=Path("amiga/assets/game_palette.pal"))
    parser.add_argument("--preview-bmp", type=Path, default=Path("amiga/assets/game_scene_preview.bmp"))
    args = parser.parse_args()

    text = args.input.read_text(encoding="utf-8", errors="ignore")
    lines = text.splitlines()
    table = extract_sprite_table(lines)
    sprites = extract_sprite_data(lines)

    if not table:
        raise ValueError(f"Could not find spritelookuptable in {args.input}")

    real_game_colors: list[int] | None = None
    real_instrument_colors: list[int] | None = None
    real_sprite_colors: list[int] | None = None
    if args.palette_input.exists():
        palette_lines = args.palette_input.read_text(encoding="utf-8", errors="ignore").splitlines()
        real_game_colors = extract_palette_words(palette_lines, "palettegamemaster", 4)
        real_instrument_colors = extract_palette_words(palette_lines, "paletteinstruments", 4)
        real_sprite_colors = extract_palette_words(palette_lines, "sprite_colours", 15)
        if len(real_game_colors) < 4:
            print(f"Warning: could not find palettegamemaster in {args.palette_input}, using approximation")
            real_game_colors = None
        if len(real_instrument_colors) < 4:
            print(f"Warning: could not find paletteinstruments in {args.palette_input}")
            real_instrument_colors = None
        if len(real_sprite_colors) < 15:
            print(f"Warning: could not find sprite_colours in {args.palette_input}, using approximation")
            real_sprite_colors = None
    else:
        print(f"Warning: {args.palette_input} not found, palette will use approximated colors only")

    tiles = bytearray()
    for label in table:
        tiles.extend(tile_to_amiga(sprites.get(label, [])))
    scene = build_scene_map()

    args.out_tiles.parent.mkdir(parents=True, exist_ok=True)
    args.out_map.parent.mkdir(parents=True, exist_ok=True)
    args.out_pal.parent.mkdir(parents=True, exist_ok=True)
    args.preview_bmp.parent.mkdir(parents=True, exist_ok=True)

    args.out_tiles.write_bytes(tiles)
    args.out_map.write_bytes(scene)
    write_palette(args.out_pal, real_game_colors, real_instrument_colors, real_sprite_colors)
    write_preview(args.preview_bmp, bytes(tiles), scene, args.out_pal)

    print(f"Wrote {args.out_tiles} ({len(table)} tiles, {len(tiles)} bytes)")
    print(f"Wrote {args.out_map} ({len(scene)} bytes)")
    pal_note = "sky/land/black/sea" if real_game_colors else "approximated background"
    pal_note += " + real 15-colour sprite_colours" if real_sprite_colors else " + approximated sprite colours"
    print(f"Wrote {args.out_pal} (32 colors, {pal_note})")
    print(f"Wrote {args.preview_bmp}")


if __name__ == "__main__":
    main()
