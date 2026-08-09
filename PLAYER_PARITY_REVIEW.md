# Player parity review: CPC versus Amiga

Status: Sprint 15.87.0. The CPC vendor source is read-only. References below
name the actual routines and state in
`HarrierAttackSourceNew2_alt_CRTC_CART16.asm`; Amiga references name the
corresponding functions in `amiga/main.c`.

## Decision rule

Classic uses CPC gameplay rules with Amiga presentation. A CPC correction is
shared by Enhanced unless an Enhanced exception has already been deliberately
approved. The currently approved gameplay exceptions are:

- both modes require a release before rocket/bomb fire can re-arm;
- Enhanced uses radar admission, three aircraft, recoverable aircraft failure,
  rescue after eject and respawn protection;
- Enhanced may spawn the extra-aircraft powerup;
- pixel-smooth movement, scrolling, sprites/BOBs, graphics and audio are shared
  presentation improvements.

Everything else starts from the CPC rule. New exceptions must be explicit.

## CPC player map

| Area | CPC evidence and state transition |
|---|---|
| Input/movement | `checkplayerplanemovement` (around line 6906) reads the player controls and clamps the character-grid movement. `playerspeed` influences horizontal progress separately. |
| Speed | `checkplayerspeed` (6757+) updates `playerspeed`; landing blocks normal speed control through `playerfrigatestatus`. |
| Takeoff | `settakeoffsprite` (1847+), the carrier setup around 2301+, and the normal game-loop entry create the deck-to-flight transition. |
| Landing | `beginlandingapproach` (3009+) selects the landing sprite and enters `landinghoverloop` (1509+). Its deck/tower tests choose landed versus `wecrashedonlanding`. |
| Carrier collision | `writefrigatetilemap` and its three object-map rows (2892-2916) make deck, tower and aircraft logical objects, not merely artwork. |
| Terrain/object collision | `checkplayeragainstobjectmap` (7525+) dispatches object IDs. `planehitbyobject` (8127+) makes non-flak contact fatal. |
| Rockets | `checkfireplayermissile` (3636+) checks inventory/status and chooses normal versus Maverick. `decrementrocketrange`/`incrementrocketrange` (1969+) constrain the normal range to 10..20 cells; `checkplayermissilemove` (7199+) retires it at that counter. It polls held input; it is not edge-triggered. |
| Bombs | `checklaunchbomb` (963+) seeds `bombmomentum=3`; `decreasebombmomentum`/`movebombmomentum` consume the forward phase before normal descent. Held input can launch another bomb after the prior one ends. |
| Maverick | `checkfireplayermissile`, `checkplayermissilemove` (7199+), `playermissilemovemaverick` (7249+) and `playermissilemovemaverickguidance` (7285+) acquire guidance after the initial range, update `playermaverickdirection` while a target exists, and retain the last direction when lock is lost. `maverickmovetable` contains nine directions. |
| Fuel | `timercountdown` (6735+) compares the 300 Hz `KL TIME` clock. Each high-byte change (256 ticks) decrements a 14-count divider; 16 gauge levels exhaust the tank. Full duration is `14*16*256/300 = 191.15 s`, about 9558 PAL frames. |
| Flak | `flakdamagecount` and `totalflakdamagecount` (205-206), setup around 2929/3005, and `checkplayeragainstobjectmap` implement accumulated non-fatal damage until the skill-derived total is reached. |
| Missile/aircraft hit | The collision dispatch around 6150+ and `planehitbyobject` use the fatal broken-aircraft path rather than graded armour damage. |
| Crash | The `planebrokepart*data` path creates three fragments whose deltas continue generally forward; it then resolves the run outcome. |
| Eject | `checkejectorseat` (1073+) is called from the normal `gameloop` (around 2341), so eject is legal before damage. The seat/parachute animation ends through `docheckejectorseat` (3105+) and does not grant another CPC life. |
| Restart/lives | CPC is one-aircraft gameplay. Eject/crash resolves to score/menu flow; there is no Enhanced-style protected airborne respawn. |
| Wingman CPU | `controlwingmanfunc` (2714+), `wingmanbelowplayer`, `wingmantakeoff`, waypoint/intercept routines (2377-2783), missile logic (7178+) and bomb state implement takeoff, alternating formation, interception, bombing and landing. |
| Wingman Player 2 | `wingmanon=2` dispatches the second control path, including `checklaunchbombwingmanpl2`; it shares CPC object-map consequences. |
| Left behind/revival | `wingmantakeoff` retains destroyed/left-behind state. `wingmanpowerupstatus`, the powerup checks around 7525+, and `wingmandestroyed` (8088+) make a Wingman pickup the recovery route. |

