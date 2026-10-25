# Enhanced campaign and scene testing

The regular ADF now contains the terrain game and all three special encounters.
Classic retains its terrain/landing loop.

## Campaign order

1. Sortie 1: take off, terrain and city, carrier landing, carrier defence.
2. Sortie 2: terrain and city, open-sea approach with gradual braking to VTOL
   as the oil platform enters from the right, defence, then automatic departure
   over open sea to the carrier approach, landing and carrier defence.
   Helipad service during combat is optional; victory requires no touchdown
   from either player. Surviving P2 follows the departure; CPU Wingman still
   returns only via a later red pickup.
3. Sortie 3: terrain and city, open sea and battleship encounter, final carrier landing.
   Harrier and surviving Wingman retain their screen positions on entry.
   The boss camera starts at the terrain scrolling speed and eases down to
   the ship-relative speed. Scene preparation retains the previous game image
   in the existing menu bitmap, without displaying a menu/loading page.
4. The next three-sortie cycle starts automatically after the landing fanfare.
   Mission number continues increasing; existing skill progression is retained.
   Carrier wave count and aircraft quota also increase with progress, with caps.

Oil-platform health is separate from carrier hull. Carrier hull, gun damage,
repair cargo, player lives, ammunition, armour and score survive scene changes.
A lost Wingman stays lost. Oil-platform CPU Wingman follows its existing rule:
return through a later red pickup. P2 uses the platform's shared-lives rules.

Encounter balance: the repairable oil platform takes two thirds of normal hull
damage (rounded up) and waits one third longer between aircraft spawns.
Its quota is 7 + 2 per skill step, plus 2 for CPU or 4 for P2. Carrier
damage and spawn timing are unchanged. Battleship cannon intervals are
60/51/42 ticks across its three damage stages, and missile checks use 100 ticks;
the existing two-shell/four-flak-cloud limits still apply.

## Oil sea transitions (2026-10-15)

The last coastal strip before the platform and the entire return approach use
open sea, with no harbour buildings or invisible land collisions. The friendly
carrier enters normally on the return. Aircraft coordinates and the current
sky/palette fade state survive the handoffs. The sea camera keeps its pixel alignment and repeating surface
pattern; platform arrival gradually reduces the inherited scrolling velocity.
Victory departure preserves both Harriers' height instead of resetting them.

`test-campaign-transition.ps1` runs the real scene handoffs through platform
arrival, automatic departure and carrier hover in muted WinUAE. Contract tests
also check sky preservation, water geometry/collisions and aircraft positions.

## Debug scene jump

- At the main menu press Shift+D (Ctrl+D also works).
- Set SCENE JUMP to ON, return to the main menu and start Enhanced.
- During play press Shift+L to pause and open the scene selector. Shift+D
  (or Ctrl+D) always opens the framerate/telemetry overview, even when scene
  jumping is enabled. Esc or Shift+L closes the selector without leaving play.
- The scene selector is redrawn only on opening or a changed selection, not
  every paused frame, to avoid repeated clear/redraw flicker.
- Left/right selects Takeoff, Land + City, Carrier Attack, Oil Platform,
  Battleship or Landing. Fire/Return activates; Esc closes without jumping.
- The current run keeps its resources. Using a jump disqualifies that run from
  high-score entry. A fresh game clears the jump marker.

## Hard landing

Enhanced Carrier and platform landings accept up to 1 pixel per logical update
of downward velocity (256 in 8.8 fixed point). A faster deck contact removes
30 armour percentage points and bounces the aircraft upward. Gentle landings
are unchanged. This is collision damage, independent of the friendly-fire option.
Helicopters derive their facing from actual horizontal motion and briefly use
the middle pose during turns; the hovering pose is front-facing.

## Verification (2026-10-09)

`test-campaign.ps1` executes Amiga-native contract tests in WinUAE:
scene order, automatic special-scene requests, no repeats, Classic exclusion,
state preservation, hard/soft landing and helicopter facing.

