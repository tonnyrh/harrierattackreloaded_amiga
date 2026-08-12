#!/usr/bin/env python3
"""Convert the carrier/gunship promoted CPC+ sprite pieces into pre-masked
planar Amiga tiles, so the runtime can blit them like ordinary game tiles
instead of drawing them pixel-by-pixel.

Each output tile is 40 bytes (8 rows x 5 bytes/row), the exact same footprint
as a regular amiga/assets/game_tiles.bpl tile - but where a regular tile's 5th
plane byte is always zero (game tiles never need it, see Sprint 14.94 Part 5),
this repurposes that same byte as a per-row 1-bit opacity mask, since unlike
flat terrain tiles the carrier/gunship art has real transparency (background
sea/sky showing through) mixed within individual 8x8 tile cells - confirmed by
directly scanning the source pixel data before writing this script, not
assumed. See drawGameScrollTileMasked() in amiga/main.c for the matching
runtime blit (masked read-modify-write per row instead of a flat overwrite).

Input is a temporary audit generated from the external CPC reference checkout.
The resulting promoted_sprite_tiles.h is the authoritative Amiga build asset.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

TILE_SIZE = 8
DISPLAY_PLANES = 4
TILE_BYTES = TILE_SIZE * (DISPLAY_PLANES + 1)

# Source of truth for CPC Plus pen -> existing 4-plane playfield colour.
# This table is also emitted into the generated C header, so the runtime
# pixel path and the pre-baked carrier/gunship tiles cannot drift apart.
#
# CPC Plus pens 1-6 are greys 111,333,555,777,AAA,FFF. Quantise that ramp
# to the playfield's existing black/dark/mid/light/white colours. In
# particular, pen 5 is bright grey and must never map to black.
PEN_TO_GAME_COLOR = {
    0: 0, 1: 10, 2: 4, 3: 4, 4: 3, 5: 2, 6: 1, 7: 5, 8: 5,
    9: 9, 10: 10, 11: 4, 12: 1, 13: 1, 14: 15,
}
DEFAULT_GAME_COLOR = 1  # matches the C switch's `default: return GAME_COLOR_WHITE;`


def pen_to_color(pen: int) -> int:
    return PEN_TO_GAME_COLOR.get(pen, DEFAULT_GAME_COLOR)


class Canvas:
    def __init__(self, width: int, height: int):
        self.width = width
        self.height = height
        self.pens = [[0] * width for _ in range(height)]

    def blit(self, pixels: list[list[int]], width: int, height: int, ox: int, oy: int, xscale: int) -> None:
        for y in range(height):
            row = pixels[y]
            for x in range(width):
                pen = row[x] & 15
                if not pen:
                    continue
                for sx in range(xscale):
                    cx = ox + x * xscale + sx
                    cy = oy + y
                    if 0 <= cx < self.width and 0 <= cy < self.height:
                        self.pens[cy][cx] = pen

    def tile_bytes(self, tile_x: int, tile_y: int) -> bytes:
        out = bytearray(TILE_BYTES)
        for row in range(TILE_SIZE):
            py = tile_y * TILE_SIZE + row
            mask_byte = 0
            plane_bytes = [0] * DISPLAY_PLANES
            for col in range(TILE_SIZE):
                px = tile_x * TILE_SIZE + col
                pen = self.pens[py][px] if 0 <= py < self.height and 0 <= px < self.width else 0
                if not pen:
                    continue
                bit = 0x80 >> col
                mask_byte |= bit
                color = pen_to_color(pen)
                for plane in range(DISPLAY_PLANES):
                    if color & (1 << plane):
                        plane_bytes[plane] |= bit
            base = row * (DISPLAY_PLANES + 1)
            for plane in range(DISPLAY_PLANES):
                out[base + plane] = plane_bytes[plane]
            out[base + DISPLAY_PLANES] = mask_byte
        return bytes(out)

    def tiles_wide(self) -> int:
        assert self.width % TILE_SIZE == 0
        return self.width // TILE_SIZE

    def tiles_tall(self) -> int:
        assert self.height % TILE_SIZE == 0
        return self.height // TILE_SIZE

    def mirrored(self) -> "Canvas":
        """Full pixel-level horizontal flip of the assembled composite - used
        for the end-carrier's reversed sprite (CPC's endfrigatesprite comment:
        "FRIGATE REVERSED, SO IT CAN COME IN SCREEN FROM OPPOSITE SIDE").
        Flipping the whole finished image (rather than re-placing pieces
        mirrored) guarantees a pixel-perfect mirror without having to reason
        about which piece goes on which side once reversed."""
        out = Canvas(self.width, self.height)
        for y in range(self.height):
            src = self.pens[y]
            out.pens[y] = [src[self.width - 1 - x] for x in range(self.width)]
        return out


def format_bytes(data: bytes, indent: str = "\t") -> str:
    lines = []
    for index in range(0, len(data), 16):
        chunk = data[index : index + 16]
        lines.append(indent + ", ".join(str(value) for value in chunk))
    return ",\n".join(lines)


def format_ints(values: list[int], indent: str = "\t") -> str:
    lines = []
    for index in range(0, len(values), 16):
        chunk = values[index : index + 16]
        lines.append(indent + ", ".join(str(value) for value in chunk))
    return ",\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=Path(".tmp/cpc-asset-audit/cpc_plus_sprites.json"))
    parser.add_argument("--output", type=Path, default=Path("amiga/assets/promoted_sprite_tiles.h"))
    args = parser.parse_args()

    sprites = json.loads(args.input.read_text(encoding="utf-8"))
    by_id = {sprite["asset_id"].lower(): sprite for sprite in sprites}

    def piece(source_id: str) -> tuple[int, int, list[list[int]]]:
        sprite = by_id[source_id.lower()]
        return sprite["width"], sprite["height"], sprite["pixels"]

    # Carrier canvas: matches drawPromotedCpcCarrierRangeAt()'s exact relative
    # offsets (back/body/front at y+32, top/top2 at y+16 relative to the
    # caller's own y=80 base - shifted up by 16 here so the canvas itself
    # starts at row 0; the runtime blit re-adds that 16px/2-tile offset when
    # positioning the composite). body uses xScale=2 there, everything else 1.
    carrier = Canvas(96, 24)
    w, h, px = piece("sprite_pixel_data11")  # back
    carrier.blit(px, w, h, 0, 16, 1)
    w, h, px = piece("sprite_pixel_data10")  # body (drawn twice, x+16 and x+48)
    carrier.blit(px, w, h, 16, 16, 2)
    carrier.blit(px, w, h, 48, 16, 2)
    w, h, px = piece("sprite_pixel_data13")  # top
    carrier.blit(px, w, h, 40, 0, 1)
    w, h, px = piece("sprite_pixel_data14")  # top_2
    carrier.blit(px, w, h, 56, 0, 1)
    w, h, px = piece("sprite_pixel_data12")  # front
    carrier.blit(px, w, h, 80, 16, 1)

    # Build the reversed carrier body before adding the separate aircraft.
    # CPC reverses the final carrier, but the parked/landing Harrier remains
    # oriented in the player's landing direction; it is not part of the
    # reversed endfrigatesprite.
    carrier_without_wingman = carrier.mirrored().mirrored()
    carrier_reversed = carrier.mirrored()
    carrier_reversed_without_wingman = carrier_reversed.mirrored().mirrored()

    # CPC movefrigateonscreen places the separate grey landed wingman on the
    # forward deck (sprites 14/15 loaded from wingmanlanded1/2). These are
    # direct CPC Plus pen-index sprites, not Mode 0/1 packed graphics. Their
    # two visible 8px halves are combined exactly like
    # buildAttachedSpriteFromCpcPlusHalves() in the Amiga runtime.
    _, h_left, left = piece("sprite_pixel_data_wingmanlanded1")
    _, h_right, right = piece("sprite_pixel_data_wingmanlanded2")
    second_harrier = [left[y][:8] + right[y][:8] for y in range(min(h_left, h_right))]
    carrier.blit(second_harrier, 16, len(second_harrier), 73, 8, 1)
    # Mirror only its deck position (73 -> 96-73-16 = 7), not its pixels.
    carrier_reversed.blit(second_harrier, 16, len(second_harrier), 7, 8, 1)

    # Gunship canvas: left/right side by side, matches
    # drawPromotedCpcGunshipRangeAt()'s (x+0)/(x+16) offsets exactly.
    gunship = Canvas(32, 16)
    w, h, px = piece("sprite_pixel_data15")  # gunship_left
    gunship.blit(px, w, h, 0, 0, 1)
    w, h, px = piece("sprite_pixel_data16")  # gunship_right
    gunship.blit(px, w, h, 16, 0, 1)

    lines: list[str] = [
        "/* Converted Amiga runtime assets. Do not hand-edit.",
        "   Generated from a temporary external CPC audit. Pre-masked planar",
        "   tiles for the carrier/gunship promoted sprites - see",
        "   drawGameScrollTileMasked() in amiga/main.c for the matching runtime blit. */",
        "#ifndef HAR_CPC_PROMOTED_SPRITE_TILES_H",
        "#define HAR_CPC_PROMOTED_SPRITE_TILES_H",
        "",
        "static const UBYTE harCpcPlusPenToGameColor[16] = {",
        format_ints([PEN_TO_GAME_COLOR.get(pen, DEFAULT_GAME_COLOR) for pen in range(16)]),
        "};",
        "",
        f"#define HAR_CARRIER_TILE_BYTES {TILE_BYTES}",
        f"#define HAR_CARRIER_TILES_WIDE {carrier.tiles_wide()}",
        f"#define HAR_CARRIER_TILES_TALL {carrier.tiles_tall()}",
        f"#define HAR_GUNSHIP_TILES_WIDE {gunship.tiles_wide()}",
        f"#define HAR_GUNSHIP_TILES_TALL {gunship.tiles_tall()}",
        "",
    ]

    def emit_canvas(name: str, canvas: Canvas) -> None:
        all_tile_bytes = bytearray()
        skip_flags: list[int] = []
        for ty in range(canvas.tiles_tall()):
            for tx in range(canvas.tiles_wide()):
                tile = canvas.tile_bytes(tx, ty)
                fully_transparent = all(b == 0 for b in tile)
                skip_flags.append(1 if fully_transparent else 0)
                all_tile_bytes.extend(tile)
        lines.append(f"static const UBYTE har{name}TileData[{len(all_tile_bytes)}] = {{")
        lines.append(format_bytes(bytes(all_tile_bytes)))
        lines.append("};")
        lines.append("")
        lines.append(f"/* 1 = this grid cell is fully transparent end to end - skip drawing it entirely. */")
        lines.append(f"static const UBYTE har{name}TileSkip[{len(skip_flags)}] = {{")
        lines.append(format_ints(skip_flags))
        lines.append("};")
        lines.append("")

    emit_canvas("Carrier", carrier)
    emit_canvas("CarrierReversed", carrier_reversed)
    emit_canvas("CarrierWithoutWingman", carrier_without_wingman)
    emit_canvas("CarrierReversedWithoutWingman", carrier_reversed_without_wingman)
    emit_canvas("Gunship", gunship)

    lines.append("#endif")
    lines.append("")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {args.output}")
    print(f"Carrier: {carrier.tiles_wide()}x{carrier.tiles_tall()} tiles ({carrier.tiles_wide()*carrier.tiles_tall()} total)")
    print(f"Gunship: {gunship.tiles_wide()}x{gunship.tiles_tall()} tiles ({gunship.tiles_wide()*gunship.tiles_tall()} total)")


if __name__ == "__main__":
    main()
