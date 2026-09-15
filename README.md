# Harrier Attack Reloaded â€” Amiga

Standalone Amiga OCS port targeting a stock PAL Amiga 500 with 512 KiB chip
RAM and 512 KiB expansion RAM. The port preserves the original gameplay rules
while using Amiga-native smooth scrolling, Paula audio and hardware sprites or
pixel BOBs where appropriate.

The repository contains only the Amiga source, build scripts, development
tools and converted build-ready Amiga assets. Original Amstrad CPC assembler,
cartridge/disk builds, extraction tools and audit files are intentionally not
kept here.

## Project status

**Public Beta 4 (v0.9.0-beta.4) remains a prerelease.** The complete
mission loop is playable: carrier takeoff, generated sea/terrain/city route,
air and ground combat, powerups, return flight, carrier landing and progression
to the next mission. Solo, CPU Wingman and local Player 2 modes are available,
along with the alternating attract demo.

This beta adds Enhanced Missile Silos, terrain-following helicopters, low-speed
landing mode, missile armour damage and two-button emergency eject. F depots
grant 20% fuel, longer routes have a landing fuel reserve, and Fuel/Armour
meters now have distinct colours. It also includes A500 Wingman sprite
publication changes, terrain impacts for tank missiles and bounded helicopter
machine-gun bullets. The editor includes the new encounter and fuel-depot art.
See [encounter behavior and validation](amiga/ENHANCED_ENCOUNTERS.md) for details,
performance measurements and remaining real-hardware checks.

The port includes the CPC-derived weapon, collision, scoring, difficulty and
level rules; hardware-assisted smooth scrolling; OCS sprites and pixel BOBs;
terrain radar; eject/aircraft lives; persistent high scores; Paula sound and
music; menus, Field Guide and an optional in-game telemetry/debug hub. Classic
mode follows the CPC gameplay rules with the shared Amiga enemy-flight motion
adjustment, while Enhanced mode keeps that foundation
and applies explicitly documented Amiga presentation and balancing additions.

The primary release target is a stock PAL Amiga 500 with Kickstart 1.3,
68000, OCS, 512 KiB chip RAM and 512 KiB expansion RAM. Builds are also
regularly exercised in WinUAE and on faster compatible Amigas. Automated
Classic-contract and headless full-route tests cover core gameplay and
performance regressions.

The remaining work is beta QA rather than major feature development:
extended real-hardware playtesting, edge-case and two-player regression,
writable-media high-score verification, final packaging, and documentation
and licence review. The CPC repository remains a read-only external gameplay
reference and is not modified by this project.

