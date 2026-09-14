# Harrier Attack Reloaded Amiga - Public Beta 4

**v0.9.0-beta.4 is a public prerelease.** Target: PAL Amiga 500, 68000/OCS,
512 KiB Chip RAM plus 512 KiB expansion RAM. No ROMs or system software are included.

## Changes since Public Beta 3 RC1

- Enhanced Missile Silos fire accelerating vertical missiles independently of
  enemy aircraft. Editable closed/open silo graphics.
- Terrain-following helicopters appear from mission 2, remain ahead for a limited
  time and stop before cities. Two missile hits bring them down. Rotor audio,
  sparse damage smoke, right-facing runtime art and at most two small machine-gun
  bullets at once; bullets disappear on terrain contact.
- Very low throttle activates landing mode (L), landing gear, a deeper engine
  tone and triple fuel consumption. Enter at throttle 2 or below; exit at 4.
- Ordinary missiles remove one third of Harrier armour; Missile Tank shots remove
  one half. Wingman dies from one missile. Hold both fire buttons with the E lamp
  lit to eject; the existing E key remains available.
- Rare, editable 8x8 F depots grant 20% fuel when destroyed by the player or
  Wingman. Fuel is turquoise and Armour yellow. Effective difficulty 3/4/5 gains
  10/20/30% fuel endurance for longer routes. Classic fuel timing is unchanged.
- Wingman sprite data is staged and published during vertical blank, addressing
  active-display writes implicated in the reported A500 corruption. Physical
  confirmation of the original symptom remains outstanding.
- Missile Tank missiles climb faster and explode on terrain contact. Wingman
  formation convergence and camera/Copper update timing were also refined.

Separate Classic/Enhanced scores, independent 80/90/100% Tempo, Skill labels,
Missile Tanks and editable weapon/city graphics from previous betas remain included.

## Downloads and installation

- **ADF:** bootable floppy image.
- **HD.zip:** executable, Workbench icon, loading bitmap, README and version file.
  Keep the files together and launch the executable or its icon.
- **SHA256SUMS.txt:** checksums for both packages.

Preserve `harrier_scores*.dat`, `harrier_classic2_*.dat` and
`harrier_enhanced2_*.dat` when upgrading. Saving requires writable media.

## Validation and beta limits

- Focused emulator contracts passed for encounters, sprite staging, bullet
  restoration, missile damage/eject and fuel supply.
- Fuel tests exercised 80 generated routes across all five effective difficulties.
  Whole-route cruise at throttle 5 or higher, 10 seconds departure allowance and
  30 seconds triple-consumption landing left at least 10% fuel without pickups.
  Extended hovering or slow flight can still exhaust fuel.
- A500/OCS/Kickstart 1.2, 512 KiB Chip + 512 KiB Slow RAM, normal weapon load:
  46-50 FPS in measured gameplay intervals, with zero late Copper commits.
- A1200, 2 MiB Chip and no Fast RAM, 90% Tempo: 50 FPS in ordinary sampled
  intervals of the hardware-feedback test; scripted pause excluded.
- Dense two-player stress can drop substantially below 50 FPS. This beta does
  not claim locked 50 FPS. PAL remains the qualified timing target.

Please report model, Kickstart, memory, mode, Skill, Tempo and reproduction steps.
Real-hardware Wingman confirmation and extended playtesting remain important.

Downloads enable `HAR_HARDWARE_PLAYER_ROCKET=1`,
`HAR_HARDWARE_PROJECTILE_CHAIN=1` and `HAR_CRASH_DEBRIS_BOBS=1`.

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
