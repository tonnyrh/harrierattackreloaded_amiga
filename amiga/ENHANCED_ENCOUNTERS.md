# Enhanced encounters and low-speed flight

These changes are local development work after Public Beta 3 RC1; they are not a new published release.

- Landing mode enters at speed level 2 or below, remains active at 3, and retracts at 4. Actual carrier approach also engages it. The yellow L cockpit lamp and existing landing-gear artwork indicate the state. Fuel drains at three times the ordinary rate, measured in simulation steps; pause and tempo interpolation do not consume extra fuel. Completed landing/refuelling retains the existing rules.
- Hover uses the existing Paula engine loop at a lower pitch and slightly higher volume. No full sample regeneration or extra voice is required on a mode transition.
- Missile Tank climb changes from two to three vertical pixels per four simulation steps. Horizontal drift and cruise speed are unchanged.
- Missile Silo occupies the same 16x8 target footprint. It has separate closed/open indexed masters. Its placement stream is independent of the original terrain RNG and never replaces a Missile Tank. Silos use the same 8-12-tank recurrence, with a 42-column gap between anchors. Each intact silo can fire once. Its separate vertical missile slot is independent of enemy planes and their missiles. The launch predicts current aircraft motion; the missile accelerates without tracking after launch.
- Helicopters appear from mission 2 over procedural land, one at a time. They follow terrain with forward clearance, stay ahead for 500 simulation steps plus 100 per subsequent mission (capped at 1500), and then stop advancing in world space. They stop 40 pixels before a town. They fire two small aimed bullets with slight vertical spread per 90-step burst, only while ahead of the Harrier. At most two bullets can be active. Each is a single light pixel and disappears on terrain contact or after 90 steps. Harrier takes ordinary flak damage; Wingman keeps its existing flak immunity.
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


## A500 hardware feedback adjustments

- Wingman's complete hardware-sprite data is prepared in CPU memory. Its 72-byte image/control block is copied to Chip RAM at vertical blank, before the gameplay Copper list starts. The display never reads the staging buffer. This removes live sprite-data writes during the active display; confirmation on the reporting A500 is still required.
- The reported machine is an A500 with Kickstart 1.2 and 512 KB expansion. Validation uses OCS and 512 KB Chip plus 512 KB Slow RAM.
- Missile Tank shots now test the terrain across their swept horizontal footprint and produce the existing impact effect on contact.
- Helicopter runtime graphics are mirrored to face right, preserving the indexed editing masters. Both the normal and preshifted banks use the same mirrored pixels.
- Machine-gun bullets use fixed-point motion, a strict two-slot pool and masked single-bit background restoration. No per-frame allocation or additional audio channel is used. They are included in tempo interpolation and ring-stream overlap protection.


Validation of the hardware-feedback build (`BETA 3 DEV2`):

- A500, OCS, Kickstart 1.2, 512 KB Chip + 512 KB Slow, mission 2/skill 3, automatic Wingman, maximum cruise, normal weapon load: 46-50 FPS in sampled gameplay intervals. One silo, one helicopter and eleven machine-gun rounds were exercised. The two-round cap was selected after comparing three rounds and no rounds on the same route. The no-round baseline measured 46-50 FPS; the three-round version had one 37-FPS interval.
- A1200, 2 MB Chip, no Fast RAM, 90% tempo: all ordinary sampled gameplay intervals were 50 FPS. The scripted pause is excluded. Nine machine-gun rounds were exercised.
- Zero late Copper camera commits on both machines. Maximum observed commit line was 4 on the A500 and 1 on the A1200, including the new Wingman publication.
- The encounter contract covers the staging/publish boundary, both Wingman artwork states, hidden sprites, bullet cap/spread/terrain expiry, Tank missile terrain impact, exact BOB/pixel restoration, and tempo interpolation with live bullets. Physical A500 confirmation of the original Wingman symptom remains outstanding.


## Fuel supply and route reserve

Fuel uses a fixed turquoise HUD pen; Armour stays yellow. Both retain their red critical warning. The fuel colour is set only below the world/HUD split and cannot follow mission palette changes.

