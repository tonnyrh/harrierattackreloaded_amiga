# Enhanced graphics

These assets are visual replacements used only by Enhanced mode. Classic
mode keeps the CPC-derived presentation and gameplay contract unchanged.

The indexed PNG files are the editable source of truth. Start the project
editor from the repository root:

```powershell
.\edit-amiga-graphics.ps1
```

It locks dimensions, the 16-colour game palette and transparent pen 0. Saving
automatically validates and packs the runtime `.bpl` banks. A headless
validation is also available:

```powershell
.\edit-amiga-graphics.ps1 -Check
```

`tank_16x8.png` uses the original two horizontal tiles; radar, launcher/car
and gun/flak use one 8x8 tile each. The compact art uses shading and camouflage
from the existing 16-colour palette without increasing the object footprint.
`tank_16x8_masked.bpl` contains two 8x8 masked tiles (four colour
bytes plus one opacity byte per scanline, 80 bytes total).
`ground_targets_8x8_masked.bpl` contains radar, launcher/car and gun/flak,
one cell each (120 bytes total). `amiga-build.ps1` runs the same packer, so stale banks and
invalid PNG masters fail the build instead of reaching the game.

LibreSprite, Aseprite or GrafX2 can optionally be opened from the editor for
advanced pixel work when installed. Preserve indexed mode, exact dimensions,
the first 16 palette entries and transparent index 0. Reload and Validate in
the project editor afterwards.

`python tools/seed-compact-enhanced-assets.py --force` resets the current
compact masters to the built-in pixel art. It refuses to overwrite edits
without `--force`. Normal editing uses the editor instead.
The older 16x16/8x16 PNGs, banks and `generate-enhanced-*-assets.py` scripts
are retained as design references; they are no longer loaded by the game or editor.

The ground-art overlay preserves the original target footprints. The original procedural target
IDs still own collision, destruction, smoke and score. The tank keeps its
paired-column damage model; radar, launcher and gun keep their original
single-column gameplay footprint. Classic mode never draws this asset bank.

## City elements

The editor also exposes eight complete city blocks (`town_0.png` through
`town_7.png`): installation, truck, house, radar buildings, building complex,
tank, tower and armed buildings. Widths follow the original maps (8–40 pixels),
with three 8-pixel graphic rows above the terrain. Empty cells are locked and
must remain transparent; the ground itself is not part of these masters.
The initial art keeps the original silhouettes with restrained grey facades.

Each block is independently editable, including repeated windows and tiles.
Bright grid lines mark 8x8 cells. Scrollbars and 6x/8x editing zoom support
larger blocks; preview zoom shows the complete element. Mirroring is disabled
for city blocks because their gameplay cell layout must remain fixed.

Save + pack writes `town_tiles.bpl` (69 five-plane cells, 2,760 bytes) and
`town_graphics.h`. Enhanced render columns reference these cells; Classic,
collision, scoring and destruction continue using the original maps. The
normal build embeds the bank, so rebuild the game after editing PNGs.

`python tools/seed-enhanced-town-assets.py` imports only missing city masters
from the checked-in Amiga tiles and never overwrites existing PNG edits.
To open directly on a building: `python tools/amiga-graphics-editor.py --asset town_4`.

## Rectangle copy and paste

Choose **Select rectangle**, drag an area, then **Copy** / Ctrl+C. Select a
destination image in the same editor window, choose **Paste** / Ctrl+V and
click the destination's top-left pixel. The mouse preview shows the stamp;
Escape cancels. The internal copy buffer survives image changes. Save any
edits when prompted before changing images.

Paste copies transparent pixels too by default. Enable **Skip transparent
pixels** to overlay just the opaque artwork. Each paste is one undo step.
A red outline indicates a placement outside the image or over locked city
cells; invalid placements do not modify the image. Copy a smaller rectangle
or choose another position. The original gameplay cell layout stays fixed.

White/light-blue checks identify transparency in both the editing grid and
preview. Locked city cells appear blue-grey in the editing grid. These
background colours are display aids only and are never written to the PNG.

