# Harrier Attack Reloaded Amiga — Release Candidate 1

**v0.9.0-rc.1 is a prerelease for final testing, not the final release.**
One main ADF includes the complete Enhanced campaign: terrain and cities,
carrier defence, oil-platform defence, Battleship and the campaign finale.
Classic retains its regular mission loop.

## Changes since Public Beta 6

- Fix ocean-transition loading starvation that could prevent reaching the oil
  platform. Loading always advances, including on busy frames.
- Restore moving surface ripples in sea transitions and before Battleship;
  optimize their rotation to reduce rendering cost.
- Give human P2 a distinct grey Harrier and manual carrier-landing VTOL,
  including momentum, braking, touchdown and hard-landing recoil.
- Restore oil-platform graphics correctly when repair drops pass over them.
- Show clearer low-and-fast radar feedback and fix low bomb hits on depots.
- Intensify Battleship's final defence even with few surviving guns, while
  retaining bounded projectile counts and existing damage values.
- Keep gulls and their calls out of the campaign fireworks finale.

## Downloads and requirements

The ADF is the integrated main game; no separate scene disks are needed.
SHA256SUMS accompanies the download. Boot the ADF afresh: old USS snapshots
contain old executable code and do not acquire these fixes.

Target: PAL A500, OCS/68000, Kickstart 1.3, 512 KiB Chip plus 512 KiB expansion.
No ROM or Amiga system software is included. Preserve existing high scores.

## Validation and remaining testing

The final RC ADF passed a muted, cycle-exact WinUAE cold boot to the game menu
with Kickstart 1.3, 512 KiB Chip and 512 KiB Slow RAM, without mounted hard
disks. Startup reported 74,168 free Chip bytes after runtime allocation.

Focused muted WinUAE checks passed for P2 landing, depot collisions, repair-drop
restoration, Battleship defence, water rendering and finale effects. The four
oil-platform/Battleship ocean transition paths passed buffer-equivalence,
enemy-clearance and projectile checks; maximum measured work was one video
field in that test. This is not a complete playthrough or a general FPS guarantee.

Real-hardware, two-player and balance feedback remain welcome. The legacy full
carrier contract has landing expectations that conflict with hard-landing
behaviour; focused checks do not constitute a passing full carrier suite.

## Credits

Harrier Attack was created by Robert White and published by Durell Software.
Thanks to Chris Perver for Harrier Attack Reloaded for Amstrad Plus, its source
and encouragement. Amiga port by Tonny Roger Holm. Thanks to all Amiga, FPGA
and WinUAE testers. This is an unofficial, non-commercial fan project; no
endorsement by the original rights holders is implied.
