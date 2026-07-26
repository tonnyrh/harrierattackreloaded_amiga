#!/usr/bin/env python3
"""Convert Harrier CPC Mode 0 screen data to Amiga interleaved bitplanes."""

from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

CPC_WIDTH = 160
CPC_HEIGHT = 200
CPC_BYTES_PER_LINE = 80
CPC_SCREEN_SPAN = 0x3FD0
AMIGA_WIDTH = 320
AMIGA_HEIGHT = 200
AMIGA_PLANES = 5
AMIGA_ROW_BYTES = AMIGA_WIDTH // 8


def parse_int(token: str) -> int:
    token = token.strip()
    if token.startswith("&") or token.startswith("#"):
        return int(token[1:], 16)
    if token.startswith("%"):
        return int(token[1:], 2)
    return int(token, 0)


def parse_palette(path: Path, label: str, count: int) -> list[int]:
    text = path.read_text(encoding="utf-8", errors="ignore")
    lines = text.splitlines()
    start = None
    for index, line in enumerate(lines):
        if re.match(rf"\s*{re.escape(label)}\s*:", line, re.IGNORECASE):
            start = index + 1
            break

    if start is None:
        raise ValueError(f"Could not find palette label {label!r} in {path}")

    values: list[int] = []
    for line in lines[start:]:
        stripped = line.split(";", 1)[0].strip()
        if not stripped:
            continue
        if stripped.endswith(":") and values:
            break
        match = re.search(r"\bdefw\b(.+)$", stripped, re.IGNORECASE)
        if not match:
            continue
        for token in match.group(1).split(","):
            if token.strip():
                values.append(parse_int(token))
                if len(values) == count:
                    return values

    raise ValueError(f"Found only {len(values)} palette entries in {path}; expected {count}")


def cpc_plus_grb_to_amiga_rgb(value: int) -> int:
    green = (value >> 8) & 0x0F
    red = (value >> 4) & 0x0F
    blue = value & 0x0F
    return (red << 8) | (green << 4) | blue


def decode_mode0_byte(value: int) -> tuple[int, int]:
    left = (
        ((value >> 7) & 1)
        | (((value >> 3) & 1) << 1)
        | (((value >> 5) & 1) << 2)
        | (((value >> 1) & 1) << 3)
    )
    right = (
        ((value >> 6) & 1)
        | (((value >> 2) & 1) << 1)
        | (((value >> 4) & 1) << 2)
        | (((value >> 0) & 1) << 3)
    )
    return left, right


def cpc_offset(y: int, byte_x: int) -> int:
    return ((y & 7) * 0x800) + ((y >> 3) * CPC_BYTES_PER_LINE) + byte_x


def decode_cpc_pixels(data: bytes) -> list[int]:
    if len(data) < CPC_SCREEN_SPAN:
        raise ValueError(f"CPC screen data is {len(data)} bytes; expected at least {CPC_SCREEN_SPAN}")

    pixels: list[int] = []
    for y in range(CPC_HEIGHT):
        for byte_x in range(CPC_BYTES_PER_LINE):
            left, right = decode_mode0_byte(data[cpc_offset(y, byte_x)])
            pixels.extend((left, right))
    return pixels


def double_width_pixels(cpc_pixels: list[int]) -> list[int]:
    amiga_pixels: list[int] = []
    for y in range(CPC_HEIGHT):
        row = cpc_pixels[y * CPC_WIDTH : (y + 1) * CPC_WIDTH]
        for color in row:
            amiga_pixels.extend((color, color))
    return amiga_pixels


def pack_interleaved_bitplanes(pixels: list[int]) -> bytes:
    expected = AMIGA_WIDTH * AMIGA_HEIGHT
    if len(pixels) != expected:
        raise ValueError(f"Expected {expected} Amiga pixels, got {len(pixels)}")

    output = bytearray(AMIGA_HEIGHT * AMIGA_PLANES * AMIGA_ROW_BYTES)
    for y in range(AMIGA_HEIGHT):
        row_base = y * AMIGA_PLANES * AMIGA_ROW_BYTES
        for x in range(AMIGA_WIDTH):
            color = pixels[y * AMIGA_WIDTH + x]
            bit = 0x80 >> (x & 7)
            byte_index = x >> 3
            for plane in range(AMIGA_PLANES):
                if color & (1 << plane):
                    output[row_base + plane * AMIGA_ROW_BYTES + byte_index] |= bit
    return bytes(output)


def write_palette(path: Path, colors: list[int]) -> None:
    padded = colors[:16] + [0] * (32 - len(colors[:16]))
    path.write_bytes(b"".join(struct.pack(">H", color) for color in padded))


def write_bmp(path: Path, pixels: list[int], palette: list[int]) -> None:
    rgb = []
    for color in palette[:16]:
        red = ((color >> 8) & 0x0F) * 17
        green = ((color >> 4) & 0x0F) * 17
        blue = (color & 0x0F) * 17
        rgb.append((red, green, blue))

    row_size = ((AMIGA_WIDTH * 3 + 3) // 4) * 4
    bitmap = bytearray()
    for y in range(AMIGA_HEIGHT - 1, -1, -1):
        row = bytearray()
        for x in range(AMIGA_WIDTH):
            red, green, blue = rgb[pixels[y * AMIGA_WIDTH + x] & 0x0F]
            row += bytes((blue, green, red))
        row += b"\x00" * (row_size - len(row))
        bitmap += row

    header_size = 14 + 40
    size = header_size + len(bitmap)
    with path.open("wb") as file:
        file.write(b"BM")
        file.write(struct.pack("<IHHI", size, 0, 0, header_size))
        file.write(struct.pack("<IIIHHIIIIII", 40, AMIGA_WIDTH, AMIGA_HEIGHT, 1, 24, 0, len(bitmap), 2835, 2835, 0, 0))
        file.write(bitmap)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=Path("compile/HARRSCR.bin"))
    parser.add_argument("--palette-source", type=Path, default=Path("boot2.asm"))
    parser.add_argument("--palette-label", default="palette")
    parser.add_argument("--out-bpl", type=Path, default=Path("amiga/assets/loading_screen.bpl"))
    parser.add_argument("--out-pal", type=Path, default=Path("amiga/assets/loading_screen.pal"))
    parser.add_argument("--preview-bmp", type=Path, default=Path("amiga/assets/loading_screen_preview.bmp"))
    args = parser.parse_args()

    cpc_data = args.input.read_bytes()
    cpc_palette = parse_palette(args.palette_source, args.palette_label, 16)
    amiga_palette = [cpc_plus_grb_to_amiga_rgb(color) for color in cpc_palette]
    cpc_pixels = decode_cpc_pixels(cpc_data)
    amiga_pixels = double_width_pixels(cpc_pixels)
    bitplanes = pack_interleaved_bitplanes(amiga_pixels)

    args.out_bpl.parent.mkdir(parents=True, exist_ok=True)
    args.out_pal.parent.mkdir(parents=True, exist_ok=True)
    args.preview_bmp.parent.mkdir(parents=True, exist_ok=True)

    args.out_bpl.write_bytes(bitplanes)
    write_palette(args.out_pal, amiga_palette)
    write_bmp(args.preview_bmp, amiga_pixels, amiga_palette)

    print(f"Wrote {args.out_bpl} ({len(bitplanes)} bytes)")
    print(f"Wrote {args.out_pal} (32 colors)")
    print(f"Wrote {args.preview_bmp}")


if __name__ == "__main__":
    main()
