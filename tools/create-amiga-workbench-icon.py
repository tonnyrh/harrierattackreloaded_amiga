#!/usr/bin/env python3
"""Create the classic Workbench tool icon used by the Amiga release.

The artwork is a deterministic 2x rendering of the checked-in CPC-derived
Harrier halves in amiga/assets/promoted_assets.h.  The .info writer emits the
Kickstart/Workbench 1.x DiskObject and Image structures in big-endian form;
no NewIcons or OS 3.5 icon.library extensions are required.
"""

from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

from PIL import Image


ICON_WIDTH = 48
ICON_HEIGHT = 22
WORKBENCH_PALETTE = [
    (0x00, 0x55, 0xAA),  # Workbench blue / transparent background pen
    (0xFF, 0xFF, 0xFF),  # white highlight
    (0x00, 0x00, 0x00),  # black outline
    (0xFF, 0x88, 0x00),  # orange accent
]


def read_c_array(header: str, name: str) -> list[int]:
    match = re.search(
        rf"static const UBYTE\s+{re.escape(name)}\[\d+\]\s*=\s*\{{(.*?)\}};",
        header,
        flags=re.S,
    )
    if not match:
        raise ValueError(f"Could not find {name} in promoted_assets.h")
    return [int(value) for value in re.findall(r"\d+", match.group(1))]


def create_source_png(header_path: Path, output_path: Path) -> None:
    header = header_path.read_text(encoding="ascii")
    left = read_c_array(header, "harCpcHarrierFlyingLeftPixels")
    right = read_c_array(header, "harCpcHarrierFlyingRightPixels")
    if len(left) != 128 or len(right) != 128:
        raise ValueError("Expected two 16x8 Harrier source halves")

    image = Image.new("P", (ICON_WIDTH, ICON_HEIGHT), 0)
    flat_palette: list[int] = []
    for red, green, blue in WORKBENCH_PALETTE:
        flat_palette.extend((red, green, blue))
    image.putpalette(flat_palette + [0] * (768 - len(flat_palette)))

    # Only the first eight pixels in each extracted half contain artwork.
    # Collapse the original CPC shades to the four standard Workbench pens.
    x_origin = (ICON_WIDTH - 32) // 2
    y_origin = (ICON_HEIGHT - 16) // 2
    for source_y in range(8):
        row = left[source_y * 16 : source_y * 16 + 8]
        row += right[source_y * 16 : source_y * 16 + 8]
        for source_x, cpc_pen in enumerate(row):
            if cpc_pen == 0:
                pen = 0
            elif cpc_pen == 6:
                pen = 3
            elif cpc_pen <= 9:
                pen = 2
            else:
                pen = 1
            for dy in range(2):
                for dx in range(2):
                    image.putpixel(
                        (x_origin + source_x * 2 + dx,
                         y_origin + source_y * 2 + dy),
                        pen,
                    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path, optimize=False)


def planar_image_data(image: Image.Image) -> bytes:
    if image.mode != "P" or image.size != (ICON_WIDTH, ICON_HEIGHT):
        raise ValueError("Workbench icon PNG must be indexed 48x22")
    if image.getextrema()[1] > 3:
        raise ValueError("Workbench icon PNG must use at most four pens")

    row_bytes = ((ICON_WIDTH + 15) // 16) * 2
    output = bytearray()
    # Intuition ImageData is plane-sequential, with every row word-padded.
    for plane in range(2):
        for y in range(ICON_HEIGHT):
            row = bytearray(row_bytes)
            for x in range(ICON_WIDTH):
                if (image.getpixel((x, y)) >> plane) & 1:
                    row[x // 8] |= 0x80 >> (x & 7)
            output.extend(row)
    return bytes(output)


def create_info(png_path: Path, info_path: Path) -> None:
    image = Image.open(png_path)
    pixels = planar_image_data(image)

    # struct DiskObject followed by one struct Image and its planar ImageData.
    gadget = struct.pack(
        ">IhhhhHHHIIIiIHI",
        0,                 # NextGadget
        0, 0,              # LeftEdge, TopEdge
        ICON_WIDTH, ICON_HEIGHT + 1,
        0x0004,            # GFLG_GADGIMAGE
        0x0001,            # GACT_RELVERIFY
        0x0001,            # GTYP_BOOLGADGET
        1,                 # GadgetRender follows on disk
        0,                 # no selected image; Workbench complements it
        0,                 # GadgetText
        0,                 # MutualExclude
        0,                 # SpecialInfo
        0,                 # GadgetID
        1,                 # UserData marker used by classic icon.library
    )
    disk_object = (
        struct.pack(">HH", 0xE310, 1) +
        gadget +
        struct.pack(
            ">BBIIiiIII",
            3, 0,           # WBTOOL, pad
            0, 0,           # DefaultTool, ToolTypes
            -0x80000000,    # NO_ICON_POSITION
            -0x80000000,
            0, 0,           # DrawerData, ToolWindow
            65536,          # safe Workbench process stack
        )
    )
    image_header = struct.pack(
        ">hhhhHIBBI",
        0, 0, ICON_WIDTH, ICON_HEIGHT, 2,
        1,                  # ImageData follows
        0x03, 0x00,         # PlanePick, PlaneOnOff
        0,                  # NextImage
    )
    if len(disk_object) != 78 or len(image_header) != 20:
        raise AssertionError("Unexpected classic DiskObject structure size")

    info_path.parent.mkdir(parents=True, exist_ok=True)
    info_path.write_bytes(disk_object + image_header + pixels)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--png", type=Path)
    parser.add_argument("--info", type=Path)
    args = parser.parse_args()

    create_source_png(args.header, args.source)
    if args.info:
        create_info(args.png or args.source, args.info)


if __name__ == "__main__":
    main()
