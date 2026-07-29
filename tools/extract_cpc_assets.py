#!/usr/bin/env python3
"""Extract Harrier Attack CPC graphics into reviewable Amiga-port assets.

This is deliberately an asset-audit pipeline, not a runtime replacement yet.
It keeps source provenance and "risk notes" next to every extracted asset so
we can promote good CPC rips into the Amiga renderer, or paint native Amiga
replacements later, without losing where the original data came from.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


TILE_WIDTH = 8
TILE_HEIGHT = 8
TILE_PLANES = 5
PLUS_SPRITE_WIDTH = 16
MODE1_TO_GAME_COLOR = (0, 5, 10, 15)


# Fallback only - used if the real game-logic source can't be parsed (see
# load_real_rgb_palette() below). Deliberately arbitrary/high-contrast colours
# picked just to tell different pens apart, NOT real in-game colours - do not
# read anything into these hues (e.g. pen 6 isn't really "yellow" in-game).
FALLBACK_RGB_PALETTE: list[tuple[int, int, int]] = [
    (0x77, 0xAA, 0xFF),  # 0 sky / transparent preview background
    (0xFF, 0xFF, 0xFF),  # 1 white
    (0xDD, 0xDD, 0xDD),  # 2 light grey
    (0x77, 0x77, 0x77),  # 3 mid grey
    (0x33, 0x33, 0x33),  # 4 dark grey
    (0x66, 0xAA, 0x00),  # 5 land green
    (0xFF, 0xFF, 0x00),  # 6 yellow
    (0xFF, 0x77, 0x00),  # 7 orange
    (0x55, 0x22, 0x00),  # 8 brown
    (0xFF, 0x00, 0x00),  # 9 red
    (0x00, 0x00, 0x00),  # 10 black
    (0x88, 0x44, 0x55),  # 11 roof/brown
    (0xFF, 0xFF, 0xFF),  # 12 cloud white
    (0xCC, 0xCC, 0xCC),  # 13 pale cloud
    (0x22, 0x22, 0x99),  # 14 sea shade
    (0x00, 0x00, 0x99),  # 15 sea
]

# Mutated in place by load_real_rgb_palette() once the real game-logic source
# is available, so every function below that already refers to this name
# (blit_scaled, render_tile_contact_sheet, ...) picks up the real colours
# without threading a new parameter through the whole file.
RGB_PALETTE: list[tuple[int, int, int]] = list(FALLBACK_RGB_PALETTE)


def load_real_rgb_palette(main_source: Path) -> list[tuple[int, int, int]]:
    """Builds the real 16-pen tile palette (same one amiga/assets/game_tiles.bpl
    actually ships with) from the CPC game-logic source, for use as this
    audit tool's preview colours instead of FALLBACK_RGB_PALETTE's arbitrary
    distinguishability hues. Falls back to FALLBACK_RGB_PALETTE if the source
    can't be read or the expected palette table isn't found."""
    try:
        import sys as _sys

        tools_dir = str(Path(__file__).resolve().parent)
        if tools_dir not in _sys.path:
            _sys.path.insert(0, tools_dir)
        from cpc_game_tiles_to_amiga import build_cpc_tile_palette, extract_palette_words

        lines = main_source.read_text(encoding="utf-8", errors="ignore").splitlines()
        real_game_colors = extract_palette_words(lines, "palettegamemaster", 4)
        if len(real_game_colors) < 4:
            return list(FALLBACK_RGB_PALETTE)
        grb_words = build_cpc_tile_palette(real_game_colors)
        return [
            (((value >> 4) & 0xF) * 17, ((value >> 8) & 0xF) * 17, (value & 0xF) * 17)
            for value in grb_words
        ]
    except Exception:
        return list(FALLBACK_RGB_PALETTE)


def load_real_sprite_rgb_palette(main_source: Path) -> list[tuple[int, int, int]]:
    """Build the CPC Plus hardware-sprite palette from ``sprite_colours``.

    CPC screen tiles and CPC Plus sprites do not share a palette.  Keeping
    this separate prevents the audit gallery from showing misleading green
    or yellow pixels in the grey Wingman merely because a sprite pen happens
    to have the same number as a screen pen.
    """
    try:
        import sys as _sys

        tools_dir = str(Path(__file__).resolve().parent)
        if tools_dir not in _sys.path:
            _sys.path.insert(0, tools_dir)
        from cpc_game_tiles_to_amiga import extract_palette_words

        lines = main_source.read_text(encoding="utf-8", errors="ignore").splitlines()
        grb_words = extract_palette_words(lines, "sprite_colours", 15)
        if len(grb_words) < 15:
            return list(FALLBACK_RGB_PALETTE)
        palette = [(0, 0, 0)]
        palette.extend(
            (
                ((value >> 4) & 0xF) * 17,
                ((value >> 8) & 0xF) * 17,
                (value & 0xF) * 17,
            )
            for value in grb_words
        )
        return palette
    except Exception:
        return list(FALLBACK_RGB_PALETTE)


LABEL_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:")
DB_RE = re.compile(r"\b(?:db|defb)\b(.+)$", re.IGNORECASE)
DW_RE = re.compile(r"\b(?:dw|defw)\b(.+)$", re.IGNORECASE)


@dataclass
class SourceLine:
    number: int
    raw: str
    code: str
    comment: str


@dataclass
class TileAsset:
    table_index: int
    label: str
    source_line: int
    raw_bytes: list[int]
    comments: list[str] = field(default_factory=list)
    cpc_comment_id: int | None = None
    friendly_name: str = ""
    category: str = ""


@dataclass
class PlusSpriteAsset:
    asset_id: str
    label: str
    source_line: int
    raw_values: list[int]
    friendly_name: str = ""
    comments: list[str] = field(default_factory=list)
    width: int = PLUS_SPRITE_WIDTH
    height: int = 0
    category: str = ""
    bitplane_offset: int = 0
    bitplane_size: int = 0


@dataclass
class ObjectAsset:
    asset_id: str
    label: str
    source_line: int
    kind: str
    raw_bytes: list[int]
    comments: list[str] = field(default_factory=list)
    layout: dict = field(default_factory=dict)
    risk: str = ""


def split_code_comment(line: str) -> tuple[str, str]:
    code, sep, comment = line.partition(";")
    return code.rstrip(), comment.strip() if sep else ""


def read_source_lines(path: Path) -> list[SourceLine]:
    lines: list[SourceLine] = []
    for index, raw in enumerate(path.read_text(encoding="utf-8", errors="ignore").splitlines(), start=1):
        code, comment = split_code_comment(raw)
        lines.append(SourceLine(index, raw, code, comment))
    return lines


def split_asm_csv(text: str) -> list[str]:
    parts: list[str] = []
    token: list[str] = []
    in_string = False
    escape = False
    for char in text:
        if in_string:
            token.append(char)
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
            token.append(char)
            continue
        if char == ",":
            parts.append("".join(token).strip())
            token = []
            continue
        token.append(char)
    if token:
        parts.append("".join(token).strip())
    return parts


def parse_int(token: str) -> int:
    token = token.strip()
    if not token:
        raise ValueError("empty numeric token")
    if "+" in token:
        return sum(parse_int(part) for part in token.split("+"))
    if token.startswith("&") or token.startswith("#"):
        return int(token[1:], 16)
    if token.startswith("%"):
        return int(token[1:], 2)
    return int(token, 0)


def parse_db_values_from_code(code: str) -> list[int]:
    match = DB_RE.search(code)
    if not match:
        return []

    values: list[int] = []
    for token in split_asm_csv(match.group(1)):
        if not token:
            continue
        if len(token) >= 2 and token[0] == '"' and token[-1] == '"':
            values.extend(ord(char) & 0xFF for char in token[1:-1])
        else:
            values.append(parse_int(token) & 0xFF)
    return values


def parse_defw_labels_from_code(code: str) -> list[str]:
    match = DW_RE.search(code)
    if not match:
        return []
    labels: list[str] = []
    for token in split_asm_csv(match.group(1)):
        token = token.strip()
        if re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", token):
            labels.append(token)
    return labels


def compact_comments(comments: Iterable[str]) -> list[str]:
    compacted: list[str] = []
    for comment in comments:
        comment = comment.strip()
        if not comment:
            continue
        if comment not in compacted:
            compacted.append(comment)
    return compacted[:8]


def category_from_name(name: str, label: str = "") -> str:
    text = f"{name} {label}".lower()
    checks = [
        ("parachute", "pilot.parachute"),
        ("chute", "pilot.parachute"),
        ("ejector", "pilot.eject"),
        ("wingman", "aircraft.wingman"),
        ("gunship", "aircraft.gunship"),
        ("enemy plane", "aircraft.enemy"),
        ("harrier", "aircraft.harrier"),
        ("plane", "aircraft"),
        ("sky", "environment.sky"),
        ("cloud", "environment.cloud"),
        ("sea", "environment.sea"),
        ("hill", "environment.land.hill"),
        ("grass", "environment.land.surface"),
        ("land", "environment.land"),
        ("frigate", "ship.friendly_frigate"),
        ("carrier", "ship.carrier"),
        ("enemy ship", "ship.enemy"),
        ("ship", "ship"),
        ("launcher", "ground_target.launcher"),
        ("missile", "weapon.missile"),
        ("bomb", "weapon.bomb"),
        ("flak", "weapon.flak"),
        ("smoke", "effect.smoke"),
        ("radar", "ground_target.radar"),
        ("gun", "ground_target.gun"),
        ("tank", "ground_target.tank"),
        ("jeep", "ground_target.vehicle"),
        ("truck", "ground_target.vehicle"),
        ("roof", "town.roof"),
        ("tower", "town.building"),
        ("dock", "pier.dock"),
        ("hole", "terrain.crater"),
    ]
    for needle, category in checks:
        if needle in text:
            return category
    return "unknown"


PLUS_SPRITE_HINTS: dict[str, tuple[str, str]] = {
    "sprite_pixel_data1": ("Flying Harrier left/green half", "aircraft.harrier"),
    "sprite_pixel_data2": ("Flying Harrier right/green half", "aircraft.harrier"),
    "sprite_pixel_data4": ("Landing Harrier left half", "aircraft.harrier"),
    "sprite_pixel_data3": ("Landing Harrier right half", "aircraft.harrier"),
    "sprite_pixel_data5": ("Enemy plane flying left half", "aircraft.enemy"),
    "sprite_pixel_data6": ("Enemy plane flying right half", "aircraft.enemy"),
    "sprite_pixel_data7": ("Enemy plane broken left half", "aircraft.enemy"),
    "sprite_pixel_data8": ("Enemy plane broken right half", "aircraft.enemy"),
    "sprite_pixel_data10": ("Carrier body", "ship.carrier"),
    "sprite_pixel_data11": ("Carrier back", "ship.carrier"),
    "sprite_pixel_data12": ("Carrier front", "ship.carrier"),
    "sprite_pixel_data13": ("Carrier top", "ship.carrier"),
    "sprite_pixel_data14": ("Carrier top 2", "ship.carrier"),
    "sprite_pixel_data15": ("Gunship left", "aircraft.gunship"),
    "sprite_pixel_data16": ("Gunship right", "aircraft.gunship"),
    "sprite_pixel_data_wingmanflying1": ("Wingman flying left half", "aircraft.wingman"),
    "sprite_pixel_data_wingmanflying2": ("Wingman flying right half", "aircraft.wingman"),
    "sprite_pixel_data_wingmanlanded1": ("Wingman landed left half", "aircraft.wingman"),
    "sprite_pixel_data_wingmanlanded2": ("Wingman landed right half", "aircraft.wingman"),
    "sprite_pixel_data_parachute": ("Parachute", "pilot.parachute"),
}


def decode_mode1_byte(value: int) -> tuple[int, int, int, int]:
    return (
        ((value >> 7) & 1) | (((value >> 3) & 1) << 1),
        ((value >> 6) & 1) | (((value >> 2) & 1) << 1),
        ((value >> 5) & 1) | (((value >> 1) & 1) << 1),
        ((value >> 4) & 1) | (((value >> 0) & 1) << 1),
    )


def cpc_mode1_tile_pixels(raw_bytes: list[int]) -> list[list[int]]:
    data = list(raw_bytes[:16])
    if len(data) < 16:
        data.extend([0] * (16 - len(data)))

    pixels: list[list[int]] = []
    for row in range(TILE_HEIGHT):
        pixels.append(
            [
                MODE1_TO_GAME_COLOR[pen]
                for pen in decode_mode1_byte(data[row * 2]) + decode_mode1_byte(data[row * 2 + 1])
            ]
        )
    return pixels


def pixels_to_bitplanes(pixels: list[list[int]], planes: int) -> bytes:
    height = len(pixels)
    width = len(pixels[0]) if height else 0
    row_bytes = (width + 7) // 8
    out = bytearray()
    for row in range(height):
        for plane in range(planes):
            for byte_col in range(row_bytes):
                value = 0
                for bit_index in range(8):
                    x = byte_col * 8 + bit_index
                    if x < width and pixels[row][x] & (1 << plane):
                        value |= 0x80 >> bit_index
                out.append(value)
    return bytes(out)


def find_label_line(lines: list[SourceLine], label: str) -> int:
    target = label.lower()
    for line in lines:
        match = LABEL_RE.match(line.code)
        if match and match.group(1).lower() == target:
            return line.number
    return 0


def collect_label_db(lines: list[SourceLine], label: str) -> tuple[list[int], list[str], int]:
    values: list[int] = []
    comments: list[str] = []
    source_line = 0
    active = False
    target = label.lower()

    for line in lines:
        label_match = LABEL_RE.match(line.code)
        if not active:
            if label_match and label_match.group(1).lower() == target:
                active = True
                source_line = line.number
                if line.comment:
                    comments.append(line.comment)
                rest = line.code[label_match.end() :]
                values.extend(parse_db_values_from_code(rest))
            continue

        if label_match:
            break
        if line.comment:
            comments.append(line.comment)
        values.extend(parse_db_values_from_code(line.code))

    return values, compact_comments(comments), source_line


def extract_sprite_table_labels(lines: list[SourceLine]) -> list[str]:
    labels: list[str] = []
    active = False
    for line in lines:
        label_match = LABEL_RE.match(line.code)
        if label_match and label_match.group(1).lower() == "spritelookuptable":
            active = True
            continue
        if active and label_match and re.match(r"spr\d+$", label_match.group(1), re.IGNORECASE):
            break
        if active:
            labels.extend(parse_defw_labels_from_code(line.code))
    return labels


def extract_tiles(lines: list[SourceLine]) -> list[TileAsset]:
    table_labels = extract_sprite_table_labels(lines)
    tiles: list[TileAsset] = []
    for table_index, label in enumerate(table_labels):
        raw, comments, source_line = collect_label_db(lines, label)
        cpc_comment_id: int | None = None
        friendly_name = label
        for comment in comments:
            match = re.search(r"\b(\d+)\s*-\s*(.+)$", comment)
            if match:
                cpc_comment_id = int(match.group(1))
                friendly_name = match.group(2).strip()
                break
        category = category_from_name(friendly_name, label)
        tiles.append(
            TileAsset(
                table_index=table_index,
                label=label,
                source_line=source_line,
                raw_bytes=raw[:16],
                comments=comments,
                cpc_comment_id=cpc_comment_id,
                friendly_name=friendly_name,
                category=category,
            )
        )
    return tiles


def extract_plus_sprites(lines: list[SourceLine]) -> list[PlusSpriteAsset]:
    sprites: list[PlusSpriteAsset] = []
    current_label: str | None = None
    current_values: list[int] = []
    current_comments: list[str] = []
    current_line = 0
    comment_buffer: list[str] = []

    def finish_current() -> None:
        nonlocal current_label, current_values, current_comments, current_line
        if not current_label:
            return
        values = list(current_values)
        height = math.ceil(len(values) / PLUS_SPRITE_WIDTH) if values else 0
        if height * PLUS_SPRITE_WIDTH > len(values):
            values.extend([0] * (height * PLUS_SPRITE_WIDTH - len(values)))
        hint = PLUS_SPRITE_HINTS.get(current_label.lower())
        friendly_name = hint[0] if hint else current_label
        comments = compact_comments(([friendly_name] if hint else []) + current_comments)
        category = hint[1] if hint else category_from_name(" ".join(comments), current_label)
        sprites.append(
            PlusSpriteAsset(
                asset_id=current_label.lower(),
                label=current_label,
                source_line=current_line,
                raw_values=values,
                friendly_name=friendly_name,
                comments=comments,
                height=height,
                category=category,
            )
        )
        current_label = None
        current_values = []
        current_comments = []
        current_line = 0

    for line in lines:
        stripped = line.code.strip()
        label_match = LABEL_RE.match(line.code)
        if label_match:
            label = label_match.group(1)
            if label.lower().startswith("sprite_pixel_data"):
                finish_current()
                current_label = label
                current_line = line.number
                current_comments = compact_comments(comment_buffer + ([line.comment] if line.comment else []))
                current_values = []
                comment_buffer = []
                rest = line.code[label_match.end() :]
                current_values.extend(parse_db_values_from_code(rest))
                continue
            finish_current()
            comment_buffer = []

        if current_label:
            db_match = DB_RE.search(line.code)
            if db_match and '"' in db_match.group(1):
                # AMSTRADFONT3.asm ends the sprite block with defb "CHRIS5".
                # That is a signature, not pixel data.
                finish_current()
                continue
            db_values = parse_db_values_from_code(line.code)
            if db_values:
                current_values.extend(db_values)
                if line.comment:
                    current_comments.append(line.comment)
        elif line.comment and not stripped:
            comment_buffer.append(line.comment)
            comment_buffer = comment_buffer[-4:]
        elif stripped:
            comment_buffer = []

    finish_current()
    return sprites


def split_terminated_records(values: list[int], terminator: int = 0xFF) -> list[list[int]]:
    records: list[list[int]] = []
    current: list[int] = []
    for value in values:
        if value == terminator:
            records.append(current)
            current = []
        else:
            current.append(value)
    if current:
        records.append(current)
    return records


def grid_from_column_major(values: list[int], height: int) -> list[list[int]]:
    if height <= 0:
        return []
    width = math.ceil(len(values) / height)
    grid = [[0 for _ in range(width)] for _ in range(height)]
    for index, value in enumerate(values):
        x = index // height
        y = index % height
        if y < height and x < width:
            grid[y][x] = value
    return grid


def grid_from_row_major(values: list[int], width: int) -> list[list[int]]:
    if width <= 0:
        return []
    height = math.ceil(len(values) / width)
    grid = [[0 for _ in range(width)] for _ in range(height)]
    for index, value in enumerate(values):
        y = index // width
        x = index % width
        grid[y][x] = value
    return grid


def extract_objects(lines: list[SourceLine]) -> list[ObjectAsset]:
    object_specs = [
        ("enemy_ship_scroll_columns", "enemyshipsprite", "composite.column_major", {"height": 3}),
        ("end_frigate_scroll_columns", "endfrigatesprite", "composite.column_major", {"height": 3}),
        ("enemy_land_targets", "enemylandsprites", "records.2x2_terminated", {"record_width": 2, "record_height": 2}),
        ("solid_land_fill", "solidlandspriteblock", "raw.drawspriteblock3_fill", {"width": 11}),
        ("write_frigate_tilemap_1", "writefrigatetilemap1", "objectmap.row", {"width": 12}),
        ("write_frigate_tilemap_2", "writefrigatetilemap2", "objectmap.row", {"width": 12}),
        ("write_frigate_tilemap_3", "writefrigatetilemap3", "objectmap.row", {"width": 12}),
        ("pier_pen_data", "pendata", "raw.ascii_tile_stream", {"terminator": 255}),
    ]
    for index in range(8):
        object_specs.append(
            (
                f"town_block_{index}",
                f"blk{index}",
                "raw.drawspriteblock3_column_stream",
                {"column_height": 5},
            )
        )

    objects: list[ObjectAsset] = []
    for asset_id, label, kind, layout in object_specs:
        values, comments, source_line = collect_label_db(lines, label)
        if not source_line:
            continue
        risk = ""
        if kind == "raw.drawspriteblock3_column_stream":
            risk = "drawspriteblock3 stream is column-major; each screen-edge update consumes one vertical column."
        elif kind.startswith("raw.drawspriteblock3"):
            risk = "drawspriteblock3 command stream is stored raw; visual preview is heuristic until the CPC routine is fully decoded."
        elif kind == "raw.ascii_tile_stream":
            risk = "ASCII stream is stored raw; likely pier/deck pen/tile data and needs manual promotion."
        objects.append(
            ObjectAsset(
                asset_id=asset_id,
                label=label,
                source_line=source_line,
                kind=kind,
                raw_bytes=values,
                comments=comments,
                layout=layout,
                risk=risk,
            )
        )
    return objects


def tile_summary(tile: TileAsset, source_name: str) -> dict:
    return {
        "asset_id": f"cpc_tile_{tile.table_index:03d}",
        "kind": "cpc_mode1_tile_8x8",
        "graphics_mode": "cpc_mode1_packed_4_pixels_per_byte",
        "table_index": tile.table_index,
        "source_label": tile.label,
        "source": f"{source_name}:{tile.source_line}",
        "cpc_comment_id": tile.cpc_comment_id,
        "name": tile.friendly_name,
        "category": tile.category,
        "raw_bytes_hex": [f"{value:02x}" for value in tile.raw_bytes],
        "comments": tile.comments,
        "promotion_status": "candidate",
    }


def plus_sprite_summary(sprite: PlusSpriteAsset, source_name: str) -> dict:
    return {
        "asset_id": sprite.asset_id,
        "kind": "cpc_plus_sprite_pixels",
        "graphics_mode": "cpc_plus_direct_pen_indices",
        "source_label": sprite.label,
        "source": f"{source_name}:{sprite.source_line}",
        "name": sprite.friendly_name,
        "width": sprite.width,
        "height": sprite.height,
        "category": sprite.category,
        "raw_value_count": len(sprite.raw_values),
        "bitplane_offset": sprite.bitplane_offset,
        "bitplane_size": sprite.bitplane_size,
        "comments": sprite.comments,
        "promotion_status": "candidate",
        "notes": "Values are direct CPC Plus sprite pen indices, not packed Mode 0 or Mode 1 bytes. Pen 0 is transparent.",
    }


def object_summary(obj: ObjectAsset, source_name: str) -> dict:
    summary = {
        "asset_id": obj.asset_id,
        "kind": obj.kind,
        "source_label": obj.label,
        "source": f"{source_name}:{obj.source_line}",
        "raw_bytes_hex": [f"{value:02x}" for value in obj.raw_bytes],
        "raw_values": obj.raw_bytes,
        "layout": obj.layout,
        "comments": obj.comments,
        "promotion_status": "candidate",
    }
    if obj.risk:
        summary["risk"] = obj.risk
    if obj.kind == "composite.column_major":
        data = list(obj.raw_bytes)
        if data and data[-1] == 0xFF:
            data = data[:-1]
        summary["grid_column_major"] = grid_from_column_major(data, int(obj.layout.get("height", 1)))
    elif obj.kind == "records.2x2_terminated":
        summary["records"] = [
            grid_from_row_major(record, int(obj.layout.get("record_width", 2)))
            for record in split_terminated_records(obj.raw_bytes)
        ]
    elif obj.kind == "objectmap.row":
        summary["grid_row_major"] = grid_from_row_major(obj.raw_bytes, int(obj.layout.get("width", len(obj.raw_bytes) or 1)))
    return summary


def write_json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_csv(path: Path, fieldnames: list[str], rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def write_bmp(path: Path, width: int, height: int, pixels: list[tuple[int, int, int]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
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


def blank_pixels(width: int, height: int, color: tuple[int, int, int]) -> list[tuple[int, int, int]]:
    return [color] * (width * height)


def blit_scaled(
    dest: list[tuple[int, int, int]],
    dest_width: int,
    x0: int,
    y0: int,
    pixels: list[list[int]],
    scale: int,
    transparent_pen: int | None = None,
    palette: list[tuple[int, int, int]] | None = None,
) -> None:
    dest_height = len(dest) // dest_width
    for y, row in enumerate(pixels):
        for x, pen in enumerate(row):
            if transparent_pen is not None and pen == transparent_pen:
                continue
            color = (palette or RGB_PALETTE)[pen & 0x0F]
            for sy in range(scale):
                py = y0 + y * scale + sy
                if py < 0 or py >= dest_height:
                    continue
                for sx in range(scale):
                    px = x0 + x * scale + sx
                    if 0 <= px < dest_width:
                        dest[py * dest_width + px] = color


def draw_rect(
    dest: list[tuple[int, int, int]],
    dest_width: int,
    x0: int,
    y0: int,
    width: int,
    height: int,
    color: tuple[int, int, int],
) -> None:
    dest_height = len(dest) // dest_width
    for y in range(max(0, y0), min(dest_height, y0 + height)):
        row_base = y * dest_width
        for x in range(max(0, x0), min(dest_width, x0 + width)):
            dest[row_base + x] = color


def draw_frame(
    dest: list[tuple[int, int, int]],
    dest_width: int,
    x0: int,
    y0: int,
    width: int,
    height: int,
    color: tuple[int, int, int],
) -> None:
    draw_rect(dest, dest_width, x0, y0, width, 1, color)
    draw_rect(dest, dest_width, x0, y0 + height - 1, width, 1, color)
    draw_rect(dest, dest_width, x0, y0, 1, height, color)
    draw_rect(dest, dest_width, x0 + width - 1, y0, 1, height, color)


def render_tile_contact_sheet(path: Path, tiles: list[TileAsset]) -> None:
    scale = 3
    columns = 16
    cell_w = TILE_WIDTH * scale + 6
    cell_h = TILE_HEIGHT * scale + 6
    rows = math.ceil(len(tiles) / columns)
    width = columns * cell_w
    height = rows * cell_h
    pixels = blank_pixels(width, height, (20, 20, 30))

    for index, tile in enumerate(tiles):
        x = (index % columns) * cell_w + 3
        y = (index // columns) * cell_h + 3
        draw_frame(pixels, width, x - 2, y - 2, TILE_WIDTH * scale + 4, TILE_HEIGHT * scale + 4, (95, 95, 120))
        blit_scaled(pixels, width, x, y, cpc_mode1_tile_pixels(tile.raw_bytes), scale)

    write_bmp(path, width, height, pixels)


def plus_sprite_pixels(sprite: PlusSpriteAsset) -> list[list[int]]:
    rows: list[list[int]] = []
    for y in range(sprite.height):
        start = y * sprite.width
        rows.append(sprite.raw_values[start : start + sprite.width])
    return rows


def render_plus_sprite_contact_sheet(path: Path, sprites: list[PlusSpriteAsset]) -> None:
    scale = 3
    columns = 4
    cell_w = 16 * scale + 16
    max_height = max((sprite.height for sprite in sprites), default=16)
    cell_h = max_height * scale + 16
    rows = math.ceil(len(sprites) / columns) if sprites else 1
    width = columns * cell_w
    height = rows * cell_h
    pixels = blank_pixels(width, height, (24, 24, 32))

    for index, sprite in enumerate(sprites):
        x = (index % columns) * cell_w + 8
        y = (index // columns) * cell_h + 8
        draw_frame(pixels, width, x - 3, y - 3, sprite.width * scale + 6, sprite.height * scale + 6, (110, 110, 140))
        blit_scaled(pixels, width, x, y, plus_sprite_pixels(sprite), scale, transparent_pen=0)

    write_bmp(path, width, height, pixels)


def combine_plus_halves(left: PlusSpriteAsset, right: PlusSpriteAsset) -> list[list[int]]:
    height = min(left.height, right.height)
    left_pixels = plus_sprite_pixels(left)
    right_pixels = plus_sprite_pixels(right)
    rows: list[list[int]] = []
    for y in range(height):
        rows.append(left_pixels[y][:8] + right_pixels[y][:8])
    return rows


def render_combat_sprite_audit_sheet(path: Path, tiles: list[TileAsset], sprites: list[PlusSpriteAsset]) -> None:
    sprites_by_id = {sprite.asset_id: sprite for sprite in sprites}
    scale = 4
    items: list[tuple[str, list[list[int]]]] = []

    def add_plus_pair(title: str, left_id: str, right_id: str) -> None:
        left = sprites_by_id.get(left_id)
        right = sprites_by_id.get(right_id)
        if left and right:
            items.append((title, combine_plus_halves(left, right)))

    def add_tile(title: str, tile_id: int) -> None:
        if 0 <= tile_id < len(tiles):
            items.append((title, cpc_mode1_tile_pixels(tiles[tile_id].raw_bytes)))

    add_plus_pair("harrier_flying_plus", "sprite_pixel_data1", "sprite_pixel_data2")
    add_plus_pair("harrier_landing_plus", "sprite_pixel_data4", "sprite_pixel_data3")
    add_plus_pair("enemy_plane_plus", "sprite_pixel_data5", "sprite_pixel_data6")
    add_plus_pair("enemy_broken_plus", "sprite_pixel_data7", "sprite_pixel_data8")
    add_plus_pair("wingman_flying_plus", "sprite_pixel_data_wingmanflying1", "sprite_pixel_data_wingmanflying2")
    add_plus_pair("wingman_landed_plus", "sprite_pixel_data_wingmanlanded1", "sprite_pixel_data_wingmanlanded2")
    add_tile("bomb_launched_tile40", 40)
    add_tile("bomb_descending_tile41", 41)
    add_tile("missile_left_tile55", 55)
    add_tile("missile_right_tile56", 56)
    add_tile("missile_up_right_tile98", 98)
    add_tile("missile_down_right_tile99", 99)
    add_tile("missile_down_tile100", 100)
    add_tile("missile_up_tile101", 101)

    columns = 5
    cell_w = 80
    cell_h = 58
    rows = math.ceil(len(items) / columns) if items else 1
    width = columns * cell_w
    height = rows * cell_h
    pixels = blank_pixels(width, height, (18, 18, 26))

    for index, (_title, item_pixels) in enumerate(items):
        x = (index % columns) * cell_w + 10
        y = (index // columns) * cell_h + 10
        draw_frame(pixels, width, x - 3, y - 3, len(item_pixels[0]) * scale + 6, len(item_pixels) * scale + 6, (110, 110, 145))
        blit_scaled(pixels, width, x, y, item_pixels, scale, transparent_pen=0)

    write_bmp(path, width, height, pixels)


def render_tile_id_grid(
    grid: list[list[int]],
    tiles: list[TileAsset],
    tile_base: int,
) -> list[list[int]]:
    if not grid:
        return [[0] * TILE_WIDTH for _ in range(TILE_HEIGHT)]
    output_height = len(grid) * TILE_HEIGHT
    output_width = max(len(row) for row in grid) * TILE_WIDTH
    pixels = [[0 for _ in range(output_width)] for _ in range(output_height)]
    for grid_y, row in enumerate(grid):
        for grid_x, tile_value in enumerate(row):
            tile_index = tile_value - tile_base
            if tile_value == 0:
                tile_pixels = cpc_mode1_tile_pixels(tiles[0].raw_bytes)
            elif 0 <= tile_index < len(tiles):
                tile_pixels = cpc_mode1_tile_pixels(tiles[tile_index].raw_bytes)
            else:
                tile_pixels = [[9 if (x + y) % 2 else 10 for x in range(TILE_WIDTH)] for y in range(TILE_HEIGHT)]
            for y in range(TILE_HEIGHT):
                for x in range(TILE_WIDTH):
                    pixels[grid_y * TILE_HEIGHT + y][grid_x * TILE_WIDTH + x] = tile_pixels[y][x]
    return pixels


def object_grids_for_preview(obj: ObjectAsset) -> list[list[list[int]]]:
    data = list(obj.raw_bytes)
    if obj.kind == "composite.column_major":
        if data and data[-1] == 0xFF:
            data = data[:-1]
        return [grid_from_column_major(data, int(obj.layout.get("height", 1)))]
    if obj.kind == "records.2x2_terminated":
        return [grid_from_row_major(record, int(obj.layout.get("record_width", 2))) for record in split_terminated_records(data)]
    if obj.kind == "objectmap.row":
        return [grid_from_row_major(data, int(obj.layout.get("width", len(data) or 1)))]
    if obj.kind.startswith("raw.drawspriteblock3"):
        if data and data[-1] == 0xFF:
            data = data[:-1]
        if obj.kind == "raw.drawspriteblock3_column_stream":
            return [grid_from_column_major(data, int(obj.layout.get("column_height", 5)))]
        return [grid_from_row_major(data, int(obj.layout.get("preview_width", obj.layout.get("width", 5))))]
    return []


def render_object_contact_sheet(path: Path, objects: list[ObjectAsset], tiles: list[TileAsset], tile_base: int) -> None:
    scale = 2
    cell_w = 150
    cell_h = 78
    columns = 3
    preview_items: list[tuple[ObjectAsset, list[list[int]]]] = []
    for obj in objects:
        for grid in object_grids_for_preview(obj):
            preview_items.append((obj, grid))
    rows = math.ceil(len(preview_items) / columns) if preview_items else 1
    width = columns * cell_w
    height = rows * cell_h
    pixels = blank_pixels(width, height, (18, 18, 25))

    for index, (_obj, grid) in enumerate(preview_items):
        x = (index % columns) * cell_w + 8
        y = (index // columns) * cell_h + 8
        rendered = render_tile_id_grid(grid, tiles, tile_base)
        draw_frame(pixels, width, x - 3, y - 3, min(len(rendered[0]) * scale + 6, cell_w - 12), min(len(rendered) * scale + 6, cell_h - 12), (100, 100, 130))
        blit_scaled(pixels, width, x, y, rendered, scale)

    write_bmp(path, width, height, pixels)


def write_readme(path: Path) -> None:
    text = """# Generated CPC asset audit

