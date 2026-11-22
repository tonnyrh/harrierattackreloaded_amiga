# Harrier Attack Reloaded Amiga — Release Candidate 2

**v0.9.0-rc.2 is a prerelease for final testing.** Download the single main ADF
for the complete game. This candidate includes all Public Beta 8 fixes and the
following improvements from subsequent two-player testing.

## Changes since Public Beta 8

- F1 controller swaps retain short keypresses and play a brief confirmation
  beep. Swapping remains limited to two human players.
- Finishing an already falling helicopter now starts the normal respawn
  delay, including when both pilots hit it in the same update.
- A rescued P1 on the oil platform can aim the gun with the stick, see a
  crosshair and fire while P2 flies. There is a 12-second opening service
  window at full tempo, with radar warning during the final two seconds.
  Either landed pilot can replenish the shared ammunition supply.
- Aircraft rockets and bombs require a fresh button press. Holding a trigger
  through a scene change no longer causes an extra shot. Deck AA cannons
  retain continuous fire.
- Bombs keep their normal artwork and colours in open-sea dogfights.
- An intact Battleship helideck can launch occasional helicopters. Fighters
  enter only after the helideck is destroyed and ship health is at 35% or
  below. Aircraft use a clear flight path above the ship. New fighter raids
  wait while either pilot is over the bridge, and active fighters hold fire
  there to leave room for bombing runs.

## Downloads and requirements

The ADF contains the integrated game; no separate scene disks are needed.
The accompanying SHA256SUMS file identifies this build. Boot the new ADF
afresh: restoring an old USS snapshot restores its old executable too.

Target: PAL A500, OCS/68000, Kickstart 1.3, 512 KiB Chip plus 512 KiB expansion.
No ROM or Amiga system software is included. Preserve existing high scores.

## Validation

The final main ADF passed a muted, cycle-exact WinUAE cold boot through loading,
menu and two-player takeoff, with Kickstart 1.3, 512 KiB Chip plus 512 KiB Slow
RAM and no mounted hard disks. F1 was exercised in both directions: the same
keyboard input first launched P2, then P1 after swapping back.

Focused WinUAE checks passed for short F1 taps and beep scheduling, helicopter
respawn delay, oil-platform rescue/gunsight/rearming, Battleship helicopter and
fighter phases, and fresh-press weapon handling. All four oil-platform and
Battleship ocean transitions passed buffer-equivalence, water-motion,
enemy-clearance and projectile-visibility checks.

These checks are not a complete campaign playthrough or a general FPS
guarantee. Real-hardware, two-player and balance feedback remain welcome;
the new confirmation beep was tested without an audible listening check.

## Earlier release candidate

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