Disk-only boot and manual scene-jump smoke test used cycle-exact A500,
Kickstart 1.2, 512 KB Chip + 512 KB Slow RAM. Menu and jumps between terrain,
Carrier, oil platform, battleship and landing were inspected. Around 17 KB
Chip RAM remained in the initial combined build. This is a tight memory budget;
existing optional ambient audio allocation remains conditional on free memory.
A complete three-sortie playthrough and real-hardware timing still need testing.
Additional native regressions passed: battleship camera reversal, persistent
targets, victory and background restoration; oil platform solo/CPU/P2 combat,
health, landing, respawn and VTOL restoration.

## Standalone oil test and boot verification

Use `build-oil-platform.ps1` to build a direct-entry oil ADF from the SAME
sources as the campaign. Primary output: `amiga/out/harrier_oil_platform.adf`.
The two legacy oil ADF names in `.tmp/oil-platform-prototype/amiga/out` are
updated as aliases; they must not be rebuilt from stale prototype sources.
Run `amiga-build.ps1` afterwards to leave normal campaign objects as default.
The game executable on each disk is named `harrier_amiga.exe` deliberately.

A500 / KS1.3 / 512 KB Chip + 512 KB Slow, disk-only: cold boot, menu and
normal takeoff passed. The same setup with development DH0/DH1 mounted
reproduced a return to DOS: HUD and engine allocations failed.
The menu ticker arena now holds HUD + engine while gameplay runs, saving
19,648 bytes of Chip allocation. Their lifetimes do not overlap; session
entry rebuilds both and menu entry rebuilds the ticker. The arena is freed
only once. A compile-time size check guards against overlap. The previously
failing mounted-drive setup now boots and passes menu -> play -> menu -> play.
Optional sea/gull audio is omitted if there is insufficient free Chip RAM.
Allocation failures now print a DOS explanation instead of silently exiting.

Clean WinUAE disk profiles: `amiga/harrier_disk_a500.uae` (campaign) and
`amiga/harrier_oil_disk_a500.uae` (oil). They exclude mounted hard disks.
Boot fresh after replacing an ADF: a saved state retains the old executable
and graphics, independently of the current disk image.
# Music residency follow-up, 2026-10-09

All three MODs now use lossless packed masters and one ordinary-RAM decode
buffer. Only menu instruments reserve dedicated Chip RAM (128 bytes); the
other tracks reuse the ticker arena tail while it is not displayed. See
`assets/music/README.md` for accounting and bounds. This reduces dedicated
music Chip storage by 20,876 bytes, but reduces total data storage by only
1,624 bytes. Do not report the former as total free-memory growth.

Verified in WinUAE A500/KS1.3, 512K Chip + 512K Slow: full ADF boot to menu,
flight and menu return; all three tracks through the music browser with valid
Paula Chip sample pointers; then return to the menu and start another game.
The instrument-sharing build also boots with DH0/DH1 mounted. In that run,
after optional allocations, 4,352 Chip bytes and 15,272 Slow bytes remained;
sea ambience was available, the optional 44,100-byte gull decode buffer was not.
This is still a tight 1MB target, not a claim of abundant memory.

Run automated WinUAE tests muted (`sound_volume=100`, WinUAE attenuation).
Keep audio emulation running for meaningful Paula/DMA timing verification.

## Open-sea scene preparation (2026-10-15)

Both special-mission routes remove the harbour and carrier from the final
coastal stretch. Entry waits for existing aircraft/missiles to clear this sea
corridor. Battleship victory now routes through the same ocean return as oil,
instead of jumping directly to the carrier's hover position.

The existing menu bitmap temporarily displays an ocean with eight small
moving wavelets and live P1/P2 aircraft sprites. As of 2026-10-16, two additional
attached sprite pairs support a bounded dogfight: alternating front/rear
fighters, at most two simultaneously and four per crossing. CPU Wingman eases
into formation and fires at aligned targets ahead. P2 retains independent controls.
Rockets and bombs consume the shared magazines; damage, score and fuel carry
into the destination. P pauses/resumes the crossing. The mission's sky palette and aircraft positions survive.
The hidden world buffer is cleared in 2 KB slices, then built in at most 13-row pieces per
PAL field, stopping early at raster line 180, using the same renderer as ordinary initialization. No new Chip
bitmap is allocated. Same-mission transitions reuse the procedural world tables.
Combat runs before loading, which yields when raster time is short. Eight
compact projectile slots (four reserved for friendly fire) and tiny hit bursts
use the temporary bitmap; the normal ring-stride BOB renderer must never draw
into it. No extra Chip RAM is allocated for the encounter.
Once the buffer is ready, no new enemies or hostile shots are spawned. Existing
fighters leave normally; all projectiles and bursts must clear, then 35 quiet
PAL updates elapse before hand-off. Holding fire cannot block the hand-off once
the last enemy has left. Failed aircraft descend over open sea; eject uses the
existing reserve-life rules, without probing the hidden destination's terrain.

