# Enhanced carrier defence (development build)

Local BETA5DEV2 work. Classic retains its original mission cycle.

## Flight and mission cycle

An air-raid klaxon opens each Enhanced mission. Up lifts off. Move around the
stationary carrier in VTOL. Harrier keeps its last heading; reversing takes twelve
logical ticks with a front-facing hover pose. Release horizontal input during
the turn to remain centred. Missiles fire horizontally. Bombs can hit enemy
planes and helicopters, or your own carrier.

Bombers attack the carrier, fighters fire at Harrier and bomb the deck if they reach it, and helicopters hover and
bomb before leaving. Aircraft and helicopters enter from either side. The occasional grey heavy bomber and helicopters take two missile hits. A falling helicopter explodes on another bullet or missile hit, without another kit or score award. Missions 1-3 have two
waves, 4-6 three, then four. Effective Skill selects 3-5 aircraft per wave.
After the final wave, LAND NOW remains until touchdown. Up then departs for the
terrain mission. Over land, ordinary scrolling and Wingman controls remain;
the experimental terrain full-VTOL stops and R buildings are no longer used.

## Carrier operator: CPU or player 2

There is **no airborne Wingman during defence**. The Wingman menu setting selects
P2 for the ship or CPU otherwise (including OFF). Over land, OFF/CPU/P2 keeps its
usual meaning. P2 uses the existing second-player stick/key bindings to move a
crosshair. Fire shoots the deck guns; the Bomb button fires the concealed island missile
launcher. Holding either button repeats as its weapon reloads.

Bullets spread slightly, continue through the sight, and intercept bombs and
missiles. Four bullet hits equal one missile hit: a fighter takes four; a heavy bomber or helicopter takes eight. Bullets cannot hurt Harrier. The turret missile flies straight through
the selected aim point, doing the same enemy damage as Harrier missiles. It CAN
hit Harrier for one third armour. CPU avoids launching directly through Harrier,
but moving into a launched missile remains dangerous.

Two gun turrets rise after takeoff and retract on approach. Each survives two
bomb hits. Contact with a raised gun destroys it and takes one third armour.
Normal bombs take 12 of the carrier's 100 hull; heavy bombs take 24. A bomb hitting
Harrier takes 50 armour, also on deck. Zero hull ends the campaign.

## Repairs and pilot rescue

Every destroyed helicopter releases a blue parachute with a white R, in raids
and terrain flight. A kit adds 20 repair cargo, capped at 60 (three kits).
Collection alone never changes hull. Land on a clear deck section to apply cargo,
capped at 100 hull, and restore both deck guns. This works mid-wave, while bombs
continue falling. Terrain cargo is delivered at the next carrier landing.
Fuel, armour and ammunition replenish gradually on deck.

The existing parachute slot is reused. Up to four helicopter drops queue behind
a pickup already in flight, preserving their world origins; queued drops that
have scrolled offscreen expire. This avoids extra simultaneous pickup Bobs.

When E lights, press primary fire once to eject (Enhanced). Release and press again if fire was already held when E lit. During defence, Left/Right gives
the descending parachute limited steering. The pilot's centre must reach the
deck. Rescue consumes one aircraft and places the replacement on deck, retaining
hull, wave and cargo. No spare aircraft or landing in the sea ends the game.
An intact Harrier crashing into the sea is also fatal.

## Presentation and budget

Original instruments remain visible. Under LV, CARRIER shows HULL and REPAIR
count. AIR RAID SCRAMBLE accompanies the 4.2-second klaxon; LAND NOW persists
until landing. The engine stops on deck. Gulls may appear at UP: DEPART.

The ship reuses two bullet slots, Wingman's rocket slot and its sprite channel
for the P2 sight. Fixed pools also hold four bombs, one jet, one helicopter and
one enemy missile. No allocation or audio synthesis occurs during combat. Gun
height changes use bounded tile redraws. Mirrored flight poses are packed ahead
of time. User-edited aircraft masters remain available in the graphics editor.

Emulator contracts cover wave progression, aiming, interception, friendly fire,
repair delivery, deck rescue, parachute steering, heading, HUD, sound and sprite
payload transitions. Real A500 play remains the balance and visual-quality check.