Enhanced has occasional 8x8 F depots, editable as `Fuel depot F (+20%)`. They replace single-cell ground targets, never Missile Tanks or Silos. The first is eligible after 96-127 procedural-land columns; later depots are at least 192-255 columns apart, depending on seed and available targets. Player rockets/bombs and Wingman bombs grant exactly 20% of full fuel capacity on destruction, capped at full. Destruction persistence prevents repeated collection; depleted/failed aircraft cannot be rescued after failure has started. Existing ship servicing still fills the tank completely. No parachute fuel drop was added.

The route grows by 256 columns per effective difficulty (menu skill plus completed missions, capped at 5). Enhanced difficulty 3/4/5 now has 10/20/30% greater fuel endurance: approximately 210/229/248 seconds of normal flight at 100% tempo, versus 191 seconds at difficulty 1/2. L mode still consumes three times as much. Classic remains exactly 9558 simulation steps per full tank. Fuel runs on logical steps, so changing tempo or pausing cannot disadvantage fuel relative to distance.

The fuel contract exercises the actual terrain generator for 16 seeds at each of all five effective difficulties. It budgets the entire scroll distance at throttle 5 or higher (3 pixels/step), another 10 seconds for departure/acceleration, and 30 seconds at triple consumption for landing, with at least 10% remaining and no depot collection. This establishes a conservative fuel-feasible route, not a guarantee for unlimited hovering, slow flight or repeated approaches. All later missions share the capped route-length bound.

Run: `run-amiga-classic-contract.ps1 -ExtraCcFlags '-DHAR_HEADLESS_FUEL_SUPPLY_TEST_ONLY=1'`.

Validation (`BETA 3 DEV3`): the fuel contract passed all 80 generated routes, exact 20% refill arithmetic, saturation, repeated-destruction protection and Classic duration. A500/Kickstart 1.2, OCS, 512 KB Chip + 512 KB Slow RAM, normal load with Wingman at 100% tempo completed at 46-50 FPS in sampled gameplay intervals, matching the preceding build. Zero late Copper commits, maximum line 4. Existing ground-target bank cells were verified byte-for-byte unchanged; the depot adds one 40-byte masked cell and a 198-byte placement bitset. No new sprite channel, BOB or audio synthesis is required.


## F depot aiming assistance (after Beta 4)

The depot artwork stays 8x8. Enhanced player rockets/Mavericks and player or
Player-2 Wingman bombs accept a near miss within four pixels on either side
or above the roof: a 16x12 contact area. There is no extension below its base.
The normal impact probe has priority, so the assistance does not shoot through
terrain or substitute a depot for another directly hit object. CPU Wingman
bombing already uses the selected target's exact position. Other targets,
aircraft collisions, target locking and Classic mode retain their behavior.

The fuel contract sweeps every contact-area boundary on all 80 generated test
routes, checks returned target coordinates and excludes destroyed depots and
Classic mode. Supply amount, one-time collection and route reserve checks remain.

Validation: the expanded fuel-supply contract passed for `BETA 4 DEV1`.


## Shared enemy-plane motion (after Beta 4)

Classic and Enhanced now use the same small independent horizontal velocity:
one pixel toward the player/left per four simulation steps (12.5 pixels/second
at 100% PAL tempo). This adds about 8.3% to the 3-pixel cruise closing speed.
The aircraft continues left during its existing climbing retreat. No random
jitter or player-control delay is added. Spawn/radar rules, firing distance,
missile ownership and broken-aircraft behavior are unchanged.

The movement checks the newly entered leading terrain column at current and
intended height before crossing into it. A blocked move holds X while the
existing vertical logic continues. No additional graphics, allocation or
sprite channel is used. Classic deliberately shares this Amiga motion adjustment;
it no longer keeps an intact enemy aircraft at a fixed world X.

The focused `HAR_HEADLESS_ENEMY_SCENERY_TEST_ONLY` contract (legacy flag name)
checks identical horizontal speed in both modes, camera speeds 0..3, both
starting-column parities, occupied-missile-slot retreat and terrain blocking.

Validation: the focused enemy-flight contract passed; interactive build `BETA 4 DEV2` includes the earlier F-depot aiming assistance.
