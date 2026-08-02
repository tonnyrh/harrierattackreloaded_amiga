# Harrier Attack Reloaded - Amiga A500 Playtest

Target:

- Amiga 500 / OCS / PAL
- Kickstart 1.3
- 68000
- 512 KiB chip RAM + 512 KiB slow RAM

Kickstart ROM is not included in this package.

## Files

- `harrier_amiga.exe` - runnable Amiga executable
- `debug/harrier_amiga.elf` - debug symbols for Bartman/Abyss tooling
- `debug/harrier_amiga.map` - linker map
- `debug/harrier_amiga.s` - disassembly/source listing when available
- `VERSION.txt` - package metadata and hashes

## Controls

Menu:

- Up/Down or joystick: select item
- Fire: choose
- Left/Right: change the selected setting
- `Controls...`: edit the current session's Player 1 and Player 2 profiles
- `Aircraft: 1/3` is an intentional Amiga extension, not CPC parity. Three
  aircraft is the default.
- Esc: cancel/back where applicable

Game:

- Default Player 1 keys are arrows, Ctrl for rocket, Space for bomb and E for
  eject. E is accepted only after a fatal airborne hit/fuel failure starts the
  smoking forced descent; it is ignored during healthy flight. Joystick port 2
  defaults to primary button = rocket and second button = bomb.
- Default Player 2 keys are keypad 8/2/4/6, keypad 0 for rocket, keypad Enter
  for bomb and keypad decimal for eject. Joystick port 1 is the default.
- Every CPC action (up/down/left/right/rocket/bomb/eject), joystick port and
  weapon-button assignment can be changed under `Controls...`. Profiles last
  for the current program session; `Restore defaults` restores both players.
- The same non-empty keyboard key or joystick button cannot be assigned to two
  actions within one profile, and both players cannot claim the same joystick
  port.
- Many original Amiga joysticks expose only one physical fire button. Keep a
  keyboard bomb binding when using such hardware; button 2 requires a
  compatible two-button pad/joystick. Player 2's keyboard fallback remains
  available when no second joystick is connected.
- Player 2 Eject abandons the Wingman and leaves it recoverable through the
  normal Wingman powerup; the single spare hardware sprite remains dedicated
  to Player 1's visible eject/parachute sequence.
- P: pause; Space (or P) continues while the HUD message blinks
- Esc: return to menu

Game over:

- Fire / Space: retry immediately
- Esc: return to menu

## Current Playtest Notes

- Sprint 15.55 replaces the menu/Field Guide 30-second timers with complete
  ticker cycles. Let the tribute scroll fully off to enter the guide, then
  let the rules text scroll fully off to return. Confirm that the tank uses
  both front/rear halves and that Enemy Ship and Enemy Plane are neutral grey.
- Sprint 15.55.1 adds `Lock height: On/Off` to the main menu. Fire or
  left/right toggles it. With On, steer vertically after firing and confirm
  the ordinary rocket follows; with Off, confirm it keeps its launch height.
  Maverick must still guide toward its target in both modes.
- Sprint 15.59 restores the CPC enemy-plane hit sequence. Shoot or bomb an
  interceptor and confirm that it briefly changes to the broken-aircraft
  graphic one character cell farther left before disappearing. Score and one
  explosion sound must occur once at contact; no generic explosion tile
  should cover the broken aircraft.
- Sprint 15.60 adds the Amiga aircraft-rescue extension. Fuel/armour exhaustion,
  enemy missile contact and aircraft contact stop the engine and start a
  smoking, alarmed descent. Press E before surface impact: the seat/parachute
  sequence must finish, AIR must decrease once, and a fully supplied replacement
  must return on the opening carrier without resetting score or destroyed
  targets. Do not press E and confirm impact leads to Game Over even when reserve
  aircraft remain. Direct terrain/water/object collision remains Game Over.
- Sprint 15.60.1 fixes the failure-eject timing and parachute surface test. E may
  be pressed or held from the first alarm/smoke frame. Confirm the parachute visibly reaches land or the sea
  surface before carrier reentry/Game Over; it must not vanish immediately.
