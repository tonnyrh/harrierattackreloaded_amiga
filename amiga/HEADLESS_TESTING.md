# Headless WinUAE Testing (Self-Verifying Scroll/Perf Changes)

This documents how Claude (or anyone else) can run the Amiga build headlessly in
WinUAE, without a human watching the screen or pressing keys, and get objective
frame-timing numbers back as a CSV file. It exists because verifying a scrolling
fix normally requires a human to eyeball WinUAE and judge "does it still hitch" -
this harness replaces that judgement call with numbers.

## Why not just use the VS Code "Amiga 500 debug" launch config?

The project's `.vscode/launch.json` config starts WinUAE via the
`bartmanabyss.amiga-debug` extension, which drives it through GDB
(`winuae-gdb.exe`, `target remote localhost:2345`) so VS Code can set
breakpoints and show the debug console. Its generated `default.uae` sets:

```
debugging_features=gdbserver
debugging_trigger=:harrier_amiga.exe
```

This pauses the emulated CPU the moment `harrier_amiga.exe` is loaded, waiting
for a GDB client to attach and `continue`. That's fine when VS Code is driving
it, but useless for a non-interactive test run - nothing ever attaches, so the
game never actually starts.

For headless perf runs we don't need GDB at all: the game writes its own
result file (see below), so we just need WinUAE to boot and run freely.

## The headless `.uae` config

Copy the extension's `bin/win32/default.uae` and delete the two
`debugging_*` lines. Keep everything else (kickstart path, `filesystem=` /
`filesystem2=` mounts, chip/bogo mem sizes) identical, since the mounts are
what let the game write its result file straight into the host filesystem:

- `filesystem=rw,dh0:...\bartmanabyss.amiga-debug-1.8.2\bin\dh0` - boot volume,
  contains `s/startup-sequence` which does `cd dh1:` followed by
  `:harrier_amiga.exe`.
- `filesystem2=rw,dh1:dh1:<repo>/amiga/out,-128` - this is the important one:
  `dh1:` maps directly to the real `amiga/out/` folder on Windows. Anything the
  game writes to `DH1:` lands there and can be read straight from the host.

`use_gui=no` and `win32.start_not_captured=yes` keep it from grabbing
keyboard/mouse focus or popping the settings GUI, which matters when it's
launched from a script rather than by hand.

This exact config is checked in at `amiga/harrier_headless.uae` (paths inside
it are absolute to this machine/user - regenerate from `default.uae` if the
extension version, repo location, or Windows username ever changes). Run it
directly (no VS Code needed):

```powershell
$ext = "$env:USERPROFILE\.vscode\extensions\bartmanabyss.amiga-debug-1.8.2"
$exe = Join-Path $ext "bin\win32\winuae-gdb.exe"
Start-Process -FilePath $exe -ArgumentList @("-f", "C:\vscode\_EXT\harrierattackreloaded\amiga\harrier_headless.uae") -PassThru
```

It still opens a visible emulator window (WinUAE has no true off-screen mode),
but nothing needs to interact with it - just let it run and poll for the
output file, then kill the process (`Stop-Process`) once done.

## Build flags (`amiga/main.c`)

The headless defines near the top of `main.c` are externally overridable. All
test behavior defaults to off in normal/release builds; `Makefile` accepts the
test-only values through `EXTRA_CCFLAGS`, so source does not have to be edited
and accidentally shipped with autoplay enabled:

