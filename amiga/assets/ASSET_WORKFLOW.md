# Harrier Amiga asset workflow

The CPC extractor is intentionally a review/import pipeline, not an automatic
runtime replacement.

## Regenerate CPC source assets

From the repository root:

```powershell
.\extract-cpc-assets.ps1
```

Generated CPC audit files are written to:

```text
amiga/assets/generated/cpc/
```

Useful review files:

- `cpc_asset_manifest.json` - top-level manifest, provenance, pitfalls, and output list.
- `cpc_tiles_index.csv` - all CPC `spr1..spr102` tiles with friendly names.
- `cpc_plus_sprites_index.csv` - Plus sprite pixel assets such as Harrier, carrier, gunship, wingman, and parachute.
- `cpc_object_blocks_index.csv` - composite/object-map data from the main CPC source.
- `previews/cpc_combat_sprites_audit.bmp` - composed Harrier/enemy aircraft plus missile/bomb tiles.
- `previews/*.bmp` - visual contact sheets.

## Promote or polish assets

Keep the generated files as the CPC truth source. When an asset is ready for the
Amiga runtime, copy or convert it into the normal runtime asset area instead of
editing generated files directly.

Recommended pattern:

1. Identify the CPC source asset id in `cpc_asset_manifest.json`.
2. Compare the relevant contact sheet against the CPC game in WinAPE.
3. If the raw CPC asset works, promote it into a runtime asset file.
4. If it needs cleanup, create a separate polished/native Amiga derivative and
   keep a note pointing back to the CPC source id.
5. Only then wire the runtime renderer or route data to the promoted asset.

This keeps us free to improve graphics later without losing original Amstrad
provenance.

## Current promoted runtime header

The first runtime promotion is generated with:

```powershell
.\promote-cpc-assets.ps1
```

It writes:

```text
amiga/assets/cpc_promoted_assets.h
```

That header currently contains selected CPC Plus sprite pixel assets for the
carrier/frigate, gunship, wingman, and parachute. `amiga/main.c` uses the
carrier/frigate pieces and one gunship visual candidate now; the rest are staged
for later gameplay sprints.

Do not hand-edit the raw arrays in `cpc_promoted_assets.h`. If a sprite needs
cleanup, create a separate polished/native derivative and document which CPC
asset id it came from.
