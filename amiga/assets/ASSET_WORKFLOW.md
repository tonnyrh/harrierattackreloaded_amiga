# Harrier Amiga asset workflow

This directory contains only assets that are direct build inputs for the Amiga
port. They may preserve visual or musical provenance from the original game,
but their checked-in representation must be an Amiga runtime format such as a
planar bitmap, palette, raw Paula sample, MOD or C header.

The original Amstrad source, binaries, audit JSON and contact sheets belong
only in the upstream reference repository or in a local temporary checkout.
Read-only conversion/viewing tools may remain here, but their intermediate
outputs must stay under ignored `.tmp`.

When an asset needs comparison or regeneration:

1. Read or clone `https://github.com/chrisperver/harrierattackreloaded` as a
   separate, read-only reference checkout.
2. Run the import/view tools against that checkout. They must never write into
   it; extraction and visual auditing output goes under ignored `.tmp`.
3. Convert the selected result completely to its Amiga runtime representation.
4. Copy only that final Amiga asset into this directory.
5. Build and test on the stock A500 contract before committing it.

`promoted_assets.h`, `promoted_sprite_tiles.h` and `font8x8.bin` are already
converted Amiga build inputs and are authoritative here. Do not regenerate or
replace them merely because the external CPC source checkout changes.