| Flag | Purpose |
|---|---|
| `HAR_DEBUG_PERF_LOG` | Turns on the perf sampler (`perfLogOpen`/`perfLogFrame`) and the CSV writer described below. |
| `HAR_HEADLESS_AUTOPLAY` | Injects synthetic input so the game plays itself (see below), and auto-quits after a fixed frame budget. |
| `HAR_HEADLESS_SKILL_LEVEL` | Selects the skill used by the autoplay session. |
| `HAR_HEADLESS_MAX_FRAMES` | Safety timeout if the route cannot reach its terminal condition. |
| `HAR_HEADLESS_CRUISE_SPEED` | Selects a reproducible scroll-speed stress case. |
| `HAR_HEADLESS_WINGMAN_CONTROL` | Selects Off, CPU or Player 2 for A/B workload measurements. |
| `HAR_HEADLESS_GAME_MODE` | Selects Classic (`0`) or Enhanced (`1`, default) so profile-specific admission rules can be measured independently. |
| `HAR_HEADLESS_WEAPON_STRESS` | With Player 2 selected, keeps both aircraft supplied/alive and alternates pressed/released rocket and bomb input for both players throughout the route. |
| `HAR_VALIDATION_SESSION_SEED` | Pins the modeled CPC session RNG for true A/B runs. It defaults to `0` in F5/release builds, which keeps menu-time-derived random worlds. |
| `PERF_LOG_INTERVAL_FRAMES` | Overrides the normal 500-frame CSV window. Use 100 frames for position-aligned city profiling. |
| `HAR_DEBUG_HUD_GUARD` | Expensive per-frame HUD corruption scan. Keep this `0` for performance measurements; enable it only when diagnosing HUD memory corruption. |
| `HAR_USE_RING_WORLD_SCROLL` | The actual scrolling strategy being tested - not test-only, but this is the flag these tests exist to evaluate. |

## Sprint 15.89 validation notes

Headless autoplay is intentionally invulnerable, but collision detection and
telemetry remain active. Fatal terrain cells and flak contacts are counted
without mutating armour or pinning the synthetic pilot inside the same map
cell. This matters because a no-op crash handler previously made a renderer
test look like a game restart after the route driver entered a fatal cell.

The fixed-seed cycle-exact release matrix for Sprint 15.89 completed Enhanced
skills 1 and 5 with CPU Wingman and reached the final carrier in both runs.
Stable samples held 50 FPS, zero hitches and `maxVblDelta=1`. The Classic
contract also passed unchanged, confirming that the Enhanced-only
100/103/105/108/110 percent pressure curve does not alter CPC Classic rules.

Use the checked-in runner instead of editing these flags by hand:

```powershell
.\run-amiga-parity.ps1 -Skills 1,3,5 -CruiseSpeed 15 -WingmanControl 1 -SessionSeed 12040
```

For the reproducible two-player weapon stress profile:

```powershell
.\run-amiga-parity.ps1 -Skills 1 -CruiseSpeed 15 -WingmanControl 2 `
  -WeaponStress -ExtraCcFlags '-DPERF_LOG_INTERVAL_FRAMES=100' `
  -ResultTag weapon_stress
```

`p1Rkt`, `p1Bmb`, `p2Rkt` and `p2Bmb` in `perf_log.csv` prove that
all four launch paths actually fired inside each measurement window. Compare
rows by `scroll`, not elapsed time; the current town occupies world columns
411..610 (approximately scroll 3288..4880).

## Classic gameplay contract

Sprint 15.79.0 adds a small policy and fuel regression test which exits before
normal game allocation, high-score access and autoplay. Run it with:

```powershell
.\run-amiga-classic-contract.ps1
```

The runner builds with `HAR_HEADLESS_CLASSIC_CONTRACT_TEST=1`, boots the same
cycle-exact A500 + 512K configuration, waits for
`amiga/out/classic_contract.txt`, fails on any non-`PASS` result, terminates
WinUAE, and always restores the ordinary F5/release executable and ADF.

The contract currently covers Classic/Enhanced starting aircraft, radar,
failure descent, safe respawn, collision/eject policy, skill 1/5 flak
thresholds, and the exact 9558-PAL-frame CPC fuel lifetime. Sprint 15.80 adds
CPC's `8 + playerspeed/2` horizontal character mapping and the deliberately
shared Amiga pixel-smooth 96..186 anchor plus 1..4 pixel/frame scroll bands.
This prevents both mode leakage and a later cleanup accidentally replacing
the approved smooth-control behavior with CPC's CPU-delay implementation.

Sprint 15.81 adds the shared bomb trajectory contract. At scroll speed four,
the 2/3-pixel DDA completes CPC's four logical downward momentum rows in 13
frames while screen X remains fixed, then completes the first ordinary
descent row in three frames while world X remains fixed and screen X follows
the scenery left. These numbers test deterministic interpolation; object and
artwork contact still needs the listed manual play check.