## Amiga player map

- `GameState.gameMode` is the session mode. Policy is centralized in
  `gameplayUsesRadar`, `gameplayUsesEnhancedFailure`,
  `gameplayUsesSafeRespawn`, `gameplayStartingAircraft`,
  `gameplayUsesCpcCollisionRules` and `gameplayUsesCpcEjectRules`.
- Movement and speed are handled by `updateThrottle` and the normal-flight,
  takeoff and hover sections of the main loop.
- `updateLandingApproach`, `playerObjectMapCollision`,
  `playerOnNativeCarrierDeckPixels` and `updateGameCollisions` own landing and
  logical collision results.
- `launchRocket`, `launchBomb`, `updateWeapons`, `updateTargetLock`,
  `directionToMaverickTarget` and `moveGuidedMaverick` own player weapons.
- `updatePlayerFuel` implements the CPC 300 Hz rational cadence for both modes.
- `applyPlayerFlakDamage` and `flakDamageThresholdForSkill` own accumulated
  flak damage.
- `startAircraftFailure` selects immediate CPC crash or Enhanced descent;
  `startPlayerEject`, `updatePlayerEject`, `completePlayerEject`,
  `startPlayerCrashWithSfx` and `respawnPlayer` resolve the outcome.
- `updateWingman*`, `destroyWingman`, `activatePowerup` and the Player 2 input
  path own Wingman behaviour.

## Deviation matrix

