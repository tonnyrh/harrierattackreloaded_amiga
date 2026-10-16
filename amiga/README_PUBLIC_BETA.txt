HARRIER ATTACK RELOADED - AMIGA PUBLIC BETA 6
===========================================

Version: v0.9.0-beta.6 (prerelease)

Thank you for trying this unofficial, non-commercial Amiga port.

The game targets a stock PAL Amiga 500 with OCS, a 68000 processor,
512 KiB chip RAM and 512 KiB expansion RAM. It can also be played on
compatible Amigas, MiniMig and accurate emulators such as WinUAE.

ADF VERSION
-----------

Insert or mount the ADF and boot the Amiga. No Kickstart ROM is included.
High-score saving is attempted only when the media is writable; a failed
save is non-fatal.

HARD-DISK / WORKBENCH VERSION
-----------------------------

Keep these files together in the same drawer:

  harrier_amiga.exe
  harrier_amiga.exe.info
  loading_screen.bpl

Open the drawer and double-click the Harrier icon. EXIT TO DOS returns to
Workbench. The icon requests the stack required by the game.

DEFAULT CONTROLS
----------------

Menu:       Up/Down select, Left/Right change, Fire selects
Player 1:   Up/Down fly, Left/Right throttle, Ctrl rocket, Space bomb
Enhanced:   E ejects, P pauses, Esc returns to the menu
Player 2:   Joystick port 1 or numeric keypad; configure in Controls

Classic mode preserves the CPC gameplay contract. Enhanced mode keeps that
foundation while adding Amiga presentation and gameplay extensions such as
smooth scrolling, terrain radar and aircraft rescue.

BETA 6 - THE INTEGRATED CAMPAIGN
-------------------------------

One main ADF now contains all Enhanced encounters:
  Sortie 1: terrain/city, landing and carrier defence.
  Sortie 2: terrain/city, oil-platform defence, return and carrier defence.
  Sortie 3: terrain/city, Battleship, then final carrier landing.
The cycle repeats with increasing difficulty. Classic retains its regular loop.

Open-sea dogfights cover gradual preparation of the next scene. Colours and
water motion are retained; enemies clear before the destination appears.
After defeating Battleship, fly forwards clear of the ship before departure.

Land to refuel and deliver R kits. A kit now restores 10 hull points; kits
also work on the oil platform. Its health bar appears below it. Fuel use is
15% higher than Beta 5. Empty ammunition takes priority over a replacement
Wingman in Battleship's return-pass supply drop.

Carrier VTOL retains momentum; countersteer to brake. Heading turns
independently of drift. CPU/P2 operates carrier weapons during raids.
Wingman joins over land. Press primary fire after the E lamp lights to eject;
release a held button first. The E key is also available.

Attack alarms use a tiny generated Paula waveform. Music and short idle
bird cues share Chip memory. Original audio masters remain in the repository.

This is still a radical beta :-) Balance, transitions, two-player edge cases
and real-hardware behaviour need more testing. Busy scenes can slow down;
this is not a promise of locked 50 FPS. See release notes for test scope.

Skill runs from 1 (Easiest) to 5 (Hardest). Tempo is independently selectable
at 80%, 90% or 100%. Higher Skill and Tempo give higher score multipliers.
Classic and Enhanced have separate high scores, including Skill/Tempo data.
Old scores are shown under Scores: Legacy. Preserve all harrier_scores*.dat,
harrier_classic2_*.dat and harrier_enhanced2_*.dat files when upgrading.

PUBLIC BETA FEEDBACK
--------------------

Useful reports include machine/model, Kickstart version, memory expansion,
display standard, game mode, skill level, tempo and reproduction steps.
Please report issues at:

  https://github.com/tonnyrh/harrierattackreloaded_amiga/issues

CREDITS AND THANKS
------------------

Harrier Attack was originally created by Robert White and published by
Durell Software. This Amiga port exists because the original game made such
a lasting impression.

Our sincere thanks go to Chris Perver for creating Harrier Attack Reloaded
for the Amstrad Plus, for making its source available as an invaluable
gameplay reference, and for his kind encouragement of the Amiga port. His
careful work provided the foundation for preserving the rules and character
of the Reloaded version.

Amiga port: Tonny Roger Holm

Thanks also to everyone testing on WinUAE, real Amigas and FPGA systems.
Public-beta feedback is greatly appreciated.

This is an unofficial fan project. No Kickstart ROM or Amiga system software
is included. All names and original works remain the property of their
respective owners.