```powershell
./run-amiga-classic-contract.ps1 -ExtraCcFlags '-DHAR_HEADLESS_CARRIER_DEFENCE_TEST_ONLY=1 -DHAR_HEADLESS_CARRIER_RULES_ONLY=1 -DHAR_HARDWARE_PLAYER_ROCKET=1 -DHAR_HARDWARE_PROJECTILE_CHAIN=1 -DHAR_CRASH_DEBRIS_BOBS=1'
```

## Validation, 2026-09-17

The full carrier/encounter/tempo/art contract passed. The subsequent focused
rerun also passed new R/heli overlap restoration checks. Normal interactive ADF
rebuilt after all test builds (585,832-byte executable, 901,120-byte disk).

Cycle-exact A500 OCS, Kickstart 1.2, 512 KiB Chip + 512 KiB expansion, CPU carrier
operator, Skill 1, 100% tempo: the automated pilot completed defence, terrain,
return landing and advanced to mission 2. Settled carrier samples averaged
39-41 updates/s, terrain 48-49. This is not a locked-50 carrier scene; the
transition sample peaked at a nine-field gap. Loader samples are excluded.
Weapon stress was disabled for this comparison. Logs are under
`.tmp/tempo-isolated-A500-100-gunnery-final/` (local, not published).

## Carrier refinements, 2026-09-17

The supplied 17.09.2026.uss has Harrier at (83,88), in LAND NOW, touching the
old rectangular island proxy. Flight and landing now use the ship mask instead.
AA guns have their own editable 8x8 grey masters. Only the missile nose protrudes
above the island step. The new 16x8 bomber and front Harrier pose are editable;
side-view cockpit highlights preserve their original silhouettes.

Fighter missiles leave on a fixed Q8 trajectory at up to four pixels per tick,
faster than the launching fighter, with direction-matched art in both software
and hardware rendering. Tests cover the saved position, approach directions,
fighter bombing, heavy-bomber durability and destruction of falling helicopters.

Post-refinement verification: full graphics/tempo contract and the final focused
rules contract passed. A500 OCS/Kickstart 1.2, 512+512 KiB, Skill 1 and 100%
tempo completed the carrier raid, terrain, return landing and reached mission 2.
Settled defence samples averaged 38-42 updates/s; terrain 48-49. Departure still
has a nine-field maximum gap; this is not locked 50 Hz. Final interactive binary
is 588,236 bytes; ADF is 901,120 bytes. Local logs: `.tmp/carrier-revision-*.log`
and `.tmp/tempo-isolated-A500-100-carrier-revision/`. Start the new disk afresh:
a WinUAE save state includes the old executable and is not upgraded by a new ADF.

AA aim poses: both 8x8 deck guns select left/up/right independently from the
shared CPU/P2 aim point. Three prepacked masked tiles use 120 bytes of art;
there is no runtime rotation. Integer sector selection has a small dead band.
Only a height or visible pose change invalidates the gun cell. Contract checks
cover all three poses at every deployment height and both gun positions, plus
independent aim selection and boundary stability.

Missile overlap regression: software projectile backgrounds now unwind in
reverse draw order, including early pickup/world retirement. The new test
failed with leftover pixels before the fix and passes after it, for all eight
horizontal pixel phases. Repair pickups use a compact crate and R beneath
the canopy in the existing 16x8 footprint; other pickups keep their art.

Carrier flight now retains the Enhanced VTOL 3x fuel burn, including Land Now.
At skill 1 a full tank lasts about 63.7 seconds at 50 logical updates/s; higher
skills retain the existing fuel allowance. Touchdown stops consumption on that
very update. Deck service adds four fuel quanta every eight updates: empty to
full takes 448 updates (8.96 seconds at 50 Hz). These clocks pause with gameplay
and scale with the game tempo. Carrier regression tests cover hover consumption,
last-quantum touchdown and timed refuelling.

Heavy bombers now require four missile-equivalent hits (16 carrier bullets).
They spawn at y=20/28, repeatedly cross the carrier from alternating sides and
turn outside the screen until destroyed. Damage persists through turns; each
pass can drop heavy bombs. Ground tank/silo placement remains independently
seeded, with same-type spacing only; cross-type proximity is currently allowed.

Smoke now uses four filled grey 8x8 poses over 40 logical steps, rising and
drifting before breaking up. One existing BOB slot is shared by carrier and
helicopter plumes. Sinking clips all ship art at SEA_SURFACE_Y and lowers it
one pixel per eight steps until fully submerged; occasional water splashes
replace underwater explosions. Tests verify untouched sea pixels at every
depth, full disappearance and bounded smoke motion/lifetime.