| Area | CPC behaviour | Current Amiga behaviour | Classic status | Enhanced status | Recommended action | Priority |
|---|---|---|---|---|---|---|
| Movement response | Vertical character-grid motion; horizontal tile X is `8 + playerspeed/2` | Pixel-smooth vertical motion and one X anchor per speed level | ACCEPTABLE AMIGA PRESENTATION | Same shared baseline | Source mapping and Amiga interpolation are contract-tested; retain smooth presentation | Done |
| Speed meaning | `playerspeed` is 0..15 and selects eight logical X columns; CPU delay controls cadence | Same 0..15 gauge drives a deliberate 96..186 pixel anchor and four smooth scroll bands | ACCEPTABLE AMIGA PRESENTATION, deliberately not cycle-identical | Same shared baseline | Keep approved responsive mapping; do not label it exact CPC timing | Done |
| Takeoff | Scripted deck departure | Scripted deck departure | FUNCTIONALLY SIMILAR | Same | Manual carrier/tower test | P1 |
| Landing | Hover, deck/tower checks, relaunch | Explicit approach/hover/deck states | FUNCTIONALLY SIMILAR | Same | Manual front/rear/tower matrix | P1 |
| Logical collisions | Cloud/sky, Wingman and pickup are safe; flak accumulates; other occupied cells are fatal | Central CPC outcome table with pixel-smooth body overlap | FUNCTIONALLY LIKE CPC | Same CPC outcome | Contract-tested; retain manual edge test | Done |
| Rocket/bomb trigger | Held key may refire after projectile ends | Release-to-rearm | Intentional shared improvement | Intentional shared improvement | Keep as approved | Done |
| Rocket inventory | Skill/full/refill counters | CPC-derived counts | FUNCTIONALLY SIMILAR | Shared | Regression test inventories | P2 |
| Normal rocket range | Menu value 10..20 cells, default 10; shared with Wingman normal rocket | Selectable 10..20 setting copied into each session; pixel BOB retires after the equivalent distance | FUNCTIONALLY LIKE CPC with smooth presentation | Same shared baseline | Contract-tested; manually compare endpoint at 10 and 20 | Done |
| Bomb momentum | Launch plus three momentum updates move down while holding screen X; descent then keeps world X and follows scenery left | Four logical downward momentum rows and fixed-world-X descent, interpolated by pixel mini-BOB | FUNCTIONALLY LIKE CPC with smooth presentation | Same shared baseline | Contract-tested; manually confirm contact artwork against terrain | Done |
| Maverick | Eight-column launch phase, exact nine-direction guidance, last direction on lost/arrived lock | World-coordinate lock plus guided pixel BOB | FUNCTIONALLY LIKE CPC | Shared baseline | Manual visual test of target contact remains | P1 |
| Fuel | ~191.15 s full tank | Sprint 15.79 uses exact rational time model | FUNCTIONALLY LIKE CPC | Shared correction | Manual long-run display/failure check | Done |
| Flak | Accumulator and skill threshold | `flakDamageCount`, skill threshold | FUNCTIONALLY SIMILAR | Shared | Verify all five thresholds against CPC setup | P1 |
| Missile/plane hit | Fatal broken-aircraft result | Immediate crash in Classic | FUNCTIONALLY LIKE CPC | Enhanced intentional failure window | Keep separation | Done |
| Crash fragments | Three forward-moving pieces | Three forward-biased pieces | ACCEPTABLE PRESENTATION | Same | Visual test at high/mid/low altitude | P2 |
| Healthy eject | Legal; parachute then end run | Legal in both; Classic ends run after animation | FUNCTIONALLY LIKE CPC | Intentional rescue/aircraft loss | Manual healthy-eject test | P1 |
| Starting aircraft | One | Central policy: Classic 1, Enhanced 3 | FUNCTIONALLY LIKE CPC | Intentional Enhanced feature | Contract-tested | Done |
| Respawn immunity | No protected airborne respawn | Classic 0 frames; Enhanced 90 | FUNCTIONALLY LIKE CPC | Intentional Enhanced feature | Contract-tested | Done |
| Radar | No accumulated radar gameplay | Disabled from Classic admission; enabled Enhanced | FUNCTIONALLY LIKE CPC | Intentional Enhanced feature | Contract-tested | Done |
| Failure descent | No recoverable descent | Bypassed in Classic | FUNCTIONALLY LIKE CPC | Intentional Enhanced feature | Contract-tested | Done |
| Wingman CPU | CPC formation/intercept/bomb/landing state | Dedicated smooth state machine | FUNCTIONALLY SIMILAR, full trace pending | Shared | Long sortie test incl. left-behind | P1 |
| Wingman Player 2 | Direct P2 control, CPC world rules | Direct P2 control and independent weapons | FUNCTIONALLY SIMILAR | Shared plus Enhanced pickups | Two-player collision/ammo test | P1 |
| Wingman revival | Wingman drop revives | `POWERUP_WINGMAN` revives at pickup | FUNCTIONALLY SIMILAR | Shared | Event telemetry added | Done |

## Classic gameplay contract

Classic must maintain these invariants:

1. `gameplayStartingAircraft()` returns the CPC one-aircraft value.
2. `gameplayUsesRadar()` is false; radar cannot affect admission, firing or
   difficulty.
3. `gameplayUsesEnhancedFailure()` is false. Fatal CPC causes enter the broken
   aircraft path without an Enhanced recovery window.
4. `gameplayUsesSafeRespawn()` is false. Debug respawn cannot import Enhanced
   immunity.
5. `gameplayUsesCpcCollisionRules()` is true. This is also the baseline in
   Enhanced because no collision-rule exception has been approved.
6. `gameplayUsesCpcEjectRules()` is true: healthy eject is accepted, but the
   completed parachute sequence ends the one-aircraft run.
7. Fuel uses the CPC clock/divider model and flak uses the CPC skill threshold.
8. Enhanced-only extra-aircraft drops, radar admission and failure physics do
   not execute.
