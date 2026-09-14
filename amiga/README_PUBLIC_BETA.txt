HARRIER ATTACK RELOADED - AMIGA PUBLIC BETA 4
===========================================

Version: v0.9.0-beta.4 (prerelease)

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

BETA 4
----------

Enhanced adds Missile Silos and terrain-following helicopters from mission 2.
Helicopters take two missile hits and fire small machine-gun rounds. Missile
Tank shots climb faster and explode on terrain contact.

At very low throttle, L mode deploys landing gear and uses triple fuel.
Destroy an F depot for 20% fuel. Longer routes carry a larger fuel reserve.
Fuel is turquoise; Armour is yellow. Parachutes do not supply fuel.

Ordinary missiles remove one third of Harrier armour; tank missiles remove
one half. Wingman is destroyed by one missile. Hold rocket and bomb together
when the E lamp lights to eject; the E key remains available.

Wingman sprite updates now publish at vertical blank to address the reported
A500 corruption. Please confirm this on real hardware, especially Kickstart
1.2 with 512 KiB expansion. Busy scenes can still drop below 50 FPS.

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
