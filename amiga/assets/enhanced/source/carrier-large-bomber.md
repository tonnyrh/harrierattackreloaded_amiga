# Carrier bomber and missile turret

The editable bomber master is `../carrier_bomber_32x16.png`. It uses the game palette with transparent pen 0 and neutral pens 2, 3, 4 and 10. The packer mirrors the left-facing master and emits eight masked 8x8 tiles per direction. Runtime uses saved backgrounds, restored in reverse drawing order. Hit boxes are 32x16; four missile hits or one direct bomb still defeat the bomber.

Generated source: `bomber-large-source.png` (built-in imagegen). Source alpha bounds (26,163,1754,849) were cropped and BOX-resampled to 32x16; alpha below 96 becomes transparent, other pixels map to the nearest allowed neutral palette entry. Validation: `carrier-bomber-large-validation.json`.

## Generation prompt

Create one isolated game sprite of a large heavy military bomber plane, strict side profile facing LEFT, for an Amiga horizontal scrolling shooter. Design for an EXACT 32 columns by 16 rows logical pixel grid, scaled up with crisp square blocks. Long unmistakable sleek fuselage with rounded nose at left, pale blue cockpit at left, two engine nacelles underneath broad swept wings seen in side profile, tall vertical tail at far right, dark charcoal belly, light cool grey upper metal with restrained white highlights. Strong elegant readable silhouette, more substantial than a tiny fighter, no propellers, no guns firing, no labels, no exhaust, no shadow, no scene. Use only black, dark grey, medium grey, light grey, white and muted blue-grey, no gradients. Fill width generously but leave transparent margin. Landscape canvas 2:1. Truly transparent background. This is a production pixel-art asset to reduce to 32x16, so simple broad shapes, not a detailed illustration.

## Missile turret

Three native 8x8 indexed masters (`carrier_missile_left_8x8.png`, `carrier_missile_up_8x8.png`, `carrier_missile_right_8x8.png`) are exposed in the editor. These match the existing tile-based AA artwork and use neutral pens. The launcher follows the aim with pose hysteresis, retracts with the deck guns, and only fires when fully raised. It remains operational after the AA guns are destroyed and disappears when the carrier sinks.

## Runtime budget

The bomber uses three OCS blits for a moved, fully visible pose in the stationary carrier arena: restore the old background, save the new background, and draw the masked 32x16 image. The existing tile renderer remains the fallback at screen edges, ring seams, or if the optional CHIP allocation fails. `HAR_CARRIER_BLITTER=0` selects that CPU path for A/B measurements.

The blitter shifts image and mask at runtime. One cached pose, one mask, and one saved rectangle use 1,440 bytes of CHIP RAM with the current single world buffer. Each row includes a zero padding word, and the physical fifth plane has a zero mask. This permits one interleaved operation rather than separate plane blits. All blits finish before overlapping CPU drawing. The saved rectangle is word-aligned and 48 pixels wide; overlay retirement and persistent-world invalidation include that padding. Eight old tile footprint slots remain available for fallback. No preshift table or graphics changes are required.

## Verification (2026-09-21)

Carrier defence/repair contract: PASS, including launcher survival with both AA guns destroyed, all aim poses, raising/retracting, launch gating, bomber hit box and byte-exact BOB restoration across both directions, all eight shifts and a ring seam.

Cycle-exact PAL A500, Kickstart 1.2, 512 KB chip + 512 KB slow, 100% tempo: isolated autoplay completed. Heavy bomber intervals average approximately 22-25 fps after avoiding mirror writes, compared with approximately 16 fps in the initial implementation. This is a remaining performance limitation, not a locked-50-fps result. Other stationary intervals reach 50 fps. Local logs: `.tmp/carrier-large-bomber-final.log`, `.tmp/tempo-isolated-A500-100-large-bomber-final/perf_log.csv`, `.tmp/carrier-large-bomber-release-check.log`.

## Blitter verification (2026-09-24)

The DMA contract compares real blitter output with the CPU tile renderer for all five poses, all 16 word shifts, both screen-edge fallbacks, movement under a missile, and persistent edits in the saved rectangle's padding. It checks untouched bytes, the fifth plane, and exact background restoration. Run it with `run-amiga-classic-contract.ps1 -ExtraCcFlags '-DHAR_HEADLESS_CARRIER_BLITTER_TEST_ONLY=1'`; the carrier-defence contract also includes it.

Both the standalone DMA contract and the full carrier-defence/repair contract passed on the A500 configuration. The full suite takes approximately eleven minutes at emulated 68000 speed; the independent warp run also passed. Results are saved locally as `.tmp/carrier-blitter-only-result.txt` and `.tmp/carrier-blitter-full-final-result.txt`. The normal executable and ADF were rebuilt successfully (`.tmp/carrier-blitter-release-build.log`).

Matched cycle-exact PAL A500/Kickstart 1.2 runs, 512 KB CHIP + 512 KB slow RAM, 100% tempo, same seed and carrier autoplay (2,200 loop limit):

| Renderer | Bomber loops | PAL fields during bomber loops | Bomber FPS |
| --- | ---: | ---: | ---: |
| CPU tiles (`HAR_CARRIER_BLITTER=0`) | 202 | 515 | 19.61 |
| Blitter, original raster window | 202 | 364 | 27.75 |
| Blitter, extended safe raster window | 202 | 333 | 30.33 |

FPS is `50 * bomber loops / PAL fields`, measured only while the bomber is active. This is about 55% faster than the matched CPU baseline, not a 50 FPS guarantee. The 202 draws all used DMA; this gameplay sample did not enter edge fallback. Pose packing happens before the beam wait. The final measured maximum erase/save/draw duration was 26 scanlines; scheduling reserves 96 scanlines before the beam next reaches the first affected row. The CPU fallback retains its original earlier window.

Local evidence: `.tmp/carrier-blitter-only-result.txt` and `.tmp/tempo-isolated-A500-100-blitter-{cpu-lean,dma-lean,final}/perf_log.csv`. The `#carrier_render` record contains bomber loops, PAL fields, DMA draws, CPU fallback draws, maximum field delta, and (in the final build) maximum erase/save/draw scanlines. Benchmarks use a 500-field log interval to keep instrumentation inside the A500 memory budget.

The same final executable also completed the A1200 cycle-exact run: 202 bomber loops in 202 PAL fields (50 FPS), all 202 draws using DMA, maximum draw duration 20 scanlines. Evidence: `.tmp/tempo-isolated-A1200-100-blitter-final/perf_log.csv`.