The runner cleans and builds one test executable per skill, starts only the
cycle-exact headless WinUAE process it owns, waits for the terminal CSV,
archives results under `.tmp/amiga-parity-results`, and finally restores a
normal F5/release build without test flags.

The runner defaults to seed `12040` (`0x2f08`, the CPC reference state) and
includes it in every archived filename. Keep `-SessionSeed` identical when
comparing builds or Wingman modes. Earlier runs seeded from the menu's elapsed
frame count and could compare different terrain, cloud, flak and target
sequences while appearing to be an A/B performance test.

## Sprint 15.86 release-candidate baseline

The fixed-seed regression baseline uses seed `12040`, speed 15 and the
cycle-exact stock-A500 configuration. It consists of:

- Enhanced CPU Wingman, skills 1 and 5.
- Enhanced Player 2 with `-WeaponStress`, skill 1 and 100-frame samples.
- Classic CPU Wingman, skill 1 and 100-frame samples.
- The standalone Classic gameplay contract.

All full-route profiles must reach `reachedFinalCarrier=1`. Ignore only the
first bootstrap/startup performance interval; subsequent intervals must hold
50 FPS, zero hitches, maximum VBL delta 1 and zero HUD guard/register hits.
The Player-2 stress profile must record non-zero `p1Rkt`, `p1Bmb`, `p2Rkt`
and `p2Bmb` totals. Results are archived under
`.tmp/amiga-parity-results` with the `sprint1586_*` result tags.

## The critical gotcha: DOS file I/O deadlocks mid-game

The first version of this harness wrote each CSV row straight to disk (`Open`
once at startup, `Write` + `Flush` every 10 seconds) and it reliably froze the
emulated machine with:

```
Software error - task held
Finish ALL disk activity
Select CANCEL to reset/debug
```

Root cause: `main()` calls `TakeSystem()` early on, which does `Forbid()` and
`Disable()` and only undoes them in `FreeSystem()` at the very end of the
program - i.e. for the *entire* game session, AmigaOS task switching and CPU
interrupts are off. `dos.library` calls like `Open()`/`Write()` are not simple
register pokes - they send a message to the filesystem handler's task and wait
for a reply, which requires the scheduler to run that other task. With
`Forbid()`/`Disable()` in effect, that handler task can never be scheduled, so
the call hangs forever. AmigaOS's own watchdog eventually surfaces that hang as
the "task held" alert. `KPrintF` never had this problem because it goes
through `RawPutChar` (`exec.library`), a synchronous hardware/serial write with
no task hand-off involved.

**Fix**: never call `Open`/`Write`/`Close`/`Flush` from inside the game loop.
Instead:

1. `perfLogAppend()` appends each CSV line to a static in-RAM buffer
   (`perfLogBuffer`, `PERF_LOG_BUFFER_BYTES`) - no OS calls, safe under
   `Forbid()`/`Disable()`.
2. `HAR_HEADLESS_AUTOPLAY` breaks out of the main `while (1)` loop after the
   final carrier is reached (or the safety frame budget expires), so the program reaches its normal shutdown
   path (`FreeSystem()` -> `Enable()`/`Permit()` restore multitasking).
3. `perfLogFlushToDisk()` runs *after* `FreeSystem()` and does the one-shot
   `Open(MODE_NEWFILE)` + `Write()` + `Close()` of the whole buffer. By this
   point the OS is fully back, so the filesystem handler task can run and the
   call completes normally.

If you ever add more headless instrumentation, keep this rule: **no blocking
DOS calls between `TakeSystem()` and `FreeSystem()`.** RAM-buffer everything
and flush once at the end.

## Headless autoplay

Real input requires a human wiggling a joystick, which defeats the point of a
headless test. `HAR_HEADLESS_AUTOPLAY` injects synthetic `InputState` bits
right after the real `ReadInput()` call each frame:

1. Once `frameCounter > 100` (~2s), force a one-shot `input.select` press to
   start the game from the menu (menu already defaults to `MENU_ITEM_START`).
2. Once `game.takeoffState == TAKEOFF_STATE_READY`, force a one-shot
   `input.up` press to go airborne (takeoff roll-in is automatic, no input
   needed for that part).