The parked Wingman now lowers through the deck in eight one-pixel steps, one
every six logical ticks, and rises after the last wave. The adjacent AA gun
retracts before raising; departure waits for the lift. The animation reuses
masked carrier art and only redraws its three columns. Destroyed Wingmen are
not restored. Enhanced Harrier touchdown is y=104, matching its initial deck
position, in both carrier defence and terrain-return landing. Classic retains
its original y=105 contact rule.

Superstructure contact during carrier VTOL, including Land Now, now triggers
the normal fatal crash instead of clamping movement. The visible hull mask and
wingtip inset still allow safe descent beside the island; deck touchdown is
excluded. Existing respawn grace blocks movement rather than allowing passage.
Regression checks cover safe clearance, fatal Land Now contact, and the normal
terrain-return tower obstruction path.

Hit-flash cleanup now retires overlapping saved missile backgrounds before
changing or erasing the effect. This prevents an old flash from reappearing
after helicopter/missile overlap. Regression 181 reproduces the stale pixels
before the fix, covering every horizontal bit offset with a helicopter on top.

Enhanced eject now uses a fresh primary-fire press while E is lit, with no
hold timer or second-button requirement. Held fire does not auto-eject when
E first lights; release and press again. The dedicated eject key remains.
Carrier parachute steering moves
one pixel per tick (twice the former rate), and descent is one pixel per three
ticks instead of two. This triples horizontal reach per unit of descent;
normal terrain/Classic descent remains unchanged.

Harrier carrier missiles now use the same fresh-press rule as terrain flight.
Holding fire cannot launch again when a missile exits or is destroyed. The
17.09.2026II.uss RAM analysis found fire=1 in both main input snapshots with
no keys held in keyboardDown; the precise physical input source is unconfirmed.
The carrier loop previously treated this level as repeated fire. Tests cover
300 held ticks with one shot, followed by release/repress for the next shot.

### Enemy bomb contact priority (18.08.2026.uss report)

Enemy bombs/missiles advance and resolve aircraft/deck contact before Harrier
missiles and carrier gunnery resolve interceptions. A bomb already reaching
Harrier cannot be cancelled by a same-tick defensive shot. Earlier interception
still works. Bomb damage remains 50 armour points, in flight and on deck.
Regression cases cover simultaneous player missile/carrier bullet interception and carrier missile friendly fire at the
Harrier position and two successive normal/heavy bomb hits on level two.
The supplied save is already in Land Now, with no active bombs and full armour;
it cannot reconstruct the earlier reported impacts.

Direct Harrier bomb hits defeat a helicopter or heavy bomber in one hit.
Helicopters retain their fatal descent and repair drop, including over land.
Missiles still require two helicopter hits or four heavy bomber hits.

Enhanced overland terrain/building contact now costs 50 armour points and
bounces Harrier upward; ground targets cost 40 points and are destroyed.
Eight logical recoil ticks lift 16 pixels after surface separation and override
vertical input; continuing contact during those ticks cannot apply damage again.
Existing missile/flak damage still counts. Exhausted armour triggers normal
failure/eject handling. Classic, sea and carrier collision rules are unchanged.
The new AudioGen ground_boing sample is precomputed signed 8-bit Paula data.

Ground impact feedback now uses a short metallic thud/rattle sample instead of
the boing. During recoil the Harrier sprite alternates horizontally by two,
then one pixel, without changing gameplay coordinates. A rising grey smoke
puff at the world contact point reuses the existing single smoke BOB (impact
has priority over an existing helicopter puff). Damage and lift are unchanged.

Bomb damage to carrier hull: normal 12, heavy 24 (both +50%). Friendly rocket hits remain 8.

Carrier VTOL preserves momentum when the stick is released, on both axes. Use opposite input to brake; hold it to reverse direction. There is no automatic return to hover. Terrain flight is unchanged.

Enhanced deck presentation: landed Harrier keeps its left/right side profile. An intact CPU/P2 Wingman is shown on deck when ready for departure, lowered during the raid and raised again after the aft gun retracts. Disabled or destroyed Wingmen stay hidden, including after eject rescue; a destroyed Wingman still requires the red pickup.

