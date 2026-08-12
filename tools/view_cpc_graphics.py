#!/usr/bin/env python3
"""Build a browsable CPC graphics review page.

This is a lightweight inspection tool for the Amiga port. It consumes the
already-generated audit files under `amiga/assets/generated/cpc` and creates a
static HTML gallery with one small BMP per tile/sprite/object candidate.
"""

from __future__ import annotations

import argparse
import html
import json
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOLS_DIR = ROOT / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import extract_cpc_assets as cpc  # noqa: E402


def safe_name(value: str) -> str:
    out = []
    for ch in value.lower():
        if ch.isalnum():
            out.append(ch)
        elif ch in ("_", "-", "."):
            out.append(ch)
        else:
            out.append("_")
    return "".join(out).strip("_") or "asset"


def read_json(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return json.loads(path.read_text(encoding="utf-8"))


def render_pixels(
    path: Path,
    pixels: list[list[int]],
    scale: int,
    transparent_pen: int | None = None,
    palette: list[tuple[int, int, int]] | None = None,
) -> None:
    width = max((len(row) for row in pixels), default=1) * scale
    height = max(len(pixels), 1) * scale
    dest = cpc.blank_pixels(width, height, (24, 24, 32))
    cpc.blit_scaled(
        dest,
        width,
        0,
        0,
        pixels,
        scale,
        transparent_pen=transparent_pen,
        palette=palette,
    )
    cpc.write_bmp(path, width, height, dest)


def tile_asset_from_dict(item: dict) -> cpc.TileAsset:
    return cpc.TileAsset(
        table_index=int(item.get("table_index", 0)),
        label=str(item.get("source_label", item.get("label", ""))),
        source_line=int(item.get("source", "0:0").split(":")[-1]) if isinstance(item.get("source"), str) else int(item.get("source_line", 0)),
        raw_bytes=[int(value, 16) if isinstance(value, str) else int(value) for value in item.get("raw_bytes_hex", item.get("raw_bytes", []))],
        comments=list(item.get("comments", [])),
        cpc_comment_id=item.get("cpc_comment_id"),
        friendly_name=str(item.get("name", item.get("friendly_name", ""))),
        category=str(item.get("category", "")),
    )


def sprite_asset_from_dict(item: dict) -> cpc.PlusSpriteAsset:
    raw_values = item.get("raw_values", item.get("pen_values", []))
    if not raw_values and item.get("pixels"):
        raw_values = [pen for row in item["pixels"] for pen in row]
    return cpc.PlusSpriteAsset(
        asset_id=str(item.get("asset_id", item.get("label", ""))),
        label=str(item.get("source_label", item.get("label", ""))),
        source_line=int(item.get("source", "0:0").split(":")[-1]) if isinstance(item.get("source"), str) else int(item.get("source_line", 0)),
        raw_values=[int(value) for value in raw_values],
        friendly_name=str(item.get("name", item.get("friendly_name", ""))),
        width=int(item.get("width", cpc.PLUS_SPRITE_WIDTH)),
        height=int(item.get("height", 8)),
        comments=list(item.get("comments", [])),
        category=str(item.get("category", "")),
    )


def object_asset_from_dict(item: dict) -> cpc.ObjectAsset:
    return cpc.ObjectAsset(
        asset_id=str(item.get("asset_id", item.get("source_label", ""))),
        label=str(item.get("source_label", item.get("label", ""))),
        source_line=int(item.get("source", "0:0").split(":")[-1]) if isinstance(item.get("source"), str) else int(item.get("source_line", 0)),
        raw_bytes=[int(value, 16) if isinstance(value, str) else int(value) for value in item.get("raw_bytes_hex", item.get("raw_bytes", []))],
        kind=str(item.get("kind", "")),
        layout=dict(item.get("layout", {})),
        comments=list(item.get("comments", [])),
        risk=str(item.get("risk", "")),
    )


def copy_contact_sheets(source_root: Path, out_dir: Path) -> list[Path]:
    previews = source_root / "previews"
    copied: list[Path] = []
    if not previews.exists():
        return copied
    target_dir = out_dir / "contact_sheets"
    target_dir.mkdir(parents=True, exist_ok=True)
    for path in sorted(previews.glob("*.bmp")):
        target = target_dir / path.name
        shutil.copy2(path, target)
        copied.append(target.relative_to(out_dir))
    return copied


def html_card(kind: str, title: str, image: Path, meta: dict, search_text: str) -> str:
    rows = []
    for key, value in meta.items():
        if value in (None, "", []):
            continue
        if isinstance(value, list):
            value = " | ".join(str(v) for v in value)
        rows.append(f"<tr><th>{html.escape(str(key))}</th><td>{html.escape(str(value))}</td></tr>")
    return f"""
      <article class="card" data-kind="{html.escape(kind)}" data-search="{html.escape(search_text.lower())}">
        <div class="thumb"><img src="{html.escape(image.as_posix())}" alt="{html.escape(title)}"></div>
        <h3>{html.escape(title)}</h3>
        <table>{''.join(rows)}</table>
      </article>
    """


def build_gallery(
    source_root: Path,
    out_dir: Path,
    scale: int,
    sprite_palette: list[tuple[int, int, int]],
) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    image_dir = out_dir / "items"
    if image_dir.exists():
        shutil.rmtree(image_dir)
    image_dir.mkdir(parents=True)

    tiles = [tile_asset_from_dict(item) for item in read_json(source_root / "cpc_tiles.json")]
    sprites = [sprite_asset_from_dict(item) for item in read_json(source_root / "cpc_plus_sprites.json")]
    objects = [object_asset_from_dict(item) for item in read_json(source_root / "cpc_object_blocks.json")]

    cards: list[str] = []
    for tile in tiles:
        filename = f"tile_{tile.table_index:03d}_{safe_name(tile.friendly_name or tile.label)}.bmp"
        rel = Path("items") / filename
        render_pixels(image_dir / filename, cpc.cpc_mode1_tile_pixels(tile.raw_bytes), scale)
        cards.append(
            html_card(
                "tile",
                f"Tile {tile.table_index:03d}: {tile.friendly_name or tile.label}",
                rel,
                {
                    "category": tile.category,
                    "label": tile.label,
                    "source line": tile.source_line,
                    "comment id": tile.cpc_comment_id,
                    "comments": tile.comments,
                    "raw": " ".join(f"{b:02x}" for b in tile.raw_bytes),
                },
                f"tile {tile.table_index} {tile.friendly_name} {tile.category} {tile.label} {' '.join(tile.comments)}",
            )
        )

    for sprite in sprites:
        filename = f"sprite_{safe_name(sprite.asset_id)}.bmp"
        rel = Path("items") / filename
        render_pixels(
            image_dir / filename,
            cpc.plus_sprite_pixels(sprite),
            scale,
            transparent_pen=0,
            palette=sprite_palette,
        )
        cards.append(
            html_card(
                "sprite",
                f"Sprite: {sprite.friendly_name or sprite.asset_id}",
                rel,
                {
                    "category": sprite.category,
                    "asset id": sprite.asset_id,
                    "label": sprite.label,
                    "size": f"{sprite.width}x{sprite.height}",
                    "source line": sprite.source_line,
                    "comments": sprite.comments,
                },
                f"sprite {sprite.asset_id} {sprite.friendly_name} {sprite.category} {sprite.label} {' '.join(sprite.comments)}",
            )
        )

    # Runtime-faithful 16x8 compositions. Each CPC Plus half stores its
    # visible eight pixels in columns 0-7; the Amiga attached-sprite builder
    # combines those two visible halves in exactly this order.
    sprite_by_id = {sprite.asset_id: sprite for sprite in sprites}
    composed_pairs = (
        ("flying_harrier", "Flying Harrier composed", "sprite_pixel_data1", "sprite_pixel_data2"),
        (
            "wingman_flying",
            "Wingman flying composed",
            "sprite_pixel_data_wingmanflying1",
            "sprite_pixel_data_wingmanflying2",
        ),
        (
            "wingman_landed",
            "Wingman landed composed",
            "sprite_pixel_data_wingmanlanded1",
            "sprite_pixel_data_wingmanlanded2",
        ),
    )
    for pair_id, title, left_id, right_id in composed_pairs:
        left = sprite_by_id.get(left_id)
        right = sprite_by_id.get(right_id)
        if left is None or right is None:
            continue
        left_pixels = cpc.plus_sprite_pixels(left)
        right_pixels = cpc.plus_sprite_pixels(right)
        height = min(len(left_pixels), len(right_pixels))
        pixels = [left_pixels[y][:8] + right_pixels[y][:8] for y in range(height)]
        filename = f"sprite_composed_{pair_id}.bmp"
        rel = Path("items") / filename
        render_pixels(
            image_dir / filename,
            pixels,
            scale,
            transparent_pen=0,
            palette=sprite_palette,
        )
        cards.append(
            html_card(
                "sprite",
                f"Sprite: {title}",
                rel,
                {
                    "category": "runtime composition",
                    "asset ids": f"{left_id} + {right_id}",
                    "size": f"16x{height}",
                    "palette": "CPC Plus sprite_colours",
                },
                f"sprite composed {pair_id} {title} {left_id} {right_id}",
            )
        )

    for obj in objects:
        grids = cpc.object_grids_for_preview(obj)
        if not grids:
            continue
        for grid_index, grid in enumerate(grids):
            rendered = cpc.render_tile_id_grid(grid, tiles, tile_base=0)
            filename = f"object_{safe_name(obj.asset_id)}_{grid_index:02d}.bmp"
            rel = Path("items") / filename
            render_pixels(image_dir / filename, rendered, max(1, scale // 2))
            cards.append(
                html_card(
                    "object",
                    f"Object: {obj.asset_id}" + (f" #{grid_index}" if len(grids) > 1 else ""),
                    rel,
                    {
                        "kind": obj.kind,
                        "label": obj.label,
                        "source line": obj.source_line,
                        "layout": obj.layout,
                        "risk": obj.risk,
                        "comments": obj.comments,
                    },
                    f"object {obj.asset_id} {obj.kind} {obj.label} {obj.risk} {' '.join(obj.comments)}",
                )
            )

    contact_sheets = copy_contact_sheets(source_root, out_dir)
    contact_html = "\n".join(
        f'<a href="{html.escape(path.as_posix())}" target="_blank"><img src="{html.escape(path.as_posix())}" alt="{html.escape(path.name)}"><span>{html.escape(path.name)}</span></a>'
        for path in contact_sheets
    )

    index = out_dir / "index.html"
    index.write_text(
        f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Harrier CPC Graphics Review</title>
  <style>
    :root {{ color-scheme: dark; font-family: system-ui, Segoe UI, sans-serif; }}
    body {{ margin: 0; background: #11131a; color: #eceff8; }}
    header {{ position: sticky; top: 0; z-index: 5; background: #191d27ee; backdrop-filter: blur(8px); padding: 14px 18px; border-bottom: 1px solid #33394a; }}
    h1 {{ margin: 0 0 8px; font-size: 22px; }}
    .controls {{ display: flex; gap: 10px; flex-wrap: wrap; align-items: center; }}
    input, select {{ background: #090b10; color: #fff; border: 1px solid #596074; border-radius: 6px; padding: 8px; }}
    main {{ padding: 18px; }}
    .summary {{ color: #aeb7cc; margin: 0 0 16px; }}
    .contacts {{ display: flex; gap: 12px; overflow-x: auto; padding: 8px 0 18px; }}
    .contacts a {{ flex: 0 0 220px; color: #dce8ff; text-decoration: none; background: #1b202c; border: 1px solid #31394c; border-radius: 8px; padding: 8px; }}
    .contacts img {{ width: 100%; image-rendering: pixelated; background: #000; }}
    .contacts span {{ display: block; font-size: 12px; margin-top: 6px; overflow-wrap: anywhere; }}
    .grid {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(210px, 1fr)); gap: 12px; }}
    .card {{ background: #181c26; border: 1px solid #32394c; border-radius: 10px; padding: 10px; }}
    .thumb {{ min-height: 82px; display: flex; align-items: center; justify-content: center; background: #090b10; border-radius: 8px; margin-bottom: 8px; overflow: hidden; }}
    .thumb img {{ max-width: 100%; image-rendering: pixelated; }}
    h3 {{ margin: 6px 0 8px; font-size: 14px; }}
    table {{ width: 100%; border-collapse: collapse; font-size: 11px; color: #cbd3e8; }}
    th {{ width: 78px; color: #8792ad; text-align: left; vertical-align: top; font-weight: 600; }}
    td {{ overflow-wrap: anywhere; }}
    tr + tr th, tr + tr td {{ border-top: 1px solid #252b3a; }}
    .hidden {{ display: none; }}
  </style>
</head>
<body>
  <header>
    <h1>Harrier CPC Graphics Review</h1>
    <div class="controls">
      <input id="search" type="search" placeholder="Search: harrier, ship, land, missile..." size="42">
      <select id="kind">
        <option value="all">All graphics</option>
        <option value="tile">Tiles</option>
        <option value="sprite">Plus sprites</option>
        <option value="object">Object blocks</option>
      </select>
      <span id="count"></span>
    </div>
  </header>
  <main>
    <p class="summary">{len(tiles)} tiles, {len(sprites)} Plus sprites, {len(objects)} object records. Generated from <code>{html.escape(str(source_root))}</code>.</p>
    <section class="contacts">{contact_html}</section>
    <section class="grid" id="grid">
      {''.join(cards)}
    </section>
  </main>
  <script>
    const search = document.getElementById('search');
    const kind = document.getElementById('kind');
    const count = document.getElementById('count');
    const cards = [...document.querySelectorAll('.card')];
    function update() {{
      const q = search.value.trim().toLowerCase();
      const k = kind.value;
      let visible = 0;
      for (const card of cards) {{
        const okKind = k === 'all' || card.dataset.kind === k;
        const okSearch = !q || card.dataset.search.includes(q);
        const show = okKind && okSearch;
        card.classList.toggle('hidden', !show);
        if (show) visible++;
      }}
      count.textContent = `${{visible}} / ${{cards.length}} shown`;
    }}
    search.addEventListener('input', update);
    kind.addEventListener('change', update);
    update();
  </script>
</body>
</html>
""",
        encoding="utf-8",
    )
    return index


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a browsable CPC graphics review gallery.")
    parser.add_argument("--source-root", type=Path, default=ROOT / ".tmp/cpc-asset-audit")
    parser.add_argument("--main-source", type=Path, default=ROOT / "HarrierAttackSourceNew2_alt_CRTC_CART16.asm")
    parser.add_argument("--out", type=Path, default=ROOT / ".tmp/cpc-graphics-viewer")
    parser.add_argument("--scale", type=int, default=6)
    parser.add_argument("--open", action="store_true", help="Open the generated HTML in the default browser.")
    args = parser.parse_args()

    # Real in-game colours instead of extract_cpc_assets's arbitrary
    # distinguishability palette - mutates cpc.RGB_PALETTE in place, which
    # cpc.blit_scaled() (used below via cpc_mode1_tile_pixels/plus_sprite_pixels)
    # reads at render time.
    cpc.RGB_PALETTE[:] = cpc.load_real_rgb_palette(args.main_source)

    sprite_palette = cpc.load_real_sprite_rgb_palette(args.main_source)
    index = build_gallery(args.source_root, args.out, max(1, args.scale), sprite_palette)
    print(f"Wrote {index}")
    if args.open:
        import os

        os.startfile(index)  # type: ignore[attr-defined]


if __name__ == "__main__":
    main()