A return supply drop on Battleship prioritizes an empty rocket or bomb magazine
over a lost enabled Wingman. If both are empty, the normal alternating ammo
selector applies. With both magazines nonempty, Wingman retains priority.
There is still at most one drop per return, and no armour drop.

`test-campaign-transition.ps1` exercises oil arrival/exit, carrier approach,
Battleship arrival and another ocean return. It compares incrementally built
bitmaps with full reference rendering and reports CIA-A VSync work spans.

The Battleship raw sea-row writer now clips to the actual 168-pixel world
bitmap. The logical map has 25 tile rows; drawing all 25 previously wrote
beyond the 21 visible rows and could corrupt adjacent allocations. The boss
contract includes a guard region beyond the framebuffer.

Validation: muted, cycle-exact WinUAE A500/KS1.3, 512 KB Chip + 512 KB Slow.
Campaign contract and Battleship contract (ammo priority and framebuffer guard)
passed. The live four-handoff oil/return/boss/return test passed, including
byte-equivalent world buffers and normal AmigaOS teardown. Maximum measured
preparation work span was one CIA-A VSync; this measures each preparation
slice, not a claim that every scene-entry setup instruction is zero-cost.

2026-10-16 combat validation: the muted cycle-exact 512 KB Chip + 512 KB Slow
four-transition run passed buffer equivalence, spawn/shot drain checks, ammo
exhaustion/release-to-rearm, hit scoring and rear-entry contracts. Per-update
work stayed within one CIA-A VSync after HUD updates were split into individual
fields/digits. The first all-at-once HUD version measured two VSyncs and was
rejected. Native WinUAE screenshots also confirmed fighters/projectiles over
open sea. This does not certify every entry/exit setup cost or replace testing
on real hardware.

The final combined ADF was also cold-booted, muted, without mounted hard disks
on the same A500/KS1.3 512+512 KB profile. Its normal main menu was visually
confirmed; runtime reported 6,984 free Chip bytes. All three ADF variants were
rebuilt from the same source revision (working tree, not yet committed).

2026-10-16 frigate missile regression: the route's one-shot ship triggers now
respect the same open-sea/special-scene exclusions as ship rendering. Triggers
are still consumed while hidden, so no delayed shot can appear later. Muted
A500 campaign contracts passed visible frigates in Classic and Enhanced,
the first ship on a special route, hidden final/return ships, no deferred shot,
and an already occupied missile slot. The reported 16.10.2026.uss was located;
this validation uses deterministic contracts, not a replay of that savestate.

2026-10-16 ocean-wave validation: transition tests now inspect the compact
visible water bitplanes after each wave update, checking that the new crest
is white and the old trailing pixel is sea, including horizontal wraparound.
All four muted A500 transitions passed (moved=1, wrapped=1, failed=0), with
max-work-vsyncs=01 and buffer equivalence intact. Existing release animation
already updates before lazy loading and independently of aircraft speed;
this change adds regression coverage, not a different release animation.

2026-10-16 repair balance: each R still consumes one kit (20 cargo units),
but now restores 10 hull points instead of 20. The oil platform uses the
same 75-tick landing service, including a landed P2, and carried kits are
available on arrival. Remaining kits return to the carrier; platform repair
does not alter saved carrier hull/guns. Full health never consumes a kit
(except the carrier may still repair damaged guns). Fire tiles are restored
when hull changes so repaired platform fires disappear. Cockpit HULL text
is removed; world health bars and the cockpit R count remain.
Validation: muted oil-platform contract passed shared P1/P2 service, kit
consumption/capping and carrier-health preservation. Focused carrier repair
contract passed interval/cancellation, 10-point gain, gun repair and full-health
cargo preservation. The broader legacy carrier suite reaches an outdated
soft-landing expectation (131); a controlled-descent experiment then failed
to produce a final result and was stopped. That unrelated test edit was reverted;
the broad suite is not claimed as passing.

