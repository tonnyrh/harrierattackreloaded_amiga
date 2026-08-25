# Harrier Attack Reloaded — Amiga

Standalone Amiga OCS port targeting a stock PAL Amiga 500 with 512 KiB chip
RAM and 512 KiB expansion RAM. The port preserves the original gameplay rules
while using Amiga-native smooth scrolling, Paula audio and hardware sprites or
pixel BOBs where appropriate.

The repository contains only the Amiga source, build scripts, development
tools and converted build-ready Amiga assets. Original Amstrad CPC assembler,
cartridge/disk builds, extraction tools and audit files are intentionally not
kept here.

## Project status

**Sprint 15.97.1 is the first public-beta candidate.** The complete
mission loop is playable: carrier takeoff, generated sea/terrain/city route,
air and ground combat, powerups, return flight, carrier landing and progression
to the next mission. Solo, CPU Wingman and local Player 2 modes are available,
along with the alternating attract demo.

The port includes the CPC-derived weapon, collision, scoring, difficulty and
level rules; hardware-assisted smooth scrolling; OCS sprites and pixel BOBs;
terrain radar; eject/aircraft lives; persistent high scores; Paula sound and
music; menus, Field Guide and an optional in-game telemetry/debug hub. Classic
mode is the CPC gameplay contract, while Enhanced mode keeps that foundation
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

Outputs are written under `amiga/out`:

- `harrier_amiga.exe` — AmigaDOS executable
- `harrier_amiga.exe.info` — Workbench 1.x tool icon (64 KiB stack)
- `harrier_amiga.adf` — bootable floppy image
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
.\package-amiga.ps1 -Version 0.9.0-beta.1
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
