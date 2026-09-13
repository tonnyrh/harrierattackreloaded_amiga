# Enhanced encounters and low-speed flight

These changes are local development work after Public Beta 3 RC1; they are not a new published release.

- Landing mode enters at speed level 3 or below, remains active at 4, and retracts at 5. Actual carrier approach also engages it. The yellow L cockpit lamp and existing landing-gear artwork indicate the state. Fuel drains at three times the ordinary rate, measured in simulation steps; pause and tempo interpolation do not consume extra fuel. Completed landing/refuelling retains the existing rules.
- Hover uses the existing Paula engine loop at a lower pitch and slightly higher volume. No full sample regeneration or extra voice is required on a mode transition.
- Missile Tank climb changes from two to three vertical pixels per four simulation steps. Horizontal drift and cruise speed are unchanged.
- Missile Silo occupies the same 16x8 target footprint. It has separate closed/open indexed masters. Its placement stream is independent of the original terrain RNG and never replaces a Missile Tank. Silos use the same 8-12-tank recurrence, with a 42-column gap between anchors. Each intact silo can fire once. Its separate vertical missile slot is independent of enemy planes and their missiles. The launch predicts current aircraft motion; the missile accelerates without tracking after launch.
- Helicopters appear from mission 2 over procedural land, one at a time. They follow terrain with forward clearance, stay ahead for 500 simulation steps plus 100 per subsequent mission (capped at 1500), and then stop advancing in world space. They stop 40 pixels before a town. A flak burst can appear 32 pixels ahead of the Harrier every 65 steps, only while the helicopter remains ahead.
- The first missile hit produces sparse cosmetic smoke (one short puff per 100 steps) and a slower rotor pulse. A second missile hit stops the rotor/audio and starts an accelerating fall. The rotor uses a precomputed 256-byte pulse at ambient priority, below gameplay effects. No procedural synthesis runs in the game loop.
- Helicopter and silo artwork starts at 16x8 and is editable in the existing graphics editor. These are provisional pixel-art masters for further art direction; all existing user-edited PNGs remain authoritative.
- Wingman formation clamps its final horizontal step to the pixel-aligned target instead of overshooting on an eight-pixel grid.

## Rendering and A1200 investigation

Camera Copper updates now run immediately after the PAL line-311 deadline. The gameplay Copper list waits until line 16 before consuming fine scroll and pointer halves. This prevents updates from straddling the list restart; visual verification of the reported initial carrier reversal is tracked separately from functional tests.

Reference: [Commodore Amiga Hardware Reference Manual, Copper](https://www.theflatnet.de/pub/cbm/amiga/AmigaDevDocs/hard_2.html), particularly Copper restart at vertical blank and pointer setup before display fetch.

Silo prediction uses a fixed acceleration table and binary search; viewport scanning examines only newly exposed columns. Helicopter terrain clearance is cached per world column.

New moving graphics use the existing four-plane masked row renderer. The helicopter uses a single 16-pixel renderer with shifts prepared by the graphics packer. The shift bank costs 1,920 bytes. At ring boundaries, the two halves retain their independent mirror destinations. Saved backgrounds are retired in reverse composition order, including overlapping missiles and cosmetic smoke. Interpolation includes the new object positions. Rare menu/debug screens use size optimization to preserve A500 memory headroom without reducing optimization of the normal scrolling loop.

## Validation commands

```powershell
python tools/pack-enhanced-graphics.py
python tools/amiga-graphics-editor.py --check
.\run-amiga-classic-contract.ps1 -ExtraCcFlags '-DHAR_HEADLESS_ENHANCED_ENCOUNTER_TEST_ONLY=1'
```

The encounter contract covers initialization, hover hysteresis/fuel, silo prediction, spacing and incremental viewport-cache entry/reversal, two-hit helicopter damage, Wingman settling, byte-exact helicopter rendering, overlapping BOB restoration and ring seams. The same run also checks tempo interpolation, Missile Tank rules, and editable projectile colours. Stock-machine test logs are kept under `.tmp` during development. Performance instrumentation reports `#encounters` as silo launches, helicopter admissions, flak bursts, helicopter hits, kills, smoke puffs, late Copper commits, maximum commit line within the new field.


## Measured limits (local development build)

- The combined contract passed on the emulated 68000, including indexed-pixel comparison for both helicopter frames, every bit shift and ring seam. The silo acceleration table also matched its step-by-step integration; 1,701 vertical interception searches matched a linear reference.
- Stock A1200, 68020, 2 MB Chip, no Fast RAM, 90% tempo: ordinary measured intervals were 49-50 FPS. The scripted pause is excluded from that statement. Zero late Copper camera commits; maximum observed commit line in the new field was 0. This is timing evidence, not a visual reproduction of the user's original carrier corruption.
- Stock A500 with normal weapon load, mission 2, skill 3, automatic Wingman and maximum cruise speed: 46-50 FPS in measured gameplay intervals. The run exercised one silo launch, one helicopter and eight flak bursts. No late camera commits.
- Stock A500, 512 KB Chip + 512 KB Slow RAM, 100% tempo, mission 2, skill 5, simultaneous two-player weapon stress: the run completed with one silo launch, one helicopter, seven flak bursts and one damaging helicopter hit. The busy intervals were 24-29 FPS, versus 24-41 in the matching no-encounter baseline. This worst-case load is **not** locked at 50 FPS. Additional flak also changes subsequent workload, so intervals are not exact frame-for-frame pairs.
- In that stress run, encounter logic averaged about 12 PAL raster lines per active update and drawing about 26 lines per gameplay loop, after reducing the initial scan/drawing costs. Maximum camera commit line was 2, with zero late commits. No new per-frame allocation or sample synthesis is used.

Performance and detailed behavior logs are local under `.tmp/tempo-isolated-*`. The public RC1 is unchanged. The interactive build is labelled `BETA 3 DEV`.


## Missile damage and emergency eject

Enhanced Harrier armour now loses exactly one third of full health per ordinary enemy/silo missile, or one half per Missile Tank missile. Fractional damage is retained internally (300 units of full health); the cockpit rounds remaining health upward, so ordinary hits show 67, 34, then 0. Flak and missile damage accumulate together. Health pickups, servicing and respawning clear both damage sources. Classic missile collisions retain the original fatal behavior. Wingman still dies from one missile hit.

When the E lamp is lit in Enhanced, hold the player's rocket and bomb buttons together for 12 simulation steps (0.24 seconds at 100% tempo) to eject. Releasing either button or cancelling resets the hold. Pause/interpolation cannot advance the counter. The existing E key and its control binding remain available.

Validation: `run-amiga-classic-contract.ps1 -ExtraCcFlags '-DHAR_HEADLESS_MISSILE_DAMAGE_EJECT_TEST_ONLY=1'` checks damage, fractional exhaustion, flak interaction, health reset, respawn protection, silo consumption, Wingman destruction and the gated eject hold.