2026-10-16 fuel balance: shared burn advances by 345 clock units instead of
300 (+15%), including P2 platform VTOL. Tank capacity, higher-difficulty
reserves, the existing VTOL multiplier and refill quantities are unchanged.
Muted campaign contract passed full-tank depletion for both modes, all five
difficulties and both flight states (20 cases). Base full-tank endurance is
8,311 simulation ticks, formerly 9,558. Updated the campaign pickup fixture
to add a kit to the shared oil-platform cargo rather than overwrite it.

2026-10-16 lazy-entry hitch: measured the previously uninstrumented entry
before the transit loop, using CIA-A PAL fields. Baseline freeze/reset totals
were 45/18/53/33 fields for oil/debug-new-mission, oil return, boss/debug-new-
mission and boss return. Final totals are 14/2/17/2 (freeze=2 each). The two
new-mission fixtures still synchronously generate procedural tables, unlike
ordinary same-mission handoffs; do not claim those debug jumps are hitch-free.
The ocean entry uses D-only blitter fills and an A->D HUD copy, preserving the
waterline and palette. Hidden session HUD redraw is skipped; an already live
engine loop is reused rather than synthesised twice. Battleship roof searches
are stepped one scanline per transit tick and its fire tiles one frame per tick.
Non-ocean jumps retain full snapshots and direct boss startup stays synchronous.
No extra Chip allocation. Four muted cycle-exact A500 transitions passed world
buffer equivalence, combat drain and displayed-wave checks; max loop work was
one VSync. Entry timing excludes the subsequent diagnostic log write and does
not certify zero end-to-end presentation latency or real-hardware timing.
Logs: .tmp/oct16-entry-baseline.log, .tmp/oct16-entry-fast.log,
.tmp/oct16-entry-final-test.log; detailed timing in campaign-transition-run's
winuaelog.txt belongs to the last run.

2026-10-16 oil difficulty: platform hull damage now rounds up two thirds of
base damage, previously four fifths. Normal/heavy bombs deal 8/16 instead
of 10/20; rockets deal 6 instead of 7. Carrier damage, enemy quotas and the
10-point repair kits are unchanged. Muted oil contract passed, including
explicit normal/heavy/rocket damage and unchanged carrier damage checks.

2026-10-16 bomber audibility: drone volume is 20 (was 12 in flight and 14
in the sound-bank preview). Waveform, pitch, damage cough and ambient priority
are unchanged; no new sample memory or audio channel. Muted audio contract
passed DMA-loop length, cough silence, restored volume 20 and shutdown after
destruction. This checks registers/scheduling, not a subjective listening mix.

2026-10-16 battleship departure: the 224-tick destruction finale now enters
phase 3 instead of marking the scene finished. Aircraft continue forward with
P1/P2 steering and shared Wingman movement, easing the camera to 2 px/tick.
A prior reverse pass cannot reverse the victory departure. Scene completion
requires camera >= (64+120)*8+64 = 1536, so the entire bow is behind the left
screen edge with 64 px of ocean clearance. Weapons/pickup are retired at the
start of departure. Muted focused boss-exit contract passed from both normal
and reverse camera directions, retaining steering and Wingman and preventing
early finish. The generic printed boss-test label covers more than this focused
run; only the exit fixture was selected via HAR_HEADLESS_BATTLESHIP_EXIT_TEST_ONLY.

## Carrier gull audio (2026-10-16)
The first existing idle master is trimmed to 800 ms (original WAV unchanged).
Its 7,056 decoded bytes share the runtime arena tail with MOD instruments.
The previous separate 35,280-byte optional Chip allocation is removed. Only
the first clip is embedded; the legacy second debug slot aliases it.
The first bird of each idle flock requests one cue. Incremental preparation
continues while idle, with no periodic playback without birds. Departure or
loss of eligibility cancels it; MOD startup stops effect DMA before replacing
shared samples. Compile-time bounds protect the arena.
The muted audio contract passed preparation without birds, event-triggered
DMA, shared sample pointer, cancellation on departure and pending-event
cancellation. Its legacy result label remains depot-reload-and-bomber-loop.
This verifies playback logic, not subjective quality of the shortened clip.

