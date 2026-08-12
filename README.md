# Harrier Attack Reloaded — Amiga

Standalone Amiga OCS port targeting a stock PAL Amiga 500 with 512 KiB chip
RAM and 512 KiB expansion RAM. The port preserves the original gameplay rules
while using Amiga-native smooth scrolling, Paula audio and hardware sprites or
pixel BOBs where appropriate.

The repository contains only the Amiga source, build scripts, development
tools and converted build-ready Amiga assets. Original Amstrad CPC assembler,
cartridge/disk builds, extraction tools and audit files are intentionally not
kept here.

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
- `harrier_amiga.adf` — bootable floppy image
- debug ELF/map artifacts used by the VS Code integration

The checked-in files under `amiga/assets` are authoritative Amiga build
inputs. The build does not require the original CPC repository.

## Tests and packaging

```powershell
.\run-amiga-classic-contract.ps1
.\run-amiga-parity.ps1
.\package-amiga.ps1
```

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