9. Release-to-rearm remains a deliberate shared input improvement.
10. Rendering, scrolling, sprites/BOBs, smoke, splash and Paula audio do not
    alter these logical outcomes.

The compile-time `HAR_HEADLESS_CLASSIC_CONTRACT_TEST` checks the central mode
rules, the exact 9558-frame full-fuel lifetime, CPC's `8 + speed/2` horizontal
source mapping, and the approved Amiga monotonic anchor/scroll bands without
allocating gameplay graphics or touching high-score media.

## Player event telemetry

Telemetry remains disabled by default. When enabled, the event ring records
movement limits, speed changes, rocket/bomb release, Maverick lock/loss, flak,
missile and aircraft/object collisions, failure, eject, crash, respawn,
landing start/completion, Wingman left-behind and Wingman revival. Each new
record includes frame, world column, player X/Y, speed, fuel, armour, mode,
reason and the deterministic/random value supplied by the caller.

## Still requiring manual play tests

- Classic healthy eject: full seat/parachute animation, then Game Over.
- Enhanced healthy and damaged eject: rescue, one aircraft consumed, no state
  leakage into Classic.
- Fuel exhaustion after roughly 191 seconds at both zero and maximum speed.
- Carrier deck rear/front, tower and parked-Wingman collision zones.
- Maverick acquisition, destroyed target, lost target and retained direction.
- Bomb contact artwork at several speed/height combinations; logical momentum
  and scrolling relationship are contract-tested.
- Two-player Wingman left-behind/revival and shared Classic pickup rules.
- Skill 1-5 flak thresholds and first-start versus Enhanced respawn placement.

## Sprint 15.79 conclusion

Actually wrong: distance-based fuel, Classic inheriting Enhanced respawn
protection, Classic being unable to perform normal CPC eject, and mode policy
being scattered at the changed call sites.

Already correct or intentionally retained: release-to-rearm, pixel-smooth
movement/rendering, CPC-shaped fatal object outcomes, skill-derived ammunition,
Wingman revival, and Enhanced radar/failure/rescue as explicit extensions.

Changed: exact shared CPC fuel cadence, centralized mode policies, Classic
one-aircraft/eject/respawn contracts, discrete player telemetry, and a
compile-time contract test. Maverick timing, exact movement cadence and the
complete Wingman state trace remain evidence-gathering tasks; visual bomb
contact remains a manual check even though its logical trajectory is now
contract-tested. The subsequent Sprint 15.82 audit resolves the Maverick
source trace; complete Wingman state parity remains an evidence task.

## Sprint 15.80 movement/speed audit

`checkplayerplanemovement` proves that CPC horizontal position is
`8 + playerspeed/2` character columns: speed 0..15 selects columns 8..15.
The port's 96..186 pixel anchor and 1..4 pixel/frame world-scroll bands came
from the explicitly approved responsive-control passes in Sprints 14.11 and
14.36. They are therefore retained, identically, in Classic and Enhanced as
an Amiga presentation/control improvement. The contract test now locks both
the real CPC source mapping and the deliberate Amiga mapping, including
monotonicity and screen bounds. Exact CPC CPU-delay cadence is not claimed.

## Sprint 15.81 bomb trajectory audit

The earlier interpretation of `bombmomentum` as four purely horizontal steps
was incorrect. CPC `movebombmomentum` increments H (the bomb row) on launch
and on each of the three remaining momentum updates while L remains fixed on
screen. After status changes to descent, `decreasebombheight` decrements L to
follow the already-scrolling scenery and increments H again. In world terms,
the first four rows inherit forward world motion from scrolling; subsequent
rows retain a fixed world X.

The shared Amiga path now implements exactly that logical relationship while
retaining its approved 2/3-pixel mini-BOB interpolation and release-to-rearm.
The contract test verifies four downward momentum rows, fixed screen X during
momentum, fixed world X during descent, and the deterministic interpolation
cadence. Classic and Enhanced use the same corrected bomb physics.

## Sprint 15.82 Maverick audit

