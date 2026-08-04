# Sea and carrier ambience

Sprint 15.69 adds an Amiga-only presentation layer. It does not change the
CPC gameplay model, collision data, level route, spawn logic, score rules or
gameplay random sequence.

## Frame architecture

The current playfield uses one physical interleaved ring buffer
(`GAME_WORLD_BUFFER_COUNT == 1`). The ambience footprints are nevertheless
indexed by buffer so a later return to multiple buffers will not restore
background captured from the wrong page.

The late-frame order is:

1. erase projectile footprints; erase failure smoke, gulls and full wave
   groups only when their retained render state changed or streaming will
   touch them;
2. stream/rebuild authoritative world columns;
3. draw world-anchored sea ripples;
4. redraw only invalidated carrier-idle gull BOB groups;
5. draw existing impact, powerup, failure-smoke and projectile BOBs;
6. present the ring buffer and hardware sprites through the existing Copper
   program.

Both new visual effects are small CPU-masked BOBs. Their byte footprints are
too small for Blitter setup to pay for itself, and no full tile row or screen
is redrawn for their animation.

## Sea ripples

- At most 16 visible segments are selected by a fixed hash of world column.
- Four animation phases advance every six PAL frames. A phase-only change is
  composited directly from the footprint's saved background; it no longer
  exposes an erased intermediate frame in the single playfield buffer.
- Each segment touches at most 8 x 2 pixels and is accepted only when the
  authoritative object cell is sea.
- Positions are world anchored and do not use or advance gameplay RNG.
- The surface/deep-water placement keeps the marks away from carrier and ship
  geometry. Hardware sprites remain in front of the playfield.

## Carrier gulls

- A deterministic group of one to three CPU BOBs begins arriving after
  two seconds of carrier idle time. Each bird enters through an off-screen
  edge, with a varied 60-187-frame gap, instead of appearing in the playfield.
- Three pre-rendered distance banks (8px, 12px and 16px), three wing phases,
  two grey/white variants and independent fixed-point velocities avoid runtime
  scaling and a shared mechanical loop. Birds approach from a small distant
  silhouette; takeoff scatter steps back down through the banks.
- Their private LFSR is reset per game session and never touches gameplay RNG.
- Starting takeoff switches every active gull to a fast upward/outward scatter
  state. The same state is cleanly reset on menu/session changes.
- Overlapping gull footprints are restored in reverse draw order. This keeps a
  later gull from being captured and restored as part of an earlier gull's
  background.

## Paula sea/wind bed

The ambience is not a WAV asset. At startup, a private fixed-seed LFSR and two
integer filter stages generate a 4096-byte signed 8-bit broad surf/noise buffer
directly in Chip RAM. The tail crossfades into the beginning instead of
announcing every DMA wrap with a periodic fade to silence. A roughly ten-second
volume swell plus a tiny period drift supplies the slow ocean pulse without
rewriting DMA memory.

Channel policy:

- music owns all four voices and disables the bed;
- the engine keeps channel 3;
- the bed claims only a completely free channel, preferring 2 then 1 then 0;
- it registers at ambient priority, so weapons, impacts and player cues may
  evict it immediately;
- it fades to volume 11 while stationary on a carrier and volume 5 (or 3) when
  flying low over sea, then fades to zero outside those contexts.

No source WAV is generated or overwritten by this feature.

## Resource budget

- Chip RAM: 4096 bytes for the Paula loop.
- Static state/background footprints: approximately 1.5 KiB outside the
  explicit Chip allocation (one-buffer configuration).
- Worst visual footprint traffic: below 1K small byte restore/composite
  operations per frame with all three gulls and all 16 waves visible; normal
  flight has no gull cost and usually 10-12 visible waves.
- No sprite channels, Copper instructions or extra bitplanes are consumed.

The fixed-seed headless skill-1/speed-15 A500 route completed the full map with
50 FPS in both measured gameplay intervals, zero gameplay hitches and
`maxVblDelta == 1`. The one startup interval includes emulator/bootstrap time
and is not a gameplay performance sample.