Routine builds/releases use only the main ADF as of 2026-10-16. Separate scene
ADFs remain legacy developer tools; use debug scene selection in the main game.

Public Beta 6 main ADF cold boot verified on 2026-10-16 in muted cycle-exact
WinUAE: A500/KS1.3/512K Chip + 512K Slow, floppy only. Loading, Enhanced menu,
Start Game, carrier deck, takeoff and terrain entry were visually confirmed.
Startup logged 92,800 free Chip bytes. Evidence: `.tmp/beta6-boot/001` through
`005` screenshots and `winuaelog.txt`. This is a boot smoke test, not a full run.

## Enhanced Easy and supply balance (2026-10-17)
Menu Skill 1 and 2 reduce nonfatal damage amounts to 85% (rounded up),
including carrier/platform hull. Their flak hit budget increases to match.
This uses starting menu skill, so assistance persists as mission difficulty
rises. Fatal geometry/fuel failure, Classic and Skill 3-5 are unchanged.
Enhanced regular supply admissions retain their existing gates and empty
rotation slot. Empty ammunition takes reward priority; at <=25% capacity,
three of four selector outcomes favour ammunition. Compare remaining fractions
of capacity, varying tie outcomes. Urgent supplies can precede replacement
Wingman. Battleship return-pass supply uses the same need selector, still one
drop per pass; repair and extra-aircraft rewards remain dedicated rewards.
Muted WinUAE focused contract: PASS enhanced-easy-and-needed-ammo.

## Seven-position AA guns (2026-10-17)
Oil-platform CPU AA and Battleship cannon turrets now share seven prepacked
8x8 elevations, generated alongside carrier art in the existing grey palette.
The original carrier and missile launcher poses are unchanged. Turns advance
at most one pose every three logic ticks; oil starts at the vertical pose.
Battleship computes its ahead-of-aircraft flak point before selecting the
graphic, using the same vector as shell launch rather than tracking Harrier.
A cannon waits until aligned before firing. Only changed poses dirty boss
tiles; no runtime pixel rotation or extra Chip buffer. The additional seven
tiles occupy 280 bytes of ordinary constant data plus their pointer table.
Muted focused contract passed seven distinct masks, angle ordering, bounded
turn cadence and a lead-angle case. Packed-pixel preview visually inspected
at `.tmp/oct17-aa-poses.png`. No full gameplay performance claim from this test.

### Oil-platform pilot rescue (2026-10-17)

A parachute touchdown anywhere on the visible helipad now queues the shared
carrier restart at the beginning of the same mission, instead of placing a new
Harrier on the rig. The eject completion consumes one reserve aircraft; the
restart does not charge again. Score and destroyed terrain targets follow the
existing rescue rules. Carrier hull/guns and remaining repair kits are restored
from the platform encounter before taking the rescue snapshot. The oil encounter
is not counted as completed. Water and a destroyed platform cannot rescue pilots.

### Enhanced Easiest balance (2026-10-17)

Menu skill 1 now takes 65% of base incoming damage instead of 85%, for
Harrier and the defended carrier/platform. Flak tolerance uses the same
percentage. Skill 2 stays at 85%; higher skills and Classic stay unchanged.
The modifier follows the selected menu skill throughout campaign progression.

### Shared ocean fighters and visible rockets (2026-10-17)

The compact lazy-loading display now uses the ordinary land interceptor art
for frontal attacks, and the land pursuer's shared horizontal movement for
rear attacks. Left input lets the rear fighter overshoot. Frontal fighters
align, fire within the ordinary ten-tile attack range, then climb away.
The compact renderer/pools remain separate: world BOB stride and terrain
queries cannot safely address the loader display or unfinished destination.

Transit rockets reuse the Enhanced indexed rocket art instead of five-pixel
white strokes. Bytewise masked writes retain the compact display stride and
clip at screen edges; no new Chip RAM is allocated. Shots persist until a
collision or full departure from the screen, including while loading closes
the encounter. The transition contract checks shot lifetime, pursuer braking,
and actual visible bitplanes after loader work. An unexplained mid-flight
missile disappearance has not yet been reproduced.

Validation: muted A500 WinUAE transition test passed all four handoffs
(`max-work-vsyncs=01`, stable buffer equivalence); missile visual checks
reported zero failures in all four corridors.