This folder is generated by `tools/extract_cpc_assets.py` via `extract-cpc-assets.ps1`.

The files are intentionally review assets, not automatic runtime replacements yet:

- `cpc_asset_manifest.json` is the top-level index.
- `cpc_tiles.json` / `cpc_tiles_index.csv` contain CPC `spritelookuptable` tiles.
- `cpc_plus_sprites.json` / `cpc_plus_sprites_index.csv` contain `sprite_pixel_data*` pen-index sprites.
- `cpc_object_blocks.json` / `cpc_object_blocks_index.csv` contain composite/object-map data found in the main CPC source.
- `previews/*.bmp` are contact sheets for visual QA.
- `binary/*.bpl` are Amiga-friendly bitplane dumps, but the current game build does not consume them automatically.

For combat graphics, start with `previews/cpc_combat_sprites_audit.bmp`.
It composes the two-half CPC Plus aircraft sprites and shows the CPC tile
missiles/bombs used by the current Amiga runtime.

Two object contact sheets are generated:

- `cpc_object_blocks_tiles_raw.bmp`: object bytes are treated as zero-based tile indices.
- `cpc_object_blocks_tiles_minus1.bmp`: object bytes are treated as one-based tile indices.

That deliberate duplication helps catch CPC table off-by-one conventions before we wire assets into gameplay.
"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def write_outputs(
    out_root: Path,
    cpc_font_source: Path,
    cpc_main_source: Path,
    tiles: list[TileAsset],
    plus_sprites: list[PlusSpriteAsset],
    objects: list[ObjectAsset],
) -> None:
    binary_dir = out_root / "binary"
    preview_dir = out_root / "previews"
    binary_dir.mkdir(parents=True, exist_ok=True)
    preview_dir.mkdir(parents=True, exist_ok=True)

    tile_bpl = bytearray()
    for tile in tiles:
        tile_bpl.extend(pixels_to_bitplanes(cpc_mode1_tile_pixels(tile.raw_bytes), TILE_PLANES))
    (binary_dir / "cpc_tiles_8x8_5p.bpl").write_bytes(tile_bpl)

    plus_bpl = bytearray()
    for sprite in plus_sprites:
        sprite.bitplane_offset = len(plus_bpl)
        sprite_pixels = plus_sprite_pixels(sprite)
        bitplanes = pixels_to_bitplanes(sprite_pixels, 4)
        sprite.bitplane_size = len(bitplanes)
        plus_bpl.extend(bitplanes)
    (binary_dir / "cpc_plus_sprites_16w_4p.bpl").write_bytes(plus_bpl)

    render_tile_contact_sheet(preview_dir / "cpc_tiles_contact_sheet.bmp", tiles)
    render_plus_sprite_contact_sheet(preview_dir / "cpc_plus_sprites_contact_sheet.bmp", plus_sprites)
    render_combat_sprite_audit_sheet(preview_dir / "cpc_combat_sprites_audit.bmp", tiles, plus_sprites)
    render_object_contact_sheet(preview_dir / "cpc_object_blocks_tiles_raw.bmp", objects, tiles, tile_base=0)
    render_object_contact_sheet(preview_dir / "cpc_object_blocks_tiles_minus1.bmp", objects, tiles, tile_base=1)

    tile_dicts = [tile_summary(tile, cpc_font_source.name) | {"pixels": cpc_mode1_tile_pixels(tile.raw_bytes)} for tile in tiles]
    plus_dicts = [plus_sprite_summary(sprite, cpc_font_source.name) | {"pixels": plus_sprite_pixels(sprite)} for sprite in plus_sprites]
    object_dicts = [object_summary(obj, cpc_main_source.name) for obj in objects]

    write_json(out_root / "cpc_tiles.json", tile_dicts)
    write_json(out_root / "cpc_plus_sprites.json", plus_dicts)
    write_json(out_root / "cpc_object_blocks.json", object_dicts)

    write_csv(
        out_root / "cpc_tiles_index.csv",
        ["table_index", "label", "source_line", "cpc_comment_id", "name", "category", "raw_bytes_hex"],
        [
            {
                "table_index": tile.table_index,
                "label": tile.label,
                "source_line": tile.source_line,
                "cpc_comment_id": "" if tile.cpc_comment_id is None else tile.cpc_comment_id,
                "name": tile.friendly_name,
                "category": tile.category,
                "raw_bytes_hex": " ".join(f"{value:02x}" for value in tile.raw_bytes),
            }
            for tile in tiles
        ],
    )
    write_csv(
        out_root / "cpc_plus_sprites_index.csv",
        ["asset_id", "label", "source_line", "name", "width", "height", "category", "bitplane_offset", "bitplane_size", "comments"],
        [
            {
                "asset_id": sprite.asset_id,
                "label": sprite.label,
                "source_line": sprite.source_line,
                "name": sprite.friendly_name,
                "width": sprite.width,
                "height": sprite.height,
                "category": sprite.category,
                "bitplane_offset": sprite.bitplane_offset,
                "bitplane_size": sprite.bitplane_size,
                "comments": " | ".join(sprite.comments),
            }
            for sprite in plus_sprites
        ],
    )
    write_csv(
        out_root / "cpc_object_blocks_index.csv",
        ["asset_id", "label", "source_line", "kind", "byte_count", "layout", "risk"],
        [
            {
                "asset_id": obj.asset_id,
                "label": obj.label,
                "source_line": obj.source_line,
                "kind": obj.kind,
                "byte_count": len(obj.raw_bytes),
                "layout": json.dumps(obj.layout, separators=(",", ":")),
                "risk": obj.risk,
            }
            for obj in objects
        ],
    )

    manifest = {
        "schema_version": 1,
        "generated_by": "tools/extract_cpc_assets.py",
        "purpose": "CPC source asset audit for Amiga 500 port; outputs are candidates until promoted into runtime assets.",
        "source_files": {
            "font_and_tile_source": str(cpc_font_source),
            "game_logic_source": str(cpc_main_source),
        },
        "counts": {
            "cpc_tiles": len(tiles),
            "cpc_plus_sprites": len(plus_sprites),
            "object_blocks": len(objects),
        },
        "outputs": {
            "tiles_json": "cpc_tiles.json",
            "tiles_csv": "cpc_tiles_index.csv",
            "plus_sprites_json": "cpc_plus_sprites.json",
            "plus_sprites_csv": "cpc_plus_sprites_index.csv",
            "objects_json": "cpc_object_blocks.json",
            "objects_csv": "cpc_object_blocks_index.csv",
            "tile_bitplanes": "binary/cpc_tiles_8x8_5p.bpl",
            "plus_sprite_bitplanes": "binary/cpc_plus_sprites_16w_4p.bpl",
            "previews": [
                "previews/cpc_tiles_contact_sheet.bmp",
                "previews/cpc_plus_sprites_contact_sheet.bmp",
                "previews/cpc_combat_sprites_audit.bmp",
                "previews/cpc_object_blocks_tiles_raw.bmp",
                "previews/cpc_object_blocks_tiles_minus1.bmp",
            ],
        },
        "promotion_policy": [
            "Do not overwrite runtime assets automatically.",
            "Use contact sheets and JSON provenance to choose CPC-faithful assets.",
            "If a CPC asset needs cleanup, keep the original record and add a separate native/polished derivative later.",
        ],
        "pitfalls": [
            "CPC object data sometimes uses tile ids with different base conventions; compare both object preview sheets.",
            "drawspriteblock3 streams are stored raw until fully decoded.",
            "CPC Plus sprite_pixel_data uses pen indices, not packed Mode 1 screen bytes.",
            "Pen 0 is transparent for Plus-sprite previews but may mean sky/background in tile data.",
            "Some CPC visuals relied on palette/raster effects; raw tile promotion may still need Amiga-native cleanup.",
        ],
        "assets": {
            "tiles": [tile_summary(tile, cpc_font_source.name) for tile in tiles],
            "plus_sprites": [plus_sprite_summary(sprite, cpc_font_source.name) for sprite in plus_sprites],
            "object_blocks": [object_summary(obj, cpc_main_source.name) for obj in objects],
        },
    }
    write_json(out_root / "cpc_asset_manifest.json", manifest)
    write_readme(out_root / "README.md")


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract CPC Harrier Attack graphics into reviewable Amiga-port assets.")
    parser.add_argument("--font-source", type=Path, default=Path("AMSTRADFONT3.asm"))
    parser.add_argument("--main-source", type=Path, default=Path("HarrierAttackSourceNew2_alt_CRTC_CART16.asm"))
    parser.add_argument("--out-root", type=Path, default=Path("amiga/assets/generated/cpc"))
    args = parser.parse_args()

    font_lines = read_source_lines(args.font_source)
    main_lines = read_source_lines(args.main_source)
    RGB_PALETTE[:] = load_real_rgb_palette(args.main_source)

    tiles = extract_tiles(font_lines)
    plus_sprites = extract_plus_sprites(font_lines)
    objects = extract_objects(main_lines)

    if not tiles:
        raise SystemExit(f"No CPC tiles found in {args.font_source}")
    if not plus_sprites:
        raise SystemExit(f"No CPC Plus sprite_pixel_data labels found in {args.font_source}")
    if not objects:
        raise SystemExit(f"No CPC object blocks found in {args.main_source}")

    write_outputs(args.out_root, args.font_source, args.main_source, tiles, plus_sprites, objects)

    print(f"Wrote CPC asset manifest: {args.out_root / 'cpc_asset_manifest.json'}")
    print(f"Extracted {len(tiles)} tiles, {len(plus_sprites)} Plus sprites, {len(objects)} object/composite blocks")
    print(f"Preview sheets: {args.out_root / 'previews'}")


if __name__ == "__main__":
    main()
