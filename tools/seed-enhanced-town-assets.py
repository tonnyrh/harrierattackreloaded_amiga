"""Import original city geometry into independently editable Enhanced blocks.

Only missing masters are created; existing editor work is never overwritten.
"""
from amiga_graphics_format import (
    ROOT, TOWN_ASSETS, TOWN_LAYOUTS, save_indexed_master, build_runtime_banks,
)


def generate():
    tiles = (ROOT / "amiga/assets/game_tiles.bpl").read_bytes()
    for spec, layout in zip(TOWN_ASSETS, TOWN_LAYOUTS):
        if spec.path.exists():
            continue
        pixels = [0] * (spec.width * spec.height)
        for cell, tile in enumerate(layout):
            if tile in (0, 1):
                continue
            for y in range(8):
                for x in range(8):
                    pen = sum(((tiles[tile * 40 + y * 5 + plane] >> (7 - x)) & 1) << plane
                              for plane in range(5))
                    if pen > 15:
                        raise ValueError(f"Town tile {tile} uses unsupported pen {pen}")
                    # Retain shapes and windows; give facades a restrained grey
                    # finish. No enlargement or invented gameplay cells.
                    if pen == 10:
                        pen = (14 if y == 0 else 4) if tile in range(59, 66) else 3
                    px, py = cell // 3 * 8 + x, cell % 3 * 8 + y
                    pixels[py * spec.width + px] = pen
        save_indexed_master(spec.path, (spec.width, spec.height), pixels)
        print(f"Created {spec.filename}: {spec.width}x{spec.height}")
    for path, data in build_runtime_banks().items():
        path.write_bytes(data)


if __name__ == "__main__":
    generate()