### Solid-surface recoil and transit sprite repair (2026-10-18)

Enhanced carrier, oil-platform and Battleship structure contacts now use the
land collision damage, bounce sound and upward recoil policy. Repeated contact
during recoil does not stack damage. P2 on the rig also bounces with equivalent
health loss. Water, exhausted armour and existing hard-touchdown rules remain
separate. The focused muted WinUAE test passed actual carrier, rig P1/P2 and
Battleship collision paths, clearance, recoil and low-armour failure.

Transit attached fighters now use the actual Copper hardware pairs 2/3 and
4/5 rather than assuming that compact storage order equals hardware order.
Reusing that storage also invalidates attached-sprite residency before copying:
otherwise subsequent corridors could reuse stale crash/missile imagery.
The regression test decodes Copper DMA pointers and compares both actual
fighter pairs word-for-word with the normal aircraft builders on each handoff.
Transit player rockets also honour the normal rocket-height-lock option.

Validation: muted A500 WinUAE passed all four transitions with both actual
hardware sprite pairs matching on every handoff, zero missile visibility
failures, stable destination-buffer equivalence and max-work-vsyncs=01.
This catches the reproduced sprite corruption; it does not establish a
separate gameplay cause for the previously reported disappearing rockets.

### Oil-platform CPU crew and beam-safe transit ink (2026-10-18)

An arriving CPU Wingman now parachutes to the shared AA gun. A platform hit
removes a stationed crew; after 450 wave ticks without a crew, an unoccupied
pickup slot can receive a red Wingman supply. Repair drops retain priority.
Collecting red supply starts another crew descent, never a replacement aircraft
on the rig. A crew still stationed when departure begins awards 500 base points
through the ordinary skill/tempo scoring path, once. The CPU aircraft remains
unavailable after departure until the ordinary revival path. Human P2 retains
its existing aircraft controls.

Transit simulation now runs while the previous frame's weapon ink remains
visible. Erase/draw is committed together immediately after WaitVbl; fighter
sprite updates follow. Previously simulation ran between erase and redraw on
the actively displayed bitmap. The hardware test additionally compares the
commit raster with the earliest affected projectile row, since checking final
bitmap contents alone cannot detect an erased weapon at DMA fetch time.

Validation: the extended muted oil-platform contract passed crew descent,
platform-hit loss, red supply/collection, remanning, one-time base bonus and
unchanged P2/mission progression. Four muted campaign transitions passed with
zero missile visibility failures and zero late projectile commits; latest
commit rasters were 32, 32, 17 and 26, all ahead of the affected shot rows.
Destination buffers remained equivalent and max-work-vsyncs stayed 01.
The supplied 18.10.2026 snapshot was inspected offline: the compact Copper
uses the expected four-plane stride and the captured sky bitmap contains no
weapon ink. A single snapshot cannot reconstruct a projectile already hit.

### No duplicate parked Wingman on arrival (2026-10-18)

Campaign restore marks an intact companion available on the deck; airborne
terrain, landing and return scenes then restore the live flying companion.
Those scenes now explicitly clear the parked composite before lazy loading
builds any carrier columns, avoiding a baked duplicate alongside the sprite.
The focused HAR_HEADLESS_CAMPAIGN_WINGMAN_TEST passed in muted WinUAE for all
18 combinations of scene, CPU/P2/off and destroyed/intact status. This was the
focused branch of the campaign contract, not the unrelated full contract.

### Campaign completion fireworks (2026-10-18)

Enhanced sortie-three landing replaces LANDED with a single centered CAMPAIGN
COMPLETE! label inside the existing panel. After the carrier slide, a roughly
480-tick fireworks show replaces that finale's MOD fanfare so existing Paula
launch/ground-impact samples can sound (the MOD otherwise owns all channels).
Normal landing fanfares remain unchanged. Fire/Space or Up skips the show.
Three fixed eight-spark groups launch overlapping colored salvos from deck
height. No new samples, Chip allocation or sprite channels. Saved pixels are
restored in reverse order, including ring mirrors, before rebasing the carrier.
Session initialization clears the effect state. Muted focused WinUAE test
passed bounded particle storage and byte-exact background restoration through
a full show. Exported renderer frame inspected in .tmp/oct18-finale-preview.png;
this is not a full hardware frame-rate or audible mix verification.