- Sprint 15.60.2 changes the failure warning to a short rising-pitch cockpit
  bleep (about 0.25 second) followed by 0.5 second of silence. Confirm that it
  repeats at that cadence and stops immediately on eject or impact.
- Sprint 15.60.3 gives a failed Harrier a hard impact bang on surface contact.
  Do not eject: confirm the alarm stops, one bang plays, and the Harrier breaks
  into three forward-moving pieces before the Game Over panel appears.
- Sprint 15.60.4 preserves the abandoned aircraft after eject. Eject while
  high: confirm the intact Harrier continues falling, hits with one bang and
  becomes three pieces while the parachute independently reaches the surface.
  With AIR remaining, carrier reentry follows instead of Game Over.
- Sprint 15.60.5 adds `SKn LVnn` to the HUD top row. A new run must show
  `LV01`; eject/rescue must leave both values unchanged; landing and beginning
  the next board must show `LV02` and increase SK by one up to its maximum 5.
- Sprint 15.60.6 permits voluntary eject from a healthy, airborne Harrier. Press E
  once outside takeoff/landing: the abandoned aircraft must impact separately,
  the chute must reach the surface, and exactly one AIR must be consumed. A
  held E must not trigger again after rescue. Earn 2000 points on one board and
  verify that one white extra-aircraft parachute appears before final approach;
  collecting it adds one AIR and it cannot respawn after eject/reentry.
- Sprint 15.60.7 stabilises guided missiles and falling drops. Fire a Maverick
  at a target almost directly ahead: it may correct vertically once, but must
  not alternate up/down as a travelling V. Watch several parachute powerups
  over both sky and detailed terrain; they must descend at a steady one pixel
  per frame without cadence judder, flashing or damaged background rows.

- Smooth horizontal scroll uses Amiga bitplane pointer scrolling.
- Sprint 13.0 introduces the first CPC-style gameplay `ObjectMap` foundation; the visible world is now rendered from object classes instead of the older decorative `gameWorldTileAt()` routine.
- Sprint 14.0 makes the route visible: start carrier/open sea -> enemy ship -> coastline/land -> town/buildings/ground targets -> pier -> final frigate.
- Sprint 15.53 restores CPC town spacing: every building must have exactly one
  flat terrain column before it. Buildings should no longer form an almost
  unbroken wall, while their sourced shapes, hit smoke and collisions remain
  unchanged.
- Sprint 15.54 lets CPC's final selected town building finish instead of
  cutting it at the 200-column timer. Depending on the session seed, the pier
  and every later event may therefore begin up to four columns later. Check
  that the last building is whole and that pier, enemy ship and final carrier
  remain aligned.
- Sprint 14.1 moves the route and land/object placements into `assets/level_route.h`, a table-based format that can later be generated by a level editor.
- Sprint 14.2 keeps the Harrier near a horizontal camera anchor while left/right scrolls the map, changes enemy plane spawning to route triggers in `assets/level_route.h`, and temporarily uses clean solid land tiles instead of raw CPC hill/coast tiles.
- Sprint 14.3 changes left/right into throttle controls: the map always advances at minimum speed, right accelerates, left decelerates, and the Harrier shifts horizontally with speed. It also adds a runtime-generated Paula engine sound that changes with speed instead of looping an AudioGen/sample effect.
- Sprint 14.4 renders pier/deck/frigate sections with Amiga-native deck/carrier shapes instead of raw CPC deck tiles, avoiding the sawtooth/garbled graphics seen in Sprint 14.3.
- The current `ObjectMap` is still generated up front as a full wide buffer. Sprint 14.5 should turn this into right-edge/dirty-column generation.
- Harrier, weapons, enemies, and enemy missile are hardware sprites for this slice.
- Multiple lives, respawn, session high score, and AudioGen-generated Paula SFX are enabled.
- Collision is still simple sprite-box collision, not pixel-perfect.
- Terrain/object-map collision is intentionally not active yet; that starts in Sprint 15.
- High score is session-only and is not saved to disk yet.
- Many enemies/projectiles should move from hardware sprites to blitter/Bob objects later.