3. From then on, hold `input.up` every frame. This isn't a takeoff detail -
   forward scroll speed is automatic regardless of input, but height is not,
   and without deliberately climbing the plane reliably flies into terrain and
   dies almost immediately. Holding "up" keeps it climbed and clear of most
   terrain for long enough to get a useful sample. (Earlier test runs without
   this repeatedly hit "Game over" within the first ~60-70 seconds - visible in
   the CSV as `scrollX` going flat and `armour` cycling 100/0 as the plane
   respawned and died over and over.)
4. It accelerates to `HAR_HEADLESS_CRUISE_SPEED`, traverses the complete route,
   and exits 50 frames after the final landing/hover state is reached. The
   frame limit is only a deadlock/stall guard, not the normal completion rule.

This is a blunt instrument (no obstacle/enemy avoidance beyond "fly high") -
good enough to get several tens of seconds of mostly-continuous forward
scrolling for a perf comparison, not a real playtest.

### Enemy-plane movement profile

`run-amiga-parity.ps1 -EnemyPlaneExercise` changes only the compiled headless
input profile.  The reference route still climbs to maximum height until an
enemy plane appears; during each encounter it then alternates the Harrier
between two fixed flight levels every 24 frames.  Use a low `-CruiseSpeed`
(normally 1) so the attacker receives several CPC eight-pixel decisions before
it crosses the player.  This supplements rather than replaces the ceiling
route:

- the ceiling route measures spawn, horizontal approach, missile release,
  retreat and full-map performance;
- the exercise route measures vertical pursuit, two-cell obstruction and the
  lag between logical 8x8 positions and pixel-smooth display positions.

`-EnemyPlaneRates 1,2,3` builds separate diagnostic variants.  Both the CPC
logical cadence and the visual interpolation rate are scaled, but every
rendered frame remains pixel-smooth (no 8-pixel jumps).  The release/F5 build
defaults to 1 and does not contain the synthetic input profile.

The autoplay build is invulnerable. Collision detection and the rest of the
gameplay workload still run, but a terrain contact cannot turn the remaining
samples into a stationary game-over screen. This behavior is compiled out of
normal builds.

## Output files

The Amiga writes `perf_log.csv`, `land_log.csv`, `parity_log.csv` and the
diagnostic `enemy_plane_log.csv` once at
the end via the `DH1:` mount. The runner moves named copies to
`.tmp/amiga-parity-results` using skill, speed and Wingman mode in each name.
`parity_log.csv` is the single-row completion oracle. Its leading fields now
separate selected `skill`, effective CPC `difficulty`, `mission` and generated
`landLength`; the remaining fields cover terrain min/max and transition counts,
target/enemy/flak/Wingman events, pier events, final scroll, landing state and
whether the final carrier was reached.

`enemy_plane_log.csv` records spawn, logical step, fire, blocked step and
despawn events, including visual/logical/target coordinates, tile distance,
visual lag and real frames since the previous logical decision.  Its trace has
application-session lifetime: losing a life may reset gameplay state but must
not discard earlier encounters from the same headless run.

Town-generator parity is recorded as `townBlocks`, `townBuildingCols`,
`townFlatCols`, `townLength`, `townOverflowCols` and `townClippedCols`. CPC
state 6 must contribute exactly one flat separator before every selected
state-7 building. State 7 completes its last block after the 200-column timer,
so `townLength` may be 200..204 and the complete post-town route moves by
`townOverflowCols`. `townClippedCols` is retained for result-file compatibility
and must always be zero.

Sprint 15.71.0 additionally records `townRStart`, `townREnd` and
`townRChecksum`. For a fixed session seed all three must remain stable. Across
varied seeds the checksum should vary, while `townBuildingCols + townFlatCols
== townLength` and `townOverflowCols == max(townLength - 200, 0)` remain true.

`perf_log.csv` has one header row plus one row per completed 10-second interval
(`PERF_LOG_INTERVAL_FRAMES = 500` frames @ 50 Hz PAL):

```
frame,seconds,loops,minFps,maxFps,avgFps,hitches,maxVblDelta,scroll,speed,origin,job,stage,tileX,tileCols,objCols,pages,fuel,armour,rockets,bombs,p1Rkt,p1Bmb,p2Rkt,p2Bmb,...
```

The two fields that matter most for scrolling smoothness:

- **`hitches`** - count of frames in that 10s window where more than 1 VBlank
  passed between two consecutive main-loop iterations (i.e. a real frame was
  dropped). Measured via `frameCounter` deltas, independent of game logic
  state (crash/respawn pauses don't inflate this - it's wall-clock frame
  timing, not gameplay pacing).
- **`maxVblDelta`** - the single worst VBlank gap seen in that window. At 50 Hz,
  a value of e.g. 208 means one single freeze of ~4.2 real seconds - this is
  the number that corresponds to a human saying "it hitches."

`scroll` (world scrollX), `origin` (which world-buffer page is active) and
`job`/`stage`/`tileX` (background world-render job progress) let you correlate
a hitch with "was this at a page turnover."

`wingWorldProbes` and `wingWorldHits` audit Sprint 15.70.3's CPC Wingman world
collision. A live Wingman normally contributes two probes per gameplay frame;
`wingWorldHits` increments only when solid geometry destroys it. These counters
reset for each 10-second performance window.

## Sprint 15.70.4 Classic cadence result

The cycle-exact A500 + 512 KiB weapon-stress route was run with Classic,
skill 1, Player 2, cruise 15 and seed 12040. It reached the final carrier.
After the emulator/bootstrap sample, both moving gameplay windows reported
50 FPS min/max/average, zero hitches and `maxVblDelta=1`. P1/P2 rocket and
bomb launch counters were all non-zero. The result files are tagged
`sprint_15_70_4_classic` under `.tmp/amiga-parity-results`.

## Sprint 15.70.5 shared-cadence result

The matching cycle-exact Enhanced weapon-stress route used skill 1, Player 2,
cruise 15 and seed 12040. It reached the final carrier at `scrollX=5352`.
After bootstrap, both moving windows held 50 FPS minimum/maximum/average,
zero hitches and `maxVblDelta=1`; all four P1/P2 weapon counters were non-zero.
The result files are tagged `sprint_15_70_5_enhanced`.

## Known limitations

- Single continuous flight per run, not a loop - once it hits the frame budget
  or truly dies (out of lives, `scrollX` stops advancing, `armour` stays 0),
  remaining rows are dead time, not signal. Sanity-check the CSV for a flat
  `scroll` column before trusting later rows.
- `cycle_exact=true` in the `.uae` config matters - without it WinUAE's timing
  doesn't correspond to real A500 hardware and hitch numbers aren't meaningful.
- This harness only measures frame-timing/backbuffer-readiness. It says
  nothing about visual correctness (e.g. a wrap-seam glitch in a ring-buffer
  scroller) - that still needs a human or a screenshot diff.

## Sprint 15.48 complete-route parity

Cycle-exact A500 plus 512 KiB runs at full cruise reached the final carrier at
`scrollX=5160` for skills 1, 3 and 5. All runs contained 48 procedural targets,
11 terrain-state events and one pier event. Measured terrain rows were 11..14,
9..14 and 7..14 respectively, confirming the sourced difficulty-dependent
descent floor while retaining the complete city and final approach.

An A/B run at speed 1 isolated CPU Wingman as the remaining avoidable workload:
Wingman Off averaged 45 FPS, whereas the original CPU path averaged 26.8 FPS
and recorded 1717 hitch frames. Two behavior-neutral changes were applied:

- formation terrain safety is cached until its tile-column/player-row inputs
  change, and already-computed safe answers are no longer queried again;
- immutable CPC Wingman pixels are packed into hardware-sprite words once;
  movement now updates only the two sprite control words.

The identical CPU-Wingman route now averages 37.2 FPS with 694 hitch frames.
It reaches the same final scroll and retains the skill-3 terrain fingerprint
`min=9,max=14,flat=153,climb=47,descend=47,targets=48`. This is deliberately a
low-speed worst case; no Copper, ring-buffer, object decision or collision
rule changed, and the cache adds only four small fields to `WingmanState`.

Sprint 15.72.0 keeps this CPU-Wingman profile as the regression route for the
complete CPC 0..8 formation direction model. Run with Wingman control `1`, a
fixed seed and cycle-exact timing; it must reach the final carrier without a
formation/world collision, preserve moving-window performance, and never
stall in the bounded eight-neighbor obstruction fallback.