### Battleship desperation and restrained storm (2026-10-18)

Below 35% combined health, surviving cannon batteries get three opportunities
per two old firing periods, grouped with a pause; missile turrets use 85 rather
than 100 ticks. A surviving visible bridge can attempt one sector-limited
emergency missile every 150 ticks. The single missile pool, four-cloud/shared
shell budget, damage and destroyed-target state remain unchanged.

The distinct 160-tick foghorn loops the existing 32-byte procedural waveform
at a much lower period, with a gentle envelope/drift; no new Chip sample.
The sky fades over roughly six seconds toward a dark blue-grey gradient.
A subdued four-tick palette flash every 700 ticks has delayed, low-volume
existing ground-impact thunder. No rain overlay or extra cloud BOBs.

Lazy combat can admit up to eight fighters while loading remains unfinished,
with two simultaneous actors still the limit. Once ready, no new admissions;
all enemies/projectiles still leave before handoff. Campaign boss entry starts
96 pixels farther into the empty-ocean lead-in, with earlier horn/warning fire.
Compact wave speed now eases toward the ordinary flight-speed pixel rate,
instead of a fixed two pixels per frame. This does not unify the separate
idle wave artwork with the compact corridor pattern.

Muted four-transition test passed stable buffer equivalence, water checks,
missile visibility and raster deadlines (max-work-vsyncs=01). Focused
HAR_HEADLESS_BATTLESHIP_MOOD_TEST_ONLY passed the 20/30 firing-opportunity
comparison, bridge missile pool guard and palette fade/flash restoration.
The latter is a focused branch, not the full existing Battleship contract.
Foghorn/timing mix has not been auditioned aloud; WinUAE tests stay muted.

### Distinct P2 VTOL and manual carrier approach (2026-10-20)

Human P2 now uses the existing three-grey Wingman sprite palette for all five
VTOL headings on the oil platform and during final carrier approach. The
existing Harrier silhouettes are converted only when the pose changes; movement
only updates sprite position. The attached channel is hidden. No duplicate art
bank, new DMA buffer or Chip allocation is required.

Enhanced carrier hover no longer hands human P2 to the CPU landing waypoints.
P2 uses the same fixed-point acceleration, retained neutral momentum, opposing
input braking and heading transition as P1. Safe touchdown holds position;
Up lifts off again. Hard landing costs 30 health and bounces, tower contact costs
38 with contact grace, and water loses the companion. Completion still waits
for an active P2 to land. CPU Wingman and Classic retain their existing approach.

Muted focused WinUAE checks passed movement, neutral momentum, braking, native
deck contact, settled position, takeoff, hard-landing recoil and five grey sprite
poses. The oil-platform contract passed with its updated single-channel check.
Main ADF rebuilt; floppy-only A500/KS1.3 with 512 KB Chip + 512 KB Slow reported
78,176 free Chip bytes after runtime allocation. Pose preview:
`.tmp/oct20-p2-poses.png`. Logs: `.tmp/oct20-p2-test.log`,
`.tmp/oct20-p2-oil-test.log`, `.tmp/oct20-p2-main-build.log` and
`.tmp/oct20-p2-boot/winuaelog.txt`.

### Enhanced radar feedback and low-release depot contact (2026-10-22)

The R lamp now has a small 20-step detection meter beneath it during ordinary
flight. LOW+FAST appears when speed is at least 8 and clearance is below the
radar threshold. This exposes the existing radar benefit without changing
balance; updates are cached and quantized rather than drawn every frame.

P1 and human P2 bombs check their swept 4x3 footprint against intact ammunition
and fuel depots before checking the ground beneath them. Low releases can no
longer miss a depot merely because the bomb tip is below its bottom edge.
Classic and other object contact rules are unchanged. The ammunition depot
contract covers low releases, swept contact, misses below the target, Classic
isolation and radar feedback states.

### Oil platform repair-drop restoration (2026-10-22)

Power-up background restoration now treats the oil platform and its health
bar as composed overlays, like carriers and battleships. Only overlapping
drops use the existing row compositor; ordinary sky drops retain the fast
scanline path. The oil contract moves and removes an R drop across the rig
and health bar at all eight pixel alignments and checks the whole ring buffer
against its original checksum, including the duplicate placement.

