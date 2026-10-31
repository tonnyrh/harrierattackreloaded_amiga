# Harrier Attack Reloaded Amiga - Public Beta 7

Boot the main ADF to play v0.9.0-beta.7. This beta still needs polish, and feedback on two-player balance and real Amiga hardware is welcome.

## Changes since Release Candidate 1

- Player 2 can take off first and fire rockets or drop bombs while Player 1 is still parked. Both weapons now move and expire normally.
- Press F1 during a two-human-player game to swap controllers. Press again to swap back.
- On Enhanced skill levels 1 and 2, the Harrier gets about 10% more effective shielding against ordinary incoming damage.
- Oil-platform waves include a service break. Radar warns before attacks resume and during carrier defence.
- A damaged Battleship calls in fighters from ahead and behind. Extra supply drops help crews replenish ammunition and recover a lost second player.

# Harrier Attack Reloaded Amiga - Public Beta 6

**v0.9.0-beta.6 is a public prerelease.** This is still a radical beta :-)
It needs further polish, balancing and real-hardware testing.

## Changes since Beta 5

- One main ADF integrates terrain/city missions, carrier defence, oil-platform
  defence and the giant Battleship. A three-sortie cycle repeats with increasing
  difficulty; Classic keeps its regular mission loop.
- Open-sea dogfights accompany incremental scene preparation. Sky colour and
  wave motion persist; the next scene waits for buffers and departing enemies.
- Battleship departure continues forwards until the ship is entirely behind you.
  Return-pass supplies prioritise empty ammunition before a lost Wingman.
- Oil-platform defence is more forgiving. Land to use R repair kits there too;
  each kit restores 10 hull points. World health bars replace the HUD hull text.
- Fuel consumption increased by 15%. The bomber drone is clearer, and invisible
  frigates no longer fire in the campaign's open-sea transition areas.
- Smaller audio assets and shared music/idle Chip memory. Attack alarms use a
  32-byte Paula waveform; a short first idle cue accompanies arriving gulls.
- Debug scene selection and timing diagnostics support campaign testing.

## Downloads

- **ADF:** the single integrated, bootable main game.
- **HD ZIP:** main executable, Workbench icon, loading bitmap and instructions.
- **SHA256SUMS:** checksums for both packages.

Target: PAL A500, OCS/68000, 512 KiB Chip + 512 KiB expansion, Kickstart 1.3.
No ROM or Amiga system software is included. Preserve existing high-score files.

## Validation and limitations

The final main ADF passed a muted, cycle-exact WinUAE cold boot with PAL A500,
Kickstart 1.3, 512 KiB Chip + 512 KiB Slow RAM and no mounted hard disks:
loading -> menu -> Start Game -> carrier takeoff -> terrain flight. Screenshots
confirmed each step; startup reported 92,800 free Chip bytes in this setup.

Focused muted WinUAE contracts passed for fuel consumption, repairs,
oil-platform damage, Battleship departure and audio-buffer/event handling.
Four campaign transition paths were tested during development, including
buffer readiness, water animation and enemy draining. These are focused
checks, not a complete playthrough or a guarantee of 50 FPS.

A legacy full carrier contract still contains landing expectations that conflict
with the newer hard-landing behaviour; the focused checks do not constitute a
passing full carrier suite. Two-player and real-hardware playtesting remain
important. The shortened idle cue has not received a listening check here.

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
