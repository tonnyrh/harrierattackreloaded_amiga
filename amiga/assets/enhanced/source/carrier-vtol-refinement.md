# Carrier refinement, 2026-09-22

## Presentation

The 32x16 bomber is a retained background BOB, below transient weapons. Integer world position, Y and visible pose form its cache key. Unchanged render calls leave the pixels and saved background untouched. A world/impact mutation underneath invalidates the cache; that necessary repair is distinct from redundant frame-by-frame redraw. Changes retire only overlapping upper snapshots, then restore and paint together behind the bomber rows. This update runs before the wait for lower projectiles.

The launcher now occupies world X112..119/Y88..95, above the island roof, with the missile origin aligned to it. Its existing three directions and safety retraction remain.

Bomber destruction uses three 32x16 burst frames over 24 logical ticks. These reuse the eight bomber footprint slots. A normal hit retains the small flash; destruction triggers the full effect. This is visual only, not new area damage.

## Carrier VTOL only

Two fixed-point velocity/thrust axes replace direct two-pixel carrier movement. Input adjusts acceleration gradually, opposing input brakes before reversing, and neutral input removes thrust while preserving the current velocity on both axes. Fractional motion accumulates deterministically at the existing logical update cadence. Velocity is capped at 1.5 pixels per tick. Touchdown and crashes reset accumulated motion.

The seven native 16x8 poses are side/quarter/tilt/front/tilt/quarter/side, generated from the existing pixel art at pack time. No runtime rotation or new sprite channels are required. Terrain VTOL and normal flight still use their existing controls and artwork.

## Verification

Carrier contract tests cover acceleration, opposite input, neutral momentum retention and deliberate braking, tower/sea collision, landing, retained bomber draws, every pixel shift and both directions, overlap restoration, three explosion frames and turret position. The test-only bomber draw counter verifies that eight repeated unchanged renders perform zero additional bomber draws.

Cycle-exact A500 testing uses Kickstart 1.2, 512 KB chip and 512 KB expansion memory. Performance logging uses a 500-frame interval: a 100-frame interval reserves an extra 20 KB and can itself trigger allocation failure on this configuration. The shipping build does not contain that log buffer.

Local validation logs: `.tmp/carrier-vtol-contract-final.log`, `.tmp/carrier-vtol-a500-early.log`, `.tmp/carrier-vtol-a1200-early.log`. Pixel previews: `.tmp/carrier-vtol-poses.png`, `.tmp/bomber-blast-preview.png`.

Final results: carrier-defence-and-repair contract PASS. Both isolated emulator runs completed. In ten-second active-wave windows A500 averages ranged from about 20 to 44 FPS, with the heaviest bomber windows around 20-23 FPS; stock A1200 active-wave windows averaged about 40-49 FPS. Quiet windows reached 50 FPS. These are scripted-run measurements, not a claim of locked 50 FPS or a substitute for joystick feel testing.

Updated control preference: releasing the stick no longer auto-brakes or restores hover. Opposing input slows the craft, then reverses it if held. Touchdown/crash resets and screen limits remain physical stops. Terrain controls are unchanged.