Preview defaults to **Sky**: transparent pixels are not drawn, so the plain
sky backdrop shows through. **White** offers a neutral backdrop; **Checks**
restores the checkerboard. The editing grid retains its light checkerboard.

## Original graphics library

**Original graphics…** opens a separate read-only browser for all 102 original
8x8 game tiles, an atlas, the eight original city blocks, and the separately
stored carrier/gunship graphics. It reads the checked-in converted Amiga
assets directly and never writes them. Use the dropdown or Previous/Next to
browse, and choose a zoom level as needed.

Drag a rectangle, or double-click an atlas tile to select its full 8x8 area.
Ctrl+C / **Copy selection** sends indexed pixels to the main editor's copy
buffer. Return to the main window, Ctrl+V and click to place. The image being
edited stays open with all unsaved changes preserved while browsing.
**Select all** / Ctrl+A selects an entire individual graphic or composite.
To open both windows: `python tools/amiga-graphics-editor.py --originals`.

## Save safety and game colours

Saving compares the file on disk with the version loaded into this window.
If another window or an external editor changed it, Save refuses to overwrite
the newer file. The unsaved pixels remain in memory and are exported to
`.tmp/graphics-history/*-unsaved.png`. Normal saves also preserve the previous
PNG there as `*-before.png`. Revert asks before discarding edits. Floating
paste previews must be placed or cancelled before Save.

Use **Game lighting** to display Day, Dusk, Night or Dawn colours in the
editor, preview and original browser. **PNG** shows the raw master palette.
These settings change the display only; Save always writes the same indexed
master palette. Runtime constants are read from `amiga/main.c`:

- pen 5 is terrain green/brown and changes with the mission lighting;
- pen 15 is the cloud colour above raster 112 (including city art at y=88–111),
  and switches to sea blue below that raster;
- pen 14 is overridden to the fixed cyan rocket-powerup colour in gameplay.

The preview represents settled lighting above the horizon. It does not
simulate every sky-gradient row or intermediate palette-fade step. Pen 0
remains transparent. The ordinary build packs PNGs into bitplanes without
rewriting the PNGs or `game_palette.pal`.

Old editor processes keep their old code. Finish or preserve work in those
windows and close them before relying on the new save-conflict protection.


## Weapons and Missile Tank

The editor includes player rocket/Maverick directions, enemy missile
directions, Missile Tank rise/cruise, and both bomb phases. Rockets and
missiles remain **8x8**; bombs remain **4x3**. The missile tank remains
**16x8**, in its own PNG initially copied from `tank_16x8.png`.
It never shares the editable master with the ordinary tank.

`projectile_0_8x8.png` through `projectile_12_8x8.png` follow the named editor
entries. The ordinary rocket and right-facing Maverick share entry 3;
left and down-left Maverick use entries 2 and 1 respectively. They use the original left and down-left silhouettes and can be edited
independently in Enhanced.
`bomb_0_4x3.png` is launch; `bomb_1_4x3.png` is descent.

`weapons_masked.bpl` contains 13 eight-row projectile images followed by
2 three-row bomb images: **550 bytes**, five bytes per row (four planes and
opacity). Bombs occupy the high four bits. `weapons_graphics.h` records which
projectile masters contain only transparent, black (10) and yellow (6).
These use the existing cheap multiplexed hardware sprites. Masters with
other colours automatically use masked BOBs, preserving the same palette
indices instead of silently recolouring the edit. Additional BOB work can
cost more CPU than the default hardware-compatible artwork.

The fixed 16-colour palette and lighting previews work as before. Atmosphere
pens still vary with the game's raster/lighting effects; stable pens are
preferable for weapons. Saving never quantises or changes pixel indices.
Classic's original weapon graphics remain independent of these PNGs.

Focused emulator checks:

```powershell
.\run-amiga-classic-contract.ps1 -ExtraCcFlags '-DHAR_HEADLESS_WEAPON_ART_TEST_ONLY=1'
.\run-amiga-classic-contract.ps1 -ExtraCcFlags '-DHAR_HEADLESS_MISSILE_TANK_TEST_ONLY=1'
```
