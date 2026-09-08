#!/usr/bin/env python3
"""Pack the indexed 320x200 loading-screen master for the Amiga runtime."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

from PIL import Image


WIDTH = 320
HEIGHT = 200
PLANES = 5
ROW_BYTES = WIDTH // 8
EXPECTED_BYTES = HEIGHT * PLANES * ROW_BYTES


def load_master(path: Path) -> tuple[Image.Image, list[int]]:
    image = Image.open(path)
    image.load()
    if image.mode != "P":
        raise ValueError(f"{path} must be an indexed PNG (mode P)")
    if image.size != (WIDTH, HEIGHT):
        raise ValueError(f"{path} must be exactly {WIDTH}x{HEIGHT}")
    pixel_data = (image.get_flattened_data() if
                  hasattr(image, "get_flattened_data") else image.getdata())
    pixels = [int(pixel) for pixel in pixel_data]
    invalid = sorted({pixel for pixel in pixels if pixel < 0 or pixel >= 32})
    if invalid:
        raise ValueError(f"{path} uses palette indices outside 0..31: {invalid}")
    palette = image.getpalette()
    if palette is None or len(palette) < 32 * 3:
        raise ValueError(f"{path} does not contain a complete 32-colour palette")
    for component in palette[:32 * 3]:
        if component % 17:
            raise ValueError(
                f"{path} contains a colour that is not on the OCS 4-bit RGB grid"
            )
    return image, pixels


def palette_bytes(image: Image.Image) -> bytes:
    palette = image.getpalette()
    assert palette is not None
    output = bytearray()
    for index in range(32):
        red, green, blue = palette[index * 3 : index * 3 + 3]
        colour = ((red // 17) << 8) | ((green // 17) << 4) | (blue // 17)
        output.extend(struct.pack(">H", colour))
    return bytes(output)


def bitplane_bytes(pixels: list[int]) -> bytes:
    output = bytearray(EXPECTED_BYTES)
    for y in range(HEIGHT):
        row_base = y * PLANES * ROW_BYTES
        for x in range(WIDTH):
            pen = pixels[y * WIDTH + x]
            bit = 0x80 >> (x & 7)
            byte_x = x >> 3
            for plane in range(PLANES):
                if pen & (1 << plane):
                    output[row_base + plane * ROW_BYTES + byte_x] |= bit
    return bytes(output)


def decode_bitplanes(data: bytes) -> list[int]:
    if len(data) != EXPECTED_BYTES:
        raise ValueError(f"Expected {EXPECTED_BYTES} packed bytes, got {len(data)}")
    pixels = [0] * (WIDTH * HEIGHT)
    for y in range(HEIGHT):
        row_base = y * PLANES * ROW_BYTES
        for x in range(WIDTH):
            bit = 0x80 >> (x & 7)
            byte_x = x >> 3
            pen = 0
            for plane in range(PLANES):
                if data[row_base + plane * ROW_BYTES + byte_x] & bit:
                    pen |= 1 << plane
            pixels[y * WIDTH + x] = pen
    return pixels


def write_if_changed(path: Path, data: bytes) -> bool:
    if path.is_file() and path.read_bytes() == data:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--out-bpl", type=Path,
                        default=Path("amiga/assets/loading_screen.bpl"))
    parser.add_argument("--out-pal", type=Path,
                        default=Path("amiga/assets/loading_screen.pal"))
    parser.add_argument("--preview-bmp", type=Path,
                        default=Path("amiga/assets/loading_screen_preview.bmp"))
    parser.add_argument("--check", action="store_true",
                        help="validate and require all generated files to be current")
    args = parser.parse_args()

    try:
        image, pixels = load_master(args.input)
        packed = bitplane_bytes(pixels)
        if decode_bitplanes(packed) != pixels:
            raise ValueError("Internal five-plane round-trip mismatch")
        palette = palette_bytes(image)

        stale = []
        for path, data in ((args.out_bpl, packed), (args.out_pal, palette)):
            if not path.is_file() or path.read_bytes() != data:
                stale.append(path)
        preview_rgb = image.convert("RGB")
        if args.check:
            if stale:
                raise ValueError(
                    "stale loading-screen output: " + ", ".join(str(path) for path in stale)
                )
        else:
            write_if_changed(args.out_bpl, packed)
            write_if_changed(args.out_pal, palette)
            args.preview_bmp.parent.mkdir(parents=True, exist_ok=True)
            preview_rgb.save(args.preview_bmp, format="BMP")
    except (OSError, ValueError) as error:
        print(f"Loading-screen error: {error}", file=sys.stderr)
        return 1

    print(
        f"Loading screen valid: {WIDTH}x{HEIGHT}, 32 OCS colours, "
        f"{len(packed)} interleaved bytes"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