CPC `checkfireplayermissile` accepts any non-zero
`enemylandlocationlock`; `scrollenemylandlocationlock` alone expires it at
screen character column 5. `playermissilemovemaverick` flies from range 2 to
10 before guidance. `getdirectionfromcoords` performs exact coordinate-sign
comparisons over a 3x3 direction table, and
`playermissilemovemaverickguidance` retains the previous direction when the
lock disappears or the exact target returns direction zero.

The shared Amiga path now follows those rules. It no longer rejects a valid
lock merely because the Harrier has passed it, and it no longer treats a
four-pixel band as an aligned axis. The smooth four-pixel BOB mover shortens
only its final step to an exact target axis, preventing oscillation without
changing the CPC steering decision. A contract trace covers launch distance,
one-pixel steering, final-step clamping and retained direction after lock
loss. Target-contact timing still needs a live visual check.

## Sprint 15.83 collision and carrier audit

CPC `checkplayeragainstobjectmap` makes cloud, sky, Wingman and the separately
handled pickup non-fatal, sends object ID 10 through accumulated flak damage,
and sends every other occupied object to `planehitbyobject`. The Amiga outcome
selection is now centralized in `cpcPlayerCollisionForObjectId` and covered by
the contract test. The existing smooth inset body overlap remains a shared
presentation/collision sampling choice because it prevents visible wings from
passing through terrain; no Classic/Enhanced split was introduced.

CPC `writefrigatetilemap` defines the carrier as twelve solid deck cells, four
middle tower cells and only two upper tower cells. Amiga previously used one
32-pixel tower rectangle shifted eight pixels to the right for both heights.
That made clear air behind the CPC tower fatal. The logical carrier tower now
uses the exact stepped 2/4-cell mask. Sprint 15.84.2 additionally restored
CPC's 2x1-character player probe for this tower test: using the smooth Amiga
sprite rectangle reached into the next tower row up to seven pixels before
`currentplayerlocation` would do so on CPC. The parked-Wingman landing
exclusion is retained as the already approved carrier-deck rule. Rear/front
deck contact and the revised tower edge still require a live landing test.

## Sprint 15.84 bomb-contact sampling

The CPC-derived bomb trajectory remains unchanged: four logical momentum rows
follow world progress and subsequent rows retain world X. The Amiga mini-BOB
interpolates those rows at 2/3 pixels per PAL frame. Player 1 collision was
still sampled only when a complete eight-pixel logical row elapsed, however,
while Player 2 already sampled the visible BOB every frame. A diagonal bomb
could therefore visibly cross a ground-target cell between probes.

## Sprint 15.85 Wingman bomb trajectory

CPC `checklaunchbombwingman`, `checklaunchbombwingmanpl2` and the Player 1
path all enter the same `dolaunchbomb`/`decreasebombmomentum` state chain.
Consequently neither CPU nor Player 2 owns a permanently vertical special
bomb: it holds screen X through the four initial logical momentum rows and
then remains at fixed world X while scrolling continues. The CPU approach
also subtracts five character columns (`dec l` five times) before release.

Amiga previously world-anchored both Wingman bombs immediately and the CPU
approached almost directly above its target. Both Wingman controls now share
the same four-row interpolated momentum as Player 1. CPU uses CPC's five-tile
lead; Player 2 still releases freely from the human-controlled position.

Both player bombs now use the actual lowest opaque mini-BOB pixel and test it
every frame. This is an Amiga presentation/collision correction shared by
Classic and Enhanced; it does not change launch position, momentum, fall rate,
inventory, target damage or CPC logical motion.

Sprint 15.84.1 fixes the compositor ordering around those contacts. Flying
bomb/rocket BOBs save the background they cover. On a hit, that saved
pre-impact background must be restored before the persistent CPC smoke,
crater or destroyed tile is rendered. The previous reverse order let the
late-frame erase overwrite the new world state, most visibly as a missing
enemy-ship stern without its replacement smoke. Player 1, Player 2 and CPU
Wingman projectile paths now retire their footprints before world mutation.