Fixed-seed result for skill 1, speed 15, seed 12040: final carrier reached at
scroll 5160; both steady moving intervals were 50 FPS with maximum VBL delta
1, and 2000 Wingman world probes produced zero solid contacts.

For an explicit nine-direction exercise, add
`-WingmanFormationExercise` together with `-WingmanControl 1`. The autoplay
alternates between two safe heights while accelerating and parity CSV adds
`wingFormationStops`, `wingFormationCardinal`, `wingFormationDiagonal` and
`wingFormationEvasive`. A valid direction audit must reach the final carrier,
record both cardinal and diagonal movement and keep `wingWorldHits` at zero.

Sprint 15.76.0 adds `highScoreLevel` to `parity_log.csv`. A deterministic,
disk-safe metadata check can be run through `run-amiga-parity.ps1` with:

```powershell
.\run-amiga-parity.ps1 -Skills 5 -WingmanControl 0 -ResultTag highscore_level `
  -ExtraCcFlags "-DHAR_HEADLESS_HIGHSCORE_TEST=1 -DHAR_HIGHSCORE_DISK_IO=0"
```

The run exits after committing the synthetic 1234-point score in memory;
`highScoreLevel` must be `5`. Disabling disk I/O ensures the exercise cannot
replace a real saved table.

Sprint 15.77.0 adds interactive CPC-style name entry, but this headless path
deliberately keeps the automatic `PLAYER` name. Automation therefore remains
deterministic and never waits for keyboard input.

Sprint 15.82.0 extends `run-amiga-classic-contract.ps1` with deterministic
Maverick checks: the CPC eight-column launch distance, exact one-pixel axis
steering, Amiga final-step clamping and retained direction after lock loss.
The test does not replace a live check that the visible BOB contacts each
ground-target silhouette at the expected position.

Sprint 15.83.0 also locks CPC player-object outcomes and the carrier's stepped
two-cell/four-cell tower mask. Physical rear/front deck contact, parked
Wingman exclusion and the visible tower edge remain manual landing checks;
the headless contract deliberately does not allocate the world renderer.

Sprint 15.84.0 keeps the contracted bomb trajectory/cadence but makes gameplay
contact follow the interpolated 4x3 mini-BOB every frame. This closes the
previous gap where Player 1 collision was sampled only on eight-pixel logical
row boundaries; visual target-edge contact still requires a manual play test.

Sprint 15.84.1 changes render ordering only: projectile footprints are retired
before persistent impact cells are redrawn. Verify manually that an enemy-ship
hit leaves CPC smoke instead of a blank stern/hull section.

## Sprint 15.41 baseline and target

Cycle-exact A500 run on 2026-08-01, excluding the startup interval and the
stationary final-carrier rows:

- mean interval FPS: 45.8;
- worst moving 10-second interval: 38 FPS;
- worst moving-frame gap: 3 VBlanks (about 60 ms PAL);
- heaviest region: approximately `scrollX=3896..4702`;
- final-carrier hold begins at approximately `scrollX=5192` and must be
  excluded from scrolling comparisons.

The optimisation target is at least 48 average FPS in every moving interval,
with `maxVblDelta <= 2` and materially fewer hitch frames. Use identical
cycle-exact configuration and autoplay duration for before/after comparisons.

Do not enable `HAR_DEBUG_HUD_GUARD` for these comparisons. Its old 960-byte
memory comparison on every 68000 frame reduced the measured average to about
26 FPS, making the telemetry itself dominate the result.

### Sprint 15.41.2 result

The dense town renderer previously resolved every tile's runtime-flak cell by
linearly scanning as many as 64 active entries. A 128-column tagged lookup was
added alongside the unchanged, bounded gameplay list. Identical cycle-exact
autoplay produced:

- mean moving FPS: 48.2 (was 45.8);
- worst moving interval: 45 FPS (was 38);
- moving hitch frames: 63 (was 310);
- worst moving gap: 2 VBlanks (was 3).

The remaining hotspot is the interval around `scrollX=3242..4146`, which
enters the dense town. It should be split by subsystem in the next profiling
pass; the renderer lookup itself is no longer allowed to scale with the total
number of retained flak cells.
