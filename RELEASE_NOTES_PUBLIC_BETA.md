# Harrier Attack Reloaded Amiga — Public Beta 3 RC1

Version **v0.9.0-beta.3-rc.1** is a public beta release candidate, not a final
release. Target: PAL Amiga 500, 68000/OCS, 512 KiB Chip RAM plus 512 KiB
expansion RAM, Kickstart 1.3. No ROMs or Amiga system software are included.

## Changes since Public Beta 2

- Latest user-edited Enhanced weapon and Missile Tank graphics. Editor sizes
  remain 8x8 for missiles, 4x3 for bombs and 16x8 for tanks.
- Rare Missile Tanks: first after 4–6 tanks per mission, then every 8–12,
  subject to spacing so two never share a screen. A survivor fires as it
  exits left, unless an enemy aircraft/missile is already active. Its missile
  rises, then locks the lower aircraft's height and accelerates forwards.
  Enemy aircraft cannot spawn while this missile remains active.
- Missile Tank added to the Enhanced Field Guide with the current artwork.
- Separate Tempo setting: 80%, 90%, 100%. Clearer Skill 1–5 labels.
- Score bonuses combine starting Skill and Tempo, up to x1.32. Landing now
  awards 2000 base points. Power-up thresholds still use unscaled points.
- Separate Classic/Enhanced score tables with Skill/Tempo metadata. Earlier
  scores remain available in the read-only Legacy archive.
- Enemy planes stay anchored to scrolling scenery, with vertical attack and
  retreat behaviour.

## Downloads and installation

- **ADF:** bootable floppy image for compatible hardware/emulators.
- **HD.zip:** AmigaDOS executable, Workbench icon, loading bitmap and README.
  Keep these together; launch the executable or its Workbench icon.
- **SHA256SUMS.txt:** SHA-256 checksums for the ADF and HD ZIP.

Keep existing `harrier_scores*.dat` files when upgrading: they supply Legacy.
The new mode-specific `harrier_classic2_*.dat` and `harrier_enhanced2_*.dat`
files store current records. Do not overwrite or delete these score files
when copying a new build. Saving requires writable media.

## Testing and beta limits

Focused emulator contracts cover score/disk persistence, weapon palette and
BOB/sprite equivalence, and Missile Tank placement/exclusion/flight. Classic
regression and tempo tests have also passed during development. Release
checks passed for the current weapon art, plus 1200-frame smokes on A500
at 100% Tempo and stock A1200 at 90%. Results are in the rendering work log.

PAL remains the qualified timing target. Dense scenes and additional coloured
BOBs can miss 50 FPS on a stock A500; this is not a locked-50-FPS claim. Graphics
using colours outside the hardware projectile palette retain their colours
through BOB rendering. Real-hardware feedback, especially on pacing, coloured
weapons and the Missile Tank attack, remains valuable. Include machine,
memory, mode, Skill, Tempo and reproduction steps with reports.

Downloads enable `HAR_HARDWARE_PLAYER_ROCKET=1`,
`HAR_HARDWARE_PROJECTILE_CHAIN=1` and `HAR_CRASH_DEBRIS_BOBS=1`.
Default source builds keep these optional flags off.

## Credits and appreciation

**Harrier Attack** was originally created by **Robert White** and published by
**Durell Software**. We remember their work with great respect.

Our sincere thanks go to **Chris Perver** for creating Harrier Attack Reloaded
for the Amstrad Plus, making its source available as an invaluable gameplay
reference, and offering such generous encouragement to this Amiga port. The
goal has been to preserve the rules and character of Chris's Reloaded version
while giving it a natural home on Amiga hardware.

Amiga port by **Tonny Roger Holm**.

Thank you as well to everyone who has tested the game in WinUAE, on real Amiga
hardware and on FPGA systems. Your beta feedback will help make the final
release stronger.

This is an unofficial, non-commercial fan project. No endorsement by Durell
Software or the original rights holders is implied. All names and original
works remain the property of their respective owners.
