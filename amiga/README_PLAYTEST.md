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
- `Mode: Enhanced` is the default and enables the intentional Amiga gameplay
  extensions: three aircraft, failure/eject/rescue, the white extra-aircraft
  drop, shared/rebindable Player 2 inventory and terrain-relative accumulating
  radar.
- `Mode: Classic` uses one aircraft, immediate CPC-style destruction/Game Over,
  CPC player-only powerup collection, Off/CPU/Player 2 Wingman choices and CPC's
  absolute player-height enemy-plane gate. Visual, scrolling and audio polish
  remains enabled. The Enhanced-only RADAR label and gauge are omitted from
  the Classic HUD; its right-hand gauge slot intentionally remains blank.
- Esc: cancel/back where applicable

Game:

- Default Player 1 keys are arrows, Ctrl for rocket, Space for bomb and E for
  eject in Enhanced mode. Classic ignores E and destroys the single aircraft
  immediately. Joystick port 2
  defaults to primary button = rocket and second button = bomb.
- A qualifying Game Over score asks for a six-character high-score name.
  Type letters/numbers directly, or use joystick Up/Down to choose a character,
  Left to delete and Right/Fire to accept. Return finishes a shorter keyboard
  entry; the sixth character also accepts automatically. Escape accepts the
  current name (or `PLAYER` when empty) and returns to the menu.
- Default Player 2 keys are keypad 8/2/4/6, keypad 0 for rocket, keypad Enter
  for bomb and keypad decimal for eject. Joystick port 1 is the default.
  Classic fixes P2 to these keys/port, ignores keypad-decimal eject and keeps
  P2 support ordnance independent of Player 1's HUD. Press Up to launch P2
  before the opening carrier leaves the screen.
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
- Enhanced Player 2 Eject abandons the Wingman and leaves it recoverable through the
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
- Sprint 15.67 replaces `Aircraft: 1/3` with the explicit Classic/Enhanced
  profile described above. In Classic, verify that P2 cannot be selected,
  white extra-aircraft drops never spawn, P2 cannot collect a pickup, fatal
  airborne events use the three CPC wreck parts instead of failure descent,
  and enemy admission reacts directly to `playerTileY < 11-skill`. Enhanced
  must preserve the previous three-aircraft, P2, pickup and radar behavior.
- Sprint 15.68 adds a second VS Code launch choice, `Amiga 500 loader + game
  (KS1.3, 1MB)`. Use it to verify that title/loading artwork appears while the
  full game executable loads. Keep `Amiga 500 debug (KS1.3, 1MB)` for game
  breakpoints. The ADF always boots through the loader.
- Sprint 15.68.1 removes the Enhanced-only radar gauge from Classic and adds a
  deterministic two-player weapon stress profile. Its cycle-exact A500 run
  must show non-zero P1/P2 rocket and bomb counters throughout the route.
- In Classic, fly in the eligible middle route at the required absolute height
  and compare several encounters: admission is now a private temporal 1-in-16
  roll at CPC 8-pixel logic cadence, not a fixed world-column pattern and not
  Enhanced radar detection. Then watch the final ship heatseeker through a
  complete approach; it should no longer blink at fine-scroll/ring phases.
- Sprint 15.70.2 restores Classic Player 2. Select Classic + Player 2, press Up
  on joystick port 1 or keypad 8 before the opening carrier leaves, and verify
  smooth independent flight. Keypad 0/fire and keypad Enter/bomb must launch
  weapons without changing Player 1's HUD counters; keypad decimal/Eject and
  P2 rebinding must remain Enhanced-only.
- Sprint 15.70.3 restores CPC Wingman collision in both profiles. With Player
  2, fly either half of the Wingman into land, a building, ship or the sea and
  confirm one explosion and loss of the Wingman. Flying through flak, hit
  smoke, Player 1 or a falling powerup must not destroy it. Direct enemy-plane
  contact must still destroy both aircraft. Also complete a CPU-Wingman run
  and confirm its terrain avoidance and carrier landing remain intact.
- Sprint 15.70.4 separates Classic logic from Amiga presentation for falling
  objects. In Classic, drop a bomb high over open terrain: it must travel
  smoothly forward for four CPC character steps, then fall straight down;
  it must not use Enhanced's short diagonal opening. Watch a parachute drop:
  its canopy must move smoothly in a repeating five-frame/eight-pixel descent,
  while pickup and ground contact happen on CPC logical rows. Repeat both in
  Enhanced and confirm its existing diagonal bomb and one-pixel drop remain
  unchanged.
- Sprint 15.70.5 applies the same CPC-logical, Amiga-smooth bomb and powerup
  cadence to Enhanced. Drop bombs and watch pickups in both profiles: their
  paths and contact timing must now match. Enhanced radar, rescue, reserve
  aircraft and the white bonus-aircraft drop must still work as before.