Fully visible bombers now use the OCS blitter for background restore, background save and masked drawing. The artwork is unchanged. A 1,440-byte CHIP cache avoids a preshift table; unsupported ring-seam contexts and allocation failure retain the CPU fallback. Partly visible carrier-scene edges now use clipped blitter poses (see the September 27 update below). Dirty-region handling includes the full word-aligned saved rectangle. Pose packing happens before raster synchronisation, and the DMA path can use a wider safe drawing window.

Matched cycle-exact tests at 100% tempo measured 19.61 FPS with CPU tiles versus 30.33 FPS with the blitter while the bomber was active on A500 (512 KB CHIP + 512 KB slow, Kickstart 1.2). The A1200 run measured 50 FPS for the same bomber sequence. These are measured sequence averages, not a universal frame-rate guarantee. See [bomber renderer verification](assets/enhanced/source/carrier-large-bomber.md#blitter-verification-2026-09-24) for test coverage and local evidence.

Carrier corrections (2026-09-26): vertical takeoff retains the parked side
profile until lateral input begins the VTOL turn. The missile turret and its
launch origin are eight pixels lower (turret Y96, missile top-left Y92).
Bomb erasure now unwinds overlapping missile snapshots and the later Wingman
bomb first, using byte-wide overlap tests; this prevents stale projectile
pixels from being restored during the late render pass.

Enhanced final-carrier landing shares the raid's fixed-point VTOL acceleration,
velocity limits, countersteering and intermediate aircraft poses. Releasing the
stick preserves velocity. Entering manual landing clears earlier VTOL state;
touchdown stops momentum and engine sound. Classic landing and normal terrain
flight retain their existing controls.

## Submarine and bomber turn (2026-09-27)

The final wave introduces one 32x8 submarine ahead of the carrier. It rises over 141 simulation ticks, including a 45-tick periscope-only pause,
and sinks over 48 simulation ticks. Its bottom meets SEA_SURFACE_Y (121). Only Harrier bombs damage it; two direct
hits sink it and award 1,000 base points. Deck service can replenish bombs.
The boat launches one 8x16 ballistic missile at a time, initially after 100
surfaced ticks. The missile rises vertically, spends 150 ticks offscreen,
then returns vertically above a selected point on the carrier. Player and
carrier missiles intercept it for 100 base points; gun bullets cannot. An
unintercepted strike deals 24 hull damage. Aircraft contact uses the normal
Enhanced missile damage rule. A launched missile survives destruction of its
submarine. The wave stays active until both threats have been cleared.

The submarine is a persistent four-tile overlay above the waterline, repainted
only as its emergence depth changes. The missile uses two bounded transient
BOB slots. Both indexed masters are exposed in the graphics editor. No new
hardware sprite channels or runtime-generated samples are used.

The bomber waits 125 simulation ticks beyond the edge before returning,
retaining damage. Empty offscreen poses are not drawn. Partly visible planes
now use the blitter, with source clipping before the ring-buffer margin.


Verification on 2026-09-27: the full headless carrier contract passed, including
submarine bomb-only damage, ascent/descent interception, independent missile
lifetime, wave completion, clipped missile restoration and byte-exact bomber
blitter output/restoration at both screen edges. Both new indexed PNG masters
passed the OCS palette/dimension checks.

A cycle-exact A500 test (Kickstart 1.2, 512 KB CHIP + 512 KB slow, 100% tempo)
forced the final wave, kept hull/respawn protection enabled for sustained load,
and completed 2,640 submarine-active updates with 12 launches and 11 returns.
Post-startup ten-second windows averaged 26–42 FPS; this deliberately overlapping
bomber/submarine workload is not a steady 50 FPS. Bomber draws used the blitter
1,843 times with zero CPU fallback draws and at most 26 measured raster lines.
This is an automated stress measurement, not a substitute for visual hardware
playtesting. Local evidence: .tmp/carrier-submarine-final.log and
.tmp/tempo-isolated-A500-100-submarine-wave-2709/perf_log.csv.

The same diagnostic executable and forced-wave input completed on stock A1200
(68020, 2 MB CHIP, no Fast RAM, Kickstart 3.0) at 50 FPS in every post-startup
window. Submarine counters matched the A500 run (12 launches, 11 returns);
bomber render time peaked at 20 raster lines with no missed field while active.
Evidence: .tmp/tempo-isolated-A1200-100-submarine-wave-2709/perf_log.csv.
The normal executable/ADF was rebuilt after diagnostics; test-only invulnerability
and forced-wave settings are not present in that release build.


Waterline correction (2026-09-27): the submarine is clipped across tile rows
14/15 with its lowest pixel at Y120, directly above the sea surface at Y121.
Its first two source rows are the periscope: they emerge over 24 ticks and
remain visible alone for 45 ticks before the hull rises over another 72 ticks.
Collision and launch positions follow the new waterline. Harrier bombs outside
the deck now fall to the sea surface rather than disappearing at deck height.

Waterline validation: full carrier contract passed, followed by the focused
submarine contract including an actual falling-bomb update across Y110. The
periscope-only interval, complete rise, two-hit sinking, interception and
independent missile lifetime all passed. Logs: .tmp/submarine-waterline-test.log
and .tmp/submarine-waterline-focused.log. Normal ADF rebuilt afterward.


Carrier heading and ballistic descent (2026-09-27): lateral input turns Harrier
through the existing front/angled poses in six simulation ticks, independently
of velocity. A released stick completes the turn and retains heading; opposite
input still brakes the existing momentum. Final Enhanced carrier landing uses
the same heading helper, while terrain flight is unchanged.

Returning submarine missiles descend one pixel every two simulation ticks
(25 pixels/second at normal 50 Hz simulation), with the existing CPC parachute
above the missile and no exhaust flame. The canopy costs one extra 8x8 BOB slot;
all three tiles participate in clipping and background retirement. The missile
body remains the intercept/collision target. Packed 8x24 art validated against
the indexed OCS palette and was inspected enlarged; no sprite channels added.

Validation: full carrier contract PASS (.tmp/carrier-heading-parachute.log),
including a completed turn while velocity remains opposite, retained facing on
release, slow descent cadence, missile interception and three-tile restoration.
Normal executable and ADF rebuilt after the contract.


Carrier VTOL bomb momentum (2026-09-27): each Enhanced carrier-phase bomb
snapshots Harrier's signed horizontal and vertical velocity at release, using
1/256-pixel fractions. Turning or braking afterward does not steer the bomb.
Gravity adds 16/256 pixel per update to vertical velocity, capped at two pixels
per update downward; horizontal momentum is retained. Bombs drifting beyond
screen edges are retired. Normal terrain/Classic bomb motion is unchanged.

Validation: focused carrier/submarine contract PASS, including bomb launch
velocity snapshots, upward release followed by gravity, both horizontal drift
directions, independence from facing/countersteering and offscreen retirement.
Evidence: .tmp/vtol-bomb-momentum-final.log. Release ADF rebuilt afterward.


Wingman deck readiness (2026-09-27): parked visibility follows enabled CPU/P2
control and the real destroyed flag. Both initial staging and post-raid lift
show the available aircraft; its existing CPU/P2 takeoff removes the parked
copy. Departure waits for the lift to finish. Focused tests cover CPU, P2,
disabled and destroyed states, secure/depart/wave phases and gun clearance.


Carrier tower repair (2026-09-28): saved state 28.09.2026.uss contains intact
carrier art and an earlier repair-pickup footprint at world column 13, Y103.
Both fast pickup restoration paths previously copied only base terrain/sky,
erasing promoted carrier graphics as the R fell through the island. Pickups
over promoted ships or the surfaced submarine now restore through the world
compositor; ordinary sky retains the scanline fast path. Expanded tile restores
retire overlapping missiles before rebuilding the static objects.

Submarine presentation now exposes only the first four source rows (periscope
and conning tower); the hull remains underwater. Emergence takes 93 simulation
ticks, including the existing 45-tick periscope pause. Bomb collision is a
16-pixel-wide area centred on the tower, and still requires two bombs. The
original full-hull master stays available in the editor.

Validation: focused carrier tests PASS (.tmp/carrier-tower-submerged.log).
The repair-drop regression compares all four colour planes across the island
and its ring mirror before and after a falling R, including the vertical fast
path and final removal. Submarine rise, two-bomb sinking, ballistic interception
and independent missile lifetime also pass. Normal ADF rebuilt afterward.


Presentation tuning: the submerged submarine's conning tower is now 16 pixels
wide and eight pixels tall including its periscope. The packer derives it from
the existing master's periscope/cabin colours without exposing the hull.
Emergence again takes 141 ticks with the same periscope pause. Carrier VTOL
turns take nine ticks (0.18 seconds at 50 Hz), keeping three visible front-facing
ticks between the angled poses; heading remains independent of momentum.