### Moving sea surface at Battleship arrival (2026-10-22)

The compact ocean corridor now scrolls its two retained surface scanlines at
the same eased speed as the wavelets. Previously these scanlines were a still
snapshot. Battleship retains the ordinary sea tiles on those two lines instead
of overwriting them with solid water; its existing independent Copper scroll
moves the surface as well as the lower wavelets, including the opening approach.
No new graphics allocation is needed (40 bytes of temporary CPU stack only).
The boss contract checks the actual surface bytes against the ordinary tiles
and exercises a three-pixel shift and full wrap in the compact display.

### Battleship last stand and quiet finale sky (2026-10-22)

Below 35% health, surviving cannons now have four salvo opportunities per two
normal firing periods (previously three). With at most two visible live guns,
an intact visible gun/bridge/helipad can additionally fire a pair of emergency
shells, 20 ticks apart, every 150 ticks on Easy or 110 otherwise. These use the
existing shell/cloud pool and four-projectile combined limit, unchanged damage,
and burst ahead at alternating heights. Destroyed targets and fuel stores do
not fire; sinking disables this support.

Fireworks clear gull actors without discarding their saved backgrounds and
suppress both new flocks and idle calls throughout the show. Focused contracts
cover emergency fire after bridge loss, pause/budget limits, zero-health
shutdown and gull suppression alongside finale background restoration.

### Ocean-loader starvation fix (2026-10-24)

The 24.10.2026 USS was inside prepareCampaignOceanWorld on sortie 2 after
2296 transit ticks. Both fighters and all eight projectiles were inactive;
eight fighters had spawned, six were shot down, but closed and quiet were
still zero. The destination had not become ready. The loader previously
skipped every unit of work whenever the HUD/water pass reached raster 160.
A consistently late frame could therefore wait forever despite an empty sky.

Late frames now receive a bounded minimum slice: 128 clear bytes or one world
tile row (including its duplicate and overlays), or one boss preparation step.
Earlier frames retain the larger adaptive budget. Enemy clearance and the
35-tick quiet guard remain mandatory. HAR_HEADLESS_TRANSIT_FORCE_LATE_LOAD
forces this path during the complete four-handoff WinUAE regression run.

The first forced-late run also exposed excessive CPU cost in the recent
byte-at-a-time surface ripple rotation. The compact surface now rotates ten
32-bit words per plane/line instead of forty individual bytes, retaining the
same pixels, phase and 40-byte scratch space. The transition performance gate
remains one display field; it was not relaxed to accommodate the slowdown.

A larger raster window was rejected because it produced two-field updates.
The final loader retains the 160/180 raster thresholds and instead skips
already-cleared empty encounter sky and rows below the allocated world bitmap.
Oil loads only its structure rows and sea; Battleship keeps all possible raised
weapon rows within the ship bounds. Full buffer equivalence remains checked
against an ordinary complete render in the live transition test.

Final live regression: PASS campaign-oil-battleship-ocean-transits-buffer-equivalence;
4 handoffs, stable=1, max-work-vsyncs=1. Oil/return/boss/return used
1707/822/1410/885 update fields respectively. The deliberately forced minimum
budget separately completed oil and return before its original 6500-frame
fixture limit; that partial stress run was not reported as a full pass.


### Oil-platform cooperative rescue (25 October 2026)

Human P2 now waits for a red Wingman drop after losing the aircraft instead
of returning on a timer. Red drops recur while a human aircraft is missing,
including after the wave is cleared. Either flying pilot can collect them.
A safely ejected P1 who reaches the helipad mans the existing auto-aim gun;
P1 fire controls its shots while P2 continues flying. A red drop collected by
P2 returns P1 above the rig without restarting the raid or charging another
life. If P2 is also absent, safe P1 recovery restarts both at the carrier via
the existing mission recovery path. Solo/CPU rescue keeps its existing rules.
P2 health and fuel telemetry redraws only on displayed-value changes or HUD
invalidation. The gun reuses P1's sprite memory and existing artwork.

Validation: muted WinUAE oil contract passed, including no timed P2 respawn,
red-drop revival, P1 gun occupancy/fire, raid-state preservation, both-pilot
carrier recovery and unchanged-HUD-write checks. Full cooperative playthrough
on real hardware remains to be tested.
