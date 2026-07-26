#!/usr/bin/env python3
"""Extract the CPC menu font and menu strings for the Amiga port."""

from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

FONT_FIRST_CHAR = 32
FONT_NUM_CHARS = 96
GLYPH_WIDTH = 8
GLYPH_HEIGHT = 8

MENU_LABELS = {
    "titletext2f": "HAR_TEXT_TITLE",
    "skillleveltext2f": "HAR_TEXT_SKILL_LEVEL",
    "skillleveltext3f": "HAR_TEXT_SKILL_VALUE",
    "controlstextf": "HAR_TEXT_INPUT_LABEL",
    "inputmethodf": "HAR_TEXT_INPUT_METHOD",
    "joysticktextf": "HAR_TEXT_JOYSTICK",
    "keyboardtextf": "HAR_TEXT_KEYBOARD",
    "startgametextf": "HAR_TEXT_START_GAME",
    "redefinekeystextf": "HAR_TEXT_REDEFINE_KEYS",
    "tributetextf": "HAR_TEXT_TRIBUTE",
}


def parse_int(token: str) -> int:
    token = token.strip()
    if token.startswith("&") or token.startswith("#"):
        return int(token[1:], 16)
    if token.startswith("%"):
        return int(token[1:], 2)
    return int(token, 0)


def strip_comment(line: str) -> str:
    return line.split(";", 1)[0].strip()


def parse_db_values(line: str) -> list[int]:
    match = re.search(r"\bdb\b(.+)$", line, re.IGNORECASE)
    if not match:
        return []
    values: list[int] = []
    for token in match.group(1).split(","):
        token = token.strip()
        if token:
            values.append(parse_int(token))
    return values


def decode_mode1_byte(value: int) -> tuple[int, int, int, int]:
    """Decode one CPC Mode 1 byte into four 2-bit pen numbers."""
    return (
        ((value >> 7) & 1) | (((value >> 3) & 1) << 1),
        ((value >> 6) & 1) | (((value >> 2) & 1) << 1),
        ((value >> 5) & 1) | (((value >> 1) & 1) << 1),
        ((value >> 4) & 1) | (((value >> 0) & 1) << 1),
    )


def extract_font_table(lines: list[str]) -> list[str]:
    in_table = False
    labels: list[str] = []
    for line in lines:
        clean = strip_comment(line)
        if re.match(r"amsfonttable\s*:", clean, re.IGNORECASE):
            in_table = True
            continue
        if in_table and re.match(r"amsfont\s*:", clean, re.IGNORECASE):
            break
        if not in_table:
            continue
        match = re.search(r"\bdefw\b\s+([A-Za-z_][A-Za-z0-9_]*)", clean, re.IGNORECASE)
        if match:
            labels.append(match.group(1).lower())
    return labels


def extract_glyph_data(lines: list[str]) -> dict[str, list[int]]:
    glyphs: dict[str, list[int]] = {}
    current: str | None = None
    for line in lines:
        clean = strip_comment(line)
        label = re.match(r"\s*(t\d+)\s*:?", clean, re.IGNORECASE)
        if label:
            current = label.group(1).lower()
            glyphs.setdefault(current, [])
            rest = clean[label.end() :]
            glyphs[current].extend(parse_db_values(rest))
            continue
        if current:
            values = parse_db_values(clean)
            if values:
                glyphs[current].extend(values)
    return glyphs


def glyph_to_1bpp(data: list[int]) -> list[int]:
    if len(data) < 16:
        return [0] * GLYPH_HEIGHT

    # The original font is stored as an 8x8 CPC Mode 1 bitmap. 0x0f
    # decodes to four pixels in pen 2, which is the glyph background.
    background_pen = 2
    rows: list[int] = []
    for y in range(GLYPH_HEIGHT):
        row_mask = 0
        cpc_pixels = (
            decode_mode1_byte(data[y * 2])
            + decode_mode1_byte(data[y * 2 + 1])
        )
        for x, pen in enumerate(cpc_pixels):
            if pen != background_pen:
                row_mask |= 0x80 >> x
        rows.append(row_mask)
    return rows


def extract_strings(text: str) -> dict[str, str]:
    strings: dict[str, str] = {}
    for label, define in MENU_LABELS.items():
        match = re.search(rf"^\s*{re.escape(label)}\s*:\s*defb\s+\"([^\"]*)\"", text, re.IGNORECASE | re.MULTILINE)
        if match:
            strings[define] = match.group(1)
    return strings


def c_escape(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def write_header(path: Path, strings: dict[str, str]) -> None:
    lines = [
        "#pragma once",
        "",
        "/* Generated from AMSTRADFONT3.asm by tools/cpc_font_to_amiga.py. */",
    ]
    for define in MENU_LABELS.values():
        value = strings.get(define, "")
        lines.append(f'#define {define} "{c_escape(value)}"')
    lines.append("")
    path.write_text("\n".join(lines), encoding="ascii")


def write_font_preview(path: Path, font: bytes) -> None:
    cell = 10
    cols = 16
    rows = 6
    width = cols * cell
    height = rows * cell
    pixels = [(0, 0, 0)] * (width * height)
    for index in range(FONT_NUM_CHARS):
        gx = (index % cols) * cell + 1
        gy = (index // cols) * cell + 1
        for y in range(GLYPH_HEIGHT):
            row = font[index * GLYPH_HEIGHT + y]
            for x in range(GLYPH_WIDTH):
                if row & (0x80 >> x):
                    pixels[(gy + y) * width + gx + x] = (255, 255, 255)

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
    parser.add_argument("--out-font", type=Path, default=Path("amiga/assets/cpc_font8x8.bin"))
    parser.add_argument("--out-text", type=Path, default=Path("amiga/assets/harrier_menu_text.h"))
    parser.add_argument("--preview-bmp", type=Path, default=Path("amiga/assets/cpc_font8x8_preview.bmp"))
    args = parser.parse_args()

    text = args.input.read_text(encoding="utf-8", errors="ignore")
    lines = text.splitlines()
    table = extract_font_table(lines)
    glyphs = extract_glyph_data(lines)

    font = bytearray(FONT_NUM_CHARS * GLYPH_HEIGHT)
    for index in range(FONT_NUM_CHARS):
        if index < len(table):
            rows = glyph_to_1bpp(glyphs.get(table[index], []))
            font[index * GLYPH_HEIGHT : (index + 1) * GLYPH_HEIGHT] = bytes(rows)

    args.out_font.parent.mkdir(parents=True, exist_ok=True)
    args.out_text.parent.mkdir(parents=True, exist_ok=True)
    args.preview_bmp.parent.mkdir(parents=True, exist_ok=True)

    args.out_font.write_bytes(font)
    write_header(args.out_text, extract_strings(text))
    write_font_preview(args.preview_bmp, bytes(font))

    print(f"Wrote {args.out_font} ({len(font)} bytes, {len(table)} table entries)")
    print(f"Wrote {args.out_text}")
    print(f"Wrote {args.preview_bmp}")


if __name__ == "__main__":
    main()