- Sprint 15.70.6 restores the common tribute scroll on the main menu. Toggle
  Classic/Enhanced and confirm the front-page text neither changes nor jumps
  back to its beginning. Open Items Overview/Field Guide in each mode and
  confirm that only this page alternates between the Classic CPC-rule text and
  the Enhanced terrain-radar/extra-aircraft text.
- Sprint 15.71.0 formalises the town's continuous CPC R stream. Play several
  new sessions so menu dwell time produces different seeds: building order may
  vary, but every building must remain whole, exactly one flat column must
  precede each building, and pier/final ship/carrier alignment must survive a
  final building that extends beyond the 200-column town timer.
- Sprint 15.71.1 keeps a surviving Wingman visible on the carrier between
  missions. Land both aircraft, wait through `LANDED` and the carrier return,
  then verify Wingman remains on its forward deck pad and climbs continuously
  from that exact position on the next takeoff. A previously destroyed
  Wingman must remain absent.
- Sprint 15.72.0 restores CPC's nine-direction formation steering. With CPU
  Wingman enabled, change altitude and speed repeatedly over sea, hills and
  town. Wingman should correct horizontally, vertically and diagonally in
  smooth two-pixel presentation steps, never teleport at intercept/bomb/
  landing transitions, and still avoid solid terrain. After landing, it must
  remain on its forward deck coordinate until takeoff begins.
- Sprint 15.72.1 adds no release-game behavior. Its headless direction audit
  verifies that the Wingman actually uses both straight and diagonal logical
  corrections across a full route.
- Sprint 15.72.2 unifies return-to-formation with the bounded waypoint mover.
  After Wingman completes an intercept or bomb run, watch its entire return:
  it must avoid terrain, move without a horizontal/vertical hitch and join
  formation without teleporting.
- Sprint 15.73.0 exposes CPC damage on the promoted enemy ship. Hit the opening
  or final hostile vessel with a rocket or bomb: the exact struck 8x8 section
  must be replaced by persistent smoke, score must increase by 500 and intact
  neighbouring ship sections must remain visible.
- Sprint 15.74.0/15.74.1 restores the complete CPC campaign palette phase.
  Complete mission 1 and land: the retained carrier scene stays at dusk while
  waiting on deck, then fades smoothly to a dark green night during mission-2
  takeoff. Mission-2 town fades toward dawn. After the next landing, mission 3
  retains dawn on deck and fades back to day during takeoff. Sea must stay dark
  blue and HUD colours must remain unchanged throughout. World terrain should
  darken with the night phase while the HUD's green gauges do not.
- Sprint 15.75.0 restores CPC's later-board difficulty and land duration.
  Start at skill 1: the first town now begins only after roughly 551 generated
  land columns, and pier, second ship, its missile and final carrier must all
  remain aligned after the longer route. Land and begin mission 2: the HUD must
  still show `SK 1` while `LV` advances to 02, terrain/rearm/flak use effective
  difficulty 2, and the land section grows by another 256 columns. Effective
  difficulty and route growth cap at 5 without missing terrain or objects.
- Sprint 15.76.0 makes the menu high-score `LEVEL` column match CPC
  `leveldifficulty`. A new score at selected skill 5 must show `05` even when
  the run ends early; after landing from skill 1 and ending on mission 2 it
  must show `02`. Existing saved rows remain valid and are not rewritten.
- Sprint 15.77.0 restores CPC-style six-character name entry for qualifying
  scores. Verify letters, digits, Backspace, early Return, automatic commit at
  six characters and Escape-to-menu; the resulting menu row must keep the
  correct LEVEL, HITS and SCORE values.
- Sprint 15.78.0 adds the CPC joystick editor. Up/Down must select the pending
  character, Left must delete, and Right/Fire must accept it. Right/Fire with
  no pending character finishes a shorter name. Confirm a held direction or
  Fire button produces only one action per press.

## Current implementation summary

- Smooth horizontal scrolling uses OCS bitplane pointer/fine scrolling with an
  incrementally streamed object-map ring buffer.
- The complete route is active: carrier, sea, enemy ship, coast, procedural
  land, town and targets, pier, final ship and landing carrier.
- Player, Wingman, weapons, enemy aircraft, missiles, powerups and world
  objects use the final mixed hardware-sprite/Bob allocation documented in
  `assets/COMBAT_SPRITE_AUDIT.md`.
- Object-map and aircraft collision, destructible targets, persistent damage,
  mission progression and later-board difficulty are active.
- Classic keeps CPC-style one-aircraft rules. Enhanced adds reserve aircraft,
  eject/rescue, terrain-relative radar and the bonus-aircraft drop.
- High scores use a checksummed two-slot persistent format and six-character
  keyboard/joystick name entry. Failure to read or write remains non-fatal.