Public-beta downloads are published under
[GitHub Releases](https://github.com/tonnyrh/harrierattackreloaded_amiga/releases).
Use the ADF on real hardware, MiniMig or an emulator, or use the HD ZIP for a
Workbench/hard-disk installation. Kickstart ROMs are never included.

## Difficulty

The menu runs from **Skill 1: Easiest** to **Skill 5: Hardest**, with Easy,
Normal and Hard in between. This selects the starting difficulty; each
completed mission raises the effective difficulty by one, capped at five.

| First-mission rule | Skill 1 | Skill 2 | Skill 3 | Skill 4 | Skill 5 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Maximum hill rise above row-14 baseline (pixels) | 24 | 32 | 40 | 48 | 56 |
| Added procedural land columns | 256 | 512 | 768 | 1024 | 1280 |
| Classic flak damage threshold | 23 | 21 | 19 | 17 | 15 |
| Enhanced flak damage threshold | 23 | 21 | 19 | 16 | 14 |
| Enhanced radar masking clearance (pixels) | 18 | 16 | 14 | 12 | 10 |
| Starting bombs / rockets | 60 / 30 | 75 / 30 | 90 / 45 | 105 / 45 | 120 / 60 |

Higher hills, longer land sections and less flak tolerance increase pressure.
Enhanced also reduces safe radar clearance. Skill and Tempo are independent.
Ammunition increases to support the longer route. Classic's shared enemy/power-up admission altitude moves upward with
skill, so not every individual encounter becomes harder at the same altitude.
The ordering is an overall difficulty intent, not a guarantee for every seed.

The focused emulator test `HAR_HEADLESS_DIFFICULTY_TEST_ONLY=1` checks all
five levels in both modes, mission progression, terrain height limits, flak
thresholds, Enhanced radar thresholds, ammunition and menu text length.

## Tempo

The separate **Tempo** menu row offers **80%, 90% and 100%** (default).
Left/right changes the setting independently of Skill. Both game modes use
it. At reduced tempo, gameplay advances in fixed steps while the renderer
interpolates scrolling and moving objects for the PAL display. Physics,
collisions, ammunition, cooldowns and fuel continue to use logical steps;
menus and audio retain their real-time cadence. Short input presses are
latched until the next gameplay step.

The main loop finishes its work and synchronizes to the next PAL raster
boundary; it does not append a constant delay. At 80/90%, elapsed fields are
sampled from the hardware clock, and accumulated work is bounded to avoid
an endless catch-up loop under overload. If the machine misses deadlines,
actual gameplay can fall below the selected rate. The target remains 50
display updates per second; the heavy A500 stress route still drops frames.
At 100%, the existing simulation cadence is retained. Settings currently
last for the running program; high scores are not separated by tempo.

## Scores and leaderboards

Classic and Enhanced have independent leaderboards. Changing Mode selects
its current list; **Scores: Current / Legacy** switches to the read-only
archive of pre-multiplier records. The HUD's high score always belongs to
the running mode. The columns are name, **SK** (starting skill), **TMP**
(tempo percent), **LV** (effective difficulty reached), hits and score.
Unknown settings in old records are shown as dashes.

Each hit/bonus uses `base points × skill factor × tempo factor`:

| Setting | Factor |
| --- | ---: |
| Skill 1 / 2 / 3 / 4 / 5 | 1.00 / 1.05 / 1.10 / 1.15 / 1.20 |
| Tempo 80 / 90 / 100% | 1.00 / 1.05 / 1.10 |

The menu displays the exact combined factor (up to **x1.3200**).
Starting skill and tempo are fixed for the run; later mission difficulty
increases do not change its multiplier. Throttle does not affect points.
Fractional points carry between awards, rescues and missions; the displayed
score saturates at 999999. Extra-aircraft eligibility still uses unscaled
mission points so the scoring bonus does not accelerate power-ups.
Landing now awards **2000 base points**, matching the CPC's 200 internal
units displayed with a trailing zero (the previous Amiga award was 200).

New records use separate version-2 A/B saves:
`harrier_classic2_a.dat`, `harrier_classic2_b.dat`,
`harrier_enhanced2_a.dat`, `harrier_enhanced2_b.dat`.
Each mode validates its own identity, metadata, checksum and generation,
and falls back to its previous valid slot after an interrupted write.
The former `harrier_scores_a.dat`, `harrier_scores_b.dat` and raw
`harrier_scores.dat` are read only for Legacy; they are never overwritten
or mixed into the new lists. Keep these files beside the executable when
moving an installation. As before, saves require writable media.

Focused verification:
`./run-amiga-classic-contract.ps1 -ExtraCcFlags '-DHAR_HEADLESS_SCORE_TEST_ONLY=1'`.
It uses a newly created scratch directory to test all 15 multiplier
combinations, fractional accumulation, score cap, mode isolation, metadata,
legacy import, actual disk round trips and corrupt-slot recovery.

## Development setup

From PowerShell:

```powershell
.\setup-dev-amiga.ps1
```

The script installs/configures the Bartman/Abyss VS Code integration. A
Kickstart 1.3 ROM must be available under `.tools/Amiga/Kick`.

Open the repository in VS Code and run the **Amiga 500 debug (KS1.3, 1MB)**
configuration with F5.

## Build

```powershell
.\amiga-build.ps1
```

To reproduce the experimental sprite-multiplexing and crash-debris BOB
configuration used in recent A500 playtests, build in a separate PowerShell
session with:

```powershell
$env:EXTRA_CCFLAGS = '-DHAR_HARDWARE_PLAYER_ROCKET=1 -DHAR_HARDWARE_PROJECTILE_CHAIN=1 -DHAR_CRASH_DEBRIS_BOBS=1'
.\amiga-build.ps1
```

These three options remain disabled in default builds while beta testing
continues. The bomb and power-up changes are included in both configurations.

Outputs are written under `amiga/out`:

- `harrier_amiga.exe` â€” AmigaDOS executable
- `harrier_amiga.exe.info` â€” Workbench 1.x tool icon (64 KiB stack)
- `harrier_amiga.adf` â€” bootable floppy image
- debug ELF/map artifacts used by the VS Code integration

When `amiga/out` is mounted as an Amiga hard disk, open its drawer in
Workbench and double-click the Harrier icon to start the game. **Exit to DOS**
returns cleanly to Workbench. The build also places the icon on the ADF.

The checked-in files under `amiga/assets` are authoritative Amiga build
inputs. The build does not require the original CPC repository.

## Tests and packaging

```powershell
.\run-amiga-classic-contract.ps1
.\run-amiga-parity.ps1
.\package-amiga.ps1 -Version 0.9.0-beta.4
```

The packaging command creates versioned ADF and HD ZIP release assets plus a
SHA-256 checksum file under `dist/release`. Debug symbols are excluded from the
player package; add `-IncludeDebug` to generate a separate symbols archive.

See [amiga/HEADLESS_TESTING.md](amiga/HEADLESS_TESTING.md) for the headless
WinUAE regression setup and [AMIGA_PORT_PLAN.md](AMIGA_PORT_PLAN.md) for the
implementation history.

## Asset maintenance

Sound masters live in `amiga/assets/sfx-sourcefiles` and are converted by
`prepare-amiga-sfx.ps1`. Music and the remaining runtime graphics are already
stored in their Amiga-ready formats. Do not add CPC source, build outputs or
extraction dumps to this repository. Conversion/viewing tools may remain, but
they must use read-only access to
`https://github.com/chrisperver/harrierattackreloaded` in a separate checkout
and write intermediate data only under ignored `.tmp`:

```powershell
.\extract-cpc-assets.ps1 -CpcSourceRoot C:\path\to\harrierattackreloaded -Clean
.\promote-cpc-assets.ps1
.\view-cpc-graphics.ps1 -CpcSourceRoot C:\path\to\harrierattackreloaded
```

Only the final Amiga-format header, bitplane, palette or audio asset is imported
into `amiga/assets`.

`amiga/assets/loading_screen.png` is the indexed 320x200, 32-colour OCS master
for the loading page. `amiga-build.ps1` validates it and regenerates the
five-plane `loading_screen.bpl`, its 64-byte palette and the BMP preview through
`tools/pack-loading-screen.py`. The runtime bitmap remains exactly 40,000 bytes.

Enhanced ground, city and weapon graphics have editable, indexed PNG masters. Open the
palette-locked project editor with:

```powershell
.\edit-amiga-graphics.ps1
```

Left-drag paints, right-drag makes pixels transparent, and Save validates the
fixed OCS palette and repacks the runtime bitplane banks. The ordinary Amiga
build repeats this validation. See
[`amiga/assets/enhanced/README.md`](amiga/assets/enhanced/README.md) for exact
dimensions, external-editor use and the protected reset workflow.


Enhanced also includes a rare **Missile Tank**: first after 4–6 tanks in
each mission, then approximately every 8–12 tank encounters, with at least
42 columns between them so two cannot share the
screen. Its separate 16x8 master starts as a copy of Tank. A surviving truck
fires once when its full footprint exits the left edge, provided no enemy
plane or missile is active. Blocked shots are skipped, not queued. Enemy
planes cannot spawn while the truck missile is active.

The missile rises slowly diagonally from the left, targeting the lower of
Harrier and an active Wingman. On reaching that height it locks the height
and accelerates to 7 world pixels per logical step, above the Harrier's
maximum camera-plus-horizontal motion. Normal collision and interception
rules still apply. Classic does not use this enemy variant.
