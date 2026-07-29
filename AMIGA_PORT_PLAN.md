# Amiga 500 Port Plan

Target: stock Amiga 500, PAL, Kickstart 1.3, 68000, OCS, 512 KiB chip RAM + 512 KiB slow RAM. Every sprint must end with a runnable `F5` build in VS Code using `Amiga 500 debug (KS1.3, 1MB)`.

## Rules

- Keep the CPC version as the gameplay oracle. Compare behavior and screenshots against `compile/build/HarrierAttackReloaded.cpr`.
- Default rule: port CPC gameplay, scoring, weapon behavior, object destruction, collision, spawning, and level progression as-is.
- Allowed Amiga-specific improvements are limited to smooth hardware-assisted scrolling/presentation, Paula sound/effects implementation, and technical restructuring needed to run the same behavior cleanly on a stock Amiga 500 target.
- Do not add new gameplay rules, new balancing, or "better" object behavior unless it is explicitly marked as a temporary debugging aid or the user requests a deliberate enhancement.
- When in doubt, inspect the Amstrad source first and port the smallest observable behavior slice.
- Port data first, then behavior. Avoid mechanical Z80-to-68k translation unless a tiny routine is performance-critical and well isolated.
- Put Amiga runtime code under `amiga/`.
- Put host-side converters under `tools/`.
- Keep each sprint runnable, debuggable, and small enough that regressions are obvious.
- Use A500-safe features only: OCS, 68000 instructions, chip RAM for bitplanes/sprites/copper, no AGA/020 assumptions.
- Keep visual debug overlays behind explicit flags in `amiga/main.c`, so normal playtest screens stay clean while debugger resources remain available.

## Current Baseline

- Amstrad build works and produces `compile/build/HarrierAttackReloaded.cpr`.
- Amiga toolchain works and produces `amiga/out/harrier_amiga.exe` and `.elf`.
- Bartman/Abyss VS Code debug starts WinUAE with Kickstart 1.3.
- CPC loading screen is `compile/HARRSCR.bin`, built from `HARR_SCR2.asm`.
- `HARR_SCR2.asm` says ConvImgCpc, CPC Mode 0, 80x200, `org &C000`.

## Sprint 0 - Amiga Debug Baseline

Status: done.

Runnable result: Bartman demo starts in WinUAE and can be debugged from VS Code.

Done checks:

- `F5` builds and starts WinUAE.
- Breakpoints in `amiga/main.c` work.
- Output files exist in `amiga/out/`.

## Sprint 1 - Loading Screen

Status: done.

Runnable result: Amiga boots straight to Harrier loading screen, then waits for fire/key.

Tasks:

- Create `tools/cpc_screen_to_amiga.py`.
- Decode `compile/HARRSCR.bin` as CPC Mode 0 image data.
- Convert to Amiga bitplane data, likely 320x200 by doubling CPC 160x200 pixels horizontally.
- Create an initial CPC-to-Amiga palette table.
- Embed converted loading image in `amiga/`.
- Replace the Bartman demo visuals with a minimal loading-screen program.

Done checks:

- `F5` shows the loading screen in WinUAE.
- It runs on A500 profile without fast RAM.
- One breakpoint after loading-screen setup is usable.

## Sprint 2 - Text And Menu Shell

Status: done after Sprint 2.1 fixes.

Runnable result: loading screen transitions to a simple Harrier title/menu screen.

Tasks:

- Extract title/menu strings from `AMSTRADFONT3.asm`.
- Convert or recreate the CPC font for Amiga.
- Implement `draw_text(x, y, text, color)`.
- Show title, skill level, controls, and "Start game" text.
- Add a tiny menu state machine with Start/Input/Skill/Redefine entries.
- Sprint 2.1: use a cleaner 8x8 menu font and update only changed menu rows.

Done checks:

- `F5` shows loading screen, then title/menu.
- Text is readable and stable with the clean menu font.
- Cursor/selection can be moved with joystick down/up or right mouse button.
- Left mouse/fire activates the selected item; Start opens a placeholder game screen.
- Selection movement does not redraw the full screen.

## Sprint 3 - Input Layer

Status: done.

Runnable result: menu can be controlled on Amiga.

Tasks:

- Harden joystick port 2 reading.
- Add keyboard fallback for arrows/WASD, Space/Return/Ctrl fire, Escape eject/cancel.
- Create logical input flags matching the CPC routines: up, down, left, right, fire, bomb, eject.
- Add visible input debug text showing current input state and last rawkey.

Done checks:

- Joystick and keyboard both update visible state.
- Menu start action moves to a placeholder game screen.
- Placeholder returns to menu with fire/select or Escape.

## Sprint 4 - Static Game Screen

Status: done after Sprint 4.1 cleanup.

Runnable result: after Start, Amiga renders a non-scrolling game scene.

Tasks:

- Identify tile graphics and tilemaps from `HarrierAttackSourceNew2_alt_CRTC_CART16.asm` symbols (`TILEMAP`, `TILEMAPSEA`, `STARTOBJECTTILEMAP`).
- Write converters for CPC tile graphics to Amiga bitplanes.
- Convert CPC `spritelookuptable`/`spr1..spr102` drawtile graphics from `AMSTRADFONT3.asm`.
- Implement a byte-aligned 8x8 tile renderer.
- Render one fixed 40x25 map position with sky, sea, a Harrier, and a couple of horizon objects.
- Sprint 4.1: keep the scene deliberately clean: no cloud tiles or large land blocks until the palette/raster behavior and object compositor are ported.
- Sprint 4.1: remove input debug overlay from the game scene; keep it on the menu only.
- Sprint 4.2: replace the hand-assembled CPC frigate/deck tile cluster with a clean Amiga-native carrier placeholder.

Done checks:

- `F5` reaches a static game scene.
- Tile alignment is visually plausible and no longer looks like bitplane corruption.
- No sprite/game logic yet.
- Fire/select or Escape returns to menu.

## Sprint 5 - Scrolling World

Status: implemented as Sprint 5.2 pointer-scroll foundation; needs visual F5 confirmation in WinUAE.

Runnable result: joystick scrolls the world with Amiga hardware fine scrolling.

Architecture note:

- Prefer an Amiga-first scroller over copying the CPC CRTC behavior directly.
- Keep world state in tile coordinates plus fine pixel offset.
- Avoid redraw during scrolling. OCS bitplane pointers are word-aligned, while `BPLCON1` covers the in-between pixels.
- Use hardware fine horizontal scroll between tile boundaries via `BPLCON1`.
- Use VBL-safe copper updates for both `BPLCON1` and bitplane pointers.
- Render hidden data beyond the visible 40 tiles, so fine scroll can reveal pixels without redrawing the whole screen every frame.
- Keep HUD/instruments outside the scrolling playfield, likely via a separate static area or later copper split.
- Vertical movement can start as tile/coarse redraw; add hardware/pointer-assisted vertical smooth scroll after horizontal is solid.

Tasks:

- Add a staged scrolling playfield buffer wider than the visible screen. Superseded by Sprint 5.2.
- Sprint 5.2: pre-render a 160-tile world buffer plus left fetch margin in chip RAM.
- Sprint 5.2: scroll by changing bitplane pointers and `BPLCON1`; no full redraw when crossing word/tile boundaries.
- Sprint 5.3: queue scroll changes after input, then apply bitplane pointer/`BPLCON1` updates at the start of the next VBL before keyboard polling, avoiding first-keypress flashes.
- Add fine horizontal hardware scroll with `BPLCON1`. Done.
- Update copper scroll value and bitplane pointers safely after VBL. Done.
- Keep a fallback debug mode that disables fine scroll for comparison. Pending.

Done checks:

- World scrolls left/right smoothly at sub-tile granularity.
- Crossing word/tile boundaries does not redraw the whole screen.
- No visible tearing in normal WinUAE speed.
- Frame time remains acceptable on A500 profile.
- Menu remains stable; HUD split comes in the HUD sprint.

## Sprint 6 - Player Harrier

Status: implemented as Sprint 6.1 hardware-sprite foundation; needs visual F5 confirmation in WinUAE.

Runnable result: player Harrier moves over the scrolling world.

Tasks:

- Identify player sprite data and sprite dimensions. Done: CPC flight tiles 75/76 become a 16x8 player.
- Convert player sprite/mask to Amiga format. Done for hardware sprite 0: CPC tile colors reduced to a 2-plane OCS sprite.
- Use hardware sprite 0 for the player over the scrolling playfield.
- Add sprite pointers to the copper list and enable sprite DMA.
- Add sprite palette colors in game palette registers 17-19.
- Port initial player position and scroll speed into a C `GameState`.
- Masked blit drawing remains later for larger software sprites/effects, not the player Harrier foundation.

Done checks:

- Player moves with input.
- Player remains visually stable over scrolling background.
- Basic position/speed variables are visible in debugger.
- Left/right still scroll the world; up/down visibly move the Harrier. Left/right also nudge the player within screen bounds.

## Sprint 7 - HUD And Instruments

Status: implemented as Sprint 7.1 copper-split HUD foundation; needs visual F5 confirmation in WinUAE.

Runnable result: playable-looking screen with HUD values updating.

Tasks:

- Port score, high score, speed, fuel, rockets, bombs, armour text/values. Done for score/speed/fuel/armour/rockets/bombs placeholders; high score later.
- Add simple numeric formatting. Done with small fixed-width decimal text routine; no `sprintf` dependency.
- Keep HUD separate from scroll area. Done with copper split at `HUD_TOP`.
- Recreate palette zones if needed using copper. Basic shared game palette is enough for Sprint 7.1; palette split remains optional later.
- Add dedicated `hud_buffer.bpl` debugger resource.
- Sprint 7.2: avoid HUD redraw in the scroll hot path; redraw only when quantized visible HUD values actually change.
- Sprint 7.3: double-buffer the HUD and swap HUD bitplane pointers in VBL; update only small numeric fields in the inactive HUD buffer to avoid blinking and scroll hitches.

Done checks:

- HUD updates as scroll/distance changes.
- Scroll area and HUD do not corrupt each other.
- Playfield remains hardware-scrolled above the HUD.

## Sprint 8 - Weapons And Explosions

Status: implemented as Sprint 8.1 hardware-sprite weapon slice; needs visual F5 confirmation in WinUAE.

Runnable result: player can fire rockets/drop bombs and see impacts/placeholders.

Tasks:

- Port weapon state variables: rockets, bombs, missile position/range/status. Done for one rocket, one bomb, and one impact marker.
- Implement projectile movement. Done in screen-space for Sprint 8.1.
- Add temporary collision markers before full enemy logic. Done: rocket edge hit and bomb sea hit trigger impact flash.
- Convert explosion sprites/effects. Done as a tiny hardware-sprite impact placeholder; original CPC explosions later.
- Use hardware sprite 1 for rocket and hardware sprite 2 for bomb/impact.
- Update HUD rocket/bomb ammo counts using the existing double-buffered HUD path.
- Keep bomb sprite on black/yellow pens only; red is reserved for the impact flash.

Done checks:

- Fire and bomb actions work.
- Projectiles move and expire.
- At least one visible explosion/effect exists.
- Escape returns to menu; fire now shoots during gameplay.

## Sprint 9 - Enemies And Collisions

Status: implemented as Sprint 9.3 first enemy/collision pressure slice with game-over stop; ship/flak logic still later.

Runnable result: one complete gameplay slice: player, enemies, shots, score.

Tasks:

- Port enemy plane, missile, ship/flak state. Done for one incoming enemy plane via hardware sprite 3 and one enemy missile via hardware sprite 4.
- Implement collision checks. Done as initial sprite AABB checks for rocket-vs-enemy, player-vs-enemy, and enemy-missile-vs-player.
- Implement weapon interaction. Done: rocket-vs-enemy, bomb-vs-enemy, and rocket-vs-enemy-missile all resolve with impact feedback.
- Add scoring and damage. Done: rocket/bomb enemy hits add bonus score, missile shoot-down adds a small bonus, player/enemy collisions and enemy missiles reduce armour.
- Stop gameplay at zero armour. Done: `ARM 000` shows a game-over HUD message, active threats stop, and Escape returns to menu.
- Keep the first enemy missile horizontal for readability; angled/smart shots should become a later enemy behavior.
- Keep behavior close to CPC using symbol names as references.
- Keep hardware sprite use temporary: sprites 3/4 are fine for Sprint 9.2, but many enemies/projectiles should become blitter/Bob objects later.

Done checks:

- One enemy type can spawn, move, be hit, and affect score/damage.
- One enemy projectile can spawn, move, and damage armour.
- Enemy projectile can be shot down.
- Bombs can affect the current enemy plane in this playable slice.
- Armour reaching zero produces a visible game-over state.
- Game remains runnable after several minutes.

## Sprint 10 - Sound

Status: implemented as Sprint 10.2 AudioGen-to-Paula SFX slice; engine loop, CPC-accurate effects, and music still later.

Runnable result: gameplay slice has Amiga sound effects.

Tasks:

- Map AY-style CPC effects from `CPSoundEffectGenerator2.asm` and `AMSTRADFONT3.asm` to Paula samples or generated waveforms. Started with tiny embedded chip-RAM waveforms, then replaced with AudioGen WAVs converted to 11025 Hz signed 8-bit Paula raw samples.
- Start with fire, bomb, explosion, engine/flight noise. Done for fire, bomb, impact/explosion, player hit, game over, and menu click using `amiga/assets/sfx/*.raw`; engine loop remains later.
- Add music only after SFX is stable.
- Stop Paula DMA channels with frame timers so short effects do not drone.
- Delay Paula DMA re-enable by one VBL after stopping a channel to avoid emulator/hardware audio DMA pointer timing issues.

Done checks:

- Sound plays without disturbing frame rate.
- Channels do not lock up or drone unintentionally.
- Escape/menu transitions silence active SFX.
- WinUAE should no longer report audio DMA wait-hack warnings during normal SFX retriggering.

## Sprint 11 - Game Flow

Status: implemented as Sprint 11.2 first lives/eject/respawn slice; crash animation and persistent high score still later.

Runnable result: title/menu -> game -> death/game over -> title.

Tasks:

- Port lives/status/crash/eject/game-over flow. Done as first slice: HUD lives counter, armour-zero life loss, respawn, manual eject with `E`, and final game-over when lives reach zero.
- Add start/restart. Done: game can start from menu and restart directly from game-over with fire/select.
- Add high score display; persistence can wait. Done: session high score appears on menu; disk persistence later.
- Add wingman only after single-player flow is stable.

Done checks:

- A full round can be started and ended.
- Restart works without rebooting emulator.
- Player has multiple lives, respawns after non-final death, and can eject manually.
- Escape from a live or ended round returns to menu with the current high score.

## Sprint 12 - A500 Polish And Packaging

Status: implemented as Sprint 12.1 playtest packaging slice; profiling, ADF generation, and template cleanup still later.

Runnable result: packaged Amiga build suitable for repeated playtesting.

Tasks:

- Profile frame time in Bartman debugger.
- Reduce chip RAM use.
- Remove template/demo leftovers.
- Generate `.adf` or clean `.exe` artifact. Done for clean timestamped `.exe` playtest package plus zip under `dist/amiga`; `.adf` remains later.
- Add documented controls and build/run instructions. Done with `amiga/README_PLAYTEST.md`, `VERSION.txt`, and packaged debug metadata.
- Do not package Kickstart ROM; use local `.tools/Amiga/Kick` only for development/debug.

Done checks:

- Runs on A500 profile with 1 MiB total memory.
- No debug-only requirement for normal play.
- Build is reproducible from PowerShell and VS Code.
- Playtest package is reproducible with `.\package-amiga.ps1`.

## Original Gameplay Gap Analysis

The current Amiga build is a strong technical prototype, but it is not yet the full CPC game loop. The CPC source has a much richer scrolling mission system than the Amiga slice currently exposes.

Important CPC systems found in `HarrierAttackSourceNew2_alt_CRTC_CART16.asm` and `AMSTRADFONT3.asm`:

- The CPC keeps a gameplay object map separate from screen pixels: `startobjecttilemap`, `tilemap`, and `tilemapsea`.
- `gamelevelprogress` drives the mission sequence:
  - `0` start/open sea setup
  - `1` start enemy ship
  - `2` enemy ship fired missile
  - `3` do land
  - `4` display enemy land object
  - `5` descend mountains down to town level
  - `6` flat townland
  - `7` generate building
  - `8` start of pier
  - `9` end of pier
  - `10` enemy ship fired missile
  - `11` start frigate
  - `12` end frigate
  - `13` landing on frigate
  - `14` open sea
- The tile/sprite set includes land, sea, frigate deck/front/back, enemy ship front/mid/rear, radar, missile launcher, gun, tank, flak, land craters, carrier/frigate parts, gunship, wingman, parachute/ejector seat, enemy plane normal/broken, and missiles in several directions.
- The original collision path is object-map based:
  - `checkplayeragainstobjectmap` handles sky/cloud, powerups, flak chip damage, wingman collision, and fatal terrain/object impact.
  - `checkenemyhit` handles bombs/rockets hitting sea, land, craters, enemy ship, own frigate, enemy plane, enemy missile, wingman, and powerups.
- The original mission has real right-edge generation: sea, enemy ship, coastline, rising/falling terrain, random land enemies, town/buildings, pier, final frigate/landing area.
- The original has anti-aircraft pressure through `launchflakattack`, `flakdamagecount`, and `totalflakdamagecount`.
- The original has a guided/Maverick weapon path using `enemylandlocationlock`.
- The original has a wingman system with takeoff, formation, collision avoidance, enemy-plane interception, bombing runs, landing, death, and summon/powerup behavior.

Current Amiga gaps:

- `gameWorldTileAt()` is still decorative/procedural and repeats sea/ships; it does not implement CPC mission progression.
- There is no real object map yet, so gameplay collision is still simple sprite-box collision.
- Land, town, pier, frigate landing, ground AA, radar/missile launcher/gun/tank, craters, wingman, powerups, guided locks, and proper landing/refuel/rearm are still missing.
- Enemy plane and enemy missile exist as a playable slice, but they are not yet integrated with the original object map, stage rules, or wingman targeting.

## Revised Gameplay-Port Architecture

The next phase should keep the Amiga hardware-scroll foundation, but replace the decorative world with a CPC-faithful gameplay model.

- Keep object state separate from visual rendering.
  - `ObjectMap` owns gameplay ids, health, flags, and transient timers.
  - The rendered world buffer is only a cache of what the player sees.
  - Collisions must query `ObjectMap`, not pixels.
- Keep smooth scrolling cheap.
  - Continue using bitplane pointer scroll + `BPLCON1`.
  - On fine-scroll frames, do not redraw the world.
  - On coarse column boundaries, generate only the new right-edge columns and patch dirty columns.
- Use hardware sprites deliberately.
  - Reserve hardware sprites for player, key aircraft, and fast missiles while possible.
  - Move numerous ground targets, flak, explosions, craters, powerups, and later wingman extras toward blitter/Bob or tile/object rendering.
- Add explicit debug flags before complexity grows.
  - Keep current compile-time style such as `HAR_DEBUG_INPUT_OVERLAY`.
  - Add flags such as object-map overlay, hitbox overlay, force-stage, freeze-scroll, invulnerable-player, and slow-motion.
  - Normal playtest builds should default these off.

## Sprint 13 - Object Map And Stage Generator Foundation

Status: done as Sprint 13.0.

Runnable result: the existing smooth scrolling game runs from an Amiga-side object map instead of the current decorative `gameWorldTileAt()` world.

Tasks:

- Add CPC-inspired object ids/classes for sky, cloud, sea, land, own frigate, enemy ship, player weapon, ground target, flak/smoke, player plane, enemy plane, wingman, and powerup. Done.
- Add an `ObjectMap` ring buffer wide enough for the rendered scroll world plus right-edge generation. Done as the first full-buffer foundation; later sprints can turn it into a true dirty-column ring.
- Port the skeleton of `gamelevelprogress` as an Amiga `StageState`. Done as stage ids and generated section skeletons for start/open sea/enemy ship/land.
- Replace repeated decorative world generation with object-map driven sea/open-sky columns. Done.
- Keep the current hardware scroll path untouched; only the source data feeding the wide world buffer changes. Done.
- Add `HAR_DEBUG_OBJECT_MAP_OVERLAY` and `HAR_DEBUG_FORCE_STAGE` flags, default off. Done.

Done checks:

- `F5` starts a playable build with the same smooth scroll as Sprint 12.
- The world can be regenerated from `ObjectMap`.
- Debug overlay can show object classes without affecting normal visuals.
- No gameplay behavior regresses: player, HUD, rocket, bomb, enemy plane, enemy missile, lives, and SFX still work.

## Sprint 14 - CPC Mission Terrain: Sea, Coast, Land, Town, Pier, Frigate

Status: partially done through Sprint 14.4.

Runnable result: flying right shows a real mission route: open sea, enemy ship lead-in, coastline, land/mountains, town/buildings, pier, final frigate/landing area.

Tasks:

- Port the right-edge generation flow from `docountdowntoenemyship`, `drawseatiles`, terrain height updates, `drawflatterrain`, town generation, pier generation, and frigate insertion. Started: the current full-buffer generator now creates the same visible route order, but not yet as incremental right-edge generation.
- Port or approximate `enemyshipsprite`, `endfrigatesprite`, `solidlandspriteblock`, `townspritestable`, and `enemylandsprites` as data the Amiga renderer can consume. Started: enemy ship, land surface, town/building, pier, final frigate, radar/launcher/gun/tank placeholders are now generated from `ObjectMap`.
- Move the first route/land definitions out of gameplay code. Done in Sprint 14.1: `amiga/assets/level_route.h` now contains broad route segments plus explicit object placements, so a later editor can generate this file or a binary equivalent.
- Convert object blocks into Amiga columns/tiles without full-screen redraw. Deferred to Sprint 14.6 after Sprint 14.5's asset extraction pipeline; current route is generated up front into the wide world buffer and runtime scrolling still avoids redraw.
- Keep stage lengths configurable so we can tune for Amiga playtesting.
- Make the carrier/frigate visually distinct from enemy ships. Done: enemy ship uses CPC tiles, friendly carrier/frigate uses the existing Amiga-native carrier silhouette.
- Make route playtesting less confusing. Done in Sprint 14.2: horizontal input scrolls the map immediately while the Harrier stays near its camera anchor, enemy planes now spawn from `harEnemyPlaneTriggers[]`, and the raw CPC hill/coast tiles are temporarily replaced with a clean solid land tile.
- Port the CPC speed concept. Done in Sprint 14.3: the map now advances continuously at a non-zero minimum speed, left/right adjust throttle/speed, and the Harrier shifts horizontally with speed instead of using right as a direct debug-scroll button.
- Add a non-sample-loop engine sound. Done in Sprint 14.3 as a runtime-generated Paula noise/tone bed that mutates its chip-RAM buffer and changes period/volume with speed, inspired by CPC `flightnoise`/`doflightnoise`.
- Clean up raw CPC deck/pier tile artifacts. Done in Sprint 14.4: pier/deck/frigate route entries now keep object-map ids but use Amiga-native deck/carrier rendering instead of raw CPC deck tiles.

Done checks:

- A player can fly continuously over sea into land/town/pier/frigate without hitching.
- Terrain appears from generated columns, not from a hardcoded repeated scene.
- Scrolling remains smooth when a new terrain section enters.
- Force-stage debug can jump to land, town, pier, and frigate for quick testing.

## Sprint 14.5 - CPC Asset Extraction And Review Pipeline

Status: done.

Runnable result: no runtime visual change; the existing Amiga build still runs, and the CPC graphics are now extracted into reviewable generated assets.

Tasks:

- Add `tools/extract_cpc_assets.py` as a conservative CPC graphics audit pipeline. Done.
- Add `extract-cpc-assets.ps1` so the asset audit can be regenerated from PowerShell without remembering Python arguments. Done.
- Extract all `spritelookuptable` / `spr1..spr102` CPC Mode 0 tiles from `AMSTRADFONT3.asm`. Done: 102 tiles.
- Extract `sprite_pixel_data*` Plus sprite pixel assets for Harrier, enemy plane, carrier, gunship, wingman, and parachute. Done: 20 Plus sprite records.
- Extract main-source object/composite blocks such as `enemyshipsprite`, `enemylandsprites`, `townspritestable`/`blk0..blk7`, `endfrigatesprite`, `writefrigatetilemap1..3`, and pier pen data. Done: 16 object/composite records.
- Store generated outputs under `amiga/assets/generated/cpc/`, including JSON, CSV, Amiga-friendly bitplane dumps, and contact-sheet BMP previews. Done.
- Keep existing runtime assets untouched; generated CPC assets are candidates until deliberately promoted or replaced with polished/native Amiga variants. Done.
- Generate both zero-based and one-based object preview sheets so CPC tile-id conventions can be visually checked before runtime integration. Done.

Done checks:

- `.\extract-cpc-assets.ps1` regenerates the manifest and previews.
- `amiga/assets/generated/cpc/cpc_asset_manifest.json` records provenance, promotion policy, pitfalls, and output paths.
- `amiga/assets/generated/cpc/previews/` shows review sheets for tiles, Plus sprites, and object blocks.
- `.\amiga-build.ps1` still succeeds without consuming the new generated assets automatically.

## Sprint 14.6 - Promote First CPC Plus Assets Into Runtime

Status: done after Sprint 14.6.2 visual cleanup; needs visual F5 confirmation in WinUAE.

Runnable result: carrier/frigate visuals now use promoted CPC Plus sprite data where safe, with the previous Amiga-native rectangle carrier kept behind a compile-time fallback.

Tasks:

- Add `tools/promote_cpc_assets.py` to promote selected audited CPC Plus sprites into a runtime C header. Done.
- Add `promote-cpc-assets.ps1` as the PowerShell entry point. Done.
- Generate `amiga/assets/cpc_promoted_assets.h` from the CPC asset audit, preserving source ids and provenance comments. Done.
- Wire `amiga-build.ps1` to regenerate the audit/header when CPC sources or promotion scripts change. Done.
- Render the friendly carrier/frigate using promoted CPC Plus carrier parts: body, back, front, top, and top 2. Done.
- Preserve the older rectangle carrier under `HAR_USE_PROMOTED_CPC_PLUS_ASSETS` fallback. Done.
- Add a first CPC Plus gunship visual candidate to the route near the pier. Done.
- Sprint 14.6.1: prevent native deck strips from overpainting the promoted CPC carrier/frigate body. Done.
- Sprint 14.6.1: move the gunship candidate higher so it reads as an aircraft instead of a ship/tower. Done.
- Sprint 14.6.2: promote CPC town `blk0..blk7` streams as 5-tile-high column-major building blocks. Done.
- Sprint 14.6.2: replace half-building route placeholders with full town-block anchors. Done.
- Sprint 14.6.2: prevent composite anchor cells from drawing their block id as a stray raw tile. Done.
- Sprint 14.6.3: audit combat sprites and add `cpc_combat_sprites_audit.bmp` for Harrier/enemy/missile/bomb QA. Done.
- Sprint 14.6.3: switch player Harrier and enemy plane hardware sprites to promoted CPC Plus two-half sprite data. Done.
- Sprint 14.6.3: switch player rocket, enemy missile, and bomb shapes to CPC tile rips instead of hand-drawn placeholders. Done.
- Sprint 14.6.4: tested town-block anchor placement; Sprint 14.7 later settles it at `terrainY-3`, matching CPC row `0x0b` when town land top is row `0x0e`. Done.
- Sprint 14.6.5: add a short coast-fall terrain segment before the pier/sea transition and move the last town block away from the vertical land cutoff. Done.
- Sprint 14.6.6: skip CPC town tile `1` land-filler while drawing promoted town blocks, because the Amiga terrain layer already owns solid land. Done.
- Sprint 14.6.7: make coast-fall columns hand control back to the sea layer at and below the waterline, avoiding a full-height green wall during the land-to-sea transition. Done.
- Keep gameplay/collision unchanged; this sprint is visual promotion only. Done.

Done checks:

- `.\promote-cpc-assets.ps1` regenerates `amiga/assets/cpc_promoted_assets.h`.
- `.\amiga-build.ps1` succeeds.
- Existing player, scroll, HUD, weapons, enemy, and route logic are not intentionally changed.
- Visual playtest should check carrier shape, final frigate/deck area, and the new gunship candidate.

## Sprint 14.7 - Bake The Full CPC Route For Visual Comparison

Status: done; needs visual F5 confirmation in WinUAE.

Runnable result: the Amiga playtest now contains a much longer explicit route that mirrors the CPC `gamelevelprogress` flow closely enough to compare the full mission structure.

Tasks:

- Expand the world from 160 to 704 tile columns so the full CPC-style route can fit in the current whole-buffer renderer. Done.
- Convert the CPC countdown/state flow into editor-friendly `level_route.h` segments: start sea, first enemy ship, missile sea, land, long hill/target run, town, pier, second enemy ship, final sea, final frigate, landing approach. Done.
- Add terrain kinds for CPC-style randomized land and town descent, using deterministic baked terrain instead of live CPC RNG. Done.
- Place repeated CPC land targets over the long land section using original tile ids `0x2a..0x2e`. Done.
- Place CPC town blocks across the full town section and anchor them at `terrainY-3`, matching the original `HL=0x0b27` town draw row. Done.
- Add direct CPC `pendata` pier tiles (`JHIJHIJHIJHI`) and a second enemy ship stream after the pier. Done.
- Keep final frigate visually on the promoted CPC Plus carrier for now, with a deck span matching the original final approach. Done.

Done checks:

- `.\amiga-build.ps1` succeeds.
- Route data remains plain tables, ready for a later editor/exporter.
- Visual playtest should verify: first ship, long land run, target density, town block height, pier tiles, second ship, final frigate timing, and whether the 704-column CHIP buffer is acceptable on the configured A500 profile.

Sprint 14.7.1 follow-up:

- Use the CPC opening screen screenshot as the menu/HUD reference. Done.
- Replace the prototype framed Amiga menu with a CPC-style black opening screen: title, highscore columns, left menu choices, right options, score/high-score line, and lower gauge bars. Done.
- Keep debug input overlay behind `HAR_DEBUG_INPUT_OVERLAY`, so normal playtest no longer exposes debug rows on the opening screen. Done.
- Leave the in-game HUD unchanged for this slice; gauge-style in-game HUD parity should be a later dedicated HUD sprint. Done.

Sprint 14.7.2 follow-up:

- Add the CPC-style post-menu takeoff scene: the start carrier scrolls in from the left and stops before gameplay begins. Done.
- Use the promoted CPC landing Harrier sprite while parked on the carrier deck. Done.
- Wait for player UP before switching to the flying Harrier sprite, starting the engine sound, and enabling normal gameplay scroll/input. Done.

Sprint 14.7.3 follow-up:

- Add CPC-style sky bands over the sea/takeoff/gameplay area using copper color changes for color 0 instead of repainting the bitmap. Done.
- Keep the lowest sky band at the existing game palette sky color near the horizon, with darker bands above it to match the CPC reference screenshots more closely. Done.
- Restore color 0 before the HUD split so the HUD remains stable. Done.

Sprint 14.7.4 follow-up:

- Add enemy-ship missile triggers after the CPC enemy ship streams, matching the original `ff`-terminator timing. Done.
- Make ship-launched missiles fatal: one hit starts a Harrier destruction sequence instead of merely reducing armour. Done.
- Use the CPC broken-Harrier tile ids `67`, `68`, and `69` as three hardware-sprite crash fragments. Done.
- Pause normal player input/scroll during the short crash sequence, then respawn if lives remain or enter game over. Done.
- Initial placeholder allowed rocket-vs-missile destruction; Sprint 14.7.5 corrects this to match CPC behavior. Superseded.

Sprint 14.7.5 follow-up:

- Match the CPC enemy missile/object-map behavior: if the enemy missile hits player missile object id `5`, the player missile is destroyed while the enemy missile continues. Done.
- Also clear player bomb on enemy missile overlap, matching CPC object id `6` handling. Done.
- Add a CPC-style heatseeker cutoff: ship missiles adjust vertical position while far away, then stop retargeting inside roughly five tile columns so the player can dodge by changing height. Done.

Sprint 14.8 follow-up:

- Fix the Sprint 14.7 startup regression on stock-style A500 memory profiles. Done.
- Keep the CPC-style route at 704 logical tile columns, but reduce the physical CHIP world bitmap to a 64-tile visible window plus 2-tile scroll margin. Done.
- Translate hardware-scroll byte offsets through a local buffer offset plus `worldOriginColumn`, so the Copper only points inside the small active world strip. Done.
- Rebuild the active object-map/window when coarse scroll crosses into a new source column, rather than allocating/rendering the whole mission as one huge bitmap. Done.
- Sprint 14.8.1: fix CPC-style menu text collisions after the startup fix: remove the overlapping right-title remnant and separate menu/options, score line, and gauges vertically. Done.
- Sprint 14.8.2: make takeoff use a fixed origin-0 world window, so the carrier roll-in scrolls by Copper offsets without regenerating the object-map/window every few frames. Done.
- Sprint 14.8.2: stop writing live bitplane pointer/fine-scroll registers during runtime scroll updates; update only the Copper-list shadow so the next frame picks up changes without HUD tearing. Done.
- Sprint 14.8.3: change gameplay windowing from "render every word boundary" to paged local Copper offsets. The 66-tile bitmap is now reused across roughly 22 byte columns before the object-map/window is regenerated, reducing A500 scroll stalls dramatically while keeping pointer offsets word-aligned. Done.
- Sprint 14.8.4: fix HUD split timing by preparing the HUD bitplane/fetch/modulo switch late on the scanline before `HUD_TOP`, after world fetch has completed but before the first HUD line starts. Done.
- Sprint 14.8.4: make HUD background use color 0 and set color 0 to black at the HUD split, avoiding a fragile "black" background made from filled bitplanes. Done.
- Sprint 14.8.4: add double-buffered world windows so page re-renders happen into an inactive bitmap; the Copper swaps to the new bitmap only after rendering is complete, avoiding visible partial redraw/horizontal jumps. Done.
- Sprint 14.8.5: replace blocking page renders during gameplay with a tiny background render job. The next world page is built over several VBLs: base tile columns first, object/native overlays second, then the Copper swaps only when complete. Done.
- Sprint 14.8.5: service audio before each render chunk so Paula engine/SFX updates are not starved by C-side world generation. Done.
- Sprint 14.8.6: reduce world background-render chunks from 8 to 4 columns per frame, and increase the world stripe from 64 to 80 visible tiles so there is more time between page boundaries. Done.
- Sprint 14.8.6: move background render service to the end of the frame loop, after audio/input/gameplay/HUD updates, so streaming work uses leftover time instead of delaying the visible frame path. Done.
- Sprint 14.8.7: increase the world stripe again to 128 visible tiles and reduce object overlay chunks to one column per frame, giving the streaming renderer more lead time before a page boundary. Done.
- Sprint 14.8.7: split promoted CPC town-block overlays by visible column during background rendering, so wide building blocks no longer render as one large frame spike. Done.
- Sprint 14.8.7: replace the temporary solid-land-only surface with CPC terrain tiles: grass surface tiles 32-35 and hill up/down tiles 24-31 selected from neighboring terrain height changes. Done.
- Sprint 14.8.7: make the long land section use a denser deterministic CPC-style height profile instead of broad flat plateaus. This is still an approximation of the original `R`-driven CPC generator, but it now uses the ripped terrain tiles rather than placeholder flat land. Done.
- Keep `level_route.h` as editor-friendly table data; a later sprint can replace it with a JSON/CSV-like editor source file plus converter.
- Dirty-column patching for destructible terrain/targets remains planned for Sprint 15/16, once object-map collision/destruction is live.

## Sprint 14.9 - CPC Map And Gameplay Mechanics Audit

Status: started as Sprint 14.9.0.

Runnable result: same playable build, but with an evidence-backed checklist of where the Amiga port still differs from the CPC game, plus the first fixes for map density and obvious movement oddities.

Playtest notes from 2026-07-21:

- Terrain is better than the flat placeholder, but the later route still becomes too flat.
- Ground-target density is too low; there is only the occasional tank where the CPC game should provide more anti-air/land activity.
- Enemy planes and missiles move in a way that still feels wrong compared with the original.
- Confirm the original speed model: the Harrier should not fully stop; it should advance at a low minimum speed and speed up/down from there.
- Keep land/route definitions external/editor-friendly so a later route editor can modify terrain, target placement, and object triggers without rewriting gameplay code.

Tasks:

- Trace the CPC terrain path more carefully around `drawflatterrain`, hill generation, `gamelevelprogress`, and the `R`-register/random-height logic.
- Sprint 14.9.0: replace the short repeated Amiga land profile with a longer CPC-walk-style profile that continues varying through the late land section. Done.
- Decide whether Sprint 15 should consume a baked CPC-derived height/target table or a closer runtime generator; prefer baked/editor-friendly data if accuracy and tooling are both good enough.
- Expand `level_route.h` or generate a companion route data file with denser target placement: radar, missile launcher, gun, tank, town blocks, ship/deck/pier, and transition markers.
- Sprint 14.9.0: increase long-land target density in `level_route.h` using CPC target tiles 42 radar, 43 missile launcher, 44 gun, and 45/46 tank. Done.
- Audit enemy plane movement, ship missile launch, missile homing/cutoff, player rocket/bomb paths, and collision rules directly against the Amstrad source.
- Sprint 14.9.0 audit note: current Amiga enemy plane/missile code is still simplified screen-space logic. The CPC code ties movement and missile behavior into object-map drawing, player speed, and `enemymissilestatus`; do not add more enemy types until this is mapped.
- Verify known CPC behavior before changing it: player rocket should not destroy the enemy ship missile; dodging should be possible by changing height after missile lock/cutoff.
- Sprint 14.9.0 audit note: current Amiga speed model has non-zero minimum scroll and throttled left/right speed changes, which matches the broad CPC concept, but it uses 1..4 pixel steps instead of CPC's 0..15 `playerspeed` gauge/X-position model. This remains a tuning/mechanics follow-up.
- Sprint 14.9.1: reduce repeated explosion audio by playing only the first short one-shot slice of `impact.raw` and adding a small same-SFX retrigger guard per Paula channel. Done.
- Sprint 14.9.2: make all short Paula SFX more stall-safe by copying each sample into a CHIP one-shot playback buffer with a silence tail. If heavier map rendering delays `updateSfx`, Paula now loops silence instead of audibly repeating the sample. Done.
- Add debug-only inspection toggles for route column, stage id, terrain height, target id, and enemy/missile state without showing them in normal builds.
- Keep performance notes open: any denser target/map work must not reintroduce world-render hitches or HUD flicker on stock A500 + 1 MiB.

## Sprint 14.10 - Tile/Scroll Renderer Hardening

Status: started as Sprint 14.10.0.

Runnable result: the current route remains visually the same, but background preparation of large promoted CPC objects is split into small column slices so scrolling/audio have less chance to hitch.

Tasks:

- Keep the CPC-like tile/object-map model as the main landscape pipeline: route/object data -> object-map cells -> tile columns into the Amiga scroll bitmap -> Copper/BPLCON1 smooth scroll.
- Sprint 14.10.0: add clipped/range drawing for promoted CPC Plus carrier and gunship assets during `drawObjectMapNativeObjectsRange`. Done.
- Sprint 14.10.0: avoid drawing a full carrier/gunship when only one background-render chunk column is being serviced. Done.
- Later: convert promoted large objects into pre-rendered Amiga BOB/blitter-friendly chunks, once the clipped CPU path is confirmed visually correct.
- Later: replace page-window rebuilds with a true circular dirty-column scroll buffer only after mechanics/collision are stable.

Done checks:

- Carrier/gunship still render correctly in takeoff, sea, pier, and final frigate sections.
- Background render hitches are reduced when large objects enter the route.
- SFX should not audibly repeat even when a large object page is prepared.

## Sprint 14.11 - CPC Speed Model First Pass

Status: started as Sprint 14.11.0.

Runnable result: the player speed model is closer to the CPC game while keeping the Amiga hardware-scroll pipeline smooth.

Tasks:

- Port the broad CPC `playerspeed` concept: speed is a 0..15 level, not just 1..4 raw pixels per frame.
- Sprint 14.11.0: replace `GameState.scrollSpeed` with `GameState.speedLevel` and show that level in the HUD. Done.
- Sprint 14.11.0: derive world scroll pixels from speed level as 1..4 pixels/frame, so the map still never fully stops. Done.
- Sprint 14.11.0: derive Harrier screen X from speed level, matching the CPC idea that speed moves the plane forward/back on screen. Done.
- Later: tune the exact 0..15-to-scroll timing against CPC's interrupt delay/speed behavior.
- Later: feed the same speed/world delta into enemy plane and missile movement, because the current enemy logic is still simplified screen-space movement.

Done checks:

- Holding right accelerates through speed levels rather than jumping directly between four coarse values.
- Holding left decelerates but the map continues moving at the minimum rate.
- Harrier no longer travels freely all the way to the far right; its X position follows speed level.
- Existing takeoff, weapons, HUD, SFX, and scroll build cleanly.

## Sprint 14.12 - Enemy Movement Uses World Delta

Status: started as Sprint 14.12.0.

Runnable result: enemy plane and enemy missile motion reacts to the same world-scroll delta as the player speed model, instead of moving as fully independent screen-space placeholders.

Tasks:

- Sprint 14.12.0: pass the current `scrollPixels` from the main frame update into enemy plane and enemy missile updates. Done.
- Sprint 14.12.0: apply world-scroll compensation to enemy plane X movement, so faster player speed makes incoming enemies close faster on screen. Done.
- Sprint 14.12.0: apply the same world-scroll compensation to enemy missile X movement. Done.
- Later: compare the exact closing speeds and missile homing/cutoff against CPC `launchmissile`, `launchheatseekingmissile`, and `aimheatseakingmissile`.
- Later: decide whether player rocket/bomb should also be represented as world/object-map coordinates rather than pure screen-space.

Done checks:

- Build stays runnable.
- Enemy objects should feel more connected to speed/world scrolling.
- If missiles become too aggressive at high speed, tune missile base dx after visual F5 testing rather than removing world coupling.

## Sprint 14.13 - Enemy Plane Missile Fire Timing

Status: started as Sprint 14.13.0.

Runnable result: enemy planes fire before they are almost off the left edge, even when player speed/world-scroll makes them cross the screen faster.

Tasks:

- Sprint 14.13.0: replace the fixed `enemyPlane.timer == 54` missile launch with a position-based trigger at screen X 224, plus a timer fallback. Done.
- Keep this as a first-pass approximation until the CPC enemy plane state machine is fully ported.
- Later: compare against CPC `enemyplanestatus` and missile launch/target selection around `launchmissile` and `missiletargetwingman`.

Done checks:

- Enemy planes should launch missiles while still visible on the right/mid screen area.
- Higher speed should no longer delay plane missile launch until the plane is exiting left.
- Ship-launched missiles remain unchanged.

## Sprint 14.14 - Player Weapons Use World Delta

Status: started as Sprint 14.14.0.

Runnable result: player rocket and bomb motion reacts to world-scroll speed, matching the same screen/world coupling now used by enemies.

Tasks:

- Sprint 14.14.0: pass `scrollPixels` into the weapon update. Done.
- Sprint 14.14.0: apply world-scroll compensation to player rocket X movement. Done.
- Sprint 14.14.0: apply world-scroll compensation to bomb X movement while keeping its downward fall. Done.
- Sprint 14.14.0: safely clear rockets/bombs if they drift off the left side at high speed. Done.
- Later: decide whether bombs should hit terrain/object-map instead of only sea height.
- Later: tune rocket base speed/range against CPC `checkplayermissilemove`.

Done checks:

- Rockets should no longer feel detached from speed/world movement.
- At high speed, rockets may have less forward screen gain; tune base speed if playtest feels too weak.
- Bombs should drift with the scrolling world instead of sliding strangely forward.

## Sprint 14.15 - Bombs Hit Object-Map Terrain

Status: started as Sprint 14.15.0.

Runnable result: bombs can hit the generated world/object-map instead of only exploding at a fixed sea-height line.

Tasks:

- Sprint 14.15.0: add a small world-point probe that maps screen x/y plus `scrollX` to a generated object-map column. Done.
- Sprint 14.15.0: make bombs explode on land, ground targets, and enemy ship cells. Done.
- Sprint 14.15.0: award a small placeholder bonus for bombs hitting target/ship cells. Done.
- Sprint 14.15.1: fix a bomb-trigger hang caused by constructing a full `ObjectMap` probe inside the per-frame weapon hot path. The point probe now checks route/terrain/object data directly. Done.
- Keep this non-persistent for now; it does not yet crater terrain or permanently destroy targets.
- Later: replace the one-column probe with collision against the active/pending object-map window once dirty-column persistence exists.
- Later: add target ownership/health so two-tile tanks and ship segments destroy as one logical object.

Done checks:

- Bombs should explode when they touch land/targets instead of falling through to the old sea line.
- Bombs over open sea still explode at sea height.
- No persistent crater/destruction yet; this is the first collision slice.

## Sprint 14.16 - Rocket Forward Motion Tuning

Status: started as Sprint 14.16.0.

Runnable result: player rockets no longer appear to hang in mid-air at high scroll speed.

Tasks:

- Sprint 14.16.0: raise player rocket base speed from 4 to 7 pixels/frame. Done.
- Keep world-scroll compensation, but ensure max player speed still leaves positive on-screen rocket movement.
- Later: tune rocket range/speed against CPC `checkplayermissilemove` rather than treating this as final.

Done checks:

- At max scroll speed, rocket should still move forward visibly.
- At low speed, rocket should be fast but not absurd.

## Sprint 14.17 - Rockets Hit Object-Map Terrain

Status: started as Sprint 14.17.0.

Runnable result: player rockets can hit land, ground targets, and enemy ship cells through the same lightweight route/object probe used by bombs.

Tasks:

- Sprint 14.17.0: add object-map point collision for the player rocket nose. Done.
- Sprint 14.17.0: make rockets explode on land, ground targets, and enemy ship cells. Done.
- Sprint 14.17.0: award a small placeholder bonus for rocket hits on target/ship cells. Done.
- Keep sprite-vs-enemy-plane collision in the later collision pass for now.
- Later: unify rocket/bomb target scoring and persistent destruction with object ownership/health.

Done checks:

- Rockets should no longer pass through terrain or ships.
- Rockets should still hit enemy planes through the existing sprite collision.
- Terrain/target hits are visual/score only; no persistent destruction yet.

## Sprint 14.18 - Persistent Ground Target Destruction State

Status: started as Sprint 14.18.0.

Runnable result: ground targets hit by rockets or bombs are remembered as destroyed for the current run and skipped when object-map columns are regenerated.

Tasks:

- Sprint 14.18.0: add a small runtime `destroyedTargetColumns` table. Done.
- Sprint 14.18.0: reset destroyed target state at new game start. Done.
- Sprint 14.18.0: mark ground target columns destroyed when rocket/bomb object-map collision hits them. Done.
- Sprint 14.18.0: skip destroyed ground targets in route/object rendering and collision probes. Done.
- Later: add immediate dirty-column redraw so a destroyed target disappears from the already-visible active scroll buffer without waiting for a rebuild/window transition.
- Later: add logical ownership so both halves of a tank, and multi-column ship objects, destroy together.

Done checks:

- Hitting a ground target should stop it from being regenerated later in the run.
- Existing visual explosion/score still occurs.
- Target may remain visible in the current already-rendered bitmap until dirty-column redraw is implemented.

## Sprint 14.19 - Early Ground Target Playtest Belt

Status: started as Sprint 14.19.0.

Runnable result: ground targets appear earlier and more frequently after land begins, making the recent rocket/bomb collision and destruction-state work easier to verify.

Tasks:

- Sprint 14.19.0: add an early route-data belt at columns 104, 110, 123, 141/142, and 158. Done.
- Sprint 14.19.1: move ground targets from terrainY-1 to terrainY-2 and add a small visible playtest marker overlay, because the raw CPC target tiles were too easy to lose in the mountain/grass edge. Done.
- Keep this in `level_route.h` rather than gameplay code, so it can later become editor data or be tuned back toward CPC generation.
- Keep in-game HUD gauge parity planned, but deferred until collision/destruction slices are easier to test.

Done checks:

- Player should see targets shortly after land starts.
- Recent Sprint 14.18 destroyed-target state becomes easier to observe.
- If density feels too arcade/test-like, tune the route table later rather than changing collision code.

## Sprint 14.20 - World-Anchored Impacts And Wider Bomb Probe

Status: started as Sprint 14.20.0.

Runnable result: explosions caused by world/terrain hits stay anchored to the scrolling world instead of following the player/screen, and bombs are less likely to miss small target markers.

Tasks:

- Sprint 14.20.0: add `worldAnchored`/`worldX` to weapon/impact state. Done.
- Sprint 14.20.0: use world-anchored impacts for rocket/bomb hits on terrain, ground targets, ships, and sea. Done.
- Sprint 14.20.0: draw world-anchored impact sprites at `worldX - scrollX`, hiding them if they scroll out. Done.
- Sprint 14.20.0: widen bomb collision from one probe point to several probe points around the falling bomb. Done.
- Later: immediate dirty-column redraw should remove the destroyed target marker at the same moment as the world impact.

Done checks:

- Explosions on land/sea should remain fixed to the ground/water as the world scrolls.
- Bombs should register small target hits more reliably.
- Player/enemy-air impacts can remain screen-space for now.

## Sprint 14.21 - Immediate Dirty-Column Target Redraw

Status: started as Sprint 14.21.0.

Runnable result: ground target markers disappear immediately when destroyed by rocket/bomb, instead of waiting for a later scroll-buffer rebuild.

Tasks:

- Sprint 14.21.0: factor direct world-column cell lookup out of the screen-point collision probe. Done.
- Sprint 14.21.0: add direct single-column render for the active scroll buffer format. Done.
- Sprint 14.21.0: after marking a ground target column destroyed, redraw that world column in every valid world buffer that currently contains it. Done.
- Enemy ship/friendly frigate destruction remains separate; this sprint only handles ground targets.

Done checks:

- Bombing/rocketing a ground target should remove the visible marker immediately.
- Explosions should stay world-anchored from Sprint 14.20.
- Enemy ship/fregate hits may still only show explosion/score until ship ownership/destruction is implemented.

## Sprint 14.22 - Persistent Land Craters

Status: started as Sprint 14.22.0.

Runnable result: rockets and bombs that hit plain land now leave a persistent CPC-derived crater/hole tile in the scrolling world.

Tasks:

- Sprint 14.22.0: add a small runtime `landCraterColumns`/`landCraterRows` table. Done.
- Sprint 14.22.0: reset crater state at new game start. Done.
- Sprint 14.22.0: render CPC tile 97 (`HOLE IN LAND SPRITE`) for cratered land cells in both object-map generation and dirty-column redraw. Done.
- Sprint 14.22.0: carry collision tile row out of weapon terrain probes so rocket/bomb impacts can mark the correct world column and row. Done.
- Sprint 14.22.0: dirty-redraw the hit world column immediately when a new crater is created. Done.
- Later: decide whether destroyed ground targets should leave a crater/smoke owner record instead of simply disappearing.

Done checks:

- Bombing or rocketing plain land should visibly leave a small persistent hole.
- The hole should scroll with the terrain and survive normal scroll-buffer redraws.
- Bombs that hit sea should still only create a splash/explosion and no land crater.

## Sprint 14.23 - Destroyed Targets Leave Ground Damage

Status: started as Sprint 14.23.0.

Runnable result: destroyed ground targets now leave a persistent crater/hole on the land surface beneath them instead of only disappearing.

Tasks:

- Sprint 14.23.0: add a helper that resolves the land surface tile row for a world column. Done.
- Sprint 14.23.0: when a rocket destroys a ground target, also mark a crater on the land surface in that column. Done.
- Sprint 14.23.0: when a bomb destroys a ground target, also mark a crater on the land surface in that column. Done.
- Sprint 14.23.0: keep the target removal and crater drawing in the same dirty-column redraw. Done.
- Later: replace this simple one-tile scar with object-specific wreckage/smoke for tanks, radar, guns, and launchers.

Done checks:

- Ground targets should still disappear immediately when hit.
- A small crater/hole should remain on the land surface below the destroyed target.
- Plain land craters from Sprint 14.22 should still work.

## Sprint 14.24 - Persistent Enemy Ship Column Damage

Status: started as Sprint 14.24.0.

Runnable result: enemy ship columns hit by rockets or bombs are remembered as destroyed and disappear immediately from the active scrolling world.

Tasks:

- Sprint 14.24.0: add a small runtime `destroyedShipColumns` table. Done.
- Sprint 14.24.0: reset destroyed ship-column state at new game start. Done.
- Sprint 14.24.0: skip destroyed enemy ship columns in route/object rendering and collision probes. Done.
- Sprint 14.24.0: mark enemy ship columns destroyed on rocket/bomb hits and dirty-redraw the affected column. Done.
- Later: replace per-column damage with a logical ship owner/health record so a whole CPC enemy ship can sink/explode as one object.

Done checks:

- Hitting an enemy ship column should remove that visible column immediately.
- The removed column should not become an invisible collision object.
- Other columns of the same ship may remain until we add ship-level ownership/health.

Patch:

- Sprint 14.24.1: fixed direct world-column redraw fallback so sea rows redraw with `seaTileForColumn()` after a ship column is removed, instead of clearing the sea to tile 0.

## Sprint 14.25 - Logical Enemy Ship Health

Status: started as Sprint 14.25.0.

Runnable result: the two enemy ship streams are now treated as logical ship groups with shared HP; after enough hits the whole ship disappears together and score is awarded once.

Tasks:

- Sprint 14.25.0: add explicit enemy ship group definitions for the first ship (`50..53`) and second ship (`629..632`). Done.
- Sprint 14.25.0: reset ship HP/destroyed state on new game start. Done.
- Sprint 14.25.0: route rocket/bomb ship hits through `damageEnemyShipAtColumn()`. Done.
- Sprint 14.25.0: remove the whole ship group and dirty-redraw all its columns when group HP reaches zero. Done.
- Sprint 14.25.0: award enemy ship score once on group destruction instead of per-column/per-hit. Done.
- Later: move ship group definitions into `level_route.h` or the future route editor data.
- Later: add sinking/smoke/explosion sequence instead of instant disappearance.

Done checks:

- A single hit should explode but not necessarily remove the ship.
- After repeated hits, the whole enemy ship should disappear at once.
- The score should increase once when the ship is destroyed, not on every hit.

Patch:

- Sprint 14.25.1: added a ship-only 3x3 tile fallback probe around rocket/bomb impact points so visually valid hits on the compact CPC enemy ship are less likely to miss due to a one-tile edge/row mismatch.
- Sprint 14.25.2: tightened bomb ship fallback so bombs no longer snap upward into ship hits, removed enemy-ship hits from the bomb top probe, and restored immediate per-hit visual ship-section removal while keeping whole-ship HP/score.
- Sprint 14.25.3: removed bomb-to-ship fallback entirely and restricted side/bottom bomb probes to land/ground targets; enemy ships are now hit only by the bomb's center-bottom probe.

## Sprint 14.26 - Enemy Ship Wreck Smoke

Status: started as Sprint 14.26.0.

Runnable result: destroying an enemy ship now leaves a small persistent CPC smoke/wreck marker where the ship was, instead of the ship simply disappearing into clean sea.

Tasks:

- Sprint 14.26.0: add a small world-anchored `shipWreckSmoke` table. Done.
- Sprint 14.26.0: use CPC smoke tiles 51/52 as wreck markers. Done.
- Sprint 14.26.0: reset ship smoke at new game start. Done.
- Sprint 14.26.0: when an enemy ship group is destroyed, remove the ship columns and add smoke in the former ship area. Done.
- Sprint 14.26.0: render smoke through the same direct world-column lookup so it survives scrolling and redraws. Done.
- Later: convert smoke to timed/animated effects once we add a general world-effect system.
- Later: add sinking/splash animation and CPC-correct scoring/timing.

Done checks:

- Enemy ship still takes three valid hits.
- On destruction, the ship disappears and small smoke/wreck tiles remain in the ship area.
- Sea underneath should still redraw correctly.

## Sprint 14.27 - CPC-Style Enemy Ship Hit Response

Status: started as Sprint 14.27.0.

Runnable result: enemy ship hits now follow the Amstrad `bombhitenemyship`/`drawsmokesprite` behavior more closely: every valid hit awards ship-hit score and replaces the hit area with smoke, instead of waiting for an Amiga-only HP death moment.

CPC source reference:

- `checkenemyhit` dispatches enemy ship hits to `bombhitenemyship`.
- `bombhitenemyship` sets score value `50`, calls `explosionnoise`, then calls `drawsmokesprite`.
- `drawsmokesprite` draws smoke tile 52 at the hit and smoke tile 51 one tile left only if that tile is sky.

Tasks:

- Sprint 14.27.0: add a CPC-style hit-smoke helper using smoke tiles 52 and optional 51-left-if-sky. Done.
- Sprint 14.27.0: award enemy ship score on every valid ship hit. Done.
- Sprint 14.27.0: replace the active whole-ship HP/death-score behavior with per-hit smoke/section removal. Done.
- Sprint 14.27.0: dirty-redraw the hit column and possible left smoke column immediately. Done.
- Later: remove or move the now-less-important ship group HP scaffolding once the rest of the CPC object-map hit rules are ported.

Done checks:

- Each valid rocket/bomb hit on an enemy ship should create smoke immediately.
- Score should increase on each valid ship hit.
- The old “no smoke until sinking” behavior should be gone.

## Sprint 14.28 - CPC Sea Hit Parity

Status: started as Sprint 14.28.0.

Runnable result: bombs that hit sea disappear without creating an Amiga-only splash/impact effect, matching CPC `bombhitsealand`.

CPC source reference:

- `checkenemyhit` sends sea object id and pier object id to `bombhitsealand`.
- `bombhitsealand` pops the hit coordinate and returns without drawing smoke, crater, score, or explosion.
- Land still goes to `bombhitlandmakehole`.
- Enemy ship still goes to `bombhitenemyship`.

Tasks:

- Sprint 14.28.0: document the stricter porting rule: CPC gameplay is authoritative; Amiga-specific changes are limited to smooth scrolling, sound implementation, and technical restructuring. Done.
- Sprint 14.28.0: remove the Amiga-only `startWorldImpact()` splash when a bomb reaches sea height. Done.
- Keep bomb cleanup on sea contact so the weapon does not fall forever. Done.

Done checks:

- Bombs hitting sea should simply disappear.
- Bombs hitting land should still create crater/hole.
- Bombs hitting enemy ship should still create CPC-style smoke and score.

## Sprint 14.29 - First CPC Player Object-Map Collision Slice

Status: started as Sprint 14.29.0.

Runnable result: the player can now crash into hostile/non-safe object-map cells such as land, enemy ship remnants/targets, town blocks, and other solid route objects.

CPC source reference:

- `checkplayeragainstobjectmap` returns safe for cloud, sky, wingman, and handles powerups/flak specially.
- Most other object ids branch to `planehitbyobject`.
- This sprint ports the first fatal-object slice only; flak damage, powerups, wingman, and own-frigate landing/refuel remain separate CPC parity sprints.

Tasks:

- Sprint 14.29.0: add a small Harrier probe against direct world object-map lookup. Done.
- Sprint 14.29.0: treat sky, sea, smoke/flak, and own frigate as non-fatal for this first slice. Done.
- Sprint 14.29.0: trigger the existing Harrier crash sequence when a probe hits a fatal object-map cell. Done.
- Later: port flak damage accumulation instead of treating flak/smoke as safe.
- Later: port own-frigate landing/refuel state before making frigate collision fully CPC-correct.

Done checks:

- Flying into land should start the Harrier crash sequence.
- Flying through sky/over sea should remain safe.
- Takeoff/friendly frigate should not be broken by this first collision slice.

## Sprint 14.30 - CPC Player Sea Collision

Status: started as Sprint 14.30.0.

Runnable result: flying the Harrier into the sea now starts the crash sequence, matching CPC `checkplayeragainstobjectmap`.

CPC source reference:

- `checkplayeragainstobjectmap` returns safe for cloud and sky.
- Sea is not one of the safe cases and falls through to `planehitbyobject`.
- This is separate from `checkenemyhit`, where bombs/rockets hitting sea go to `bombhitsealand` and do not create effects.

Tasks:

- Sprint 14.30.0: remove `HAR_OBJ_SEA` from the Amiga player object-map safe list. Done.
- Keep weapon-vs-sea behavior from Sprint 14.28 unchanged. Done.
- Keep flak special handling deferred to its own CPC parity sprint. Done.

Done checks:

- Flying into sea should crash the Harrier.
- Bombs hitting sea should still simply disappear without splash.
- Flying through sky should remain safe.

## Sprint 14.31 - CPC Flak Damage Counter

Status: started as Sprint 14.31.0.

Runnable result: flying through object id 10 flak/smoke no longer counts as safe; it increments a CPC-style flak damage counter and only crashes the Harrier when health reaches zero.

CPC source reference:

- `checkplayeragainstobjectmap` treats object id 10 as flak.
- It increments `flakdamagecount`, calls `updatehealth`, compares against `totalflakdamagecount`, and only then branches to `planehitbyobject`.
- `totalflakdamagecount` is initialized to 100.

Tasks:

- Sprint 14.31.0: add `flakDamageCount` to Amiga `GameState`. Done.
- Sprint 14.31.0: reset flak damage on new game/life respawn. Done.
- Sprint 14.31.0: make player object-map collision return safe/fatal/flak instead of treating flak as safe. Done.
- Sprint 14.31.0: decrement existing `ARM` display from 100 to 0 as the Amiga presentation of CPC health/flak damage. Done.
- Sprint 14.31.0: start Harrier crash when flak damage reaches 100. Done.

Done checks:

- Flying through flak/smoke should reduce ARM gradually rather than instant-crashing.
- Reaching ARM 0 from flak should start the crash sequence.
- Land/sea fatal collision from Sprints 14.29/14.30 should still work.

## Sprint 14.32 - First Visible CPC Flak Objects

Status: started as Sprint 14.32.0.

Runnable result: flak object id 10 now appears in the route using CPC flak tiles 57/58, giving the Sprint 14.31 flak-damage collision something visible to hit.

CPC source reference:

- `launchflakattack` draws either tile 57 or 58.
- It writes object id 10 (`FLAK OBJECT ID`) via `drawspritecheckifsky`.
- Full CPC launch timing is dynamic and stage-dependent; this sprint adds deterministic route-data flak first so collision/damage can be tested.

Tasks:

- Sprint 14.32.0: add visible `HAR_OBJ_FLAK` entries using CPC tiles 57/58 in land and town route data. Done.
- Sprint 14.32.0: keep flak as level-route data rather than render-only decoration. Done.
- Sprint 14.32.0: reuse Sprint 14.31 player flak damage handling. Done.
- Later: replace/tune deterministic flak entries with a closer port of `launchflakattack` timing and random tile choice.

Done checks:

- Flak specks should become visible during the land/town run.
- Flying through flak should reduce ARM gradually.
- Land/sea/object fatal collisions should remain unchanged.

## Sprint 14.33 - CPC Weapon Clears Smoke/Flak

Status: started as Sprint 14.33.0.

Runnable result: player rockets and bombs can clear object id 10 smoke/flak, matching the CPC `checkenemyhit` branch for `SMOKE / FLAK`.

CPC source reference:

- `checkenemyhit` checks object id 10 (`SMOKE / FLAK`) and branches to `playermissilehitenemymissile`.
- That branch calls `explosionnoise` with score value 1, then draws object id 1 sky back at the hit position.

Tasks:

- Sprint 14.33.0: add a small persistent `clearedFlak` world column/row table. Done.
- Sprint 14.33.0: filter route flak and dynamic smoke from world/object lookup once cleared. Done.
- Sprint 14.33.0: allow rocket/bomb hit probes to hit `HAR_OBJ_FLAK`. Done.
- Sprint 14.33.0: on weapon hit, clear the flak/smoke cell, award score 1, trigger impact, and dirty-redraw the hit column. Done.

Done checks:

- Shooting or bombing visible flak should remove it.
- Shooting/bombing smoke from ship hits should clear that smoke tile.
- Cleared flak/smoke should not continue damaging the player.

## Sprint 14.34 - CPC Own Frigate Weapon Hit

Status: started as Sprint 14.34.0.

Runnable result: player weapons hitting the friendly frigate/carrier now follow the CPC `bombhitownfrigate` response: set frigate status and draw smoke, with no score award.

CPC source reference:

- `bombhitownfrigate` sets `playerfrigatestatus` to 1.
- It calls `explosionnoise` with score value 0.
- It calls `drawsmokesprite`.

Tasks:

- Sprint 14.34.0: add `playerFrigateStatus` to Amiga game state. Done.
- Sprint 14.34.0: reset frigate status on new game/life respawn. Done.
- Sprint 14.34.0: allow rocket/bomb center hit probes to hit `HAR_OBJ_OWN_FRIGATE`. Done.
- Sprint 14.34.0: on friendly frigate hit, set status 1 and draw CPC-style smoke at/left of hit. Done.
- Later: port the CPC landing/refuel/rearm state machine that uses `playerfrigatestatus`.

Done checks:

- Shooting/bombing the friendly carrier/frigate should create smoke.
- Score should not increase for friendly frigate hits.
- This sprint should not change landing/refuel behavior yet.

Patch:

- Sprint 14.34.1: added an own-frigate-only fallback hit probe for Amiga-native carrier visuals, because the start carrier is rendered as a wider native object while the route has a single carrier anchor cell. Smoke is placed at the actual hit column.

## Sprint 14.35 - First Frigate Service Slice

Status: started as Sprint 14.35.0.

Runnable result: landing/contact on a friendly frigate deck can replenish the player, and firing is blocked while sitting on the deck.

CPC source reference:

- `replenishmissilesfuel` restores the fuel/rocket/bomb gauge data and recalculates flak/health capacity.
- `checklaunchbomb`/missile launch code gates firing around `playerfrigatestatus` and landing/frigate progress.
- The full CPC landing hover loop, score award, wingman wait, frigate scroll, and crash-on-bad-landing behavior are not fully ported in this slice.

Tasks:

- Sprint 14.35.0: add explicit Amiga constants for clear/hit/serviced frigate status. Done.
- Sprint 14.35.0: add a deck-contact probe using the friendly-frigate/native-carrier footprint. Done.
- Sprint 14.35.0: replenish fuel, rockets, bombs, armour, and flak damage on friendly deck contact. Done.
- Sprint 14.35.0: block rocket/bomb launch while the Harrier is on a friendly deck. Done.
- Later: split fuel into true CPC gauge/inventory state instead of recalculating from scroll distance, so refuel persists exactly like CPC.
- Later: port the full `landinghoverloop` and landing score/state behavior.

Done checks:

- Touching/landing on a friendly deck should refill rockets/bombs/armour and set fuel full for that moment.
- Firing while sitting on the friendly deck should be blocked.
- Flying into land/sea should still crash; own frigate remains non-fatal.

Patch:

- Sprint 14.35.1: fixed a visual/object-map mismatch where the friendly native carrier was drawn wider than its hit/service footprint. Player collision now checks the friendly-frigate footprint before treating sea as fatal, and direct world lookup exposes native carrier deck cells across the rendered carrier width.

## Sprint 14.36 - Amiga Scroll/Control Responsiveness Pass 1

Status: started as Sprint 14.36.0.

Runnable result: speed changes and scroll response should feel less sluggish while staying within the allowed Amiga-specific presentation improvements.

Porting-rule note:

- This is not a CPC gameplay-rule change. It is an Amiga presentation/control-feel pass: smooth scrolling and responsive input are explicitly allowed Amiga-specific improvements.

Tasks:

- Sprint 14.36.0: reduce throttle repeat delay from 10 frames to 5 frames. Done.
- Sprint 14.36.0: make scroll pixel speed ramp earlier: level 0 = 1 px/frame, 1..4 = 2, 5..8 = 3, 9..15 = 4. Done.
- Sprint 14.36.0: make player speed anchor respond every speed level instead of every two levels. Done.
- Sprint 14.36.0: reduce anchor step from 8 to 6 pixels so the Harrier still stays within a comfortable controllable band. Done.
- Later: add frame-cost/probe counters before raising max scroll beyond 4 px/frame.
- Later: split gameplay speed, HUD updates, object probes, and render budget so higher-speed flight does not hitch.

Done checks:

- Holding right should accelerate noticeably faster.
- World scroll should reach useful speeds earlier.
- Harrier horizontal position should respond more continuously to speed changes.
- Collision/object-map behavior should remain unchanged.

## Sprint 14.37 - Amiga Control/SFX Responsiveness Pass 2

Status: started as Sprint 14.37.0.

Runnable result: vertical/horizontal Harrier control should feel less sluggish, and repeated one-shot SFX should retrigger less aggressively.

Porting-rule note:

- This remains in the allowed Amiga presentation layer: input responsiveness and Paula one-shot management. CPC gameplay/object rules are not changed.

Tasks:

- Sprint 14.37.0: increase player movement speed from 1 px/frame to 2 px/frame. Done.
- Sprint 14.37.0: clamp player X/Y movement after 2 px steps so min/max bounds remain exact. Done.
- Sprint 14.37.0: replace fixed 6-frame same-SFX retrigger guard with per-SFX guards. Done.
- Sprint 14.37.0: use longer retrigger guards for impact/hit/game-over, medium for bomb, shorter for fire/menu. Done.
- Later: profile whether repeated sounds are caused by repeated collision events and suppress at source where CPC state says the object should be cleared.

Done checks:

- Up/down movement should feel more responsive.
- Repeated impact/hit sounds should be noticeably less machine-gun-like.
- Weapon, collision, and object-map rules should remain unchanged.

## Sprint 14.38 - Audio Guard And Collision Hot-Path Pass

Status: implemented as Sprint 14.38.0.

Runnable result: one-shot SFX should no longer re-trigger just because Paula finished the sample, and own-frigate/deck collision probes should cost less per gameplay frame.

Porting-rule note:

- This is an Amiga implementation/performance pass only. CPC gameplay rules remain authoritative; it just keeps the same events from over-triggering audio and avoids expensive repeated carrier lookup work.

Tasks:

- Sprint 14.38.0: keep per-channel SFX retrigger guards alive after a one-shot sample stops, instead of clearing the guard in `stopSfxChannel`. Done.
- Sprint 14.38.0: replace own-frigate near-point probing from a nested object-map scan with a direct route-object footprint check. Done.
- Sprint 14.38.1: trim bomb SFX playback to the first short one-shot slice so a bomb drop does not sound like three repeated drops. Done.
- Sprint 14.38.1: add a small bomb-launch cooldown in gameplay state to suppress key/joystick bounce from spawning multiple bomb events. Done.
- Sprint 14.38.2: trim the bomb launch slice further and suppress only the impact sound for near-instant bomb impacts, while keeping the visual impact. Done.
- Later: add optional frame-cost counters around world render, collision probes, and audio service so sluggish spots can be measured instead of guessed.
- Later: if F5 playtest still feels sticky, move more object collision helpers from generated-cell probing to cached/route-specific narrow checks.

Done checks:

- Explosion/hit sounds should not repeat several times from the same event.
- Landing/service/own-frigate collision should still work.
- Scrolling and player movement should not regress.

## Sprint 14.39 - World Render Hot-Path Pass

Status: implemented as Sprint 14.39.0.

Runnable result: scrolling should hitch less when a new buffered world page is prepared, especially around town/land object sections.

Porting-rule note:

- This is an Amiga-only renderer optimization. The object-map contents and CPC gameplay rules are unchanged.

Tasks:

- Sprint 14.39.0: narrow promoted town-block range rendering from a full world-buffer scan to only the possible source columns that can overlap the dirty/rendered slice. Done.
- Sprint 14.39.0: keep the staged world-render pipeline intact so audio still receives service between render chunks. Done.
- Later: make promoted carrier/gunship range drawing use the same pre-indexed source-list idea if they show up as measurable hitches.

Done checks:

- Town/mountain sections should feel a bit less “sticky” during scroll-page generation.
- Existing CPC-promoted town blocks should still render when their origin starts just left of the active render slice.
- No gameplay or collision behavior changes.

## Sprint 14.40 - Carrier Range Render Fix And Repo-Local Perf Logging

Status: implemented as Sprint 14.40.0.

Runnable result: promoted own-carrier graphics should render continuously across staged world-buffer object slices, and F5 debug runs can emit lightweight aggregated performance logs to a repo-local WinUAE log volume.

Porting-rule note:

- Carrier rendering is a visual correctness fix for the Amiga staged renderer; it does not change the CPC route/gameplay rules.
- Perf logging is debug-only instrumentation and should be disabled before a release/performance baseline.

Tasks:

- Sprint 14.40.0: fix staged promoted carrier rendering so carrier origin columns just left of the current render slice can still draw the middle/front of the sprite into that slice. Done.
- Sprint 14.40.0: apply the same overlap-safe range-render pattern to two-column promoted gunships. Done.
- Sprint 14.40.0: add `HAR_DEBUG_PERF_FILE` logging foundation. Superseded by Sprint 14.41's aggregated `LOGS:` logger.
- Sprint 14.40.0: log frame, scroll, speed, active world origin, render-job stage/tile, and per-interval rendered tile/object/page counters. Superseded by Sprint 14.41's min/max/avg interval format.
- Sprint 14.40.1: ensure `amiga-build.ps1` creates `.amiga-dh0` and the repo-local log disk folder if missing. Done.
- Sprint 14.40.1: disable continuous `HAR_DEBUG_PERF_FILE` writes by default after a playtest hang, pending a safer burst/log-on-demand approach. Done.
- Sprint 14.40.2: restore F5/debug output to `amiga/out`; the repo-local log disk folder is separate from the executable folder. Done.
- Sprint 14.40.3: tighten own-frigate player collision so only the lower/deck probe can be considered safe; flying the body/nose through the ship is no longer treated as deck contact. Done.
- Sprint 14.41.0: initially tried mounting repo-local `.amiga-dh2` as `DH2:`/`LOGS:`, but the active F5 WinUAE instance did not pick up the extra device reliably. Superseded.
- Sprint 14.41.1: use the existing `DH0:` runtime mount and write aggregated perf rows to `DH0:logs/harrier_perf.log`, overwriting on each game start. Done.
- Sprint 14.41.1: make `amiga-build.ps1` create the physical Bartman `bin\win32\dh0\logs` folder if missing. Done.
- Sprint 14.41.2: disable Amiga-DOS file writes for perf logging after repeatable hangs on the first 10-second write; emit the same aggregated rows through WinUAE debug console (`KPrintF`) instead. Done.

Done checks:

- The final/start carrier should no longer appear as only separated bow/stern/deck fragments during staged rendering.
- F5 launches from `amiga/out`, which the active WinUAE debug configuration mounts as `DH1:`.
- Perf logs currently go to the WinUAE/VS Code debug console every 10 seconds via `KPrintF`; do not use Amiga-DOS file writes during gameplay until a non-blocking path is proven.
- The player can land/service on the carrier deck only via the lower landing probe, not by passing through ship graphics.
- The normal F5 launch remains stock A500 512k chip + 512k slow.

Done checks:

- We can say exactly which CPC routines each Amiga movement/spawn rule maps to.
- A short playtest run shows visibly more CPC-like terrain variation and target density.
- Enemy/missile behavior has either been corrected or documented with precise source references for the next sprint.
- Normal build hides debug overlays unless explicitly enabled.

## Sprint 15 - Terrain And Object-Map Collision

Status: planned.

Runnable result: terrain and ships become real gameplay objects; hitting them matters.

Tasks:

- Port `checkplayeragainstobjectmap` behavior for player/object overlap.
- Map the player sprite to object-map cells using world coordinates.
- Implement fatal collisions with land, enemy ship, hostile ground targets, and non-landable structures.
- Implement safe/landable collision class for own frigate deck.
- Implement bomb/rocket object-map hit tests for sea, land, ship, own frigate, enemy plane, enemy missile, wingman, and powerups.
- Expand crater/hole behavior into proper terrain damage rules where CPC expects it.

Done checks:

- Flying into terrain or hostile objects costs a life.
- Bombs hitting land create a persistent crater/hole in the object map and rendered world.
- Bombs hitting sea do not create false effects.
- The existing sprite-box enemy collision still works until fully replaced.

## Sprint 16 - Destructible Ground Targets

Status: planned.

Runnable result: radar, missile launcher, gun, tank, and enemy ship segments can be attacked and destroyed.

Tasks:

- Port `enemylandsprites` target selection: radar, missile launcher, gun, tank.
- Give multi-tile objects a single owner/health record so all segments refer to the same target.
- Implement score awards matching CPC intent: small targets, enemy ship, enemy plane, enemy missile.
- Replace destroyed targets with smoke/crater/empty land as appropriate.
- Add a hidden hitbox/object-id overlay for debugging weapon hits.

Done checks:

- Ground targets spawn on land stages.
- Rockets and bombs can destroy them.
- Score/HUD updates without blinking or scroll hitching.
- Destroyed objects remain destroyed as they scroll through the visible buffer.

## Sprint 17 - Flak And Anti-Aircraft Pressure

Status: planned.

Runnable result: anti-aircraft fire makes flying low over land dangerous.

Tasks:

- Port `launchflakattack` as an Amiga system that spawns short-lived flak/smoke objects near land AA.
- Port `flakdamagecount` and `totalflakdamagecount` style chip damage.
- Show damage through HUD armour/health without requiring a full HUD redraw every frame.
- Add flak SFX mapping to Paula.
- Keep flak as Bob/tile/transient object rendering, not a permanent hardware-sprite user.

Done checks:

- AA positions fire visible flak bursts.
- Flak gradually damages the player and can cause death/eject.
- Destroying AA reduces local flak pressure.
- A500 frame time remains acceptable during active flak.

## Sprint 18 - Enemy Ship, Heat-Seeking Missiles, And Frigate Landing

Status: planned.

Runnable result: the sea/combat/landing loop becomes meaningful.

Tasks:

- Port enemy ship countdown and ship-missile launch behavior.
- Port heat-seeking missile movement in object-map/world coordinates.
- Keep player/enemy missiles shootable through the shared object-map collision path.
- Implement own frigate landing detection, refuel/rearm/repair, and flak damage reset.
- Prevent firing while in landing/refuel state, matching original intent around `playerfrigatestatus`/landing.

Done checks:

- Enemy ship appears and launches a missile.
- Player can shoot down or evade missiles.
- Final frigate can be landed on.
- Landing refills fuel/rockets/bombs/armour and resumes play cleanly.

## Sprint 19 - Maverick Lock And Guided Weapons

Status: planned.

Runnable result: the player can lock and fire a guided weapon against ground targets.

Tasks:

- Port `enemylandlocationlock` and its scrolling behavior.
- Add lock acquisition against visible hostile ground objects.
- Port normal rocket range and Maverick guided movement using the CPC 8-direction idea.
- Draw a small lock indicator behind a debug flag first, then make it a normal UI element once stable.
- Add menu/HUD indication for weapon mode/range/lock height if needed.

Done checks:

- Player can lock a ground target, fire, and see the missile guide toward it.
- Losing or destroying the target clears the lock.
- Normal rockets still behave correctly.

## Sprint 20 - Enemy Plane Integration And Original Air Combat

Status: planned.

Runnable result: current enemy plane behavior is replaced by original-style stage-aware enemy plane logic.

Tasks:

- Gate enemy plane spawns to the CPC land-progress range rather than a fixed timer.
- Port approach/retreat/fire states from `enemyplanestatus`.
- Use normal/broken enemy plane graphics.
- Integrate enemy plane collision with `ObjectMap` and weapon hit code.
- Prepare target selection so enemy missiles can later target wingman as well as player.

Done checks:

- Enemy planes appear mainly during the intended mission sections.
- Planes can fire, retreat, collide, be shot down, and leave correct score/effects.
- Current air combat remains playable but less placeholder-like.

## Sprint 21 - Powerups And Wingman Entry

Status: planned.

Runnable result: collectible powerups exist, and wingman can be summoned or restored.

Tasks:

- Port powerup status/location/speed handling.
- Add health, rockets, bombs, and wingman powerup types.
- Implement player collection through object-map collision.
- Add wingman spawn/takeoff placeholder tied to powerup collection.
- Keep wingman AI minimal in this sprint: enter formation, follow player, avoid obvious terrain.

Done checks:

- Powerups spawn, drift/drop, collide with terrain/objects, and can be collected.
- Health/rocket/bomb powerups refill the correct HUD values.
- Wingman powerup brings a visible allied plane into formation.

## Sprint 22 - Wingman AI, Bombing Runs, And Intercepts

Status: planned.

Runnable result: wingman becomes a real gameplay participant.

Tasks:

- Port `wingmantakeoff` states for on-carrier, takeoff, landing waypoints, landed, track enemy plane, killed, and cleared.
- Port wingman movement/avoidance using object-map lookahead.
- Add wingman missile/bomb blocks and reuse player weapon collision code where possible.
- Add wingman bombing run behavior against locked land targets.
- Add wingman death and friendly collision handling.

Done checks:

- Wingman can follow, avoid terrain, attack enemies, be hit, and return/land.
- Enemy missiles can target player or wingman.
- Friendly collisions and own missile hits are handled safely.

## Sprint 23 - Eject, Parachute, Death States, And Mission Loop

Status: planned.

Runnable result: death/eject/respawn behaves closer to the original.

Tasks:

- Replace the current instant eject/life-loss placeholder with ejector seat and parachute state.
- Port parachute movement and terrain/sky collision behavior.
- Add crash/landing failure states, using `playerstatus`-style state names in C.
- Continue scrolling briefly after death where appropriate.
- Ensure game-over/retry/menu paths remain stable.

Done checks:

- Fatal hit triggers the correct death/eject/parachute sequence.
- Lives, respawn, and game over still work.
- No stuck invisible-player or stuck-scroll states after repeated deaths.

## Sprint 24 - Scoreboard, Skill, Tuning, And Persistence

Status: planned.

Runnable result: the game has original-style scoring and configurable difficulty.

Tasks:

- Port score values and scoreboard/high-score presentation more closely.
- Apply skill/difficulty to spawn rates, flak threshold, terrain, enemy timing, and rocket lock/range.
- Add persistent high score if safe for the target packaging mode; keep disk write optional for debug/playtest.
- Add a compact playtest checklist per skill level.

Done checks:

- Skill level visibly affects gameplay.
- Score and high score behave consistently across a full run.
- Optional persistence never blocks normal no-disk/no-HDD play.

## Sprint 25 - Audio Parity, Performance, And ADF Release

Status: planned.

Runnable result: a near-complete Amiga playtest release with original gameplay coverage and A500-safe performance.

Tasks:

- Map remaining CPC sound calls: flight/engine, flak, frigate, missile, bomb, loud explosion.
- Add engine/flight loop carefully without starving SFX channels.
- Profile worst-case land/town/flak/wingman scenes on A500 profile.
- Move crowded objects from hardware sprites to blitter/Bobs where needed.
- Generate a clean `.adf` package in addition to the current `.exe` zip.
- Update playtest docs and known differences from CPC.

Done checks:

- Worst-case gameplay remains smooth enough on stock A500 + 1 MiB.
- Sound effects no longer read as generic beeps.
- `.exe` and `.adf` artifacts are reproducible.
- The release notes clearly list remaining intentional deviations from the CPC version.

## Sprint 14.42 - In-Memory Telemetry Screen

Status: done.

Runnable result: normal builds no longer try to write runtime logs to AmigaDOS volumes. If extra memory is available, Shift+D in the menu enables an in-memory telemetry ring buffer and shows a small red `D`; Ctrl+D during gameplay pauses and shows the latest aggregated stats; Space returns to gameplay.

Done checks:

- Telemetry allocation is optional and non-fatal.
- No `DHx:` volume is required for normal debug/playtest runs.
- Samples are aggregated roughly every 10 seconds with FPS min/max/avg, hitch count, max VBL delta, scroll/speed/origin, render-job progress, and HUD counters.
- The stats screen is entered only by the explicit Ctrl+D debug chord during gameplay.

## Sprint 14.43 - Range Render CPU Spike Reduction

Status: done.

Runnable result: promoted CPC assets still render in the same world-buffer pipeline, but range rendering avoids scanning/drawing unnecessary source pixels when only a small tile-column slice is being updated.

Done checks:

- `drawCpcPlusSpriteScrollRange` computes source x bounds from the tile-column clip range before walking sprite pixels.
- Carrier deck coverage checks scan only the possible carrier columns instead of the full object map.
- Gameplay and CPC asset placement are unchanged.
- Build remains F5-runnable.

## Sprint 14.44 - Telemetry UX And Carrier Deck Landing

Status: done.

Runnable result: telemetry can be opened during gameplay with Shift+D, the telemetry screen shows the build label, and native promoted carriers expose an explicit world-space deck collision/service surface.

Done checks:

- Ctrl+D still works for compatibility, but Shift+D avoids the fire/control key conflict.
- Telemetry screen includes `HAR_BUILD_LABEL`.
- Native carrier deck detection uses route-data world columns plus the promoted carrier deck pixel Y.
- Low-speed contact with the native carrier deck snaps to deck height, stops speed, and services fuel/armour/weapons.

## Sprint 14.45 - Hitch Map Telemetry And High-Speed Render Pacing

Status: done.

Runnable result: telemetry pinpoints worst VBL hitches by map scroll position, and high-speed gameplay uses smaller tile-render chunks for smoother frame pacing.

Done checks:

- Telemetry tracks interval max VBL delta plus scroll position.
- Telemetry tracks session max VBL delta plus scroll position, filtered to airborne gameplay.
- Stats screen shows `MAXVBL MAP I S` for interval/session hitch locations.
- At speed level 10+, world tile rendering uses smaller chunks to reduce per-frame CPU spikes.

## Sprint 14.46 - CPC-ish Vertical Player Range

Status: done.

Runnable result: player vertical movement reaches closer to the CPC top-of-screen behavior, and telemetry reports actual player Y range used during gameplay.

Done checks:

- `PLAYER_MIN_Y` lowered from 20 to 8 pixels, closer to CPC tile row 1.
- `PLAYER_MAX_Y` tightened slightly from 150 to 144 as a safe interim step before resizing playfield/HUD.
- Telemetry screen shows `PLY Y MIN MAX`.
- Full playfield-height/copper optimization remains a planned separate sprint.

## Sprint 14.47 - World Buffer Height Matches HUD Split

Status: done.

Runnable result: the scrolling world buffer is allocated and clipped only to the visible playfield above the HUD split.

Done checks:

- `GAME_WORLD_HEIGHT` is `HUD_TOP` instead of full `SCREEN_HEIGHT`.
- World buffers allocate 168 lines instead of 200.
- Scroll pixel/rect/tile drawing clips to `GAME_WORLD_HEIGHT`.
- Debug bitmap metadata reports the reduced world-buffer height.
- Telemetry shows `PFH` so playtests can confirm the active playfield height.

## Sprint 14.48 - Object Stage Row Scan Reduction

Status: done.

Runnable result: world object rendering keeps the same object placement but scans only the tile rows where current native/promoted objects can appear.

Done checks:

- Native object full-buffer and range render passes use tile rows 7..15 instead of 0..24.
- Carrier/deck, ground targets, gunships, town blocks, flak, and ship-adjacent objects remain within the scanned range.
- Build remains F5-runnable.
- Telemetry should show lower object-stage hitches if row scanning was the dominant cost.

## Sprint 14.49 - Telemetry Reset While Paused

Status: done.

Runnable result: telemetry sessions can be reset from the in-game telemetry pause screen without restarting the game.

Done checks:

- `R` on the telemetry screen clears samples, session min/max FPS, session max VBL, player Y range, and the current interval.
- Footer documents `SPACE BACK   R RESET`.
- Build remains F5-runnable.

## Sprint 14.50 - Object Render Substages

Status: done.

Runnable result: asynchronous object rendering is split into smaller substages so a single frame does not draw every promoted/native object type for a column range.

Done checks:

- Object stage is split into base/deck/targets, gunship, and town substages.
- Telemetry `RENDER STG` can now show 1, 2, or 3 during object rendering.
- Hotfix 14.50.1 raises object columns per substage to 4; one column made the next scroll buffer miss deadlines at high speed.
- Full immediate render still draws all objects as before.
- Build remains F5-runnable.

## Sprint 14.51 - Early World Buffer Prefetch

Status: done.

Runnable result: the next scroll page can start rendering before the active page boundary, reducing the chance that gameplay reaches a page swap before the next world buffer is ready.

Done checks:

- Scheduler computes current/next origin from `scrollX` instead of waiting for active buffer origin to advance.
- Prefetch starts when the local scroll position is within 24 bytes of the next page.
- Hotfix 14.51.1 expands the prefetch window to 80 bytes after telemetry showed `BTP 000` while object stage was still active.
- Telemetry shows `BTP` (bytes to page boundary) in the scroll row.
- Build remains F5-runnable.

## Sprint 14.52 - Foreground Priority During World Rendering

Status: done.

Runnable result: player, weapon, enemy, enemy missile, and HUD pointer updates are applied after gameplay and before background world-render work, reducing visible weapon/sprite lag during render hitches.

Done checks:

- Foreground sprite/HUD pending updates are flushed before `serviceWorldRenderJob`.
- World render chunk size is reduced while a player rocket or bomb is active.
- Build remains F5-runnable.

## Sprint 14.53 - Weapon Sprite Priority

Status: done.

Runnable result: active bombs no longer lose their hardware sprite slot to impact/explosion visuals.

Done checks:

- Bomb sprite has priority over impact sprite in the shared weapon/impact sprite slot.
- Later corrected in Sprint 14.57.1: normal world rendering must continue while player rocket or bomb is active, otherwise the next scroll buffer can starve over land.
- Foreground sprite/HUD updates from Sprint 14.52 remain in place.
- Build remains F5-runnable.

## Sprint 14.54 - Scroll Pipeline Telemetry

Status: done.

Runnable result: telemetry reports scroll-buffer misses and origin readiness so scroll jitter can be diagnosed from in-game measurements.

Done checks:

- Tracks desired origin, ready origin, and active origin.
- Tracks scroll misses, total wait frames, max consecutive wait, and current wait.
- Stats screen shows `MISS WAIT MAX CUR` and `DES RDY ACT`.
- Build remains F5-runnable.

Next suggested instrumentation improvement:

- Add a compact debug font so telemetry can show both scroll pipeline and FPS/player/world-render rows at once.

## Sprint 14.55 - Compact Telemetry Font

Status: done.

Runnable result: telemetry uses a compact debug-only font and can show the full measurement set on one screen.

Done checks:

- Added 4x6-ish telemetry text rendering derived from existing 8x8 glyphs.
- Normal menu/HUD/game text rendering is unchanged.
- Telemetry screen now includes FPS, hitch, session FPS map, scroll miss/wait, scroll/page, desired/ready/active origins, render stage/progress, tile/object/page counts, player Y range, and resources.
- Build remains F5-runnable.

## Sprint 14.56 - Manual Telemetry Font

Status: done.

Runnable result: telemetry uses a hand-authored 4x5 debug font instead of auto-compressing the CPC/menu 8x8 font.

Done checks:

- Added manual glyphs for A-Z and 0-9.
- Added required debug symbols: space, dash, at sign, slash, colon, dot, and fallback question mark.
- Normal game font remains unchanged.
- Build remains F5-runnable.

## Sprint 14.57 - Scroll Catch-Up Render Boost

Status: done.

Runnable result: when scroll is waiting for a not-yet-ready world buffer, the renderer can run controlled catch-up steps in that frame if no player weapon is active.

Done checks:

- Catch-up boost runs only while `telemetryCurrentScrollWait > 0`.
- Catch-up boost is skipped while player rocket or bomb is active.
- Boost runs up to 3 render-job steps per waiting frame.
- Telemetry shows boost frame/step counts on the bottom row.
- Build remains F5-runnable.

## Sprint 14.57.1 - Keep World Renderer Fed During Weapons

Status: done.

Runnable result: the normal world renderer runs one step every game frame again, including while player rockets or bombs are active.

Done checks:

- Removed the hard weapon-active skip around `serviceWorldRenderJob`.
- Kept catch-up boost conservative: extra catch-up steps are still skipped while player weapons are active.
- Fix targets the case where telemetry showed `MISS/WAIT=0`, but `BTP` was low and render was still active, causing background scroll to stop near land/object-heavy sections while missiles and bombs kept moving.
- Build remains F5-runnable.

## Sprint 14.58 - Early World Prefetch / Amiga Scroll Pipeline Step 1

Status: done.

Runnable result: next world-window rendering starts immediately after entering the current scroll page instead of waiting until the camera is close to the page boundary.

Why:

- The old trigger waited until roughly 80 bytes remained before the next world-origin boundary.
- A complete world-window render is multi-stage and spans many tile/object columns, so heavy land/object areas could still starve the next buffer.
- This is the first step toward the proper Amiga approach: scroll cheaply every frame, and stream small world updates far ahead of the visible edge.

Done checks:

- `GAME_WORLD_PREFETCH_TRIGGER_BYTES` now covers the full scroll page, so the inactive buffer starts rendering the next origin as early as possible.
- Existing copper fine-scroll and double-buffer activation logic remains unchanged for safety.
- Sprint 14.57.1's weapon-safe normal render step remains in place.
- Build remains F5-runnable.

Next Amiga-scroll architecture sprints:

1. Replace page-sized world-window swaps with a true circular/ring world bitmap.
2. Stream only newly exposed tile columns into the hidden right edge.
3. Split static terrain, destructible map changes, and active sprites so weapons never compete with terrain streaming.
4. Add telemetry for column queue depth and per-frame column draw budget.

## Sprint 14.59 - Single-Pass World Streaming Renderer

Status: done.

Runnable result: inactive world-buffer rendering now completes each tile column in one pass instead of sweeping the entire buffer through several separate tile/object stages.

Why:

- Telemetry from Sprint 14.58 still showed hard scroll waits over land: `MISS/WAIT` climbed continuously while render sat near the end of a multi-stage job.
- The old renderer did approximately four complete passes over the world window: terrain tiles, base objects, gunship objects, and town objects.
- On A500 this made buffer completion too bursty. The camera could reach the next origin while the inactive buffer was still unfinished.

Done checks:

- `serviceWorldRenderJob` now renders terrain tiles, native/object ranges, and debug overlay for the same column range before advancing.
- Page render time should drop from roughly four sweeps over 130 columns to one sweep over 130 columns.
- Existing copper fine-scroll/double-buffer activation is preserved for a safe testable step.
- Build remains F5-runnable.

Next Amiga-scroll architecture sprint:

- Move from page-oriented double buffering to a true circular/ring world bitmap with hidden-edge column streaming, so scroll never waits for a whole page to become ready.

## Sprint 14.60 - Ring World Scroll Prototype

Status: done.

Runnable result: gameplay scroll uses a circular world bitmap instead of waiting for page-oriented double-buffer origin swaps.

Why:

- Sprint 14.59 telemetry showed the inactive page was rendered (`RENDER=0`, `PAGE=01`) and `MISS=0`, yet the visible background still appeared to stop over land.
- That means page-readiness was no longer the only problem; the page/origin gating itself was still too fragile for smooth A500-style scrolling.

Done checks:

- Added `HAR_USE_RING_WORLD_SCROLL`.
- Added world-column to circular-buffer tile mapping.
- Added hidden-tail duplication for columns near the start of the ring so Amiga bitplane fetch never has to wrap mid-scanline.
- Scroll copper updates no longer wait for `desiredOrigin` while ring mode is active.
- Added per-frame ring streaming of newly exposed world columns.
- Dirty redraws from destroyed ground/ship columns update the ring-mapped physical column too.
- Build remains F5-runnable.

Known risk:

- This is the first ringbuffer pass. If visual corruption appears near wrap boundaries, inspect the duplicated tail columns and `ringWorldLastStreamedColumn`.

## Sprint 14.61 - Ring Stream Ahead + Full Column Objects

Status: done.

Runnable result: ringbuffer streaming now renders ahead of the visible edge and uses the full object range renderer for streamed columns.

Why:

- User observed a pattern of eight small smooth steps followed by a tiny pause, suggesting new columns were being streamed exactly on 8px/tile boundaries.
- User also observed flat terrain where town content should appear. The first ring column renderer only drew direct tile/object-cell content and missed promoted CPC town/gunship/carrier range assets.

Done checks:

- Added `RING_WORLD_STREAM_AHEAD_TILES`.
- Ring stream now keeps 8 tile columns ahead of the display fetch window.
- Initial ring buffer includes the same ahead margin to avoid a first-frame stream burst.
- Ring streamed columns now call terrain tile rendering plus native/object range rendering and overlay rendering.
- Build remains F5-runnable.

Next likely optimization:

- Replace per-stream-column `buildObjectMap` with a true direct column object generator so full object fidelity does not require rebuilding a 130-column ObjectMap.

## Sprint 14.62 - Direct Ring Column Renderer

Status: done.

Runnable result: ringbuffer streaming renders each newly exposed world column directly, without rebuilding the full 130-column `ObjectMap`.

Why:

- Sprint 14.61 restored promoted town/gunship/carrier objects by using the full ObjectMap range renderer for each streamed column.
- That was correct but expensive: each streamed column rebuilt a full ObjectMap.
- A500-style scrolling needs cheap hidden-edge column streaming.

Done checks:

- Added direct range-object scanning for carrier, gunship, and town blocks that can cover the streamed world column.
- Added `renderRingWorldColumnDirect`.
- Ring streaming now uses `objectCellForWorldColumnTile` for base terrain/sea/sky/destructible state plus direct promoted-object range rendering.
- Removed per-stream-column `buildObjectMap` use from ring streaming.
- Duplicate tail columns use the same direct renderer.
- Build remains F5-runnable.

Watch during testing:

- The eight-step micro-stutter should be reduced.
- Town/gunship/carrier content should still appear.
- If content disappears only at wrap edges, inspect direct range-object local tile mapping.

## Sprint 14.63 - Ring Initial Fill / Mapping Correction

Status: done.

Runnable result: ringbuffer startup fill now uses the same direct ring-column mapping as runtime streaming.

Why:

- User observed obvious wrong-world-column corruption: a land cliff appeared in the visible area while the rest of the scene was sea.
- Root cause: Sprint 14.62 mixed the old page renderer for initial fill with the new ring mapping for runtime streaming, so visible columns could contain stale/page-positioned data.

Done checks:

- Added explicit `GAME_WORLD_SCROLL_PAGE_TILES` alias for the ring logical span.
- `initRingWorldBuffer` now clears the world bitmap and fills it through `renderRingWorldColumn`, matching runtime streaming.
- Startup ring state is aligned with the same physical write mapping used after takeoff.
- Build remains F5-runnable.

Watch during testing:

- The obvious stray cliff/land column should be gone.
- If towns or carriers corrupt at wrap boundaries, focus next on direct range-object local tile mapping.

## Sprint 14.64 - Stabilize After Ring Prototype

Status: done.

Runnable result: default gameplay returns to the stable page/double-buffer scroll path while keeping the ring/direct-column code available behind a compile-time flag.

Why:

- Sprint 14.63 proved the first ringbuffer integration still had phase/mapping problems and caused heavy start delay plus obvious sea/land seam corruption.
- The direct ring-column code is useful, but it should be validated in an isolated test path before being used in normal gameplay.

Done checks:

- `HAR_USE_RING_WORLD_SCROLL` is disabled by default.
- Build label bumped to Sprint 14.64.0.
- Existing single-pass page renderer from Sprint 14.59 remains active.
- Ring/direct-column code remains in the source for controlled follow-up work.
- Build remains F5-runnable.

Next ringbuffer plan:

1. Add a dedicated ring-scroll diagnostic mode, not wired into normal gameplay.
2. First validate terrain-only ring columns with no promoted objects.
3. Then validate promoted object clipping one asset family at a time: carrier, gunship, town.
4. Only re-enable ring mode for gameplay after telemetry confirms no phase seams over at least one full wrap.

## Sprint 14.65 - Missing Desired-Origin Render Recovery

Status: done.

Runnable result: if the scroll copper needs a world origin that is not ready, and no world render job is active, the renderer immediately starts building that missing desired origin.

Why:

- Telemetry showed the actual stuck state: `DES=0172`, `RDY=----`, `ACT=0000`, `RENDER=0`.
- That means gameplay was waiting for a missing page while the renderer was idle.
- This was a state-machine hole, not merely an A500 performance problem.

Done checks:

- Added recovery inside the pending scroll-copper update wait path.
- If `desiredOrigin` is missing and `worldRenderJob.active == 0`, start a render job on the inactive world buffer for that exact origin.
- Existing catch-up servicing then has work to do instead of waiting forever.
- Build remains F5-runnable.

Watch during testing:

- `RENDER` should become active after a stuck/missing desired origin instead of staying 0.
- If scroll still pauses, it should recover once the desired page is rendered.
- If it still hard-sticks, inspect why the recovery render is not completing or why `renderedWorldOriginColumns[]` is overwritten.

## Sprint 14.66 - Next-Origin Telemetry and Render Priority

Status: done.

Runnable result: telemetry now exposes next-origin and both world-buffer origins, and the scheduler prioritizes rendering the next needed origin.

Why:

- Sprint 14.65 still showed hangs even when current origin was ready.
- Telemetry at the hang point was close to page boundary (`BTP` very low), but did not show whether the next origin was already in either buffer.
- A stale/inappropriate render job could prevent the next origin from being started early enough.

Done checks:

- Telemetry row now shows `DES RDY ACT NXT`.
- Render row now shows active state, render X, and both buffer origins (`B0`, `B1`).
- `scheduleUpcomingWorldBuffer` now starts/restarts the inactive-buffer render job for the next origin if the currently active render job does not match it.
- Build remains F5-runnable.

Watch during testing:

- Before the page boundary, `NXT` should appear in either `B0` or `B1`.
- If a hang occurs, capture telemetry and compare `NXT` against `B0/B1`.
- If `NXT` is not in either buffer and `RENDER=0`, scheduler still has a hole.
- If `NXT` is in a buffer but visible scroll still stops, the bug is in copper activation/page offset.

## Sprint 14.67 - Incremental ObjectMap Build

Status: done.

Runnable result: starting a world render job no longer builds the entire `ObjectMap` synchronously.

Why:

- Telemetry showed `NXT` already ready in a buffer, but hitches still occurred around page-boundary regions.
- `startWorldRenderJob()` still called `buildObjectMap()` in one blocking chunk.
- That meant each next-page job could begin with a 130-column object/terrain generation spike before incremental rendering even started.

Done checks:

- Added `WORLD_RENDER_STAGE_BUILD_MAP`.
- Added `WORLD_RENDER_MAP_COLUMNS_PER_FRAME`.
- `startWorldRenderJob()` now only resets ObjectMap/job state.
- `serviceWorldRenderJob()` incrementally generates ObjectMap columns before switching to tile/object rendering.
- Build remains F5-runnable.

Watch during testing:

- Boundary hitches should reduce because ObjectMap generation is no longer a single-frame spike.
- Telemetry `RENDER A X` will now show stage 0 during map-building and stage 1 during drawing.
- If pages miss again, increase map/render column budgets or start prefetch even earlier.

## Sprint 14.68 - Prewarm First Gameplay Page

Status: done.

Runnable result: the first post-start scroll page (`origin 0086`) begins rendering immediately when a game session starts, during the carrier/takeoff/ready period instead of waiting until airborne flight.

Why:

- User observed the hitch occurs at the same map position every run.
- Telemetry around that area showed `NXT=0086` and `B1=0086`, which means the first upcoming page is involved.
- Starting that page work only after gameplay/takeoff can make the first land/coast page work happen too close to visible flight.

Done checks:

- After buffer 0 origin 0 is available, start an incremental render job for `nextWorldOriginColumnForScroll(0)` in buffer 1.
- This uses the existing incremental ObjectMap/render pipeline, so it should not introduce a large synchronous start freeze.
- Build remains F5-runnable.

Watch during testing:

- The recurring early-land/page-boundary hitch should move earlier into takeoff/ready or disappear from flight.
- If telemetry still shows the same hitch, compare `B0/B1/NXT` and `RENDER` at the pause.

## Sprint 14.69 - Non-Blocking Page-Wait Fallback

Status: done.

Runnable result: if the desired page is not ready, the copper is still updated using the active buffer with a clamped absolute offset instead of freezing visible scroll until the page arrives.

Why:

- Telemetry showed `DES=0086`, `RDY=----`, `ACT=0000`, `NXT=0172`, `RENDER=1 X=072`.
- The game was waiting for desired page 0086 while render was still working.
- Ringbuffer mode got past this point because it did not wait for a full page.

Done checks:

- Added `updateGameScrollCopperFallback`.
- The wait path now updates copper/fine-scroll against the active buffer with clamped absolute offset and clears the pending scroll update.
- Rendering of the desired page continues in the background.
- Build remains F5-runnable.

Known tradeoff:

- If the desired page is very late, fallback may show the far edge of the current buffer briefly instead of correct new terrain.
- This is a gameplay-smoothness probe and a bridge toward the final ringbuffer/column-stream solution.

## Sprint 14.70 - Remove Fallback Bounce, Increase Render Budget

Status: done.

Runnable result: removes the non-blocking fallback that caused visible 8-step bounce/backtracking and instead gives the page renderer enough budget to finish closer to the boundary.

Why:

- Sprint 14.69 proved the active-buffer fallback could avoid a hard freeze, but it created a visible loop: scroll a few fine steps, clamp, then jump back.
- Telemetry showed the desired page renderer was often close to completion (`X` around 104 of 130), so the better short-term fix is to finish the page faster.

Done checks:

- Removed `updateGameScrollCopperFallback`.
- Removed fallback use in the desired-page wait path.
- Increased ObjectMap build budget from 8 to 16 columns/frame.
- Increased world draw budget from 4 to 8 columns/frame.
- Increased catch-up steps from 3 to 6.
- Build remains F5-runnable.

Watch during testing:

- The bounce/backtracking should be gone.
- If the same page wait remains, telemetry should show whether render reaches `X=130` before or during the wait.

## Sprint 14.71 - Desired Page Emergency Catch-Up

Status: done.

Runnable result: when visible scroll is blocked waiting for the exact page currently being rendered, the renderer receives a short emergency burst and then immediately attempts to activate the completed page.

Why:

- Telemetry still showed `DES=0086`, `RDY=----`, `RENDER=1 X=104`.
- The desired page was close to complete but not finishing quickly enough before scroll wait became visible.
- Previous fallback avoided hard wait but caused visible bounce, so the right short-term fix is to complete the desired page faster.

Done checks:

- Removed weapon-active skip from normal catch-up.
- Added `WORLD_RENDER_WAIT_CATCHUP_STEPS`.
- Added `serviceDesiredWorldRenderCatchup`.
- Wait path now performs desired-page catch-up, retries activation, and updates copper in the same frame if the page completes.
- Build remains F5-runnable.

Watch during testing:

- The hard wait should be shorter or disappear at the first land/page boundary.
- `BOOST STEP` will rise more during waits.
- If it still stalls with `X` stuck at the same value, inspect object drawing for the column range around that `X`.

## Sprint 14.72 - Desired Page Priority Over Next Page

Status: done.

Runnable result: when scroll is waiting for the desired page, that desired page cannot be preempted by rendering the later next page.

Why:

- Telemetry showed `DES=0086`, `NXT=0172`, and render progress reset/changed while `0086` was still not ready.
- The scheduler could start rendering `NXT` even while the copper update was still blocked waiting for `DES`.
- This made scroll wait for page 0086 while the renderer worked on page 0172.

Done checks:

- Wait path now restarts/keeps the render job for the missing desired origin even if another render job is active.
- `scheduleUpcomingWorldBuffer` is skipped while a pending scroll copper update is unresolved.
- Build remains F5-runnable.

Watch during testing:

- While `DES=0086` and `RDY=----`, render should continue toward completing 0086 instead of switching attention to `NXT=0172`.

## Sprint 14.73 - Triple World Page Buffering

Status: done.

Runnable result: world scrolling now has three page buffers so the engine can keep current, next, and next-next pages in flight at the same time.

Why:

- With two buffers, the engine could hold `current=0000` and `next=0086`, but had nowhere to render `next-next=0172` until after switching to 0086.
- This recreates the same page-late problem at every boundary.
- Ringbuffer mode got past the stuck point because it did not have the “only one future page” limitation.

Done checks:

- Increased `GAME_WORLD_BUFFER_COUNT` to 3.
- Allocates, frees, and debug-registers `world_buffer_2`.
- Telemetry now shows `B0`, `B1`, and `B2`.
- Desired-page activation searches all buffers.
- Scheduler renders `next-next` once `next` is already ready.
- Removed remaining world-scroll `active ^ 1` assumptions.
- Build remains F5-runnable.

Watch during testing:

- Before the first boundary, telemetry should ideally show `B0=0000`, `B1=0086`, and `B2=0172` or an active render toward 0172.
- If it still stalls, capture telemetry and compare `DES/NXT` against all three buffer origins.

## Sprint 14.73.1 - Optional Third Buffer

Status: done.

Runnable result: the game starts even if there is not enough chip RAM for the third world buffer.

Why:

- Triple buffering can exceed available A500 chip memory depending on Kickstart/debug/tooling allocation layout.
- User reported the game no longer started after Sprint 14.73.

Done checks:

- Third world buffer allocation is optional.
- Runtime uses `activeWorldBufferCount`.
- Buffer-origin searches and dirty redraw loops only inspect allocated buffers.
- Telemetry shows `B2=----` when the third buffer is unavailable.
- Build remains F5-runnable.

## Sprint 14.73.2 - Hard Rollback to Two World Buffers

Status: done.

Runnable result: removes runtime use/allocation of the third world buffer so the game starts again under the current A500 memory layout.

Why:

- Even optional third-buffer allocation caused startup to stop at the AmigaDOS shell in the current setup.
- The page-buffer idea is still useful, but not viable in the current memory layout without reducing world buffer size or freeing other chipmem.

Done checks:

- `GAME_WORLD_BUFFER_COUNT` is back to 2.
- Removed `worldBuffers[2]` allocation/free/debug registration.
- Removed `B2` telemetry.
- Build remains F5-runnable.

Next architecture direction:

- Do not pursue triple-buffering until chipmem budget is reduced.
- Revisit ringbuffer in an isolated terrain-only diagnostic, or reduce world-buffer dimensions before adding another page.

## Sprint 14.74 - Render/Page Flight Recorder

Status: done.

Runnable result: telemetry now includes a small render/page event recorder so we can diagnose the scroll hang from event history instead of only the final stuck state.

Why:

- Repeated small fixes did not solve the recurring page-boundary hang.
- We need to see why the desired page is missing/late: prewarm failure, scheduler restart, wait recovery, completion, or activation issue.

Event codes:

- `1` = prewarm render start
- `2` = scheduler render start
- `3` = wait/recovery render start
- `4` = render complete
- `5` = buffer activate

Done checks:

- Added latest and previous render/page event rows: `E0` and `E1`.
- Each event shows code, buffer, origin, scroll, and render X.
- `startWorldRenderJob`, render completion, and buffer activation now log events.
- Build remains F5-runnable.

Next diagnosis:

- At the hang, inspect `E0/E1` to see whether page 0086 is repeatedly restarted, completed but not activated, or never completed.

## Sprint 14.75 - Guard Second Scheduler During Page Wait

Status: done.

Runnable result: the second scheduler call at the end of the frame loop no longer overwrites a desired-page recovery render while a scroll-copper update is still waiting.

Why:

- Flight recorder proved the loop:
  - `E1 code 3`: wait/recovery started rendering desired page `0086`.
  - `E0 code 2`: scheduler immediately restarted the same buffer for `0172`.
- There were two scheduler call sites. Only the early one was guarded by `!pendingGameScrollCopperUpdate`; the late one still ran unconditionally.

Done checks:

- Added the same `!pendingGameScrollCopperUpdate` guard around the late `scheduleUpcomingWorldBuffer` call.
- Build remains F5-runnable.

Watch during testing:

- At the stuck point, `E0/E1` should no longer show code `2` for `0172` immediately after code `3` for `0086`.

## Sprint 14.76 - Landing Crash Diagnostics and Deck Tolerance

Status: done.

Runnable result: landing/deck detection is slightly more forgiving, and player crash/flak collision reasons are logged into the telemetry event recorder.

Why:

- User reached the end after the scroll fix and crashed.
- Need to distinguish an intended bad landing from a mismatch between visual carrier deck and collision deck.

Done checks:

- Event code `6` logs fatal player collision/crash with world column, scroll, and player Y.
- Event code `7` logs flak collision/damage with world column, scroll, and player Y.
- Native carrier deck pixel tolerance expanded around `CARRIER_DECK_PIXEL_Y`.
- Build remains F5-runnable.

Watch during testing:

- If landing still crashes, telemetry `E0/E1` should show code `6` and the world column/Y where collision happened.
- Compare that column against final carrier span 667-680 and deck Y around 105.

## Sprint 14.77 - Mission Complete Landing State

Status: done.

Runnable result: landing on the final carrier at low speed marks the mission as landed and stops forward scrolling.

Why:

- User landed successfully but the plane stopped at the edge while the carrier service/refill logic restored full fuel, rockets, and bombs.
- Friendly carrier refill is expected, but final landing needs an explicit state so the game does not keep scrolling off the deck.

Done checks:

- Added `missionComplete` to `GameState`.
- Landing on a native carrier deck near the final route at speed <= 3 sets mission complete and speed 0.
- HUD displays `LANDED`.
- Throttle and forward scroll stop while mission complete.
- Existing refill still restores fuel/armour/rockets/bombs on carrier deck.
- Build remains F5-runnable.

Next optimization note:

- User reports a smaller recurring hitch roughly every two seconds. Keep flight recorder for now, then profile whether periodic page render starts, ObjectMap builds, HUD swaps, or audio updates line up with that cadence.

## Sprint 14.78 - Hitch Snapshot and Smoother Normal Render Budget

Status: done.

Runnable result: telemetry now records the latest hitch context, and normal background render work is reduced back to smaller per-frame chunks while keeping emergency catch-up available for real page waits.

Why:

- User reports smaller recurring hitches roughly every two seconds after the major page-wait bug was fixed.
- Previous budget increases helped page completion but likely created periodic CPU spikes.

Done checks:

- Added hitch snapshot fields: delta, render active/stage/origin/X, and scroll.
- Telemetry bottom row now shows `HCH D A S ORG SCR X`.
- Reduced normal ObjectMap build budget from 16 to 8 columns/frame.
- Reduced normal world draw budget from 8 to 4 columns/frame.
- Kept desired-page emergency catch-up in place for actual page waits.
- Build remains F5-runnable.

Watch during testing:

- If hitches remain, capture telemetry and inspect `HCH`:
  - `A=1/S=0` suggests ObjectMap build spike.
  - `A=1/S=1` suggests world drawing spike.
  - `A=0` suggests non-render sources such as HUD/audio/collision.

## Next Implementation Slice

## Sprint 14.79 - Harrier Sprite Priority Over Carrier

Status: done.

Runnable result: the gameplay copper now gives hardware sprites priority over the scrolling game playfield, so the Harrier should draw in front of the carrier during landing instead of slipping visually behind it.

Why:

- User noticed that the Harrier was rendered behind the carrier after landing.
- This is a visual priority issue between Amiga hardware sprites and bitplane playfield graphics, not a landing-state issue.

Done checks:

- Added a gameplay `BPLCON2` priority setting for sprites over the game world.
- Applied it to the active HUD/game copper and the legacy game-scroll copper path.
- Reset `BPLCON2` back to normal at the HUD split.
- Build remains F5-runnable.

Watch during testing:

- Harrier should remain visible over the carrier deck/superstructure while landing.
- HUD should remain unchanged.
- If enemy sprites now appear in front of terrain where CPC expected them hidden, we may need per-sprite masking or a gameplay-specific exception later.

Follow-up:

- `14.79.1` exposed HUD corruption after the sprite-priority change. The likely cause is that the active game copper list was too close to the old 1024-byte allocation after adding extra split-time sprite commands.
- `14.79.2` increased `COPPER_BYTES` to 2048 so the gameplay copper has room for gradient, sprite pointers, playfield pointers, HUD split, and terminator.

## Sprint 14.80 - Smoothness Pass: Remove Duplicate Flush and Reduce Engine Mutation Cost

Status: done.

Runnable result: one-frame CPU spikes should be slightly reduced without changing gameplay.

Why:

- User captured a hitch after a small lug. Telemetry showed `HCH` with render inactive (`A=0`), so the hitch is likely outside the page renderer.
- Game loop still performed an early pending sprite/HUD flush and a second consolidated flush in the same frame.
- Engine audio mutation touched 128 sample bytes every frame, which gives non-render CPU cost not visible as active world rendering.

Done checks:

- Removed the early duplicate sprite/HUD pending flush; all gameplay visual pending work now flushes once in the bottom-of-frame section.
- Reduced engine sample mutation slice from 128 bytes/frame to 48 bytes/frame.
- Kept `COPPER_BYTES` at 2048 from the 14.79.2 hotfix.
- Build remains F5-runnable.

Watch during testing:

- Telemetry build label should show `SPRINT 14.80.0`; if not, VS Code/WinUAE is still running an old binary/session.
- If hitches continue and `HCH A=0`, next suspects are input/CIA keyboard reads, SFX channel updates, or collision/object scans.
- If hitches show `A=1`, use `HCH S/ORG/SCR/X` to identify map/object/tile render phase.

## Sprint 14.81 - Ring World Scroll: Fix Wraparound Overwrite

Status: done.

Runnable result: ring-buffer scroll mode no longer overwrites terrain tiles that are still visible on screen; the "landscape appears in the wrong place" corruption is gone.

Why:

- Sprint 14.61 added `RING_WORLD_STREAM_AHEAD_TILES=8` on top of a `neededColumn` formula of `leftColumn + GAME_WORLD_SCROLL_PAGE_BYTES + RING_WORLD_STREAM_AHEAD_TILES - 1`. That streams up to 94 world-tiles ahead of the display, but the ring buffer's physical storage is only one page period (86 tiles).
- Streaming further ahead than one period wraps around and overwrites physical tile positions that map to world columns still inside the visible fetch window this frame. This is a different bug from Sprint 14.63's init/runtime mapping mismatch (which remains fixed); this one is a genuine capacity overrun in the steady-state streaming formula, not a one-time startup corruption.
- Confirmed by walking concrete numbers: at `leftColumn=1000` (display start), the formula streamed as far as world column 1093, which maps to a physical tile position inside the currently-displayed physical window - i.e. it overwrote on-screen content with a future column's data before the display had scrolled past it.

Tasks:

- Capped ring look-ahead to `leftColumn + GAME_FETCH_BYTES + RING_WORLD_STREAM_AHEAD_TILES - 1`, comfortably under one ring period instead of almost exactly one period.
- Simplified `initRingWorldBuffer`'s fill range to exactly one page period (`GAME_WORLD_SCROLL_PAGE_TILES`) so startup no longer double-writes over itself before gameplay begins.

Done checks:

- Headless perf harness (Sprint 14.84) shows no page-boundary freeze signature in `maxVblDelta`.
- User confirmed the wrong-place landscape corruption is gone after this fix, while the underlying scroll motion stayed smooth.

## Sprint 14.82 - Ring World Scroll: Speed-Proportional Stream Budget

Status: done.

Runnable result: the small per-tile "someone tapping the gas" pulse from ring streaming is gone; scroll feels continuously smooth at every speed level.

Why:

- Sprint 14.61 already found this exact symptom ("a pattern of eight small smooth steps followed by a tiny pause... on 8px/tile boundaries") but worked around it only by adding a fixed stream-ahead margin. It never addressed the actual per-column cost.
- A single ring column is 25 rows tall; each row needs a terrain/object lookup plus a non-blitter tile draw. Doing all 25 rows synchronously in the one frame a new column becomes due is a real, repeatable per-frame CPU spike tied exactly to the 8px tile cadence.
- A first attempt at fixing this (fixed fixed-size row budget per frame, gated on "is a new column due yet") reduced the spike size but reintroduced a busy/idle rhythm: spend budget for a couple of frames, then do nothing until the next column is due. The user described this as pulsing/uneven ("as if someone is tapping the gas").

Tasks:

- Split per-column rendering into row-sized chunks (`renderWorldColumnRowsDirect`) instead of a whole 25-row column at once.
- Size the per-frame row budget proportionally to the *current* scroll speed (`scrollPixelsForSpeedLevel`) rather than a fixed constant, so production continuously tracks demand instead of bursting or idling: `rowBudget = ceil(scrollPixels * GAME_OBJECT_MAP_HEIGHT_TILES / GAME_TILE_WIDTH)`.
- Kept `RING_WORLD_STREAM_MAX_AHEAD_TILES` only as a safety backstop (comfortably under one ring period) for edge cases like a crash freeze where `scrollX` stops changing entirely.

Done checks:

- Perf CSV shows `maxVblDelta` pinned at a flat 4-5 across an entire ~90s run (vs 19 at every page turnover in double-buffer mode, and 7-9 with the earlier fixed-budget ring version).
- User confirmed the pulsing/uneven feel is gone ("Mye bedre!").

## Sprint 14.83 - Ring World Scroll Is Now The Default

Status: done.

Runnable result: `HAR_USE_RING_WORLD_SCROLL` defaults to 1; normal gameplay uses the ring/circular world buffer instead of page-oriented double buffering.

Why:

- Sprints 14.65-14.80 spent sixteen sprint entries chasing the double-buffer page-wait hitch (prewarm, incremental ObjectMap build, catch-up boosts, triple buffering reverted for chip RAM budget, flight recorder telemetry) and never fully eliminated it - Sprint 14.80 still ends with "if hitches continue...".
- With both ring-mode bugs above fixed, ring mode has no page-boundary wait at all by construction (there is no "origin swap" moment), and the headless perf harness confirms a flat ~4-5 vbl worst case with no periodic spike, replacing the page-oriented double buffer's reliable 19-vbl (~380ms) freeze at every page turnover.

Tasks:

- Flipped `HAR_USE_RING_WORLD_SCROLL` default to 1.
- Kept the page/double-buffer path fully compiled in behind the flag as a fallback; no code was deleted.

Watch during testing:

- The extensive page-buffer telemetry (`B0`/`B1`/`B2`, `DES`/`RDY`/`ACT`/`NXT`, scroll miss/wait, flight recorder `E0`/`E1`) is specific to the non-ring path and will show stale/inactive values while ring mode is active.
- A later sprint should either hide those telemetry rows in ring mode or add ring-specific telemetry (streamed column/row progress, cushion depth ahead of the display).

## Sprint 14.84 - Headless WinUAE Perf Harness

Status: done.

Runnable result: scroll-smoothness changes can be verified with objective frame-timing numbers from a scripted WinUAE run, without a human watching the screen or pressing keys.

Why:

- Sprint 14.41.1/14.41.2 already tried writing perf rows straight to AmigaDOS via `DH0:logs/harrier_perf.log` and hit repeatable hangs on the first 10-second write, so that session reverted to `KPrintF`-only console output and left the root cause undiagnosed.
- The actual root cause: `TakeSystem()` calls `Forbid()` and `Disable()` for the entire game session (only undone in `FreeSystem()` at normal program exit). Any blocking `dos.library` call (`Open`/`Write`) made from inside the game loop needs the filesystem handler task to be scheduled to reply, which `Forbid()`/`Disable()` makes impossible - it deadlocks, and AmigaOS eventually surfaces that as the "Software error - task held - Finish ALL disk activity" alert. `KPrintF` never had this problem because it goes through `RawPutChar` (`exec.library`), a synchronous call with no task hand-off.

Tasks:

- Buffer perf rows in a static RAM array (`perfLogBuffer`) during play; only do the one-shot `Open`/`Write`/`Close` in `perfLogFlushToDisk()`, called after `FreeSystem()` has restored multitasking.
- Added `HAR_HEADLESS_AUTOPLAY` to script a self-playing session (one-shot start, one-shot takeoff, hold climb to avoid terrain, auto-quit via `break` after a fixed frame budget) so a run can complete unattended and actually reach the normal shutdown path.
- Documented the full methodology, the deadlock root cause, and how to interpret the CSV in `amiga/HEADLESS_TESTING.md`, with the working `.uae` config checked in at `amiga/harrier_headless.uae`.

Done checks:

- Multiple full ~90s headless runs complete cleanly and write `amiga/out/perf_log.csv`.
- Both `HAR_DEBUG_PERF_LOG` and `HAR_HEADLESS_AUTOPLAY` default to 0 for normal builds.

## Sprint 14.85 - Remove Double-Buffer Page Renderer

Status: done.

Runnable result: no behavior change; the double-buffered page/`ObjectMap`-cache scroller that ring mode replaced in Sprint 14.83 is now gone from the source instead of sitting behind a flag.

Why:

- Sprint 14.83 made ring-buffer scroll the only default but kept the entire page-render pipeline compiled in as an inactive fallback.
- With ring mode confirmed working end-to-end (Sprints 14.81/14.82) and the user asking directly to clean up now that it works, the fallback was pure dead weight: a second `GAME_WORLD_BITMAP_BYTES` world buffer permanently allocated in chip RAM (~106.6 KiB, `168 * 5 * 130` bytes) for a code path that could never run.

Tasks:

- Removed the whole page-render pipeline: `startWorldRenderJob`, `serviceWorldRenderJob`, `chooseWorldRenderBuffer`, `tryActivateDesiredWorldBuffer`, `scheduleUpcomingWorldBuffer`, `serviceWorldRenderCatchupIfNeeded`, `serviceDesiredWorldRenderCatchup`, `worldOriginForGameState`, `maxWorldOriginColumn`, `renderWorldBufferImmediate`, `cancelWorldRenderJob`, `worldRenderJobMatches`, `readyOriginForDesired`, `worldBufferIndexForOrigin`.
- Removed the `ObjectMap` cache it rendered from - `buildObjectMap`, `generateObjectMapColumn`, `applyLevelObjectsForColumn`, `setObjectIfOnMap`, `setObjectCell`, `objectMapCell`/`objectMapCellConst`, `renderObjectMapTiles`/`renderObjectMapTileColumns`, `drawObjectMapNativeObjects`/`*Range`, `drawObjectMapOverlayIfEnabled`/`*RangeIfEnabled`, `objectMapOverlayColor`, `promotedCarrierCoversDeckAt` - none of it was reachable once ring streaming (which queries `objectCellForWorldColumnTile` directly, per column, with no cache) became the only scroller.
- Removed `worldOriginColumnForScroll`, `nextWorldOriginColumnForScroll`, `bytesUntilNextWorldOrigin`, the `WorldRenderJob`/`ObjectMap` struct types and their instances, `renderedWorldOriginColumns[]`, `activeWorldBufferCount`, and the associated `WORLD_RENDER_*`/`GAME_WORLD_PREFETCH_TRIGGER_BYTES` defines.
- Dropped `GAME_WORLD_BUFFER_COUNT` from 2 to 1 and removed the second world buffer's allocation, null-check, debug bitmap registration, and free.
- Removed the now-fully-unconditional `HAR_USE_RING_WORLD_SCROLL` flag itself, along with every `#if`/`#else` branch that used to pick between the two scrollers.
- Kept the debug telemetry screen's fields for the removed concepts (scroll miss/wait, `DES`/`RDY`/`ACT`/`NXT`/`B0`/`B1`, flight recorder `E0`/`E1`, render job stage/tileX) rather than redesigning that UI in the same pass; they now read fixed/neutral values (repurposed to ring-stream progress where cheap, zero otherwise) instead of being deleted outright. A later sprint can trim or replace them.

Done checks:

- Clean rebuild from scratch compiles with no errors.
- Headless perf harness run after the removal completes normally and produces a sane `perf_log.csv`.
- `amiga/out/harrier_amiga.exe` allocates one world buffer instead of two.

## Sprint 14.86 - CPC-Inspired Procedural Land Height Generator

Status: done.

Runnable result: the long land run (world columns 106-400) no longer repeats an identical 128-column pattern; height comes from a per-column procedural generator modeled on the CPC's own stage-3 dispatcher instead of a short hand-authored table.

Why:

- The plan's own Sprint 14.9 notes flagged the long land section as "too flat" and named the exact next step: trace `drawflatterrain`, hill generation, and the R-register/random-height logic in the CPC source.
- Static tracing (`HarrierAttackSourceNew2_alt_CRTC_CART16.asm:5423-5505`) found the mode selector byte (`l8859`, commented "COUNTDOWN TO HILLS") is read from 4 places but **never written anywhere in this source** - no direct write, no IY/IX-indexed write near it (the nearby "reuse block for wingman" IY addressing applies to a different variable block, starting at line 223, not this one).
- Live verification resolved the ambiguity conclusively, using the actual compiled cartridge (`compile/build/HarrierAttackReloaded.cpr`) in WinAPE:
  - A WinAPE snapshot taken at the menu (`gamelevelprogress=0`) showed `l8859`/`l885d` still at zero - inconclusive, game hadn't reached the land stage yet.
  - A snapshot taken mid-flight over land (`gamelevelprogress=4`, i.e. caught mid-way through the mode-3 "insert ground target" branch) showed `l885d` (the live terrain height) at `13`, one step off the `14` baseline - proof the height-changing modes really do fire during actual play.
  - To settle it beyond one static sample, a throwaway modded build (source copied outside the repo, never committed - see below) patched the mode dispatcher to flash the CPC border colour per mode (black=flat, red=toward baseline, green=toward floor, yellow=insert target) without touching any register/flag the surrounding code depends on (saves/restores AF/BC/HL/DE around the patch). Running it in WinAPE, the user confirmed the border "flashes strongly the whole time over land" - the mode cycles continuously and vigorously in real play. The exact mechanism that mutates `l8859` remains unresolved in the source (most likely a computed-address write via a neighboring label, or possibly a stack/RAM-layout coincidence), but that no longer matters: the mode is proven to vary continuously during real play, which is exactly what needed replicating.

Tasks:

- Added `CPC_LAND_PROCEDURAL_LENGTH`/`BASELINE`/`FLOOR` constants and `generateCpcLandHeightTable()`/`cpcLandProceduralProfile()` (`amiga/main.c`, right before `terrainYForWorldColumn`).
- The generator walks the full 295-column land segment once (lazily, on first query, since callers - e.g. neighbour lookahead for slope tiles - query arbitrary/non-sequential columns and can't drive a live per-frame walk), using a small deterministic PRNG (16-bit LCG) to stand in for the CPC's own not-fully-traceable mode source, and the same three-way mode logic found in the CPC dispatcher: flat / step toward baseline (14) / step toward floor (11, matching the CPC's difficulty-1 floor).
- Replaced the old hand-authored 128-entry `cpcLandProfile[]` lookup table with this procedurally generated one, keeping the same call site/signature so nothing else needed to change.
- Ground target/flak placement (`level_route.h`) is intentionally left untouched by this sprint - the CPC entangles target spawning with the same mode dispatcher, but this port already treats targets as a separate static table (Sprint 14.9's density pass), and merging the two systems is a bigger, separate change than "make the height less flat."
- Debug artifact (not part of the repo): a modded copy of `HarrierAttackSourceNew2_alt_CRTC_CART16.asm` plus a border-flash patch was assembled into `HarrierAttackReloaded_DEBUG.cpr` entirely under the session scratchpad directory, never copied into the repo or the tracked `.tools/winape` install.

Done checks:

- Clean rebuild compiles with no errors.
- Headless perf harness run over the land section completes normally (no crash/hang) with the new generator active.
- Simulated table distribution (Python, throwaway) shows height varying across the full 11-14 range with no exact repeat over 295 columns, versus the old table's exact 128-column period.
- User confirmed via the live CPC border-flash build that the real game's mode dispatcher is continuously active over land, validating the chosen approach.

## Sprint 14.87 - Real CPC Palette Extraction

Status: done.

Runnable result: the game tile palette (`amiga/assets/game_palette.pal`) is now derived from the actual CPC source's real hardware colours instead of hand-picked hex guesses, produced by an updated conversion tool.

Why:

- User compared a CPC+ screenshot against the Amiga port and asked whether we extracted from base CPC instead of CPC+, since the CPC+ visuals looked richer.
- Traced the real mechanism: the game runs almost entirely on **4 dynamically-rewritten ASIC palette registers** (pens 0-3), rewritten every frame by raster-timed interrupts (`fiftiethofasecondinterrupt2/3/5/6`, `HarrierAttackSourceNew2_alt_CRTC_CART16.asm:8363-8439`) copying from small master tables (`setasicpalettegame`/`setasicpalettemenu`, `:9302-9315`) straight into the CPC+ ASIC's memory-mapped palette at `&6400` via `ldi`. This is what produces the vertical sky-gradient look (`palettemainscreen`/`palettesky`/`palettesky2`, three slightly different shades for three screen bands) plus a separate 4-colour instrument-panel set (`paletteinstruments`).
- Also found a full day-to-dusk-to-night 20-keyframe palette fade table (`palettefadetable`, `:9374-9399`) that isn't ported to the Amiga side yet - noted for a later sprint, not touched here.
- Checked which CPC tile-pen values (0-15) actually appear in the extracted tile data (`cpc_tiles_contact_sheet`) - found the full 0-15 range is used, but no code path anywhere sets ASIC palette registers for pens 4-15; only the 4 dynamic ones are ever written. Where those other pens' colours come from (a DMA-driven palette list the disassembly wouldn't show as individual colour writes, or simply hardware default/unused) remains an open question - out of scope to resolve further for this pass.

Tasks:

- Added `extract_palette_words()` to `tools/cpc_game_tiles_to_amiga.py`: pulls `defw &XXXX` colour words following a given label directly from the CPC game-logic source, reusing the file's existing regex-based extraction style.
- `write_palette()` now takes the real extracted sky/land/black/sea colours (from `palettegamemaster`) for palette slots 0/5/10/15, and the real instrument-panel colours (from `paletteinstruments`) into slots 28-31. The remaining slots (white/grey/brown/roof/etc., which have no CPC source equivalent since the game never explicitly sets those pens) stay as clearly-commented Amiga-native approximations.
- `main()` gained a `--palette-input` argument (default `HarrierAttackSourceNew2_alt_CRTC_CART16.asm`) and prints which case applied (real values found vs. approximation fallback) so a future source change that breaks the parser fails loudly instead of silently reverting to guesses.
- Regenerated `amiga/assets/game_tiles.bpl`, `game_scene.map`, `game_palette.pal`, and the preview BMP; rebuilt the Amiga game end to end to confirm the new tool-driven assets link cleanly.

Done checks:

- Tool run prints "sky/land/black/sea from real CPC palettegamemaster", confirming the parser found and used the real table rather than falling back.
- Clean full rebuild (`amiga-build.ps1`) succeeds with the regenerated assets.
- The four confirmed-real colours (`0x0A7F`/`0x0A60`/`0x0000`/`0x0009`) match what was already hardcoded before this change - so no visible colour shift from this sprint alone, but the pipeline is now traceable to source instead of a hand-copied magic number.

Follow-ups (not done):

- Locate where CPC pens 4-15 actually get their colours (if anywhere) - may need SNA/ASIC-state binary archaeology or a documented CPC+ SNA spec reference, since no source-level write was found.
- Port the day/dusk/night palette fade table for a closer visual match during longer missions.

## Sprint 14.88 - Real CPC+ Sprite Palette via Attached Player Sprite

Status: done.

Runnable result: the player Harrier hardware sprite now shows up to 15 real CPC+ colours (grey/red/brown/white ramp) instead of 3 hand-picked ones, using the exact palette the real game's ASIC hardware sprites use.

Why:

- Following on from Sprint 14.87, traced where CPC+ hardware sprites (as opposed to the screen bitplanes) get their colours from. Found `sprite_colours` (`HarrierAttackSourceNew2_alt_CRTC_CART16.asm:9537-9553`): a single, static, 15-entry hardware sprite palette, copied once at boot into ASIC palette registers `&6422+` ("STEP 3 - Setup sprite palette... The sprites use a single 15 entry sprite palette... different to the screen palette", comment at `:385-390`). Every CPC+ hardware sprite in the game (Harrier, enemy plane, gunship, carrier, parachute) shares this one palette - there is no per-sprite custom colour table.
- This also explained why the earlier `cpc_plus_sprites_contact_sheet`/`cpc_combat_sprites_audit` previews looked colourful (green enemy plane, orange Harrier): `extract_cpc_assets.py`'s preview renderer colours pen values with a generic `RGB_PALETTE` picked purely to make different pens visually distinguishable in a debug contact sheet (`tools/extract_cpc_assets.py:669`, `color = RGB_PALETTE[pen & 0x0F]`) - it was never the real in-game palette. The real CPC+ sprites are actually a muted grey/red/brown/white ramp.
- The existing player sprite (`cpcPlusPenToPlayerHardwareColor`) reduced the CPC+ pixel data's real pen values (0-15) down to 3 arbitrary colour buckets to fit a normal 2-bitplane OCS hardware sprite. To show the real 15-colour ramp, the player needed to become an **attached sprite pair** (4 bitplanes).

Tasks:

- `tools/cpc_game_tiles_to_amiga.py`: `extract_palette_words()` (added in Sprint 14.87) now also pulls `sprite_colours`' 15 entries; `write_palette()` writes them into Amiga palette colours 17-31 (colour `16+pen` for CPC+ sprite pen 1-15), the same register block an Amiga attached sprite pair uses for its combined 4-bitplane pixel value - so no format conversion was needed beyond the existing 0GRB-to-Amiga-RGB helper.
- `amiga/main.c`: added `buildAttachedSpriteFromCpcPlusHalves()`, which builds two hardware sprite buffers from the raw CPC+ pen data (no more `mapPen` reduction) - the main buffer gets bitplanes 0-1 (low 2 pen bits), a new `playerAttachSprite` buffer gets bitplanes 2-3 (high 2 pen bits) with the OCS/ECS attach bit (`SPRxCTL` bit 7) set.
- Freed hardware sprite channel 1 for this attach role by moving the rocket to channel 5 (previously unused - only channels 0-4 were wired into the copper list). `copSetSprites()`/`buildDisplayCopperEx()`/`buildGameHudCopper()` updated accordingly; removed the now-dead `buildGameScrollCopper()` (zero callers, same cleanup rationale as Sprint 14.85).
- `playerAttachSprite` threaded alongside `playerSprite` through `startGameSession()`, `updatePlayerSprite()`, allocation/free/hide-on-pause in `main()`. Non-attached rendering paths (crash debris, game-over) explicitly hide the attach sprite so channel 1 cleanly reverts to a normal, non-combined sprite when the full 15-colour art isn't in use.
- Removed `cpcPlusPenToPlayerHardwareColor()` (now unused - the raw pen value is used directly instead of being reduced).

Known consequence (not a bug): OCS/ECS attached sprite pairs use the *entire* 16-31 colour block for their combined pixel value - there is no separate "spare" block left for the other pairs once one pair is attached. Rocket, bomb, enemy plane, and enemy missile (still normal 2-bitplane sprites on their own channels) now read colours from whatever position in this same shared 15-colour ramp their local register block happens to overlap, rather than independently-chosen hues. This mirrors the real CPC+ hardware exactly (one shared sprite palette for everything) rather than being a compromise, but it does mean those sprites' exact colours may need individual tuning in a follow-up if they look wrong once actually seen in motion.

Done checks:

- Clean rebuild compiles with no errors.
- User confirmed visually in WinUAE: "looks good so far."
- Headless perf harness run completes without crash/hang after the hardware sprite channel remap.

## Sprint 14.89 - Ground Target Density, Procedural Terrain Generator

Status: done.

Runnable result: ground targets (radar/gun/missile launcher/tank) now appear at the same density as the real CPC, sit correctly on the terrain surface instead of floating a row above it, and no longer have a coloured flag-style marker overlay drawn on top.

Why:

- User noticed the Amiga port's ground targets were much sparser than the real CPC+ game. Traced the real placement logic (mode-3 dispatch in the stage-3 land dispatcher, `HarrierAttackSourceNew2_alt_CRTC_CART16.asm:5423-5505`) and confirmed via a live CPC+ border-colour debug patch (built outside the repo, per user's instruction not to commit throwaway diagnostic builds) that the mode genuinely cycles continuously during real play - not dead code.
- Screenshots showed targets floating above the ground surface and a distracting coloured flag/marker overlay that didn't match the real game.

Tasks:

- Added `generateCpcLandHeightTable()`/`cpcLandProceduralProfile()`/`cpcLandProceduralTarget()` (`amiga/main.c`) - a precomputed table mirroring the real dispatcher's height random-walk and target-type selection, debounced so two target columns in a row don't both place one.
- Wired procedural targets into `objectCellForWorldColumnTile()`.
- Changed target row offset from `terrainY - 2` to `terrainY - 1` per visual feedback (targets were floating).
- Removed `drawGroundTargetMarkerAt()` and its call site entirely per user request ("ser rart ut").

Done checks:

- Headless run completes without crash; collision/destruction still works.
- User confirmed density and placement visually in WinUAE.

## Sprint 14.90 - CPC Tile Palette Correction and Terrain Slope Fidelity

Status: done, with known deferred follow-ups (carrier tint, gun-tile clarity, true sub-tile height resolution - see below).

Runnable result: ground renders as a solid green with a subtle anti-aliased edge (previously grey/black shimmer or, briefly, an over-corrected flat green with no texture at all); ground targets and buildings read as a black-or-transparent silhouette instead of two-tone grey; HUD text and buildings no longer collapse to pure black; hill silhouette advances through a connected multi-column slope instead of flickering randomly between unrelated tile variants every column.

Why:

- A prior attempt (outside this sprint) blanket-blacked every "unconfirmed" tile palette slot. This regressed HUD digit text and buildings/carrier to solid black and still hadn't produced a clean ground. The user suspected the CPC Mode 0 pixel-decoding logic itself might be misinterpreting bits.

Tasks:

- Verified `decode_mode0_byte()`'s bit-interleave against the real CPC Mode 0 hardware layout by hand-decoding named reference tiles (`AMSTRADFONT3.asm`) - confirmed correct; the decoder was never the bug.
- Discovered each sprite entry in `AMSTRADFONT3.asm` is actually 16 bytes across 2 `db` lines, not 8 - an earlier read had silently only captured half the pixel data for several tiles, producing incomplete/misleading manual decodes.
- Found the real static tile-content pen mapping is sky=0, land=5, black=10, sea=15 (confirmed by hand-decoding the "0 - SKY", "1 - LAND", "3 - SEA 1" reference tiles) - and that this is a *different* thing from `palettemainscreen`/`paletteinstruments` (`:9319-9339`, `:8363-8429`), which only ever write ASIC pens 0-3 as part of a separate per-scanline raster-split effect (a sky brightness gradient plus a brighter HUD-band recolour of pen 1). Conflating the two was the root cause of the regression.
- Unified pens 1 and 4 (hill/grass edge dither, confirmed via full 16-byte decode of "24 - HILL UP 1" and "33 - GRASS 1") to the *exact* value of land rather than a distinct "AA" hue - a distinct hue just repainted the dither as a visible off-colour patch; unifying it lets the tile data's genuine per-pixel staircase blend into the silhouette instead.
- Unified pens 2 and 8 (target/building silhouette accent bits, confirmed via RADAR/GUN/TANK/TOWER BLOCK decodes) to the same black as pen 10, matching a flat black-or-transparent silhouette confirmed against WinAPE.
- Investigated the real terrain dispatcher (`l9134`/`l9167`/`l9181`, `:5423-5497`): confirmed the real game also picks hill-tile variants with plain `ld a,r`-style randomness, no neighbour-matching - ruled out as the smoothness gap.
- Found the real mode-select instructions are `ld a,(l8859) / rra / rra / and 3` (`:5426-5429`) - bits 3-2 of a free-running byte, not bits 1-0. If that byte increments by small amounts call-to-call (consistent with a mistranscribed `ld a,r`), the mode persists across a short run of columns instead of re-rolling independently every column. Replaced the Amiga port's uniform-random-per-column mode selection with a slowly-incrementing counter (`rApprox`) to reproduce that autocorrelation - longer, more gradual slopes instead of rapid alternation.
- Replaced `landSurfaceTileForColumn()`'s `worldColumn & 3` hill-variant cycle (arbitrary, unrelated to slope position) with a coverage-ordered phase indexed by `currentY & 3` - HILL UP/DOWN's 4 variants measured at 15/18/26/28 land pixels respectively, so a multi-column slope now advances through that progression instead of jumping between unrelated-looking variants.
- Added a dedicated anti-alias seam pen (pen 7 in `tools/cpc_game_tiles_to_amiga.py`, confirmed unused by any of the 102 tiles via a full pen-usage audit) inserted at sky/land pixel-doubling boundaries in `tile_to_amiga()`, approximating the dithered blur the user found by zooming into a real WinAPE capture. Tuned twice on visual feedback: a straight per-channel average was nearly invisible (green pinned at max in both neighbours, so only blue moved and barely), a darkened version read as an unwanted brightness dip; settled on same-brightness-as-neighbours with only the blue channel shifted toward a teal tint.
- Turned off `HAR_DEBUG_PERF_LOG`/`HAR_HEADLESS_AUTOPLAY` (left on from Sprint 14.84's headless perf harness) - these were silently forcing continuous climb input, which was the actual cause of a reported "can't steer down" bug, unrelated to graphics.

Known deferred follow-ups (not fixed this sprint, by explicit user choice to keep this sprint bounded):

- **Carrier ship tint**: `sprite_pixel_data10-14` (`AMSTRADFONT3.asm:1709-1780`, "CARRIER BODY/BACK/FRONT/TOP") is a completely different data format - already-unpacked direct pen-value arrays, not Mode 0 packed bytes - and shares pens 1/3/4/5/6 with the world tile palette. Since 1/4 are now pinned to land green, the carrier inherits a green tint it shouldn't have. Needs either dedicated carrier-only pens or a copper-based per-band palette split (mirroring the CPC's own raster trick) to resolve properly.
- **GUN target clarity**: RADAR/GUN/MISSILE LAUNCHER are single 4x8-native-pixel tiles even on real hardware (`enemylandsprites`, `:5676-5679` - only TANK is 2 tiles front+rear), so they're inherently tiny/sparse. User reports GUN specifically still reads as "totally obscure" even with its accent pens unified to black; not addressed further this sprint.
- **Sub-tile height resolution**: Amiga's native resolution here (320x200, 32 colours) is double the CPC+'s (160x200, 16 colours), but tiles are currently a straight 1:1 pixel-doubled port of the CPC's low-res art, so that extra resolution isn't being used. A true fix (height changes in half-tile/4px steps, needing new synthesized tile art and rendering-loop changes across `terrainYForWorldColumn`/`landSurfaceTileForColumn`/the row-iteration loop) was discussed as a larger, higher-risk follow-up and intentionally not attempted this sprint.

Done checks:

- Clean rebuild compiles with no errors on every iteration.
- User confirmed via repeated WinUAE screenshots across iterations: solid green ground, black target/tank silhouette, HUD/buildings no longer pure black, graduated hill slope.
- Debug flags confirmed off; "can't steer down" input bug resolved as a side effect.
- User explicitly accepted current state as good enough to move on ("lets not get to picky if you have other improvement plans coming").

**Correction (Sprint 14.93)**: this entire sprint's narrative - `decode_mode0_byte()`, "real CPC Mode 0 hardware layout" - turned out to be built on the wrong screen mode for this specific asset class. The game tiles (`AMSTRADFONT3.asm`'s `spritelookuptable`, including RADAR/GUN/TANK/LAND/SEA/SKY/HILL) are CPC **Mode 1** (4 pens/2 bits-per-pixel), not Mode 0 (16 pens/4 bits-per-pixel) - confirmed and fixed by the user, who found the radar tile (and others) looked wrong, re-extracted using the corrected Mode 1 decoder, and confirmed it now looks right on Amiga. `tools/cpc_game_tiles_to_amiga.py`/`tools/extract_cpc_assets.py` already contain `decode_mode1_byte()`/`MODE1_TO_GAME_COLOR=(0,5,10,15)` in the current working tree, so the tool-level fix predates this note being written - only this sprint's *prose* needed the correction. See Sprint 14.93 for the follow-up work this correction motivated (per-band copper palette). Unaffected by this: the loading-screen bitmap (`cpc_screen_to_amiga.py`) and menu font glyphs (`cpc_font_to_amiga.py`) are genuinely Mode 0 assets and were never part of this mistake.

## Sprint 14.91 - Damage Scheme, Flak, and Enemy AI Fidelity

Status: code complete, headless smoke-tested clean; interactive WinUAE confirmation still outstanding.

Runnable result: every collision except flak is now instant death (matching the real CPC, replacing invented graduated damage points); ground flak now spawns dynamically near ground targets throughout a flight instead of only 10 fixed, non-regenerating spots; the enemy plane actively climbs/dives to track the player's altitude and fires based on real proximity instead of a fixed screen X/timer; enemy missiles (both plane- and ship-fired) continuously home on the player for their whole flight; a new main-menu "Lives" option lets players choose 1 life (full CPC authenticity) instead of the Amiga port's default 3.

Why: user identified three systems as inaccurate/missing versus the real Amstrad game (no working flak, wrong damage feel, "AI is off"). Traced with three parallel research passes into `HarrierAttackSourceNew2_alt_CRTC_CART16.asm` and the current `amiga/main.c`, each with exact file:line citations - full plan at the time was written to `synthetic-splashing-bee.md` and approved before implementation. Confirmed decisions with the user first: match CPC's instant-death exactly; keep Amiga's 3-life default but add a menu toggle for 1 life; defer CPC's difficulty-scaling (`leveldifficulty`, tied to a landing/relaunch loop that doesn't exist on Amiga yet) to its own future sprint.

Tasks (`amiga/main.c` unless noted):

- **Damage**: enemy-plane collision (was `damagePlayer(game, ENEMY_COLLISION_DAMAGE)`) and enemy-missile collision (was `damagePlayer(game, ENEMY_MISSILE_DAMAGE)` for the plane-fired case) both now call `startPlayerCrash(...)` unconditionally, matching the ship-missile branch that already did this - mirrors `checkplayeragainstobjectmap`/`planehitbyobject` (`:7525-7544`/`:8127-8132`), where everything but flak is instant death. Removed the now-fully-dead `ENEMY_COLLISION_DAMAGE`/`ENEMY_MISSILE_DAMAGE`/`ENEMY_SHIP_MISSILE_DAMAGE` constants and the `damagePlayer()` function itself (zero remaining callers). Added the `playSfx(SFX_HIT)` call `applyPlayerFlakDamage` was missing relative to the old `damagePlayer` pattern.
- **Lives menu toggle**: repurposed the "Redefine keys" menu slot (`MENU_ITEM_REDEFINE` -> `MENU_ITEM_LIVES`) since the menu screen is already pixel-tight to its 200px height (gauges reach the bottom edge) with no room for a 5th row - Redefine was an unimplemented stub ("COMES IN SPRINT 3" notice) anyway, so no real functionality was lost; it remains a backlog item for whenever there's a free slot or redesigned layout. Cycles "Lives: 3" / "Lives: 1", threaded through `menuItemText`/`drawMenuItem`/`drawMenuItems`/`drawMenuScreen`/`updateMenuSelection` alongside the existing `skillLevel` parameter, applied to `game.lives` right after `startGameSession(...)`.
- **Dynamic flak**: extended `generateCpcLandHeightTable()` with a parallel `cpcLandFlakTable[]` - whenever a ground target is placed, rolls a chance (reusing the same `rng` stream) to also schedule a flak marker a few columns ahead, mirroring `launchflakattack`'s (`:6033-6095`) real trigger (keyed off recent target placement, gated by a random roll, placed a few rows above the terrain). New `cpcLandProceduralFlak()` accessor; `objectCellForWorldColumnTile()` emits it via tiles 57/58 at `terrainY-5`/`terrainY-6`, matching the existing hand-placed `level_route.h` entries' exact row offsets/tiles so it reuses the same clearing/collision path (`isFlakClearedAtColumnRow`, `applyPlayerFlakDamage`) unmodified.
- **Enemy plane AI**: `updateEnemyPlane` now steps `dy` by ±1/frame toward `playerY` (clamped to screen bounds) instead of leaving it locked at 0 - mirrors `enemyplaneexitscreen`'s (`:6575-6608`) real per-tick altitude convergence.
- **Enemy missile fire trigger**: replaced the fixed `ENEMY_MISSILE_FIRE_X`/frame-fallback check with an actual horizontal-distance-to-player check (`ENEMY_MISSILE_FIRE_RANGE_PIXELS`), matching the real proximity trigger (`:6556-6561`); kept a generous frame-count fallback as a safety net.
- **Enemy missile homing**: generalized the ship-missile-only, distance-gated homing branch to apply unconditionally to both missile sources every frame, matching `heatseekposition`'s (`:6124-6165`) continuous re-tracking - removed the now-unused `ENEMY_SHIP_MISSILE_TRACKING_CUTOFF_PIXELS` gate.

Explicitly out of scope this sprint (flagged during research, not requested): Wingman AI (entirely absent - just a cosmetic "Wingman: Off" label, and CPC's enemy-plane targeting can pick either the player or the wingman, which doesn't exist here yet); enemy-plane *spawn* randomization/player-altitude gating (kept the existing fixed trigger-column spawn, only fixed post-spawn behavior); difficulty scaling (confirmed deferred).

Done checks:

- Clean rebuild compiles with no errors.
- Headless perf harness run (debug flags temporarily re-enabled per the standing rule, then switched back off) completed its full frame budget with no crash/hang - `armour` cycling 100->0 repeatedly then flatlining confirms instant-death collisions are actually firing (expected, since the dumb "always hold up, no evasion" autoplay script now dies far more easily against a more authentic difficulty).
- User confirmed via WinUAE: enemy plane altitude tracking and missile homing both visible in play.

Sprint 14.91.1 follow-up (enemy plane flak-avoidance):

- User observed enemy planes refusing to climb when a flak hazard sat above them, and asked to make this deliberate rather than accidental, matching the real CPC (`enemyplaneexitscreen` "respects flak obstructions", `:6585-6600`) - the Amiga altitude-tracking added above had no flak-awareness at all.
- `updateEnemyPlane()`: after computing the desired `dy` toward the player, checks the object-map cell the plane is about to step into (`objectCellForWorldColumnTile()` at the plane's world column and candidate tile row) and cancels the move for that frame if it's `HAR_OBJ_FLAK`. Re-evaluated every frame, so the plane naturally resumes climbing once the flak scrolls past or gets shot down - reuses the existing `isFlakClearedAtColumnRow()` gating already inside `objectCellForWorldColumnTile()`, no separate clearing logic needed.
- Confirmed via code read that the "heavy flak near the end town" the user separately noticed is pre-existing hand-placed content (4 static entries in `level_route.h` at columns 456/482/510/538, all inside the town's 411-610 column range) - Sprint 14.91's dynamic flak only fires in procedural mountain/hill terrain, not town, so it's unrelated to this sprint's changes. Left as a backlog item below to audit against the real CPC's actual town flak density rather than changing blind.

Sprint 14.91.2 follow-up (gauge-style in-game HUD, the deferred item from Sprint 14.7.1):

- Real CPC in-game HUD draws SPEED/FUEL/ROCKETS/BOMBS as tick-segmented gauge bars (`drawgauge`, `:5249-5261`) and ARMOUR as a bar that erases one segment per hit (`updatehealth`, `:2963-2985`), not plain digit readouts - this had been left as placeholder numbers since Sprint 14.7.1 ("gauge-style in-game HUD parity... later dedicated HUD sprint").
- Added `drawHudGaugeBar()` (bordered bar, live fill fraction, tick marks every 6px) reusing the same visual language as the menu screen's existing `drawMenuGaugeBar` (built as the CPC-style reference in Sprint 14.7.1), but with a real value/max fraction instead of the menu's fixed decorative fill.
- `drawHudStatic`/`drawHudValues` reworked: SCORE stays a plain number (CPC's score is a number too); SPD/FUEL/ARM/RKT/BMB become gauge bars; LIV stays a plain number (Amiga-only addition, no CPC equivalent per Sprint 14.91).
- Follow-up user feedback from a WinAPE reference capture: the real ARMOUR gauge is a single much wider/taller bar on its own row, not equally-sized alongside the others. Reworked layout: ARMOUR now spans most of row 1 (after SCORE) at 9px tall/162px wide; SPD/FUEL/RKT/BMB/LIV compact onto row 2 at 6px tall. Not yet re-confirmed visually after this specific resize.

Sprint 14.91.3 follow-up (real CPC scoring, enemy-missile destruction, HUD redraw architecture):

- User confirmed the real CPC only awards score via `explosionnoise()` (hit/kill events), never for distance flown - `updateHudValues()`'s previous `game->score = (scrollX>>4) + bonusScore` distance term removed entirely; score is now purely `bonusScore`. Rebalanced the scoring constants to match `explosionnoise()`'s actual call sites and "*10" scaling (`HarrierAttackSourceNew2...asm:1206-8239`): `ENEMY_SCORE_VALUE` 50->750, plus new `GROUND_TARGET_SCORE_VALUE`(100), `ENEMY_SHIP_SCORE_VALUE`(500), `FLAK_SCORE_VALUE`(10), `ENEMY_MISSILE_SCORE_VALUE`(10, now actually used - see below).
- Found and fixed a pre-existing gap while auditing the score system: shooting down an enemy missile previously destroyed nothing and awarded no score (the `ENEMY_MISSILE_SCORE_VALUE` constant existed but nothing referenced it). Both rocket-vs-missile and bomb-vs-missile collision blocks now destroy the missile and award the score, matching `playermissilehitenemymissile` (`:8235-8244`).
- Removed HUD double-buffering (`HUD_BUFFER_COUNT` 2->1) while chasing the HUD corruption below - a stale copper-pointer-tracking bug was found and fixed in the process (a loop iterating `SCREEN_PLANES` instead of `HUD_PLANES` on every buffer swap), but double-buffering itself turned out not to be the corruption's cause. Kept the simplification anyway: a single static buffer matches the menu screen's already-stable approach and removes an entire class of swap-race risk for no visual cost.

Sprint 14.91.4 (HUD "ghost" corruption - root cause and fix):

- Symptom: after the Sprint 14.91.2 HUD resize, a dull-white/brighter "ghost" of the HUD's own text (first noticed as "ARMOUR" bleeding into its own gauge) appeared only inside the HUD panel - never on the terrain above it - moving in discrete jumps synced to terrain scroll (not a smooth scroll) and freezing whenever scroll stopped. Reproduced identically on WinUAE (`cycle_exact=true`) and on real hardware, so not an emulation artifact.
- Several plausible-looking fixes were tried and empirically ruled out (each built, tested, and confirmed not to change the symptom): matching the HUD split's bitplane count to the world's (`HUD_PLANES`=`SCREEN_PLANES`, removing any plane-count transition); explicitly re-pointing the otherwise-unused 5th bitplane pointer for the HUD split instead of leaving it stale; and a genuine but unrelated `DIWSTOP` 8-bit-wraparound-ambiguity bug (see "Amiga hardware notes" below) - fixing it cost a visible pixel row and didn't touch the ghost, so reverted.
- Ruled out a HUD-buffer memory-corruption theory (world-buffer ring-stream overflowing into the adjacent `hudBuffer` allocation, or anything else writing where it shouldn't) via a byte-level "canary": snapshot known-static HUD buffer regions right after they're drawn once at session start (the top border row, and the ARMOUR label's own pixels - both never legitimately touched again) and diff them against the live buffer every frame. Byte-perfect for an entire autoplay run including a crash/respawn cycle - the buffer's stored content was never wrong.
- Ruled out a corrupted copper *program* by reading the copper list's own operand words directly out of chip RAM (capturing pointers to those words at build time in `buildGameHudCopper()`) rather than reading the hardware registers themselves - `custom->bplcon0`/`->ddfstrt`/`->bplpt[]` etc. are write-only on real OCS/ECS chipsets, and an earlier attempt to read them back produced obvious bus-noise nonsense (`0xFFFFFFFF`, `0xFFFF`) rather than a real answer. The copper program always contained the exactly correct `BPLCON0`/`DDFSTRT`/`DDFSTOP`/pointer values for the HUD section.
- **Root cause**: a timing shortfall in the copper's world-to-HUD transition itself. That transition needs ~17 register-write instructions (colour, `DDFSTRT`/`DDFSTOP`, `BPLCON1`, modulo, `BPLCON0`, 5 bitplane-pointer pairs) - a minimum of ~68 colour clocks - but the transition's `WAIT` position (`0xe0`) left only ~59 clocks before the HUD's own `DDFSTRT`, a calculable deficit even before any audio/disk DMA contention. When the copper fell behind schedule, the *last*-written registers in program order (the bitplane pointers) could still hold the world section's own scrolling pointer values for the first HUD scanlines it displayed - closest to the ARMOUR gauge - which is exactly why the ghost tracked scroll position and froze when scroll froze.
- Fix (`buildGameHudCopper()` / `screenScan()` in `amiga/main.c`): moved the transition's `WAIT` trigger from `0xe0` to `0xd2` (right after the world section's own `DDFSTOP` instead of near the end of that scanline), turning a ~9-clock deficit into a ~13-clock surplus; dropped the now-redundant 5th-bitplane-pointer write for the HUD split (saved 2 of the 17 instructions, and the theory it was fixing had already been ruled out). Separately, moved the whole display window up (`SCREEN_DIWSTRT_Y` 44->20) after the user noticed ROCKETS/BOMBS drawn partly below the visible screen - a PAL safe-area consequence of extending `SCREEN_HEIGHT` downward (Sprint 14.91.2) without recentring the window; `DIWSTRT`/`DIWSTOP`'s vertical baseline was previously hardcoded to `44` in two separate places (`screenScan()` and `copWaitDisplayYAt()`) and is now the one shared constant.
- Confirmed fixed by the user on real hardware.
- See the `harrier-headless-testing` skill's "Diagnosing hardware/copper-timing bugs" section for the reusable canary/copper-verification/timing-budget techniques developed during this investigation.

Amiga hardware notes worth remembering from this investigation (not acted on further, since the actual fix was the timing margin above, but real and worth flagging for future copper-list work):

- `DIWSTOP`'s vertical field is only 8 bits. A display window exactly `SCREEN_HEIGHT`(256) lines tall makes `(y+height)&0xff` mathematically equal to `y` itself for *any* `y` (256 mod 256 is always 0), colliding with the "wrapped past line 255" convention that needs `DIWSTOP`'s byte to read strictly less than `DIWSTRT`'s. A 200-line window (the original `SCREEN_HEIGHT`) never hit this because `44+200=244` fits in 8 bits with no wraparound needed at all.
- `custom->bplcon0`/`->ddfstrt`/`->ddfstop`/`->bplpt[]` (and most other custom chip registers) are write-only on real OCS/ECS hardware - reading them back returns whatever's currently on the data bus (often looking like `0xFFFF`/`0xFFFFFFFF`), not what was last written. To verify what a copper program actually contains, read the copper list's own words in chip RAM instead.

## Sprint 14.92 - Flak Timing/Density Fidelity, Menu Music, and Misc Amstrad-Parity Fixes

Status: code complete, clean rebuild + headless autoplay smoke-tested with no crash/hang; interactive WinUAE/audio confirmation still outstanding (user will test once the whole batch is in).

Runnable result: land flak now spawns at a variable height up to near the player's ceiling (not a fixed `terrainY-5`/`-6` pair) at roughly double the previous density, with a "pop" sound timed to the exact screen column it becomes visible on rather than firing after it's already scrolled into view; the player's rocket continuously tracks the player's altitude like the real CPC's `lockinmissileheighttoplayer`; respawn/initial spawn height raised to match the original feel; the main menu now reuses the exact in-game HUD-drawing code and plays a converted public-domain MOD ("Thaxted") through a small custom ProTracker player; bomb can now be dropped via joystick fire+down or mouse right-click, not keyboard only; the loading screen is vertically centered instead of pinned to the old, shorter screen height.

Why: a long round of user bug reports/feature requests distinct from Sprint 14.91's damage/AI work - covering flak realism (density/height/timing), missing menu music, an input gap that made bombing impossible on joystick/mouse, and a couple of smaller visual/gameplay polish items raised in the same pass.

Tasks (`amiga/main.c` unless noted):

- **Variable-height land flak**: replaced the fixed `CPC_LAND_FLAK_TILE_57`/`_58`-at-terrainY-5/-6 pair from Sprint 14.91 with `CPC_LAND_FLAK_MIN_ROW_OFFSET`(5)/`CPC_LAND_FLAK_MAX_ROW_OFFSET`(20) - `generateCpcLandHeightTable()` now rolls a random offset in that range (clamped to the local terrain height minus the flak-spawn lookahead distance, `CPC_LAND_FLAK_SPAWN_LOOKAHEAD_COLUMNS`=3, so flak never gets clamped using the wrong column's height) so flak can appear anywhere from just above the ground up to near the Harrier's max climb, matching the user's "should not be possible to climb over all flak" request. Town flak (`generateCpcTownFlakTable()`) gets the same variable-offset treatment (town's height is constant, so no lookahead-drift concern there).
- **Doubled land flak density**: the spawn-roll gate in the same function went from 1-in-4 (`& 3`) to 1-in-2 (`& 1`) to better match the real CPC's noticeably denser flak per the user's side-by-side comparison.
- **Flak "pop" sound, then a timing correction**: added `updateFlakPopSound()`/`flakPopLastColumn`/`resetFlakPopTracking()` to play a spawn blip when a flak column scrolls into view, mirroring the real CPC's audible flak pop. First version checked a column 1/6-screen inward from the right edge (an attempt to match "flak pops up about 1/6 screen from the right" as literally described) - user reported the sound played at the right *position* but the flak was already visible by then. Root cause: the ring-buffer world-streamer pre-renders columns well ahead of the visible screen edge, so by the time the check column (inward from the edge) is reached, that flak has been in the visible buffer for a while already - unlike the CPC, which spawns flak live with no lookahead. Fixed by checking the exact rightmost visible column (`(scrollX>>3) + GAME_MAP_WIDTH`) instead, so the sound fires the moment the column enters the streamed/visible world.
- **Flak destroy-sound fix**: rocket-vs-flak and bomb-vs-flak hit handling now force-clears `sfxChannelRetriggerGuard[SFX_IMPACT]`'s channel before playing the destroy sound, fixing a case where a recent impact sound's retrigger guard could silently swallow the "flak destroyed" cue.
- **Rocket height tracking**: `updateWeapons()` now sets `game->rocketShot.y = playerY + 2` every frame while the rocket is active, matching `lockinmissileheighttoplayer` (`:6994-7003`) - previously the rocket flew a fixed trajectory independent of further player movement after firing.
- **Respawn/spawn height**: `PLAYER_START_Y` raised from 78 to 56 (used for both initial takeoff and respawn) after the user reported respawning too low.
- **Menu HUD reuse + menu music**: `drawMenuScreen` now calls `drawMenuDemoHud()`, a thin wrapper that fills a placeholder `GameState` with representative values and calls the exact same `drawHudBuffer()` gameplay uses (a dedicated `MENU_HUD_STATE_INDEX` render-state slot keeps its delta-redraw tracking independent of the real game's), replacing the old bespoke decorative gauge-drawing code so menu and gameplay HUD can never visually drift apart again. Added a from-scratch ProTracker "M.K." MOD player (`modParseHeader`/`modAdvanceRow`/`modTick`/`startModMusic`/`stopModMusic`) driven from the main loop's VBlank tick, playing a user-supplied MOD converted from Mutopia's public-domain Thaxted MIDI (`tools/mutopia_thaxted_to_mod.py`) while the menu is shown; stops before gameplay starts and resumes on returning to the menu, sharing Paula's channels with the existing SFX system.
- **MOD player correctness fixes** (found via detailed user review of the player's Paula-timing behavior): (1) `modSetChannelHardware()` now clears the channel's DMA bit before writing new `AUDxLC`/`AUDxLEN`/period/volume, then sets it - Paula only reloads those registers on an off->on DMA transition, so retriggering a note without first clearing DMA (if the channel was already active) silently failed to restart the sample cleanly, matching the same clear-then-set convention `stopSfxChannel`/`startPendingSfxChannel` already use for the shared SFX channels. (2) `startModMusic()` now calls `modAdvanceRow()` immediately instead of waiting a full row's worth of ticks (~120ms) before the first row - and before this tune's own row-0 tempo/speed effects even took hold. (3) Added `modRestoreChannelAfterSfx()`, called when a menu-navigation blip's playback naturally ends, to immediately resume the tune's held note on that channel instead of leaving it silent until the next row happens to retrigger it. (4) Fixed an order-table parsing bug where unused padding bytes past the declared song length (not guaranteed to be zero) could inflate the computed pattern count and shift the sample-data offset.
- **Music tempo fixed at the source**: the MOD's authored tempo (`MOD_BPM` in `tools/mutopia_thaxted_to_mod.py`) was mathematically correct but felt too slow in practice; rather than hardcoding a runtime multiplier in the player, raised `MOD_BPM` from 37 to 75 in the generator script and regenerated `amiga/assets/music/harrier_menu_fixed.mod` (19.2s/loop instead of 38.9s/loop).
- **Bomb input mapping**: `ReadInput()`'s `input->bomb` was keyboard-only (`keyBomb`), meaning joystick/mouse players had no way to drop bombs at all. Now also true on `MouseRight()` or fire+down on the joystick.
- **Loading screen centering**: the loading screen bitmap is now vertically centered against the current `SCREEN_HEIGHT` instead of assuming the old, shorter pre-14.91.2 screen height.

Done checks:

- Clean rebuild compiles with no errors.
- Headless perf harness run (debug flags temporarily re-enabled per the standing rule, then switched back off) completed its full frame budget with no crash/hang; HUD diagnostic guard columns stayed at zero (no regression in the Sprint 14.91.4 HUD-timing fix).
- Screenshot-based visual/audio confirmation was not possible this pass - the headless WinUAE window came back as a fully black capture despite the process running normally (active CPU, `Responding: True`), and `IsWindowVisible` initially reported `False` even after the process had a valid window handle; forcing the window to the foreground didn't change the capture result. Flagged as a session/environment limitation rather than a game bug - unresolved, revisit if screenshot-based headless verification is needed again.
- User will do the interactive WinUAE/real-hardware confirmation pass once this and all other pending fixes from the same feedback round are in.

Sprint 14.92.1 follow-up (real DMA retrigger bug found + tempo dialed in by ear):

- User's own review of the DMA-retrigger fix above (in `modSetChannelHardware()` at the time) found a real remaining bug: clearing `DMACON` and immediately re-setting it within the same function call doesn't guarantee Paula actually latches the off-state before the on-write lands - a genuine off->on *edge* isn't assured just because the bits were written in sequence. Fixed properly: retriggering (both `modAdvanceRow()`'s normal note retriggers and `modRestoreChannelAfterSfx()`) now only clears DMA and marks the channel pending (`modBeginRetrigger()`); a new `modCompletePendingRetriggers()`, called once per frame right after `WaitVbl()` (before `updateSfx()`/`modTick()`), finishes the retrigger (writes `ac_ptr`/`ac_len`/`ac_per`/`ac_vol`, sets DMA) a full frame later - the same two-phase pattern this file's own SFX system already uses (`stopSfxChannel()`/`startPendingSfxChannel()` + `sfxChannelStartDelay`). The one-frame gap is uniform across every note, so relative timing between notes is unaffected; `stopModMusic()` also clears any left-over pending flag so a retrigger queued right as gameplay starts can't fire late onto a channel the SFX system now owns.
- This turned out to be the actual cause of the "weird rhythm" complaint, not the tempo number - confirmed by the user directly after this fix landed: tempo 75 (mathematically already brisk) still read as too slow, 200 (a large deliberate diagnostic jump) read as too fast, which only makes sense if the earlier "slow/weird" perception was really the unclean retriggers, not the underlying rate. Dialed in by ear from there in three quick rebuild/relisten iterations: 125 (ProTracker's own standard default, "even better") -> 63 (125 called "~2x too fast") -> **47**, final ("det er bra!"). `tools/mutopia_thaxted_to_mod.py`'s `MOD_BPM` is the single source of truth for this now - see the comment trail there for the iteration history.
- Practical lesson for next time a `#embed`'d asset changes without any `amiga/main.c` edit alongside it: touch `main.c`'s mtime (or make a trivial edit) before rebuilding to force recompilation - the build's dependency tracking doesn't know `main.c` depends on `assets/music/harrier_menu_fixed.mod` via `#embed`, so a plain rebuild after only regenerating the `.mod` file risks silently relinking a stale embedded copy.

## Sprint 14.93 - Mode 1 Game Tile Correction and Per-Band Copper Palette

Status: code complete; interactive confirmation pending (part of the same batch as Sprint 14.92/14.92.1).

Runnable result: the land/sea colours in the instrument panel now differ from the game-world colours (previously only the sky register changed across screen bands - land/black/sea stayed identical from top of sky to bottom of HUD), and the "sea" colour register now correctly reads as pale cloud-white in the sky bands and genuine sea-blue once the lower play area begins, matching the real CPC's own reused-register trick.

Why: the user found that the game tile graphics (RADAR, GUN, TANK, LAND, SEA, SKY, HILL, etc. - everything `AMSTRADFONT3.asm`'s `spritelookuptable` covers) had been extracted assuming CPC **Mode 0** (16 pens/4 bits-per-pixel) when they're actually **Mode 1** (4 pens/2 bits-per-pixel) - this was the real explanation for graphics (radar in particular) looking subtly wrong despite Sprint 14.90's extensive pen-mapping correction work. The user re-extracted the tiles correctly and supplied a complete reference palette (per-CPC-pen Amiga register assignments, per-screen-band RGB values, the full 16-colour "reserve" UI ramp, and the 16-entry CPC+ hardware sprite palette) to check the Amiga port against.

Tasks:

- Confirmed `tools/cpc_game_tiles_to_amiga.py` and `tools/extract_cpc_assets.py` already contain the Mode 1 decoder (`decode_mode1_byte()`, `MODE1_TO_GAME_COLOR=(0,5,10,15)`) in the current working tree - the tool-level fix predates this sprint; only Sprint 14.90's prose needed the correction (added there, see above) and the actual runtime asset needed checking against the user's new reference.
- Diffed the user's reference tables against the currently-embedded `amiga/assets/game_palette.pal` (32 words, dumped and checked byte-for-byte): the "Amiga-native reservefarger" table (COLOR01-15) and the full CPC+ hardware sprite palette (COLOR16-31) **already match exactly** - the palette-extraction side of the pipeline was never affected by the Mode 0/1 tile-decoding bug (palette words come from a separate ASM table, `palettegamemaster`/`sprite_colours`, not from decoding tile pixel bytes) and needed no changes.
- The actual gap was the copper's per-band palette: `copSetGameSkyGradient()` only ever changed `GAME_COLOR_SKY` (register 0) across the sky's 3 bands (Y=0/56/112), and the HUD transition hardcoded register 0 to plain black - `GAME_COLOR_LAND`(5)/`GAME_COLOR_BLACK`(10)/`GAME_COLOR_SEA`(15) never changed across bands at all, even though the user's reference wants land+sea to shift for the instrument panel band and sea to double as "clouds" (white) up in the sky versus real sea-blue lower down. `GAME_COLOR_BLACK` needed no runtime write anywhere - it's identical to the bulk-loaded value at every band in the reference table.
- Re-tuned `GAME_SKY_TOP_RGB`/`GAME_SKY_MID_RGB` to the user's refined upper/mid-sky values (`0x058d`/`0x069e`, replacing the Sprint 14.87-era `0x0779`/`0x079d`); `GAME_SKY_LOW_RGB` (`0x07af`) was already correct and unchanged. Added `GAME_SKY_TOP_CLOUD_RGB`(`0x0fff`)/`GAME_SKY_LOW_SEA_RGB`(`0x0009`) and extended `copSetGameSkyGradient()` to flip `GAME_COLOR_SEA` between them at the same Y=0/Y=112 wait points already used for the sky colour - no new WAIT instructions needed, just two more colour writes riding along the existing ones.
- Instrument panel band, first attempt (**reverted**, see follow-up below): tried applying the reference table's panel values for sky (`0x0fa0`) and land (`0x0ffa`) directly to `GAME_COLOR_SKY`/`GAME_COLOR_LAND` at the HUD transition. Only `GAME_COLOR_SEA`'s panel value (`GAME_HUD_PANEL_SEA_RGB`=`0x0f00`) survived - written *after* the HUD's own bitplane pointers are set up (past the end of the Sprint 14.91.4 transition's documented tight timing budget) rather than alongside the sky write inside it, specifically to avoid reintroducing that HUD "ghost" bug's timing margin issue. `GAME_COLOR_SEA` isn't one of the 5 pens (0/1/5/6/9) the HUD's own graphics currently use, so this write has no visible effect today - kept only for parity with the reference table.

Sprint 14.93.1 follow-up (instrument panel colours reverted - broke the HUD's own gauge semantics):

- User's screenshot showed the whole instrument panel washed solid orange, all gauge fills reading as near-identical pale yellow. Root cause: `GAME_COLOR_SKY` doubles as `HUD_COLOR_BACKGROUND` (the entire panel's `fillRect()` fill colour, `drawHudStatic()`) and `GAME_COLOR_LAND` doubles as `HUD_COLOR_SAFE` (the FUEL/LIVES "all good" green, distinct from `HUD_COLOR_VALUE`'s yellow) - these are custom semantic reuses specific to *this* HUD's own gauge-bar design, not the real CPC's raster instrument panel the reference table was describing. Applying the CPC's own panel RGB values to those same register *numbers* broke both distinctions: the panel background became orange instead of black, and FUEL's "safe" green became visually indistinguishable from the plain yellow "value" colour used elsewhere.
- Reverted both: `GAME_COLOR_SKY` stays plain black (`0x000`) at the panel band, matching pre-14.93 behaviour; `GAME_COLOR_LAND` is no longer overridden for the panel band at all (stays whatever the bulk-loaded `game_palette.pal` value already is, preserving `HUD_COLOR_SAFE`'s green). Only the harmless, currently-invisible `GAME_COLOR_SEA` panel write survived from the original attempt. Removed the now-unused `GAME_HUD_PANEL_SKY_RGB`/`GAME_HUD_PANEL_LAND_RGB` constants.
- Also fixed in the same pass: `input->bomb`'s `(input->fire && input->down)` combo (added to give single-fire-button joysticks bomb access) turned out to make Down-alone drop bombs under WinUAE's default numpad-joystick quickstart profile - something in that profile makes `input->fire` read true alongside Down, and Numpad-5 (the key the user expected to be fire) does nothing at all, suggesting WinUAE's default doesn't bind fire to that key in the first place. Not fixable from project files (WinUAE's quickstart profile isn't stored in `harrier_headless.uae` as editable text), and unreliable to build on, so the combo was removed - `input->bomb` is now just `keyBomb || MouseRight()`, both confirmed unaffected by the quirk.

Done checks:

- Clean rebuild compiles with no errors, twice (original attempt, then the revert).
- Diffed the user's full reference tables against the live embedded palette asset word-for-word (see above) rather than assuming - only the copper per-band logic needed a code change.
- User caught the panel-colour regression directly from a WinUAE screenshot; fix applied and rebuilt same pass.

Sprint 14.93.2 follow-up (real second joystick button for bomb, Amstrad button-parity):

- User asked to replicate the real Amstrad's joystick button roles (button0=bomb, button1=rocket) but deliberately swapped, since the Amiga port should keep its primary/only button firing the primary weapon (rocket) - i.e. `JoyFire()` (button 0) stays rocket (already the case - `input.select`/`launchRocket()`), and a genuine second joystick button becomes bomb, rather than relying on WinUAE-specific numpad quirks (explicitly called out as irrelevant to this request).
- Added `JoyFire2()` - reads the joystick port's second fire button via POTINP/POTGOR (`$DFF016`) bit 14 (`DATRY`, the hardware manual's "port 2 pin 9" - offset by one from the CIA gameport numbering used elsewhere in this file), mirroring `MouseRight()`'s existing bit-10 read of the same pin on the mouse port. Confirmed via the Amiga Hardware Reference Manual's POTGOR bit table before wiring it in, rather than guessing a bit position for an untested real-hardware register. `input->bomb` now also includes `JoyFire2()`.
- Keyboard: moved Space out of `keyFire` and into `keyBomb` (alongside B/Alt) - Enter/Control remain fire (rocket), Space is now the dedicated bomb key, giving keyboard players (and single-fire-button joystick players, who still can't reach bomb via the joystick itself - matching the real Amstrad's own single-button-mode limitation) a clean, separate key for the second weapon instead of one key trying to do both.
- Superseded Sprint 14.92's earlier `input->bomb = keyBomb || MouseRight() || (input->fire && input->down)` fire+down combo (already removed in 14.93.1 above) - the real second-button read here is the actual fix that combo was trying to approximate.

**Reverted same-day, then root-caused and fixed properly**: wiring `JoyFire2()` into `input->bomb` made the game hang before ever reaching the menu - confirmed both by the user interactively ("den krasjer og starter aldri menyen nå") and via a headless run (the perf-log CSV never got written, meaning the run never survived to its normal shutdown path; a clean full rebuild first ruled out incremental-build corruption as the cause). First reverted `input->bomb` back to `keyBomb || MouseRight()` to unblock the user immediately, confirmed the hang was gone (clean rebuild, headless run completes, user confirms "Starter fint nå" interactively).

- User then independently re-verified the bit/register choice was correct (POTGOR/POTINP `$DFF016` bit 14, active low - matches the Amiga Hardware Reference Manual) and supplied a cleaner reference implementation using `custom->potinp` (this project's header names the register `potinp`, not `potgor` as in the user's snippet - used the project's actual field name). Renamed `JoyFire2()` to `JoyFire1()` to match the user's button-numbering convention (button0=`JoyFire()`, button1=`JoyFire1()`). Re-wired it into `input->bomb` and re-verified via the same headless method - **the hang reproduced again, identically**, confirming this wasn't a coding mistake either time; the bit/register were always correct.
- **Actual root cause** (found by the user): the POT line for the second fire button was never configured with a pull-up, so with nothing driving it, `custom->potinp` bit 14 floats/reads low - i.e. `JoyFire1()` reads as *permanently pressed*. That makes `input->bomb` (and therefore `input->any`) permanently true, which hangs forever in `WaitForInputRelease()` (`do { ReadInput(&input); WaitVbl(); } while (input.any);`) - called immediately after `InitInput()` at boot, before the menu is ever drawn. Exactly matches "never starts the menu" and explains why it reproduced identically both times (same missing initialization, regardless of which pointer style read the bit).
- **Fix**: `InitInput()` now does `custom->potgo = 0xff00;` once at startup - drives the POT lines high (pull-up) so an unpressed button reads released, and a real button press pulls the line low as `JoyFire1()` expects. Verified fixed the same way the hang was verified present (headless run completes normally, CSV shows a normal frame/scroll progression) before handing back.
- Debug flags (`HAR_DEBUG_PERF_LOG`/`HAR_HEADLESS_AUTOPLAY`) were cycled 1/1 -> 0/0 twice during this investigation (once per verification pass) - confirmed both back to 0 in the final build.

## Sprint 14.94 - Scrolling/Render Hot-Path Optimization

Status: in progress, part-by-part with a headless perf measurement after each part before moving to the next.

Goal (user's own framing): smoother scrolling and more CPU margin on real A500/68000 hardware. The scroll architecture itself (hardware fine-scroll, copper-updated bitplane pointers, ring-buffer streaming with no full-screen copy) is sound and is being kept as-is - this sprint is about removing redundant work inside that architecture, not replacing it.

Why: user did a full read-through of `amiga/main.c` (6472 lines) specifically looking for CPU cost that doesn't need to exist given the current architecture, and produced a 6-item prioritized list with expected gain/risk for each. Verified the analysis against the actual current code before implementing anything (see below) - it checked out almost exactly, with two small corrections: the per-frame row-streaming budget multiplies by `GAME_OBJECT_MAP_HEIGHT_TILES`(25), not `GAME_TILE_HEIGHT`(8) as guessed (same ceiling-division shape either way); and town blocks are already tile-granular (`drawGameScrollTile`), not on the expensive per-pixel path - only the carrier and gunship promoted assets are.

Priority order (user's own ranking, gain/risk as assessed):

1. **Build one 25-tile world column once, before drawing** (`objectCellForWorldColumnTile()`, `main.c:4578-4704`, currently called once per row - up to 25x per column - each call redundantly re-resolving the column's level segment/stage/terrain AND re-scanning the full 95-entry `harLevelObjects` array from scratch). Gain: large. Risk: low.
2. **Draw the ring-buffer's wrap-duplicate column from the same already-built data instead of re-resolving it** (`renderRingWorldColumn()`/`serviceRingWorldStream()`, `main.c:4975-4982`/`5023-5068`, currently call the full per-row resolution a second time for any column within `GAME_FETCH_BYTES` of the ring seam - roughly half the ring period). Gain: large in the wrap region. Risk: low. (Naturally merges with #1 once a column is built once and cached - drawing it a second time at a different X is nearly free.)
3. **Index `harLevelObjects` by world column** instead of a 95-entry linear scan per lookup (still scanned once per column after #1/#2, plus once more inside `drawDirectColumnRangeObjects()` for the promoted-asset overlay pass). Gain: large. Risk: low-medium.
4. **Remove `%`/division from ring-buffer runtime hot paths** - confirmed 3 real sites: `seaTileForColumn()`'s `(worldColumn+tileY) % 11` (`main.c:4003`, hottest - per sea-tile-cell), `ringWorldTileXForColumn()`'s `worldColumn % GAME_WORLD_SCROLL_PAGE_BYTES` (`main.c:4969`), and `scrollLocalByteOffset()`'s equivalent (`main.c:5100`). Gain: medium-large on 68000 (non-constant modulo/division compiles to slow library calls). Risk: low.
5. **Reduce the world view from 5 to 4 bitplanes** - confirmed empirically: every `GAME_COLOR_*` constant tops out at 15 (`GAME_COLOR_SEA`), and a byte-scan of the embedded `game_tiles.bpl` found the 5th bitplane is *entirely zero* across all 102 tiles (816/816 zero byte-rows checked). The HUD band already does exactly this trick (`HUD_PLANES 4`, `main.c:47-50`) with an explicit "20% less DMA, no colour loss" comment - the main gameplay view's own `buildGameHudCopper()` (same function, `main.c:2284-2293`) just never got the same treatment. Gain: very large (20% less bitplane DMA + world buffer size + tile byte-ops during the scrolling view specifically). Risk: medium - touches the copper/BPLCON0 setup right next to the Sprint 14.91.4 HUD-timing fix, needs careful testing against that.
6. **Preconvert carrier/gunship promoted CPC-plus assets to the same planar 8x8 tile format as `gameTiles`**, replacing their current per-pixel `drawCpcPlusSpriteScrollRange()`->`putPixelScroll()` path (per-pixel clip/pen-convert/read-modify-write across 5 bitplanes) with tile-level blits like town blocks already use. Gain: large specifically when the carrier/gunship are on screen. Risk: medium (new asset conversion + verification against the existing sprite art).

Recommended order (user's own, followed here): 1-4 first since none require a graphics-format or copper change; measure on real A500/68000 profile after; only then decide on 5/6 given they touch bitplane format and the fragile HUD-timing margin.

Verification method: headless perf harness (`HAR_DEBUG_PERF_LOG`/`HAR_HEADLESS_AUTOPLAY` temporarily on, `amiga/out/perf_log.csv`'s `hitches`/`maxVblDelta`/`avgFps` columns) run before and after each part, debug flags always returned to 0/0 before handing back a build.

### Part 1+2 - Single column build, shared by both the primary and wrap-duplicate draw

Implemented together since they compound naturally: once a column's tiles are resolved into a small cache exactly once, drawing that same cache a second time at the wrap-duplicate X position costs almost nothing extra.

- Added `RenderColumn` (`UBYTE tile[GAME_OBJECT_MAP_HEIGHT_TILES]`) and `buildWorldTileColumn(worldColumn, RenderColumn*)`, placed right before `renderWorldColumnRowsDirect()`. Faithfully reimplements `objectCellForWorldColumnTile()`'s exact priority order (land > ship-wreck smoke > `harLevelObjects` > procedural land target/flak > procedural town flak > native-carrier-deck horizon row) but resolves it *for the whole column in one pass* instead of per-row: a `claimed[25]` boolean array tracks which rows are already finalized by a higher-priority rule, so a lower-priority rule (e.g. a `harLevelObjects` entry) is skipped for any row a higher-priority rule (land) already claimed - exactly reproducing the original per-row function's "first match in priority order wins" semantics, just computed via one forward pass over `harLevelObjects` instead of a fresh 95-entry scan per row. `objectCellForWorldColumnTile()` itself is untouched and still serves its other callers (collision probes, `objectCellForWorldPoint()`, enemy-plane flak-avoidance) unchanged.
- `renderWorldColumnRowsDirect()`/`renderWorldColumnDirect()` now build once and draw from the cache (a plain `drawGameScrollTile()` loop, no per-row resolution).
- `renderRingWorldColumn()` (used by `initRingWorldBuffer()`'s initial fill and `dirtyRedrawWorldColumn()`'s target/crater dirty-redraws) now builds the column once and draws it to both the primary and wrap-duplicate X positions from that single result.
- `serviceRingWorldStream()` (the incremental per-frame streaming path, which spreads one column's draw across 2-7 frames depending on scroll speed) now builds the column once - the moment a new column starts streaming (`ringStreamRow==0`) - into a cached `ringStreamTileColumn`, then every subsequent frame's partial row range for that same column (and its wrap duplicate, if any) draws from the cache instead of re-resolving.
- Explicitly out of scope for this part: `drawDirectColumnRangeObjects()`'s own 95-entry scan (carrier/gunship/town-block overlay) is untouched here - that's Part 3's target, not this one.

Measurement note: attempted a before/after comparison via the headless perf harness (`hitches`/`avgFps` in `perf_log.csv`) against an earlier CSV captured in the same session, but the comparison turned out **inconclusive** - hitches were actually higher in several mid-run intervals after this change, which contradicts the algorithmic analysis. Root cause of the noise: the "before" CSV was captured several unrelated changes earlier in the same session (palette copper writes, the `InitInput()` POTGO fix), so it isn't a clean isolated comparison; more fundamentally, this headless harness runs WinUAE with `cycle_exact=true`, which is itself bottlenecked by the *host* machine's real-time emulation throughput rather than purely reflecting the guest 68000 program's instruction count - background host load can easily swamp a real but smaller per-frame CPU saving. This harness is well-suited to catching crashes/hangs/gross multi-second freezes (as used throughout this session) but is not a reliable instrument for measuring a micro-optimization like this one. The actual verified win is at the algorithmic level: the 95-entry `harLevelObjects` scan (plus the column's level-segment/stage/terrain lookup) dropped from up to 25x per column to exactly 1x, and the wrap-duplicate column now shares that single resolution instead of paying for it twice. Real-world "smoother scrolling" confirmation needs the user's own hands-on WinUAE/real-hardware feel-test, which is the actual authority on the stated goal anyway.

**User confirmed at full gameplay speed** (headless autoplay only ever runs at minimum scroll speed, which the user separately pointed out and correctly guessed would understate any gain): "definitely looks better" - real hands-on validation of Part 1+2 before Part 3 began.

### Part 3 - Index harLevelObjects by world column

- Added a per-column index built once, lazily, from the existing 95-entry `harLevelObjects` array (itself untouched): `harLevelObjectColumnHead[]`/`harLevelObjectNext[]` (a linked list per exact world column, sized to `GAME_LEVEL_WIDTH_TILES+16`) for exact-column lookups (flak/targets/most entries), and a short `harWideObjectIndex[]` (~21 of 95 entries) for the few objects that span multiple columns (native-carrier frigate, gunship, town blocks) and need a range check instead of an exact match. The array isn't sorted by column in file order (e.g. the gunship entry at column 621 appears after pier entries up to column 628), so a binary search wasn't an option - hence building an actual index. Built via prepend (each new entry becomes its column's new head), which reverses relative order only among entries sharing the exact same column *and* the exact same computed row - an already-underspecified data-authoring collision in the original per-row scan too, not a new risk.
- Applied to all 5 real scan sites: `objectCellForWorldColumnTile()`'s main match loop and its horizon-row native-carrier check, `buildWorldTileColumn()`'s equivalents (Part 1's column-cache builder), `drawDirectColumnRangeObjects()` (every branch in it only ever matches a wide object anyway, so it was safe to scan just the short list), and `playerOnNativeCarrierDeckPixels()` (a per-frame gameplay collision check, not a rendering path, but the exact same NATIVE_CARRIER match criteria as the wide-object list, so a free win). Left `ownFrigateCellNearWorldPoint()` alone - it matches *all* `HAR_OBJ_OWN_FRIGATE` entries (pier/deck included, not just the wide native-carrier ones), a broader condition than `harWideObjectIndex[]` covers, and it's a low-frequency probe rather than a rendering hot path, so a dedicated index for it wasn't worth the added risk here.
- Verified via headless run: clean rebuild, no crash/hang, no HUD-timing-regression signature (`livBplcon0`/`expBplcon0` etc. all matched, same as every prior healthy run this session).

### Part 4 - Remove 32-bit modulo from the ring-buffer/sea-tile hot paths

- Confirmed via direct disassembly (not just code review) that a plain 68000 has no 32-bit divide instruction at all, so `LONG % constant` always calls a slow `__modsi3`/`__udivsi3`-style library routine regardless of the divisor being a compile-time constant - three real sites do this: `seaTileForColumn()`'s `(worldColumn+tileY) % 11` (hottest - once per sea-tile cell), `ringWorldTileXForColumn()`'s `worldColumn % GAME_WORLD_SCROLL_PAGE_BYTES`, and `scrollLocalByteOffset()`'s equivalent (the latter already declared `USHORT` but C's usual arithmetic conversions silently promote the subtraction back to 32-bit `int` before the modulo regardless).
- First attempt (inline cast expressions, e.g. `((UWORD)worldColumn + tileY) % 11`) looked correct on paper but a disassembly check against this exact toolchain/flags (`-m68000 -Ofast`) proved it still compiled to a `__modsi3` call for the `seaTileForColumn` case - the cast narrows the *value* but C's promotion rules still make the *operation* 32-bit. Verified this empirically with a small isolated test file compiled with the project's real flags before trusting any fix, rather than assuming from source alone.
- Working fix: an explicit 16-bit-typed *local variable* holding the pre-narrowed value, then applying `%` to that variable, reliably makes GCC recognize the bounded range and emit hardware `DIVU.W`/`DIVS.W` instead - confirmed for all three sites by disassembling the actual built `harrier_amiga.elf` (not just the isolated test) and finding zero `__modsi3`/`__divsi3`/`__udivsi3`/`__umodsi3` references anywhere in `buildWorldTileColumn`'s or `objectCellForWorldColumnTile`'s address range.
- A headless verification run appeared to hang (100s+ with no perf-log CSV written) - before reverting, the user confirmed they were actively watching the WinUAE window at the time and saw no hang at all. Extending the wait (to ~180s) confirmed the run completed normally; the apparent stall was the headless harness's own wall-clock timeout being too short under momentary host load, not a code regression - a useful reminder that this specific harness measures *real* elapsed time against `cycle_exact=true` emulation, so a slow host tick can look identical to a hang from the CSV side alone. Real-time user observation resolved it faster than continuing to dig blind would have.

### Part 5 - World view: 4 display planes instead of 5

Status: done, deliberately scoped narrower than the full idea to keep risk low - see "explicitly not done" below.

- Confirmed (again) every `GAME_COLOR_*` constant tops out at 15 and the embedded `game_tiles.bpl`'s 5th bitplane is genuinely all-zero across all 102 tiles (from Sprint 14.93's earlier byte-scan) - the world view never needed a 5th bitplane's worth of colour range.
- Implemented the *same* trick `HUD_PLANES` already uses successfully (see its own comment in `buildGameHudCopper()`): added `GAME_WORLD_DISPLAY_PLANES`(4) and changed only the world section's `BPLCON0` write and its `copSetPlanesEx()` call to use it instead of `SCREEN_PLANES`(5) - `copSetModulo()` was already correct as-is, since it always derives its stride from `SCREEN_PLANES` (the buffer's real storage layout) regardless of how many planes are actually fetched/displayed, exactly like the HUD split already relies on.
- **Deliberately did not** touch the world buffer's allocation, its 5-plane storage layout, or any tile-blit/`putPixelScroll()` write-side code - those all stay exactly as they were. This captures the DMA-bandwidth saving (fewer bitplane fetches stealing cycles from the CPU during the world's own active display, likely the dominant contributor to "smoother scrolling" frame-to-frame) without touching tile data format or asset regeneration at all - the genuinely higher-risk part of the original idea (also shrinking the buffer size and per-tile byte-copy cost by reformatting `game_tiles.bpl` itself to native 4-plane storage) was not attempted and would need its own separate, carefully-tested pass if wanted later.
- **Found and fixed a real edge case while implementing this**, not just a theoretical risk: `buildDisplayCopperEx()` (the menu/loading-screen copper build) still uses the full `SCREEN_PLANES`(5) through the *same* `activeCopperPlaneHigh[]`/`activeCopperPlaneLow[]` tracking arrays (`COPPER_TRACK_SCROLL`) that the world section uses. Without an explicit fix, switching from menu to gameplay would leave index 4 holding a stale pointer into the menu's own (now out-of-scope) copper buffer, and `updateGameScrollCopper()`'s per-frame `setCopperPlanePointers()` - which only null-guards, it doesn't know the pointer is stale - would keep writing live scroll data into that stale location every frame. Fixed by explicitly clearing `activeCopperPlaneHigh[4]`/`activeCopperPlaneLow[4]` to 0 right after the world's own (now 4-plane) `copSetPlanesEx()` call in `buildGameHudCopper()`.
- Verified two ways: the perf-log CSV's HUD-timing diagnostic columns (`livBplcon0`/`expBplcon0`, `livDdfstrt`/`expDdfstrt`, `livDdfstop`/`expDdfstop` - the exact instrumentation built during the Sprint 14.91.4 ghost-bug investigation) all matched with no crash/hang across a full headless run; and the user was watching the live WinUAE window during that same run and confirmed "Looking good!" - real visual confirmation, not just an absence-of-regression signal from the diagnostics.

### Part 6 - Preconvert carrier/gunship promoted sprites to tile blits

Status: done, user-confirmed visually. The genuinely bigger, riskier part of this sprint - new asset-conversion tooling, a new generated asset, and rewritten draw calls, not just a targeted code/config change like parts 1-5.

- Confirmed with a direct transparency scan of the source pixel data (`amiga/assets/generated/cpc/cpc_plus_sprites.json`) before writing any conversion code, not assumed: `carrier_body` is fully opaque end to end (converts cleanly), but `carrier_back`/`carrier_front`/`carrier_top`/`carrier_top_2`/both gunship pieces all have real transparency *mixed within individual 8x8 tile cells* (background sea/sky meant to show through), not just whole-tile gaps like the already-tile-based town blocks have. A plain overwrite blit (`drawGameScrollTile()`'s existing approach) would have been wrong for those pieces - it would paint over the background with whatever colour a "transparent" pixel happened to bake in, wherever the composite doesn't exactly fill its 8x8 cell.
- Added `tools/cpc_promoted_sprites_to_tiles.py`: reads the same audited JSON source `tools/promote_cpc_assets.py` already uses for the raw per-pixel pen arrays baked into `amiga/assets/cpc_promoted_assets.h`, composites the carrier's 6 pieces (back/body x2/top/top2/front, replicating `drawPromotedCpcCarrierRangeAt()`'s exact original relative offsets, including baking the body piece's `xScale=2` horizontal stretch into the pixel data at conversion time since a static tile blit can't scale at draw time) and the gunship's 2 pieces (left/right) into two small canvases, then slices each into 8x8 cells and packs them into a **pre-masked** planar tile format: 8 rows x 5 bytes/row, the exact same 40-byte footprint as a regular `game_tiles.bpl` tile - but where a regular tile's 5th plane byte is always zero (game tiles never needed it, see Part 5's byte-scan), this repurposes that same byte as a per-row 1-bit opacity mask instead of a wasted colour plane, so the format costs nothing extra to carry. A whole-tile "fully transparent, skip drawing entirely" flag is also precomputed per grid cell (`harCarrierTileSkip[]`/`harGunshipTileSkip[]`), mirroring how town blocks already skip empty grid cells.
- Sanity-checked the generated tile data itself before wiring it into any C code: rendered it back out as ASCII art (colour-per-character) in a scratch script and visually confirmed a recognisable carrier hull/superstructure and gunship silhouette, not garbled data, before trusting the pen-mapping/canvas-compositing/scaling logic.
- Added `drawGameScrollTileMasked()` in `amiga/main.c` (`amiga/main.c`, right beside `drawGameScrollTile()`) - a masked read-modify-write per row (`dest = (dest & ~mask) | (src & mask)` per active colour plane) instead of a flat overwrite, skipping any row whose mask byte is entirely zero. Still a byte-at-a-time (8 pixels/plane) operation rather than `putPixelScroll()`'s bit-level per-*pixel* read-modify-write, so still a real reduction in operation count even with the extra read the masking needs.
- Rewrote `drawPromotedCpcCarrierRangeAt()`/`drawPromotedCpcGunshipRangeAt()` to blit from the new pre-built tile grids instead of calling the old per-pixel `drawCpcPlusSpriteScrollRange()` (now fully unused - deleted, per this project's standing "don't leave dead code around" preference) 6 and 2 times respectively per column. Also **simplified their call signatures** while rewriting: the old ones took a "pixel x of the whole composite's origin + a start/end tile clip range" designed for the per-pixel renderer's own clipping math; confirmed both are always called for exactly one buffer column at a time from `drawDirectColumnRangeObjects()` (`endTileX` is always `physicalTileX+1`), so the new versions just take the buffer column and the composite's own column index directly (`worldColumn - objectColumn`, which the caller already computes) - no clipping math left to do at all, since there's only ever one column to draw per call.
- Confirmed both `drawPromotedCpcCarrierAt()`/`drawPromotedCpcGunshipAt()` (the older non-`Range` per-pixel versions) and their only caller, `drawWorldCarriers()`, have no remaining call sites anywhere in the file - genuinely dead code left over from an earlier static-scene renderer (matches a note from the original Sprint 14.94 research pass). Left them untouched rather than "fixing" unreachable code.
- Verified: clean rebuild with no errors; a headless run's perf-log CSV didn't get captured this time (the user closed WinUAE after watching the run finish, before the log's own end-of-run write happened - not a hang, confirmed by the user watching it complete normally), but the user directly observed the carrier at the very start of a flight (world column 8, the "friendly start carrier" - visible almost immediately in any playthrough) twice during testing and confirmed "Looking good" / "Looking good!" both times - real visual confirmation of the new tile-based rendering, which is what actually matters here since this part changes what gets drawn, not just how fast.

## Sprint 14.95 - CPC Authenticity Review (`Amiga-Improvement-Plan-23.04.2026.md`)

Status: All 7 parts of the review are now implemented, including both remainder items (ground-target crater-vs-smoke, ship destruction granularity) and the town-entry palette fade that turned out to be the real explanation for the "red flash" report. Nothing from this review is currently open.

### Part 7 remainder: ground-target crater -> smoke (done)

Confirmed against the review: CPC's `drawsmokesprite` replaces a destroyed radar/gun/tank/launcher's own tile with smoke (52, plus 51 above if that cell is sky) - it never touches the ground surface at all. The Amiga port instead cratered the surface tile via `markLandCraterAtColumnRow()`, one row below where the target actually stood. Both `updateWeapons()` branches (rocket/bomb) now call `addCpcHitSmokeAtColumnRow(worldColumn, tileY)` using the hit's own already-resolved row (`rocketTileY`/`bombTileY` - already the target's exact row, since `objectCellForWorldColumnTile()` only ever matches `HAR_OBJ_GROUND_TARGET` at `tileY==terrainY-1`) instead of re-deriving the ground surface row and cratering it. `markTargetDestroyedAtColumn()` is unchanged (still needed so the target stops being regenerated by the procedural table). `landSurfaceYForWorldColumn()` stays in use elsewhere (unrelated caller), not dead code.

### Post-implementation fix: flak invisible, town scrolling stutter (done)

User's first live test pass reported flak as "very seldom" and no noticeable town change. Root cause (found via code review, not guesswork): the ring buffer streams up to `RING_WORLD_STREAM_MAX_AHEAD_TILES`(64) columns ahead of the visible screen edge, so both new systems were resolving/mutating columns that had frequently already been painted many frames earlier.

- **Flak was invisible, not rare.** `trySpawnFlak()` spawns exactly at the rightmost *visible* column, which the ring buffer typically already rendered up to 64 columns/many frames before. `addRuntimeFlak()` only appended to the lookup lists - it never touched the bitplane buffer or called `dirtyRedrawWorldColumn()`. Flak existed for collision purposes while the already-painted screen kept showing plain sky, only becoming visible by coincidence if something unrelated later redrew that column. A numeric simulation of the spawn probability itself (land ~1-in-16, town ~1-in-8) showed density roughly comparable to the old table-based system (21 vs 26 markers over the 295-column land run) - confirming probability wasn't the problem. Fixed by passing `worldBuffers` into `trySpawnFlak()` and calling `dirtyRedrawWorldColumn()` immediately after a successful spawn - the same "something changed here, redraw it" idiom every other mutation in this file already uses. Moved the pop sound to fire after that redraw succeeds.
- **Town stutter, a separate real issue.** Town-block tiles were drawn synchronously in one lump the instant a streamed column finished, entirely outside `serviceRingWorldStream()`'s per-frame row budget - cheap with the old ~15 sparse blocks, expensive once nearly every town column carried building tiles. Moved town-block resolution into `buildWorldTileColumn()` itself as a new priority step, so it's now ordinary `RenderColumn` tile data riding the same distributed per-frame budget as everything else. This also let the explicit smoke-check in the old draw-pass version be dropped - the existing `claimed[]` priority ordering already gives ship-wreck/hit smoke precedence for free. The now-redundant `drawProceduralTownBlockColumn()` and its call site were removed.
- Verified: clean rebuild, headless autoplay confirms no crash/hang each time. User confirmed both fixes working on re-test ("Ser bra ut").

### Town-entry palette fade - the real explanation for the "red flash" (done)

The "red flashing screen" flagged back in Part 5 was never a rendering bug - user re-read the CPC source directly and confirmed it's `startpalettefade`, a deliberate, missing feature: CPC steps through 5 palette transitions (~13 VBlanks/~0.26s each at PAL, ~1.3s total) the instant the terrain flattens to row 14 and the town stage begins, ending on a stable dusk palette (a strong yellow/orange sea/city-light colour, confirmed CPC value GRB `&0FF0`) that holds for the whole town section, then restores day colours afterward.

- Implemented as a `GameState`-tracked step/timer (`cityFadeStep` 0-5, `cityFadeTimer` counting down from `CITY_FADE_STEP_FRAMES`(13)) driven by `updateCityFade()`, called once per frame alongside `trySpawnFlak()`. Compares the terrain kind at the screen's current left edge against `HAR_TERRAIN_TOWN` to decide the target step (5 = dusk, 0 = day) and steps one unit per 13 frames toward it - naturally fades back out on leaving town, matching the review's "old palette values are restored after the town."
- Scoped to COLOR00 (sky, 3 per-band gradient values) and COLOR15 (clouds/sea/panel accent, already the two registers this port's existing per-band gradient dedicates to atmosphere) - deliberately leaves COLOR05/COLOR10 untouched, since both are already documented as double-booked for HUD gauge semantics (`HUD_COLOR_BACKGROUND`/`HUD_COLOR_SAFE`) elsewhere in this same copper-build code, and retinting them was already flagged there as something that breaks the HUD.
- Technically: since `buildGameHudCopper()` only runs once per session (not every frame - confirmed by reading its actual call sites), a live fade can't just adjust a "current palette" and rebuild; it has to patch the already-built copper list's own embedded colour operand words directly. Added `activeCopperSkyTopColor`/`SkyMidColor`/`SkyLowColor`/`CloudTopColor`/`SeaLowColor`/`PanelSeaColor` - pointers captured at copper-build time in `copSetGameSkyGradient()`/the HUD panel-band write, exactly mirroring the existing `activeCopperPlaneHigh`/`Low` scroll-pointer-patching pattern already used elsewhere in this same file. `applyCityFadeStep()` patches all 6 words directly; the copper hardware re-executes the same program every frame regardless, so a patched value just takes effect on the next frame with no extra per-frame work needed.
- Colour data: only the CPC's dusk sea/city value (`&0FF0` GRB, read directly as Amiga `0x0FF0` RGB since R and G happen to be equal in this specific value) is a confirmed source value - the sky's intermediate/target hues (purple -> red -> orange, per the review's qualitative description) are this port's own linear-interpolation approximation from day to a chosen dusk target, since exact CPC intermediate palette-table entries weren't available to copy. Worth a visual tuning pass once seen in motion - flagged to the user as an approximation, not a verified match.
- Verified: clean rebuild, headless autoplay confirms no crash/hang (autoplay dies before reaching world column 411, so this doesn't exercise the actual fade in motion - needs the user's own play-test to confirm the transition timing/colours read well and that the fade correctly reverses on the way out of town).
- **Timing fix after live test**: user confirmed the fade works but felt late. `updateCityFade()` originally triggered off the screen's *left* edge (`scrollX>>3`, i.e. the player's own position) - by the time that reached the town segment, the rightmost `GAME_MAP_WIDTH`(40) columns of town were already visible on screen for the time it took to scroll a full screen-width, since the player is always looking ahead of their own position. Switched to the same rightmost-*visible*-column convention `trySpawnFlak()` already uses (`(scrollX>>3)+GAME_MAP_WIDTH`) - matches CPC's own trigger point (world-generation time, at the leading edge), not the player's arrival. Clean rebuild, headless smoke test passed.

### Post-implementation fix: flak invisible, town scrolling stutter

User's first live test pass reported flak as "very seldom" and no noticeable town change. A follow-up code review (against the actual post-Part-2/5 code) traced both to the same root cause: the ring buffer streams up to `RING_WORLD_STREAM_MAX_AHEAD_TILES`(64) columns ahead of the visible screen edge, so by the time a column reaches the point where `trySpawnFlak()`/the town generator actually resolve it, that column's pixels were frequently already painted many frames earlier.

- **Flak was genuinely invisible, not rare.** `trySpawnFlak()` spawns exactly at `(scrollX>>3)+GAME_MAP_WIDTH` - the rightmost *visible* column - which the ring buffer had typically already rendered up to 64 columns/many frames before. `addRuntimeFlak()` only appended to `runtimeFlakColumns[]`/`Rows[]`/`Tiles[]`; it never touched the bitplane buffer or called `dirtyRedrawWorldColumn()`. The result: `objectCellForWorldColumnTile()` (collision, sound-trigger reads) correctly saw the new flak, but the already-painted screen kept showing plain sky - flak existed for collision purposes while staying invisible, only becoming visible by coincidence if something unrelated later redrew that same column. A numeric simulation of the spawn probability itself (land ~1-in-16, town ~1-in-8) showed roughly comparable density to the old table-based system (21 vs 26 markers over the 295-column land run) - confirming the probability wasn't the problem. Fixed by passing `worldBuffers` into `trySpawnFlak()` and calling `dirtyRedrawWorldColumn(worldBuffers, checkColumn)` immediately after a successful `addRuntimeFlak()` - the same "something changed here, redraw it" idiom every other mutation in this file already uses (craters, smoke, ship/town-block hits). Moved the `playSfx(SFX_MENU)` pop sound to fire after the redraw succeeds, so sound and visible appearance happen together.
- **Town scrolling stutter, real but separate.** `drawDirectColumnRangeObjects()` called the town-block draw synchronously the instant a streamed column finished, entirely outside `serviceRingWorldStream()`'s per-frame row budget - harmless with the old ~15 sparse hand-placed blocks, but with Part 5's continuous generator nearly every town column now paid for up to 5 extra tile draws in one synchronous lump right as it scrolled past, undermining the row-budget split's whole purpose. Moved town-block resolution into `buildWorldTileColumn()` itself as a new priority step (between harLevelObjects and the procedural land-target check) - it's now just ordinary `RenderColumn` tile data, riding the same distributed per-frame budget as terrain/sea/everything else. This also let the explicit `shipWreckSmokeTileAtColumnRow()` check in the old draw-pass version be dropped entirely - the existing `claimed[]` priority ordering already gives ship-wreck/hit smoke (priority 2) precedence over town-block tiles (priority 4) for free. `drawProceduralTownBlockColumn()`/its call site were removed as fully superseded.
- Verified: clean rebuild, headless autoplay confirms no crash/hang. Both fixes are rendering/timing corrections with no gameplay-logic changes - re-test needed to confirm flak is now visible where it spawns and the town no longer stutters (or reads noticeably denser than before).

Why: user supplied a detailed technical review document comparing the Amiga port against the real CPC source (`HarrierAttackSourceNew2_alt_CRTC_CART16.asm`) across 7 topics: sound repeat bug, flak mechanism, terrain/slope-tile generation, town layout, enemy frigate/carrier, and smoke generation. Given how much flak/terrain/scrolling code changed earlier this same session (Sprints 14.92-14.94), every claim was verified against the actual current code before planning anything - summarized per part below with a TRUE/FALSE/PARTIAL verdict, not just taken on faith.

### Part 1 - Sound repeat bug (done)

Two genuinely separate bugs were causing what read as one symptom ("bomb/rocket sounds repeat"):

- **Input retrigger (bomb specifically)**: `Pressed()` is a plain one-frame edge check (`now && !previous`, confirmed unchanged) with no debounce anywhere upstream for the primary fire button, keyboard, or mouse. Added `ReadJoyFire1Debounced()` - requires `JoyFire1()` (the second joystick button, wired to bomb) to read stable for 2 consecutive frames before trusting it, riding out POT-line/contact flicker without adding perceptible input lag. Wired into `input->bomb`, reset in `InitInput()` alongside the existing POTGO pull-up fix.
- **Paula buffer loop (fire/hit specifically, NOT bomb)**: computed each SFX's real playback duration (padded one-shot buffer bytes / Paula's sample rate at that sound's `period`) against its software stop timer (`sfxChannelFrames`, decremented once/frame) before touching any code - `SFX_FIRE` played for ~342ms against a 360ms stop timer, `SFX_HIT` ~346ms against the same 360ms - both genuinely finished and Paula looped back into the real sample's own audio for the last 14-18ms before the software stop cut DMA, audibly retriggering the start of the sound. `SFX_BOMB`/`SFX_IMPACT`/`SFX_MENU`/`SFX_GAME_OVER` all had comfortable margins and were never actually affected by this - confirms the earlier bomb complaint really was the separate input-retrigger bug, not this one.
- Fixed per the review's recommended approach rather than just re-tuning padding per-sound (which would only move the same race to a different, still-fragile margin): removed the `sfxOneShotData`/`prepareSfxOneShotBuffers()`/`SFX_ONE_SHOT_SILENCE_BYTES`-padding mechanism entirely. `startPendingSfxChannel()` now plays the real sample at its own natural length, then immediately writes a permanently-silent 1-word loop buffer (`sfxSilenceLoop`) to `ac_ptr`/`ac_len`. Paula only reloads those registers on a DMA off->on transition (just triggered) or when the *currently-latched* length counter naturally reaches zero - so the immediate second write doesn't restart/interrupt the real sample, it just queues the silent buffer to take over the instant the real sample naturally finishes, regardless of how tight that timing is for any given sound. Eliminates the whole bug class rather than patching the two currently-affected sounds.
- Verified: clean rebuild; user confirmed directly by listening ("Sounds good") after testing both fixes together - the most reliable confirmation available for an audio bug, more so than any headless diagnostic.

### Part 2 - Flak gun mechanism (done)

All claims **confirmed true** against current code before implementing: flak was precomputed into `cpcLandFlakTable[]`/`cpcTownFlakTable[]` tied to ground-target/town-block placement several columns ahead (not spawned live at the screen edge like real `launchflakattack`); flak and ship-wreck smoke shared `HAR_OBJ_FLAK` with no distinct ID; the player's rocket/bomb could destroy flak and score `FLAK_SCORE_VALUE` (the real CPC lets the weapon disappear into flak with no explosion/score/smoke - flak is simply not player-destructible); `updateFlakPopSound()` fired every time a new column scrolled into view rather than only at the moment flak was actually created. User explicitly requested this be implemented per the review's own recommendations after confirming the ship-destruction-granularity analysis below.

- **Live spawn at the screen's right edge**: removed `cpcLandFlakTable[]`/`cpcTownFlakTable[]`/`generateCpcTownFlakTable()`/`cpcLandProceduralFlak()`/`cpcTownProceduralFlak()` entirely, along with the flak-scheduling code inside `generateCpcLandHeightTable()`'s target-placement branch. Added a small runtime list (`runtimeFlakColumns[]`/`Rows[]`/`Tiles[]`, `GAME_RUNTIME_FLAK_MAX`(64)) and `trySpawnFlak(game)`, called once per frame from the main loop (replacing `updateFlakPopSound()`'s old call site) - it rolls a spawn attempt only when the rightmost visible column advances (`(scrollX>>3)+GAME_MAP_WIDTH`, same edge `updateFlakPopSound()` used to check), gated on land/town stage, and places flak into an empty sky cell at a random altitude above terrain (kept the existing tuned `CPC_LAND_FLAK_MIN/MAX_ROW_OFFSET` range - a past fix for a "safe zone" near the top of the screen the plane could climb into - rather than reverting it). Per-column spawn probability (land ~1-in-16, town ~1-in-8) is an approximation of the old per-target/per-block-gated density converted to a flat per-column roll, since there's no longer a "how many targets/blocks exist" quantity to gate on - may need further live-tuning feedback. Old entries never get removed by the player (see below), so `pruneRuntimeFlakBehindColumn()` drops anything more than ~2 screens behind the current scroll position to keep the list bounded over a full flight.
- **Split `HAR_OBJ_SMOKE`(11) from `HAR_OBJ_FLAK`(10)**: ship-wreck/frigate/town-block hit smoke now reports the new `HAR_OBJ_SMOKE` id everywhere it's produced (`objectCellForWorldColumnTile()`, `buildWorldTileColumn()`) instead of sharing `HAR_OBJ_FLAK`. This has real, correct side effects beyond just labeling: `playerObjectMapCollision()`'s flak-damage check and `updateEnemyPlane()`'s flak-avoidance steering now only ever match real flak, not smoke, which they'd have accidentally matched before (harmless-looking today, but genuinely wrong - a plane shouldn't dodge smoke, and per the review, CPC's own player-collision-into-smoke-as-flak-damage behavior is described as a peculiar original shortcut, not something to deliberately preserve).
- **Flak (and smoke) made player-indestructible**: removed the `if (cell.id == HAR_OBJ_FLAK) { clearFlakOrSmokeAtColumnRow(...); score += FLAK_SCORE_VALUE; ... }` branches from both the rocket and bomb paths in `updateWeapons()`, replaced with `game->rocketShot.active = 0`/`game->bombShot.active = 0` and an explicit skip of `startWorldImpact()` - the weapon vanishes with no explosion, sound, or score, matching `checkenemyhit`'s `dec a; cp 9; jr z, bombhitsealand` path exactly. Extended the same treatment to `HAR_OBJ_SMOKE`: CPC's real engine can't distinguish flak from smoke at all (shared id 10) since `checkenemyhit` only ever sees a single object-map ID, so a hit on either is absorbed identically in the original - the ID split above is for correctness in `HAR_OBJ_FLAK`-vs-`HAR_OBJ_SMOKE`-*sensitive* code paths (collision/AI), not a reason to treat them differently at weapon-hit time. As a result `clearFlakOrSmokeAtColumnRow()`/`markFlakClearedAtColumnRow()`/`isFlakClearedAtColumnRow()`/`clearShipWreckSmokeAtColumnRow()` all became dead and were removed - smoke and flak are now both permanent until they scroll off-world, matching Part 7's "keep persistent smoke until it scrolls away."
- **Pop sound moved to spawn time**: `updateFlakPopSound()` (which re-scanned the whole rightmost column every frame looking for existing flak) is gone - `addRuntimeFlak()` now plays the sound (still `SFX_MENU`, no new sample) exactly once, only on the frame a spawn actually succeeds.
- Verified: clean rebuild; headless autoplay run completes with no crash/hang - `trySpawnFlak()` runs continuously for the whole ~60s the run survives over land terrain, the most-exercised new code path in this pass. Visual/gameplay confirmation (flak appears more organically as columns scroll in, sounds once per spawn not per glance, and a rocket/bomb now visibly vanishes into flak with no effect) needs the user's own play-test.

### Part 3/4 - Terrain/slope-tile generation (done)

Both claims confirmed true before implementing, matching the review exactly: `CPC_LAND_PROCEDURAL_FLOOR` was a fixed constant (11) capping every difficulty to the same 3-tile max height, with no difficulty-derived floor anywhere; `landSurfaceTileForColumn()` inferred hill-up/down purely by comparing `terrainYForWorldColumn()` across the previous/current/next columns, which could (and did) tag two consecutive columns as a slope tile for one real height change (e.g. heights `14,14,13,13` reading as "hill up" at both the transition column and the column after it).

- Added `cpcLandSurfaceTable[]` parallel to the existing `cpcLandHeightTable[]`, populated with the actual chosen tile directly inside `generateCpcLandHeightTable()`'s existing mode-selection loop (mode 1 -> "hill down" 28-31, mode 2 -> "hill up" 24-27, everything else -> a flat grass tile 32-35) instead of re-derived later by comparing neighbours. `landSurfaceTileForColumn()` now does a direct table lookup for `HAR_TERRAIN_CPC_RANDOM_LAND` columns - the neighbour-comparison fallback stays for the other terrain kinds (coast rise/fall, town, descend-to-town), which aren't generated by this table and have simple deterministic height formulas with no equivalent ambiguity to begin with.
- Kept the hill-tile *variant* selection exactly as this port had already deliberately settled on it (`hillPhaseByCoverage[height & 3]`, now at file scope so both the generator and `landSurfaceTileForColumn()`'s remaining fallback path share it) rather than reverting to the CPC's genuine per-column randomness - the port's own existing comment explains this was a considered choice for a smoother multi-column ramp, not an oversight, so it wasn't undone without being asked to.
- Switched the flat-grass tile variant (32-35) from a `worldColumn`-derived fixed pattern to genuinely random (an unused slice of the same `rng` stream already driving mode/target selection) - this one *was* just an arbitrary earlier placeholder, not a considered decision, and the review flagged it as a real deviation from the CPC's own `ld a,r; and 3` pick.
- **Also wired up the difficulty floor**, which needed more than a constant change: the menu's `skillLevel` selector (1-5, already fully working as a *display*) turned out to have never actually been connected to any gameplay system - `startGameSession()` didn't even receive it as a parameter. Added `skillLevel` to `GameState`, threaded it through `startGameSession()` from both its call sites, and added `cpcLandMinimumRow(skillLevel)` (`12 - skillLevel`, matching the review's own derived formula and table exactly: skill 1 caps at row 11/3 tiles, skill 5 at row 7/7 tiles) in place of the fixed floor. Since the land-height table was previously generated lazily exactly once for the whole program's lifetime (never regenerated), `startGameSession()` now also resets `cpcLandHeightTableReady` so a skill-level change between sessions actually takes effect on the next flight rather than reusing stale terrain generated under a different difficulty.
- Verified: clean rebuild, headless run completes normally with no crash/hang and no HUD-timing regression signature. Visual confirmation (does the slope silhouette actually look cleaner, does higher skill level produce visibly taller terrain) needs the user's own play-test - this is exactly the kind of thing that's hard to fully verify from code alone.

### Part 5 - Town layout and behaviour (continuous generation done; red-flash bug still unresolved)

Confirmed true before implementing: town blocks were 17 hand-placed `harLevelObjects[]` entries in `level_route.h` (no procedural continuous generator) - and more precisely than the review itself stated: the 17 entries' own widths (`harCpcTownBlockWidths[]`) summed to only 44 of the town segment's 200 columns, with real gaps of empty flat land between each isolated block (e.g. block 0 at column 413 is only 1 column wide, but the next block doesn't start until column 424 - an 10-column gap), not a tightly-packed-but-short run as the spacing alone might have suggested. Town flak generation was tied 1:1 to those hand-placed blocks (50% chance per block) rather than a continuous per-column roll - this part was already superseded by Part 2's `trySpawnFlak()`, which spawns flak for land *and* town uniformly regardless of building placement.

**Continuous generation (done)**: added `generateCpcTownBlockTable()` - precomputed once (same lazy-init pattern as `generateCpcLandHeightTable()`), keyed by local column within the 200-column town segment. Starting 2 columns in (matching the existing hand-placed data's own small gap at the coast-to-town seam), repeatedly picks a random block of 8 (`(rng>>8)&7`, approximating CPC's `ld a,r; rra; rra; and 7`) and fills its full width of columns before picking the next - dense, continuous coverage instead of ~15 sparse instances. Two lookup accessors (`cpcTownProceduralBlockId()`/`cpcTownProceduralLocalColumn()`) replace the old harLevelObjects-based data source everywhere town blocks are consulted:
- **Rendering**: `drawDirectColumnRangeObjects()` now calls a new `drawProceduralTownBlockColumn()` unconditionally per column (alongside the existing carrier/gunship `harWideObjectIndex[]` loop, which no town-block entries exist to match anymore) - looks up the segment/local column, resolves the block+row via the procedural table, and draws exactly the one column's worth of tiles, skipping cells already covered by smoke (same protection the old per-block renderer had).
- **Collision**: `townBlockCellNearWorldPoint()` (added earlier this session for the town-block-destructibility fix) was rewritten from an O(n) scan over hand-placed `harLevelObjects` entries to a direct O(1) lookup against the same procedural tables - simpler than the old version, since the generator already guarantees every in-range column maps to a valid block/local-column pair by construction.
- Removed the now-fully-dead old renderer (`drawPromotedCpcTownBlockAt()`/`drawPromotedCpcTownBlockRangeAt()`) and the `HAR_OBJ_TOWN_BLOCK` special cases in `isWideLevelObject()` and `buildWorldTileColumn()`'s harLevelObjects loop, since no such entries exist in `level_route.h` anymore. The 4 hand-placed `HAR_OBJ_FLAK` entries that used to sit between town blocks were kept (harmless, additive alongside `trySpawnFlak()`'s live spawning).
- Verified: clean rebuild, headless autoplay run completes with no crash/hang - though autoplay dies from unavoided collisions well before reaching world column 411 (the town segment start), so this only confirms the new code doesn't destabilize the rest of the game; the generator's actual in-game density/appearance needs the user's own play-test, ideally flying far enough to reach the town section.

**Red-flash bug: still not found, still needs a live repro.** Searched exhaustively again for the "red flashing screen" the review raises (`startpalettefade`, `duskpal`, `nightpal`, any dynamic palette-fade logic) - zero matches anywhere in `main.c`, no palette-fade mechanism exists for town entry at all (one static `gamePalette` used throughout). One theory already ruled out: `drawDirectColumnRangeObjects()`'s per-column draw calls (both the old per-block renderer and the new procedural one) always clip to exactly one physical column per call, at both the primary and wrap-duplicate ring-buffer positions - no unclipped wide-block draw exists that could overwrite past the ring seam. If the user has actually seen a full-screen red flash during the town section, its cause isn't identified yet and needs a live repro (which part of the town, does it correlate with a specific block/flak spawn, does it still happen with this session's changes) before guessing further.

### Part 6 - Enemy frigate and friendly carrier (carrier mirroring + deck width done; ship-HP unification deliberately not implemented)

Confirmed true: `enemyShipGroups[]` is still 2 hand-placed 4-column instances (`{50,53}`, `{629,632}`), drawn as plain tiles with no dedicated ship-composition function; `enemyShipHp[]` exists but is **never read or decremented anywhere** - vestigial dead data, not a real HP mechanic. Ship destruction is confirmed per-column (`markShipColumnDestroyed()`/`isShipColumnDestroyed()` ignore row entirely - one hit anywhere in a column removes every tile in that whole column). `destroyEnemyShipGroup()` (whole-group destruction) exists but has zero call sites - also dead code.

The just-rewritten (Sprint 14.94 Part 6) `drawPromotedCpcCarrierRangeAt()` was confirmed used identically, with no mirroring, for both the start carrier (column 8) and end carrier (column 667) - the real CPC explicitly reverses the end frigate's sprite ("FRIGATE REVERSED, SO IT CAN COME IN SCREEN FROM OPPOSITE SIDE"). `WORLD_RENDER_CARRIER_WIDTH_TILES`(96px) vs `CARRIER_DECK_PIXEL_WIDTH`(104px) were confirmed mismatched between visual/render width and the landing-collision/refuel-trigger zone's width.

**Carrier mirroring (done)**: `tools/cpc_promoted_sprites_to_tiles.py`'s `Canvas` gained a `mirrored()` method that flips the fully-assembled 96x24 composite pixel-for-pixel (not a re-placement of the 6 source pieces mirrored individually - flipping the finished image is simpler and can't get the piece ordering subtly wrong). The script now emits a second baked tile set, `harCarrierReversedTileData`/`harCarrierReversedTileSkip`, alongside the existing normal one. `drawPromotedCpcCarrierRangeAt()` takes a new `reversed` parameter selecting which array to sample from - `compositeColumn` needs no extra reversal itself since the mirroring is already baked into the tile content. Added `HAR_OBJECT_FLAG_NATIVE_CARRIER_REVERSED` (bit 16, ORed onto the existing `NATIVE_CARRIER` bit so every existing match condition still finds the object) and set it on the end carrier's `level_route.h` entry only. Verified the generated mirror is pixel-correct by rendering both tile sets to ASCII art before rebuilding - the bridge/superstructure and tapered bow correctly flip from right-of-center to left-of-center.

**Deck width reconciliation (done)**: `CARRIER_DECK_PIXEL_WIDTH` changed from 104 to 96 to match the carrier's actual rendered width (`WORLD_RENDER_CARRIER_WIDTH_TILES`(12) x `GAME_TILE_WIDTH`(8) = 96px) - the refuel/rearm trigger zone (`playerOnNativeCarrierDeckPixels()`) was reaching ~8px past the visible ship's edge. Chose to shrink the collision constant to match the existing art rather than the review's alternative of widening the rendered art to 104px, since the composite is a fixed baked asset with no extra columns to add without redrawing source sprite pieces.

**Deliberately NOT implemented - the whole-ship-HP unification**: Part 6's own text recommends replacing per-column ship destruction with a single `EnemyShipState{hp,destroyed}` instance, but it hedges its own diagnosis ("Jeg mistenker sterkest..." / "I most strongly suspect..." / "et skjermbilde... vil avgjøre umiddelbart" - "a screenshot would decide immediately") - the reviewer wasn't certain. **Part 7 of the same review, written after directly reading `checkenemyhit`/`bombhitenemyship`, contradicts this**: "There is no whole-ship health counter in this CPC path... the Amiga idea of deleting individual ship columns is closer to the CPC than a conventional ship-wide HP system." Part 7's finding is grounded in specific disassembly citations rather than a visual guess, so it's the more reliable of the two - implementing Part 6's HP-counter suggestion would have moved the port *away* from CPC accuracy, not toward it, and would have duplicated/pre-empted the still-open "ship destruction granularity" item already tracked under Part 7's remainder. Left alone. The hand-placed enemy-ship tile data quality question (short/compressed silhouette, tile ordering) also needs a live screenshot to diagnose per the review's own admission - not attempted without one.

**Verified**: clean rebuild both before and after the debug-flag headless smoke test; two full headless autoplay runs completed cleanly with no crash/hang (scroll progressed normally both times, HUD guard/regression columns stayed at zero). Autoplay dies well before reaching world column 667 (the end carrier), so the mirrored sprite's *in-game* appearance is unverified by headless testing - confirmed correct only via the offline ASCII-render check of the generated tile data. Live confirmation (fly to the mission-complete landing at the end and check the carrier faces the opposite way from the start carrier) still needed from the user.

### Part 7 - Smoke generation (town-block piece done; remainder verified, not yet implemented)

Confirmed true: destroyed ground targets (radar/gun/tank/launcher) currently get a terrain crater (`markLandCraterAtColumnRow()`) instead of the CPC's 2-tile smoke (tiles 51/52); **`HAR_OBJ_TOWN_BLOCK` was confirmed completely absent from the rocket/bomb weapon-collision filters in `updateWeapons()`** - town buildings could not be destroyed by any player weapon at all, a genuinely missing feature, not a fidelity nuance. Ship destruction removes the whole world column as noted in Part 6, though the smoke *placement itself* is already tile-precise (`addCpcHitSmokeAtColumnRow()` places smoke exactly at the hit tile, plus one tile left if that neighbour is empty sky) - only the underlying destroyed-object bookkeeping is column-wide, not the visual smoke. A shared smoke-storage helper already exists (`markShipWreckSmokeAtColumnRow()` and friends) but was only wired into the ship/frigate path, not reused for ground targets.

**Town-block-destructibility piece (done)**: the missing-feature part flagged above is fixed, using the existing shared smoke-storage helper rather than a new tracking structure. Two things had to be resolved before the fix could just be "add `HAR_OBJ_TOWN_BLOCK` to the collision filter":

- **Town blocks are wide objects registered under only their anchor column.** `buildHarLevelObjectIndex()` indexes every `harLevelObjects[]` entry (including town blocks, which are `harCpcTownBlockWidths[]` columns wide x `HAR_CPC_TOWN_BLOCK_HEIGHT`(5) rows tall) into `harLevelObjectColumnHead[]`/`harLevelObjectNext[]` keyed by `object->column` alone - so `objectCellForWorldColumnTile()`'s per-column scan (what `updateWeapons()`'s primary hit-test calls) only ever "sees" a town block at its first column, never across the rest of its visible width. Simply adding `HAR_OBJ_TOWN_BLOCK` to the existing filter list would have made hits register only along a building's leftmost column-slice. Added `townBlockCellNearWorldPoint()` (mirrors the existing `ownFrigateCellNearWorldPoint()`'s pattern - a direct scan over `harLevelObjects` with an explicit column/row range check against the object's actual footprint, used as one more fallback after the primary point check, same as the frigate helper) so a weapon can hit any visible part of a building, not just its first column. It also resolves the exact tile at the hit cell and skips transparent gaps within the block's bounding box (`tileId==0||tileId==1`, matching `drawPromotedCpcTownBlockRangeAt()`'s own skip condition) so a shot through a building's empty silhouette gaps doesn't falsely register.
- **Rendering had to stop redrawing over destroyed cells.** Town blocks are drawn by `drawDirectColumnRangeObjects()` via a separate "wide object" pass that runs *after* the ordinary per-cell tile pass and unconditionally redraws the building's full art every frame it's on screen - so marking a cell's smoke via the shared helper alone would get silently painted over on the very next frame. The renderer (at the time, `drawPromotedCpcTownBlockRangeAt()`; superseded by Part 5's `drawProceduralTownBlockColumn()`, which kept the same fix) skips any row where `shipWreckSmokeTileAtColumnRow()` already has a tile, letting the smoke (drawn by the ordinary per-cell pass, which runs first and already had priority for exactly this reason) show through untouched.
- On a hit, reuses `addCpcHitSmokeAtColumnRow()` as-is (same two-tile smoke pattern already used for the own-frigate hit case) and awards `TOWN_BLOCK_SCORE_VALUE`(350, matching the review's "35 internal units, displayed as 350 points") - each building tile is destroyed individually, matching the CPC's actual tile-based destruction rather than a whole-building removal.
- Verified: clean rebuild, headless autoplay run completes with no crash/hang (scroll progressed steadily 160->2678 px over the run, HUD guard/regression diagnostic columns stayed at zero throughout). Autoplay never fires weapons, so this only confirms the new code paths don't destabilize normal flight - it does **not** exercise the actual hit-detection/smoke-rendering logic. Live in-game confirmation (bomb/rocket a building, watch for smoke appearing at the exact tile hit and staying there instead of being redrawn over) is still needed from the user.

**Ship destruction: column-vs-tile granularity (done)**. User re-read `checkenemyhit`/`bombhitenemyship` directly and confirmed Part 7's own finding over Part 6's earlier (self-admitted, hedged) speculation: enemy ships have no whole-ship HP counter in the original - each hit just replaces the struck tile with smoke, scores, and leaves the rest of the ship standing. The Amiga port's `markShipColumnDestroyed(worldColumn)` removed every tile in the hit column at once, which over-destroys wherever a ship has more than one tile stacked vertically in the same column (confirmed real: the start ship's columns 51/52 in `level_route.h` each carry two vertically-stacked tiles).
- Replaced the column-keyed `destroyedShipColumns[]`/`isShipColumnDestroyed()`/`markShipColumnDestroyed()` with cell-keyed `destroyedShipCellColumns[]`/`Rows[]`/`isShipCellDestroyed(column,row)`/`markShipCellDestroyed(column,row)` (same array-based membership-list pattern used elsewhere in this file, e.g. `clearedFlakColumns[]` before Part 2 removed it). `damageEnemyShipAtColumnRow()` now marks only the exact `(worldColumn, tileY)` cell instead of the whole column.
- Both `objectCellForWorldColumnTile()` and `buildWorldTileColumn()`'s harLevelObjects loops had to move their `HAR_OBJ_ENEMY_SHIP` destroyed-check to *after* the row is resolved (it was previously checked column-only, before row was even known) so the per-cell check has an actual row to test against.
- Removed `destroyEnemyShipGroup()`/`addEnemyShipWreckSmoke()`/`enemyShipHp[]`/`enemyShipDestroyed[]`/`GAME_ENEMY_SHIP_HP` as newly-fully-dead code - `destroyEnemyShipGroup()` (the only writer of `enemyShipDestroyed[]`) already had zero call sites before this change per Part 6's own findings, and its whole-column-range semantics don't have a meaningful per-cell equivalent worth preserving for unused code.
- Verified: clean rebuild, headless autoplay confirms no crash/hang. Autoplay doesn't fire weapons, so the actual per-tile-vs-whole-column visual difference needs the user shooting a multi-tile ship column and confirming only the struck tile smokes.

**Scope note - remainder not yet implemented**: the ground-target crater-vs-smoke switch (replacing `markLandCraterAtColumnRow()` with the CPC's actual 2-tile smoke for radar/gun/tank/launcher kills) is the one piece of Part 7 still open - it trades an existing, already-shipped visual (crater) for a different one, worth confirming with the user before changing, same reasoning as Part 2 was before implementing it.

### Suggested order for a future session

1. ~~Part 3/4 (terrain double-slope-tile fix)~~ - done.
2. ~~Part 7's town-block-destructibility piece~~ - done.
3. ~~Part 6's carrier mirroring + deck width reconciliation~~ - done. Ship-HP unification deliberately skipped (contradicts Part 7's own better-evidenced finding - see Part 6 write-up).
4. ~~Part 2's flak rearchitecture~~ - done, per user's explicit go-ahead.
5. ~~Part 5's continuous town generation~~ - done. Red-flash bug still unresolved (needs live repro).
6. ~~Ship destruction column-vs-tile granularity~~ - done, per user's explicit re-confirmation after re-reading `checkenemyhit` directly.
7. Only remaining open item: Part 7's ground-target crater-vs-smoke switch - a gameplay-feel trade-off (trades the existing crater visual for CPC's actual 2-tile smoke), confirm with the user before implementing rather than assuming the CPC-authentic choice is automatically preferred.

## Sprint 14.96 - Main Menu Review

Status: done. User supplied a detailed review of the main menu screen (`drawMenuScreen()` and everything it draws) - all claims verified against the actual current code before implementing, same practice as Sprint 14.95.

- **Lives-before-HUD ordering bug (done)**: `startGameSession()` drew the HUD (`drawHudBuffer()`) using `initGameState()`'s default `PLAYER_START_LIVES`(3) - the caller only overwrote `game.lives` with the menu's actual `livesSetting` *after* `startGameSession()` returned. Picking "Lives: 1" could start a session with "3" baked into the HUD buffer. Added `livesSetting` as a new `startGameSession()` parameter, set right after `initGameState()`, before `drawHudBuffer()` - same pattern already used for `skillLevel`. Found and fixed a *second* call site (the game-over "press select to restart" path) with the identical bug while updating the signature.
- **Right-column status lines mislabeled (done)**: "Rocket range: 10"/"Controls: Off"/"Lock height: On"/"Wingman: Off" used the same `MENU_COLOR_CYAN` as an unselected-but-selectable left-column menu item, despite none of them being interactive. Switched to `MENU_COLOR_SHADOW` - this menu's existing "dim/inactive" colour (already used for the debug-overlay label) - so the column reads as status at a glance.
- **Fake "Input: Joystick/Keyboard" toggle removed (done, per user's explicit choice over wiring it up for real)**: confirmed `ReadInput()` always reads joystick+keyboard+mouse simultaneously regardless of this menu setting - the toggle only ever changed its own displayed text. Removed as a selectable item entirely (`MENU_ITEM_COUNT` 4->3, `MENU_ITEM_INPUT` removed, `MENU_ITEM_SKILL`/`MENU_ITEM_LIVES` renumbered) rather than gating `ReadInput()` for real. "Controls: Off" (another non-functional text-only status token) replaced with "Input: All" in the right column, since the tight 200px menu screen (`HUD_TOP`=168 sits right below the last status row) had no room to add a line instead of replacing one.
- **Skill level expanded beyond terrain height (done)**: previously only fed `cpcLandMinimumRow()`. Added `flakDamageThresholdForSkill()` using the review's confirmed CPC formula `totalflakdamagecount ~= 25 - 2*difficulty` (skill 1 tolerates ~23 flak hits, skill 5 only ~15) in place of the old fixed 100-hit budget in `applyPlayerFlakDamage()` - armour now scales proportionally against that threshold instead of a flat `100 - flakDamageCount`. Also added `enemyRespawnFramesForSkill()`/`enemyMissileFireFallbackFrameForSkill()` scaling `ENEMY_RESPAWN_FRAMES`/`ENEMY_MISSILE_FIRE_FALLBACK_FRAME` down at higher skill (roughly halved by skill 5) - **unlike the flak formula, no sourced CPC value was available for enemy-timing difficulty scaling**, so this pair is this port's own directional approximation, clearly commented as such rather than presented as a verified match.
- **Real top-7 high score table with disk persistence (done)**: replaced the permanent placeholder table (only row 0 ever showed a real score; LEVEL/HITS always displayed 0) with a real sorted `HighScoreEntry[7]` (name/level/hits/score). `game->hitsCount` added, incremented alongside every existing `bonusScore +=` award (ground target, enemy ship, town block, enemy plane, enemy missile - 10 call sites). LEVEL is read from the CPC's own `gamelevelprogress`-equivalent stage (`HarLevelStage`) at the run's final `scrollX` - since the world only ever scrolls forward, that's always the furthest point reached, no separate tracking needed. No name-entry UI was built (out of scope for this pass) - real runs are tagged a fixed "PLAYER", displacing the CPSOFT/AMSOFT/DURELL placeholder rows one at a time as they're actually beaten. Persisted to `PROGDIR:harrier_scores.dat` via `Open`/`Read`/`Write`/`Close` (dos.library, already linked), loaded once at program startup, saved whenever the table changes.
  - **Bug found and fixed during testing**: `Open()` on `PROGDIR:harrier_scores.dat` triggered AmigaDOS's blocking "Please insert volume PROGDIR in any drive" system requester when that assign doesn't resolve - confirmed live, and froze the headless test harness completely (no automated way to click through a modal OS dialog). Root cause: the headless harness doesn't launch the program through a normal AmigaDOS process invocation that would set up `PROGDIR:`, so the assign is simply absent in that environment. Fixed with the standard AmigaOS technique - set the current process's `pr_WindowPtr` to `(APTR)-1` immediately around each `Open()` call (`suppressDosRequesters()`/`restoreDosRequesters()`), which makes DOS fail the call silently instead of popping a requester. Both `loadHighScoreTable()` and `saveHighScoreTable()` already handled a failed `Open()` gracefully (falls back to defaults / just skips the save), so no other logic changes were needed once the requester itself was suppressed. This also protects any real player whose environment doesn't have `PROGDIR:` resolvable for whatever reason - the game now degrades to "high scores don't persist this run" instead of hanging on a dialog most players wouldn't know how to reach (WinUAE runs headless/full-screen for many).
- Verified: clean rebuild after every step. Headless autoplay confirmed no crash/hang for the skill-level/menu-structure changes (autoplay doesn't reach the menu-driven paths meaningfully beyond initial startup, so this mainly checks nothing regressed elsewhere). The DOS-requester fix was verified live by the user (confirmed the game now boots and autoplay flies normally, where it previously hung on the disk-insert dialog) - the deepest, most concrete verification available for exactly this kind of bug.

## Sprint 14.97 - Gameplay Systems Review (guided locks, flak model, destructible town, terrain difficulty)

Status: done, per the user's own explicit priority order (guided locks -> flak timing -> destructible town -> terrain difficulty). Powerups and Wingman deliberately not touched - user's own framing scoped the first four as "fits the current architecture," Wingman as its own future milestone, and powerups as the next thing after these four, not part of this batch.

Before touching anything, re-verified every claim against the actual current code (not the reviewer's - or my own prior summary's - mental model of it) - this caught that several things the review described as missing/flat were **already implemented** from work earlier in this same session that predates this conversation's context window: the CPC-accurate per-column running RNG state (`cpcRandomStateByColumn[]`, one `genrandomhl`-equivalent state per column, matching CPC's real single-sequential-RNG-call-per-column behaviour), the graduated `claimed[]` priority levels in `buildWorldTileColumn()` (1=soft/overridable, 2=hard/protected) that already let town-block tiles overwrite base terrain in rows 14-15, the town generator's own "don't start a block that won't fit before the section ends" check, and the double-score guard on destroyed town-block cells. Good discipline paid off here - implementing fixes for already-fixed problems would have been wasted, confusing work.

### 1. Guided target locks + Maverick rocket (done, full scope per user's choice)

Confirmed: the existing "Lock height: On" rocket behaviour (`game->rocketShot.y = playerY + 2`, overwritten every frame) is CPC's real `lockinmissileheighttoplayer` - correct for the *standard* rocket. CPC additionally has a Maverick missile type that keeps a separate locked target coordinate (`enemylandlocationlock`) instead - the Amiga port had no way to select it at all. Asked the user whether to (a) just build the tracking structure inertly, (b) build tracking + add Maverick as a real selectable weapon, or (c) defer - user chose the full option.

- Added `TargetLock{active,worldX,y,targetType}` to `GameState`, updated by new `updateTargetLock()` (same once-per-newly-revealed-rightmost-column convention as `trySpawnFlak()`/`updateCityFade()`) whenever that column holds a ground target (radar/launcher/gun/tank) - mirrors CPC's "lock updates when a new target is generated," left unchanged when that target is later destroyed (CPC just keeps the coordinate until the next target replaces it).
- Added `MENU_ITEM_ROCKET_TYPE` as a 4th selectable menu item (`ROCKET_TYPE_STANDARD`/`ROCKET_TYPE_MAVERICK`), reusing the row freed up when the fake Input toggle was removed in Sprint 14.96. `updateWeapons()`'s rocket branch now splits: Maverick with an active lock steps 1px/frame toward the locked Y (same heat-seek step rate `updateEnemyMissile()` already uses); no lock, or standard mode, falls through to the existing height-follow behaviour ("no valid lock: regular straight flight path," not a refusal to fire, per the review's own preferred fallback). The right-column "Lock height: On" status line is now dynamic ("Lock: Height"/"Lock: Target") based on the selected mode.
- Verified: clean rebuild, headless autoplay confirms no crash/hang.

### 2. Full flak timing state machine + single-tile redraw (done)

The per-column spawn probability itself (`isTown ? 1-in-8 : 1-in-16`) was unchanged from Sprint 14.96's earlier work and, per the review, can't reproduce CPC's bursty "several in a row, then a pause" groupings the way a per-target countdown (l8864) can - a flat roll is memoryless.

- Replaced the flat roll in `trySpawnFlak()` with a countdown (`flakCountdown`) that must reach zero before a placement is even attempted, reseeded from the running per-column RNG state each time it's spent (`seedFlakCountdown()`), biased shorter in town (2-7 columns) than land (6-15 columns) to match the review's "flak clearly denser in town" observation. **Exact CPC threshold values weren't available to reconstruct precisely** - this rebuilds the qualitative *structure* (countdown-gated, RNG-reseeded, phase-biased) rather than a verified formula, same honesty-about-approximation as the row-offset range it sits alongside. Flagged clearly in code comments; may need further live-tuning feedback.
- Added `dirtyRedrawWorldTile()` - writes exactly one tile at both ring-buffer positions (primary + wrap-duplicate), instead of `dirtyRedrawWorldColumn()` rebuilding and redrawing all 25 rows of a column plus the carrier/gunship overlay pass just to show one new flak tile. `trySpawnFlak()` now uses this. Confirmed safe: flak only ever spawns over land/town-stage columns, which never overlap the sea-stage columns carrier/gunship occupy, so there's no wide-object overlay to worry about missing.
- `cpcL8859ForWorldColumn()` removed as newly-fully-dead code (was only used by the flat-roll logic just replaced).

### 3. Destructible town fixes (done - only 1 of 5 sub-points needed real work)

Re-verified all 5 sub-points from the review against current code:
- Town tiles overwriting base terrain in rows 14-15 - **already done** (the graduated `claimed[]` scheme described above).
- Not starting a town block without room before the section ends - **already done** (`generateCpcTownBlockTable()`'s `if (width > remaining) break;`).
- Destroyed-state in the same cell model as the rest of the world - **already true** (town-block hit smoke already goes through the same shared `shipWreckSmokeTileAtColumnRow()` array ship wrecks/frigate hits use).
- A smoke-covered cell can't be scored twice - **already done** (`townBlockCellNearWorldPoint()`'s existing `shipWreckSmokeTileAtColumnRow()` guard).
- Only update changed tiles, not always the whole column - **this one was real**. Added `dirtyRedrawWorldTileIfSmoke()` (looks up the actual marked smoke tile via `shipWreckSmokeTileAtColumnRow()` and only draws if something is actually there - correctly handles `addCpcHitSmokeAtColumnRow()`'s "always the hit cell, conditionally the left neighbour" pattern with one shared call at each site instead of two unconditional full-column redraws). Applied to both `updateWeapons()` town-block-hit branches (rocket + bomb). Left the enemy-ship/own-frigate hit branches' full-column redraws untouched - out of scope for this review (town-specific), not part of what was asked.

### 4. Terrain difficulty scaling (verified - already done; land-section length deliberately not attempted)

`cpcLandMinimumRow() = 12 - skillLevel` (max terrain height by skill) - confirmed already implemented exactly as the review describes, from Sprint 14.95 Part 3/4. No work needed.

**Land-section length scaling deliberately not implemented.** The review's own CPC citation, `300 + leveldifficulty*256`, is presented without confirmed units - Amiga side, the land section is a *fixed* compile-time table entry (`harLevelRoute[]`, columns 106-400, 295 columns, skill-independent), and making its actual end column vary by skill would mean turning a static level-route table into something computed per-session, which every other reader of `levelSegmentForWorldColumn()` currently assumes is fixed. Without confirmed units for the CPC value (frames? an internal tick count? something else entirely) there's a real risk of getting the scale wrong by an order of magnitude - the review's own text hedges this exact point ("dersom videre kildekartlegging bekrefter det" - "if further source-mapping confirms it"). Left alone rather than guessed at; worth a dedicated pass if/when the exact CPC unit is confirmed.

### Deliberately out of scope (per the review's own explicit framing, not asked for here)

- **Powerups** (`HAR_OBJ_POWERUP` exists as a bare enum value only - no `PowerupState`, spawn rules, sprite, or collision). Next up per the review's own recommended order, but not part of this batch.
- **Wingman** - review explicitly calls this out as touching input, sprites, weapons, collisions, enemy AI, and target selection all at once, and recommends treating it as "its own larger milestone" rather than folding into this pass.

Verified: clean rebuild after every step, headless autoplay confirmed no crash/hang for the full batch. None of the new systems (Maverick rocket, flak countdown timing, town single-tile redraw) are meaningfully exercised by headless autoplay (it never fires weapons, and the countdown/redraw changes are timing/perf-only with no functional branch autoplay would trip) - live confirmation from the user still needed for all three.

## Next Implementation Slice

Backlog, roughly in the order raised:

1. Carrier ship palette fix (dedicated pens or copper-based band split - see Sprint 14.90's deferred list).
2. Audit the town section's flak *density* (4 static entries in ~200 columns, see Sprint 14.91.1) against the real CPC's actual `launchflakattack` behavior during the town stage - confirm if this is authentically dense or should be tuned. (Sprint 14.92 already gave town flak the same variable-height treatment as land flak; only the density question is still open.)
3. Sub-tile (half-height) terrain resolution, if still wanted after seeing the current AA-seam-only result in more play.
4. CPC difficulty scaling (`leveldifficulty`) - needs a landing/relaunch progression loop that doesn't exist on Amiga yet; confirmed deferred out of Sprint 14.91.
5. Wingman AI - a genuine new feature (shared targeting with the enemy plane), not a fix to existing behavior; confirmed deferred out of Sprint 14.91.
6. Landing-approach usability: user reported flying straight through the carrier deck into a fatal crash at the end of a route. Traced to `playerOnNativeCarrierDeckPixels()`/`replenishPlayerFromFrigate()` requiring a fairly narrow altitude band (Y 107-124) *and* `speedLevel<=3` at the same time the deck's X range is reached, with no forgiveness once `scrollX` hits `GAME_SCROLL_MAX_PIXELS`. User is checking this against the real CPC's actual landing sequence before deciding whether to widen the tolerance, add landing-guidance HUD cues, or hold scroll position until landed.
7. Keep all new route/land definitions editor-friendly; if needed, add a generated data file beside `level_route.h` rather than burying data in gameplay code.
8. Preserve A500 smoothness: denser map content must be streamed as small dirty/object chunks and normal builds must keep debug overlays hidden.
9. Later-session process note: finish the short GLM/OpenRouter skill report requested by the user, especially how to avoid token blow-ups during external-model calls.
10. Player crash detection against tanks/other ground targets: earlier user report ("plane doesn't crash when hitting tanks or any other ground targets") predates Sprint 14.91's damage-scheme audit. Re-read `playerObjectMapCollision()` (probes 3 points near the player sprite's front/lower body, returns `PLAYER_OBJECT_COLLISION_FATAL` for any non-sky/non-flak/non-own-frigate cell) and confirmed the classification and `startPlayerCrash()` wiring both look correct, and ground targets are reachable within the player's normal altitude range (`PLAYER_MAX_Y`=144, target row sits at `terrainY-1`, well within reach) - no code-level bug found on a second pass. Needs the user's own in-game confirmation (does it still reproduce after everything in this session, and if so under what specific altitude/approach) before spending more time guessing blind.
11. Black border vs. blue sky at the screen's left/right edges (real CPC has a black border, Amiga currently shows open sky): deferred - the sky is tile index 0 in the embedded `game_tiles.bpl` asset with pen-0 pixel data baked directly into every sky tile, so it can't be recolored/bordered from C code alone; would need the tile asset itself regenerated. Out of scope until that asset work is picked up.
12. Headless screenshot capture came back solid black for an entire test pass this session (Sprint 14.92) despite the WinUAE process running normally (real CPU usage, responsive) - `IsWindowVisible` even reported `False` on a window with a valid handle, and forcing it to the foreground didn't fix the capture. Didn't affect the CSV-based perf/crash verification (still works), but blocks the visual-regression half of headless testing until root-caused; revisit if it recurs.

## Sprint 14.98 - CPC-Log Terrain Boundary Fix

Status: done. Fixes a regression introduced by work done outside this document's own tracking: a new WinAPE-snapshot logging pipeline (`build-loggen.ps1` + `.Amstrad-snapshot/Snap4.sna` -> `amiga/assets/cpc_log_data.h`) was added to replace the reverse-engineered skill-1 terrain generator with real captured CPC state (height/R/l884b/l884c/l8864 per column, `CPC_LOG_RECORD_COUNT=189` columns) - not documented under any sprint number here, so recorded now for context alongside the fix.

The user reported a visible vertical terrain "wall" partway through a flight (screenshot), with their own diagnosis pointing at the log/procedural-fallback boundary. Reading `cpc_log_data.h` turned up a more precise root cause than a general continuity gap: the file's *own* trailing comment documents that WinAPE-vs-Amiga RNG verification mismatched at the very last logged column - `/* MISMATCH at col 188: amiga=0x76A5 cpc=0x0100 */`, `/* currtime matches: 188/189 */`. That row's height (0, vs. a surrounding 11-14 range) and other fields are leftover zero-init from an incomplete capture, not real CPC state - but `generateCpcLandHeightTable()`, `cpcFlakL884bForWorldColumn()`, and `trySpawnFlak()` all trusted it directly (`i < CPC_LOG_RECORD_COUNT`), producing a one-column spike to height 0 immediately followed by the generator's own hard reset to baseline on the next column: a double-discontinuity right at world column ~295.

- Added `CPC_LOG_VALID_COUNT` (`CPC_LOG_RECORD_COUNT - 1`) right after the `cpc_log_data.h` include in `main.c`, and switched all three per-column "trust the log" gates to it, so the corrupt last row now falls through to the existing "beyond the log" fallback instead of being read. That fallback already resolves to `CPC_LAND_PROCEDURAL_BASELINE` (14) - which exactly matches the last genuinely-verified height (index 187) - so the transition is continuous with no further restructuring of the generation loop.
- Checked the two other state-carry-over concerns raised in the review: the cloud-block table (`cpcCloudTopRowByColumn[]`/`cpcCloudBlockColumnByColumn[]`) is generated independently of the WinAPE log entirely (pure RNG-driven, full level width) - no boundary risk there. `previousWasTarget`/`pendingTankRear` are loop-scoped locals in the one continuous generation loop, so they already carry over regardless of which branch a given column takes; confirmed no tank-front placement lands on the now-excluded index 187 in the actual logged data (its `l884b` value there is 0), so no dangling `pendingTankRear` case exists in practice.
- Added the requested debug validation loop: after generating `cpcLandHeightTable[]`, walks consecutive entries and `KPrintF`s any `|delta| > 1` (gated behind `HAR_DEBUG_PERF_LOG`, zero cost in normal builds) - a permanent regression guard for future `build-loggen.ps1` re-runs.

Verified: clean rebuild, headless autoplay full pass (90s+) with no crash/hang and all HUD-corruption guard counters (`hudGuardHits`/`hudGuard2Hits`/`hudRegHits`/`hudCollisionFires`) at 0 throughout, including through the interval crossing the fixed boundary. Headless autoplay can't visually confirm the terrain now renders smoothly (no screenshot channel currently reliable, see item 12 above) - needs the user's own live look at world column ~295 (roughly 40-50s into a flight at normal speed) to confirm the wall is gone.

**Superseded by Sprint 14.99 below** - the user clarified the whole WinAPE-log-table approach this sprint patched was the wrong direction in the first place (a captured/replayed map, not a generator), and asked for it to be removed entirely rather than further patched.

## Sprint 14.99 - Real Land-Height Generator (removes the WinAPE log table)

**Important direction, confirmed with the user and now a standing constraint for all future terrain/RNG work**: the real Amstrad game does not generate the same landscape on every run - the user compared multiple real CPC playthroughs and confirmed the terrain/target/flak layout genuinely differs each time. So a fixed, reproducible Amiga map was never the right goal either, even though Sprint 14.97/14.98's WinAPE-capture pipeline (`build-loggen.ps1` + `.Amstrad-snapshot/Snap4.sna` -> `amiga/assets/cpc_log_data.h`) had drifted into effectively building one. The actual goal, restated by the user explicitly: reconstruct the CPC's *generating algorithm*, fed by a seed that varies every session like the real machine's does - "as much as possible" algorithmic fidelity, not pixel-perfect map replay.

**What changed**:
- `amiga/assets/cpc_log_data.h` deleted. All three consumption sites (`generateCpcLandHeightTable()`, `cpcFlakL884bForWorldColumn()`, `trySpawnFlak()`) no longer reference it - each now always uses the computed/generated path that previously only ran for skill levels other than 1.
- `generateCpcLandHeightTable()` rewritten as a single real generator for all skill levels, reconstructed from the actual disassembly (asm:5433-5521 `l9134`'s mode dispatch, asm:5621-5665 `insertenemylandtile`) rather than reverse-engineered-then-fudged-with-a-table:
  - Mode per column = `(l8859>>2)&3`, where l8859 is the high byte of the same genrandomhl LCG (`state = state*1509+0x29`) already modeled elsewhere in this file - but critically seeded fresh at *land's own first column*, not at the absolute world column the shared `cpcRandomStateByColumn[]` table uses. Verified during development (see below) that CPC's real captured l8859 sequence only lines up with this recurrence when indexed from land's local column 0 - using the shared table's world-column indexing for land was silently wrong (a separate, previously-undiscovered bug, now avoided by giving land its own local tick-indexed sequence rather than reusing the shared table).
  - mode 0 = flat, mode 1 = descend toward baseline height (blocked at `CPC_LAND_PROCEDURAL_BASELINE`), mode 2 = climb (blocked at `cpcLandMinimumRow(skillLevel)`), mode 3 = insert a ground target - gated on the *previous* column's dispatched mode being even (asm:5515-5521's l8861 check), else forced flat.
  - One additional rule was not found anywhere in the static disassembly but is confirmed by *every* real target placement (21/21) in the verification trace: the column right after any successful target insertion is always forced flat, regardless of what mode would otherwise fire. Encoded explicitly and flagged in code comments as empirically-derived rather than disassembly-confirmed, since no combination of the documented l8861/l8860 checks reproduced it from static reading alone (most likely a side effect of `insertenemylandtile`'s own call chain - `checkwingmandobombingrun`, `updateenemylandlocationlock` - that a static read couldn't isolate).
  - The first land column is CPC's own one-shot `startoffalklandisland` transition (asm:5409-5431) - consumes a real generation tick but draws fixed join tiles rather than running mode dispatch.
  - Tanks are visually 2 tiles wide on Amiga (`CPC_LAND_TARGET_TANK_FRONT`/`TANK_REAR` spanning two world columns), matching the existing rendering/collision model - but confirmed via the real captured trace that CPC itself draws the 2-tile tank as a *vertically*-stacked block within one generated column (`drawspriteblock3` steps by row, not column), consuming only one real tick. The rear-tile slot therefore advances the output column but intentionally does **not** advance the tick/RNG sequence, so it doesn't reintroduce the kind of indexing drift Sprint 14.98 fixed.
- **Verification method** (dev-time only, not shipped): before deleting `cpc_log_data.h`, the reconstructed algorithm above was checked column-by-column in a standalone script against that file's real WinAPE-captured height/l8859/l884b sequences (the same data the now-removed log table held). Mode selection and hill-step direction matched **100%** of every column where a height change actually occurred (56/56 unambiguous cases) - strong confirmation the core mechanism is exactly right. The full sequence matched **~87%** exactly column-for-column; the residual ~13% is a still-unidentified edge-case gating interaction (search for a cleaner rule didn't fully converge - see the code comment above `generateCpcLandHeightTable()`). Given the user's own standard is algorithmic fidelity rather than an exact byte-for-byte trace, this was judged close enough to ship rather than continuing indefinitely; revisit only if further static analysis or an instrumented run turns up the missing rule.
- **RNG is now session-random, not fixed**: `resetCpcRandomSequence()` (drives clouds/flak/town) and `generateCpcLandHeightTable()`'s own local sequence both now seed from `frameCounter` at the moment a new game starts (`startGameSession()`), instead of the previous fixed `CPC_RANDOM_INITIAL_STATE` constant - so every playthrough's terrain/cloud/flak/town layout differs, matching the confirmed real-CPC behavior. `CPC_RANDOM_INITIAL_STATE` (0x2F08) is kept only as a documented reference (the value the verification trace was captured with), not used at runtime anymore.

**Deliberately not changed** *(superseded by Sprint 14.100 below - this turned out to be wrong, not just incomplete)*: ~~the shared `cpcRandomStateByColumn[]`/`cpcRStateByColumn[]` tables (clouds/flak/town) still index by absolute world column... land's own generator sidesteps the issue entirely by using its own locally-seeded sequence instead of the shared table.~~

Verified: clean rebuild (no errors/new warnings), headless autoplay pass with no crash/hang, all HUD-corruption guard counters at 0. User confirmed live in WinUAE the terrain renders correctly ("Ser bra ut").

## Sprint 14.100 - Unify Terrain/Flak/Cloud RNG, Path-Dependent R, Blocked-Slope Fix

The user gave a detailed corrective review (Norwegian "Lederboks") of Sprint 14.99, reframing the goal precisely and catching real remaining bugs:

**Corrected goal** (supersedes Sprint 14.99's framing in places): the target was never bit-exact reproduction of the captured trace - 189/189 identical heights is a verification tool only, not a production goal. What matters: **same seed -> same Amiga landscape** (deterministic given a seed), **new seed -> a new but CPC-like landscape** (same generating algorithm, same statistical rhythm - mode distribution, height limits, target/flak/cloud correlation - not the same specific sequence). `cpc_log_data.h` (already deleted in 14.99) belongs only behind a `CPC_RNG_VALIDATION`-style test fixture if it's ever needed again, never as production lookup data - consistent with what was already done, just now with the right stated reason.

**Real bug found and fixed - terrain and flak were reading two different sequences.** Sprint 14.99's land generator seeded its own local LCG from `frameCounter ^ 0x9E17`, walked by land-local tick. But `trySpawnFlak()` reads `cpcRandomStateForWorldColumn()`/`cpcRStateForWorldColumn()` for its own row/tile decisions - the *shared*, separately-`frameCounter`-seeded `cpcRandomStateByColumn[]`/`cpcRStateByColumn[]` tables that also drive clouds and town. Two independently-seeded sequences blended into one flak decision meant the generators were only superficially CPC-like, not actually sharing state the way real CPC's single genrandomhl/currtime does. Fixed by folding land height/target generation directly into `resetCpcRandomSequence()`'s existing per-world-column walk - terrain, targets, clouds, flak and town now all read the exact same sequence at the exact same position, from one seed. `generateCpcLandHeightTable()` is now a two-line compatibility shim; the real logic lives in `resetCpcRandomSequence()`.

**Consequence of unifying**: land generation switched from local-tick indexing back to plain world-column indexing (the very thing Sprint 14.99 deliberately moved away from, to hit exact trace alignment). This is fine now that exact alignment isn't the goal, and it's actually *more* faithful architecturally - real CPC has one `currtime` ticking continuously from boot regardless of sea/land/town stage, which world-column indexing matches directly. This also makes the Sprint 14.99 "deliberately not changed" sea-section mis-seeding note moot for land specifically, since land no longer has a separate seed to be inconsistent with.

**R is now path-dependent, not a flat increment.** `CPC_R_INCREMENT` (flat `0x53` every column) replaced with `CPC_R_COST_DEFAULT`/`_FLAT`/`_HILL`/`_TARGET`/`_TANK_REAR` - R advances by a different (still approximate, not cycle-counted) amount depending on what the generator actually did that column, since real Z80 R only advances by however many instructions actually ran and `insertenemylandtile` (calls `updateenemylandlocationlock`, `checkwingmandobombingrun`, a wider `drawspriteblock3`) clearly runs more code than a flat column. Values are relative approximations, not measured cycle counts - flagged as such in the code comment, consistent with the user's own "doesn't need instruction-accurate emulation" standard.

**Blocked hill slopes now draw flat, not a contradictory sloped tile.** Real bug: when mode 1 (descend) or mode 2 (climb) was blocked by the baseline/floor, the surface-tile calculation still unconditionally used the sloped-tile formula (`28+...`/`24+...`) even though height didn't change - drawing a slope graphic with no actual elevation change, self-contradictory geometry. The real CPC's `drawflatterrain` path is a hard jump away from the slope-tile code entirely when blocked (`jr z,drawflatterrain`), landing on its own flat-grass tile calculation. Fixed: the blocked branches now compute the flat grass tile (`32+(R&3)`) like every other flat column.

**Seed-ordering bug found and fixed while unifying** (pre-existing, made active by this change): `startGameSession()` set `cpcLandSkillLevel = skillLevel` *after* calling `initGameState()` - harmless before, since land generation was lazy and only ran much later on first render. Now that `initGameState()`'s call to `resetCpcRandomSequence()` generates the land table immediately, the stale skill level (previous session's, or the `1` default) would have been used for the climb-ceiling calculation instead of the one the player just picked at the menu. Fixed by moving the assignment before `initGameState()`.

**Deliberately still deferred** (per the user's own softer phrasing - "bør etter hvert", "should eventually"): state-driven section transitions (sea -> coast -> procedural land -> descent -> town -> pier -> sea) keyed off `enemyshiptimer`/`leveldifficulty`/`l8860` the way real CPC does, instead of the current fixed-length `harLevelRoute[]` segment table. Doesn't mean every run must have a different total length - means the length should come from the same rules CPC uses. Bigger and more architecturally invasive (would touch `levelSegmentForWorldColumn()` and the whole segment-table model) than what was asked in this pass; revisit as its own dedicated sprint.

Verified: clean rebuild (no errors), headless autoplay pass with no crash/hang.

## Standing rule - CPC algorithms, never a captured map

This rule overrides any older sprint wording that can be read as reproducing
one recorded CPC landscape:

Remaining work consists primarily of modelling the Z80 R register at the
actual terrain/target, town and flak decision points. R progression must be
derived from the M1 instruction-fetch flow in the CPC assembly. CPC LOGGEN
captures are only control evidence for validating distributions, rhythm and
observed state transitions; they are not lookup tables or a source of finished
results.

The binding method is:

- The assembly code determines the model.
- The executed M1 instruction-fetch path determines how R advances.
- LOGGEN is used only to check that the model behaves credibly.
- Missing log values must not be interpolated or reconstructed as ground
  truth.
- Production code generates new results from the session seed and the code
  path actually taken.
- A validation fixture may start both models from explicit seeds, but captured
  output must never become runtime input.

Consequently, the target architecture has several R timestamps rather than
one ambiguous value per world column:

```c
typedef struct {
    UBYTE atTerrainDecision;
    UBYTE atTargetDecision;
    UBYTE atTownDecision;
    UBYTE atFlakDecision;
    UBYTE afterColumn;
} CpcRDecisionState;
```

Each member must be calculated by accumulating M1 fetches along the assembly
branch that was actually taken. Do not introduce guessed offsets merely to
populate this structure. Until a decision point has been counted, the current
column-start value remains an explicitly labelled approximation. This gives
deterministic but new landscapes from the same seed and CPC-like correlations
without copying a recorded Amstrad run.

July 2026 review also corrected two concrete issues: the per-column modeled R
value is now stored before applying that column's path cost, and the town block
cache is invalidated for every newly generated world. The modeled R start value
now varies with the session seed, reflecting that a real CPC reaches gameplay
with a menu-time-dependent refresh-register value.

The review's proposed removal of the tank continuation column was rejected
after checking the assembly. `drawspriteblock3` does advance vertically within
one invocation, but `insertenemylandtile` saves its advanced DE in `l885e`;
`gamelevelprogress=4` resumes at `l9206/l91cb` on the following scroll tick and
draws the second tank pair at the new right edge. Thus the CPC tank really does
occupy two successive world columns.

## Sprint 14.101 - Real M1/R Calibration Run, Collapse Guessed R_COST_* Constants

Built a new LOGGEN cartridge variant specifically to measure real R behaviour instead of guessing it, per the Standing Rule above ("R progression must be derived from the M1 instruction-fetch flow", "do not introduce guessed offsets").

**Instrumentation** (`HarrierAttackSourceNew2_alt_CRTC_CART16.asm`, `ifdef LOGGEN`): tags every real `l9134` mode-dispatch tick with which of 10 decision paths fired (`LOG_PATH_MODE0_FLAT`, `HILL_DOWN_OK`/`_BLOCKED`, `HILL_UP_OK`/`_BLOCKED`, `TARGET_RADAR`/`_LAUNCHER`/`_GUN`/`_TANK`, `TARGET_GATE_CLOSED`), captures R at the top of `l9134` (`rEntryLog`, before any of that column's own code runs) and again at `l91e3` (`rExitLog`, the shared exit point after drawing) - `rExitLog-rEntryLog mod 128` *is* the real M1 count for that path, no manual counting needed for the number itself. Old 16-byte-record/rLogSea-etc scheme (Sprint 14.97-era, landscape-capture oriented) fully retired - 8 bytes/record now, `gamelevelprogress==3`-gated. Added a LOGGEN-only invincibility cheat (`checkplayeragainstobjectmap`, `planehitbyobject`, and the two direct enemy-plane/missile death sites all short-circuited) so a play session can reach deep into the land section without dying, purely for calibration purposes - never compiled into the real game.

**Real finding while validating the data**: every target-insertion path (radar/launcher/gun/tank) showed an LCG-continuity gap between its logged record and the next, while `TARGET_GATE_CLOSED` and all non-target paths never did. Root cause: `insertenemylandtile` only draws 2 of each `enemylandsprites` entry's 4 bytes per call, sets `gamelevelprogress=4`, and returns; the *next* tick, `l9206` detects `gamelevelprogress==4`, redraws the remaining 2 bytes via the saved `l885e` pointer, and resets `gamelevelprogress` back to 3 - which is the tick the logger's `cp 3` guard actually accepts. So every target type is genuinely a 2-tick, 2-row sprite placement in real CPC, not just tanks as assumed earlier in this document - it only *looks* tank-specific because radar/launcher/gun's 2nd-row bytes happen to both be transparent (`00,00`), so their continuation tick draws nothing visible, while the tank's (`00,2e`) does. This confirms (with a clearer mechanism) the tank-continuation note directly above, and explains why the Amiga's existing tank-only `TANK_REAR` rendering is visually correct despite being structurally incomplete relative to the real 2-tick mechanism every target type actually uses.

**Calibration result and explicit scope decision** (per direction given in this session): "the most important thing is to recreate the behaviour and random generators on the Amiga side - 95% or better is fine without further investigation; we are not matching the landscape, only the generating code." Measured mean R-delta across 249 real logged columns from one play session: all paths cluster within roughly 60-70 (MODE0_FLAT ~65, HILL_DOWN_OK ~66, HILL_UP_OK ~68, all 4 target-insertion types combined ~63, TARGET_GATE_CLOSED ~69) - no statistically meaningful separation at this sample size, because the dominant source of per-column variance is `l914e`'s own fill-loop cost (scales with `15-height`, identical for every path that reaches it), not the deciding code itself. Given this, `amiga/main.c`'s previously-guessed `CPC_R_COST_FLAT`/`_HILL`/`_TARGET`/`_TANK_REAR`/`_DEFAULT` (0x50/0x58/0x68/0x48/0x53) are collapsed to one real-measurement-calibrated value (63 decimal) for all five - a smaller, more honest model than preserving an invented split the data didn't support.

**Relationship to the Standing Rule / `CpcRDecisionState` above**: that section describes the fuller target architecture (a separate R timestamp per decision point: terrain, target, town, flak). This sprint is a deliberately smaller, real-measurement-informed step toward it, not a replacement - explicitly scoped down per this session's direction to stop at "good enough" rather than building the full per-decision-point struct now. `CpcRDecisionState` remains the right target if/when finer-grained fidelity is ever wanted; the M1-tagged LOGGEN cartridge built here is exactly the tool that would feed it, and can be re-run/extended (e.g. splitting insertion-tick from continuation-tick as distinct logged paths) without further design work if that day comes.

**Independent second sample confirms the calibration**: a second, separate play session (`Snap7.sna`, 250 records - the log filled to its full capacity that run, vs. 249/250 the first time) reproduced the same pattern independently: every path again clustered without meaningful separation (means roughly 48-79 across all 10 paths, same shape as the first sample), and height stayed correctly within the skill-1 bounds (11-14) the entire session. Combined across both independent samples (498 real logged columns total): grand mean R-delta = 64.3, median 60 - within noise of the 63 already calibrated into `amiga/main.c`, so no further adjustment made. This is exactly the kind of cross-run agreement that makes the "no per-path difference, one shared constant is enough" conclusion trustworthy rather than a one-sample fluke.

Verified: clean rebuild (Amiga side, no errors/warnings beyond the pre-existing benign LTO ones), headless autoplay pass with no crash/hang. `amiga/main.c`'s `CPC_R_COST_DEFAULT`/`_FLAT`/`_HILL`/`_TARGET`/`_TANK_REAR` all read 63, matching the measured value from both samples - Amiga code and documented findings are in sync as of this sprint.

## Sprint 14.102 - Sea/Flak R Calibration, Skill-Scaled Ammo Fix

An independent review of Sprint 14.101 (Norwegian "Lederboks" review, same style as the corrective reviews in Sprint 14.100) agreed the land calibration (63) was now well-validated, but flagged a real remaining problem: `CPC_R_COST_DEFAULT` was given the same name/shape as the measured land constants, which makes it *look* equally validated when it was actually still a pure guess - the two LOGGEN samples only ever instrumented the land dispatch. Recommended concretely: instrument R at flak's `ld a,r`, measure sea's entry/exit cost, instrument town's building-selection `ld a,r`, and only introduce distinct calibrated values where measurement shows a real difference - not before.

**Immediate documentation fix**: reworded the `CPC_R_COST_DEFAULT` comment and the flak-tile-choice/town-block-choice comments in `amiga/main.c` to explicitly say UNMEASURED/assumed, rather than letting the shared naming convention imply otherwise.

**Extended the LOGGEN cartridge to cover sea, flak and town** (not just land): the record format gained a `recordType` byte (0=land, 1=sea, 2=flak, 3=town) so all four interleave chronologically in one shared buffer as a session naturally passes through each stage, using the same `rEntry`/`rExit` capture pattern already proven for land. Instrumented `drawseatiles` (single `ld a,r`, no branching - the cleanest possible measurement), `launchflakattack` (entry vs. its own tile-choice `ld a,r`, only logged when flak actually draws), and `buildportstanley` (entry vs. the building-selection `ld a,r`, tagged with the chosen blockId 0-7).

**Results** (`Snap8.sna`, one session, log filled to 250 records before town was ever reached - sea+land+flak crowd out town in a single sitting given the shared buffer, so town still needs a dedicated follow-up run):
- **Sea**: n=100, essentially deterministic - 99/100 samples exactly 7, one outlier at 27 (almost certainly an interrupt landing mid-capture, not a second code path; `drawseatiles` has no branching to produce one). This is a genuinely calibrated value, not an average across a noisy spread like land's.
- **Flak**: n=30, similarly tight - 29/30 exactly 55 M1 fetches from `launchflakattack`'s entry to its tile-choice read, 1 outlier at 91.
- **Land** (same session, cross-check against Snap6/7): consistent with the prior two samples, no new surprises.
- **Town**: 0 records - buffer filled before reaching it. Needs its own dedicated run (e.g. a debug stage-skip, or simply a much longer/more patient session) before `CPC_R_COST_DEFAULT`'s town half can be replaced with a real measurement.

**Applied to `amiga/main.c`**:
- New `CPC_R_COST_SEA` (7, measured) - `resetCpcRandomSequence()`'s per-column loop now checks `terrainKindForCloudColumn(column) == HAR_TERRAIN_SEA` and uses this instead of `CPC_R_COST_DEFAULT` for sea columns specifically. `CPC_R_COST_DEFAULT` now covers only town, and its comment is explicit that it's still unmeasured.
- Flak's tile-choice (`trySpawnFlak()`) now applies the measured `+55` offset before taking bit 0 (`(rState + 55) & 1`), rather than reading the column-start R value directly - since 55 is odd this is a real behavioural correction (flips which tile parity comes out), not just a comment update. Still an approximation - real CPC calls `launchflakattack` from a different point in the per-frame sequence than column generation, so "entry R lines up with column-start R" isn't guaranteed - but it's now a measurement-informed correction instead of an unexamined reuse.
- Town's block-choice is unchanged (still reads raw column-start R) pending its own measurement.

**Separately, checked which other menu-selectable skill parameters CPC actually scales** (prompted by a question about whether the R-cost calibration or anything else had been verified across skill levels - it hadn't; all four LOGGEN samples were captured at `leveldifficulty=1`, the compiled-in default, since `leveldifficulty` only changes via a landing/relaunch progression loop the Amiga port doesn't implement). Traced every `leveldifficulty` read in the CPC source:

| Parameter | CPC formula | Amiga status |
|---|---|---|
| Flak damage budget | `25 - 2*skill` | Already correct (`flakDamageThresholdForSkill`, Sprint 14.96) |
| Land max height (hill ceiling) | `12 - skill` | Already correct (`cpcLandMinimumRow`, Sprint 14.95) |
| Starting/replenished ammo | `bombs=skill+3`, `rockets=bombs/2` | **Was hardcoded 12 rockets/6 bombs at every skill level - genuinely never wired up. Fixed this sprint.** |
| Enemy plane spawn gating | player altitude row `< 11-skill` | Not implemented (fixed world-column trigger table instead) - already an explicit Sprint 14.91 scope decision, not a new gap |
| Land section length before town | `300 + skill*256` | Not implemented (fixed-length route table) - already documented as deliberately deferred (Sprint 14.97) |
| `leveldifficulty` auto-increments each successful landing (capped at 5) | asm:3040-3046 | Not implemented (no landing/relaunch loop yet) - already documented as deferred |
| Border-flash delay timing (accel/decel) | scales with skill | CPC-hardware border-color effect - no Amiga equivalent exists to compare against |

Only the ammo line was a genuine, previously-unnoticed gap. Fixed with a new `ammoForSkill(skillLevel, &bombs, &rockets)` matching the exact CPC formula (`bombs=skill+3`, `rockets=bombs/2` integer division), wired into both `startGameSession()` (game start, mirroring how `lives`/`cpcLandSkillLevel` are already set post-`initGameState()` for the same "must reflect the just-picked skill, not a stale default" reason) and `replenishPlayerFromFrigate()` (landing replenish, previously a flat 12/6 there too).

Verified: clean rebuild, headless autoplay pass with no crash/hang.

## Sprint 14.103 - Terrain-Steepness Investigation (tile anchor + real generated-data logging)

User report: terrain looks like it can change too abruptly on the Amiga port, especially uphills. Explicit review guidance: don't smooth the RNG or add artificial direction-locking (would diverge from CPC-likeness) - first make the generator's own transition type explicit and checkable, then verify the tile graphics/anchor, and only consider a deliberate wider-Amiga-slope presentation layer (kept separate from the CPC generator) if everything else checks out and it still looks too steep.

**Added `cpcLandTransitionTable[CPC_LAND_PROCEDURAL_LENGTH]`** (`CpcLandTransition` enum: FLAT/CLIMB/DESCEND/TARGET) alongside the existing height/surface tables - pure metadata, no change to generation behaviour. Populated directly in `resetCpcRandomSequence()`'s mode dispatch (mirrors the same branches that already choose height/tile).

**Extended the `HAR_DEBUG_PERF_LOG` validation loop**: beyond the existing |delta|<=1 check, now cross-checks the chosen tile against the transition it represents - climbs (height decreasing) must land in tiles 24-27, descends (height increasing) in 28-31, flat columns must never use either group. None of these fired during a headless verification pass.

**Resolved the tile/vertical-anchor question via direct pixel-data analysis, not a screenshot.** `amiga/assets/generated/cpc/cpc_tiles.json` already stores fully-decoded 8x8 pen-index arrays for every promoted tile (not just raw bytes) - rendered tiles 24-31 as ASCII art directly from that data:

```
tile 24 (HILL UP 1)      tile 28 (HILL DOWN 1)
......##                 ##......
....####                 ####....
...#####                 #####...
...#####                 #####...
..######                 ######..
..######                 ######..
.#######                 #######.
########                 ########
```

Both show a clean diagonal spanning the full tile - the edge toward the *previous* column stays near the old height almost to the tile's bottom, while the edge toward the *new* column reaches the new height near the top. This is exactly the shape needed for a smooth one-row transition anchored at the new height row, matching how `buildWorldTileColumn()` already places it (`tileY == terrainY`, the post-step height). GRASS tiles (32-35) are solid-filled below their top texture row, matching `solidlandspriteblock`'s own fill tile, so the boundary between the surface tile and the rows filled below it is seamless. Conclusion: the tile graphics and their anchor are correct - this was not the cause.

**Added `land_log.csv`**, a new debug-only dump of the actual generated table (`index,height,surfaceTile,transition` per column), gated behind a new `HAR_DEBUG_LAND_LOG` flag, using the exact same RAM-buffer-then-flush-after-`FreeSystem()` pattern `perf_log.csv` already established (same Forbid()/Disable() deadlock reasoning - see the comment on `perfLogAppend()`). Unlike the perf log, this doesn't accumulate across a session; `landLogBuild()` resets and rebuilds fully each time `resetCpcRandomSequence()` runs, so a mid-session restart doesn't produce a confusing double dump.

**Ran it via headless autoplay and compared directly against real CPC-captured data** (the 188-column WinAPE trace still held from Sprint 14.99's original verification work, not re-shipped, used here only as a one-time comparison point):

| | Real CPC (188 cols) | Amiga (295 cols, one run) |
|---|---|---|
| Height-change rate | 31.0% | 29.6% |
| Immediate reversals (climb directly followed by descend, or vice versa) as a fraction of all height changes | 17.2% | 25.3% |

Overall height-change frequency matches closely, confirming the mode-dispatch probability is right. The immediate-reversal rate looks somewhat higher in this one Amiga sample, which would read as more frequent small jagged notches rather than smooth slopes - but with only 58 vs. 87 height-change events total, this gap sits within plausible single-sample noise (not a confirmed systematic difference). Not chased further per the project's own "95%+, don't over-invest" standard from Sprint 14.101/14.102 - `land_log.csv` is now available for gathering more sessions' worth of data (from either side) if a clearer answer is ever wanted.

**Standing conclusion**: the height algorithm, tile choice, and tile vertical anchor all check out. If the terrain still reads as too abrupt after watching more of it in motion, the most likely remaining explanations are (a) CPC Mode 1's non-square pixel aspect ratio making the same logical transition look visually gentler there than on Amiga's more square pixels, and/or (b) the modestly-higher reversal rate above, if a larger sample confirms it's real. Either would call for a deliberate, separate Amiga presentation adaptation (e.g. `AMIGA_WIDE_TERRAIN_SLOPES`, spreading one logical height step across two rendered columns with the collision model updated to match) - not a change to the generator itself. Not implemented this sprint; revisit only if requested.

Verified: clean rebuild (both flag-off and flag-on configurations), headless autoplay pass with no crash/hang, `land_log.csv` confirmed generated and readable end to end.

## Sprint 14.104 - Deliberate Reversal-Smoothing Rule (Amiga-only, not CPC-faithful)

Follow-up to Sprint 14.103's "somewhat higher reversal rate, but within single-sample noise" finding: the user's own direct visual comparison of both versions confirmed real CPC's terrain reads as smoother, overriding the earlier "don't smooth the RNG" position now that this is a matter of matching the *perceived* CPC experience rather than the literal algorithm - explicitly acknowledged as a deliberate, non-CPC-faithful adjustment, not a bug fix. No such check exists anywhere in `l9134`/`l9167`/`l9181`'s real mode dispatch; this is invented for the Amiga port only.

**First cut** (in `resetCpcRandomSequence()`'s land generator): track `landLastDirection` (-1 = last column climbed, +1 = descended, 0 = flat/target) and force flat instead of dispatching a mode that would immediately reverse it - i.e. require at least 1 non-slope column between opposite-direction slopes. Verified via `land_log.csv`: 0-gap immediate reversals dropped from a measured 25.3% of all height changes to 0.

**Second cut**, prompted by a follow-up report ("downhill undershoots in a steep little dump") that the log confirmed precisely: 12 of 73 height changes (16.4%) in that first-cut run were "climb, exactly one flat column, descend" (or the reverse) - the exact shape described. Extended the rule: `landLastSlopeDirection` now persists across flat/target columns (only overwritten by a new real slope), paired with a `landFlatRunSinceSlope` counter reset on every slope and incremented on every non-slope column; a reversal is blocked until `LAND_REVERSAL_MIN_GAP` (2) non-slope columns have actually elapsed, not just one. Verified via a fresh `land_log.csv`: 0-gap reversals still 0, 1-gap reversals also dropped to 0; 2-gap reversals (5 of 53 height changes) still occur and are expected - the rule requires a minimum breather, not a permanent ban on ever reversing direction.

Both cuts keep the CPC's actual RNG sequence, height limits, and tile choice completely untouched - only whether a requested mode 1/2 dispatch is allowed to proceed is affected, and only in the specific "reversing too soon" case.

Verified: clean rebuild after each cut, headless autoplay pass with no crash/hang, all HUD-corruption guard counters at 0 throughout both verification runs.

## Sprint 14.105 - Real Root Cause Found: Descend Draw-Timing Bug (Sprint 14.104's rule removed)

A screenshot plus a precise observation ("tile seems to be set one tile too low before the next tile" on downhill) prompted re-checking the ASM one more time rather than accepting Sprint 14.104's smoothing rule as the answer - and it found the actual bug.

**Re-verified `l916a` (asm:5500-5525, mode 1/descend) and `l9181` (asm:5527-5559, mode 2/climb) side by side.** Climb explicitly does `dec h` - the register `l914e` draws to - so it draws the new (already-decremented) height immediately. Descend never touches `H` at all: it loads `H`/`L` from `l885c`/`l885d` (old height), computes the new height into `a`, stores *that* to `l885d` for the next column to read, and jumps to draw with `H` still holding the OLD height. This is a genuine, confirmed asymmetry in the real game, not a transcription slip - and this port's generator was drawing *both* modes at the post-step height, putting descend one row lower than the real game does. This is likely the earlier "hill_down_uses_old_height=False fits calibration data marginally better" choice from the original Sprint 14.99 reconstruction (a ~2% margin on a 188-column sample, i.e. noise-level) overriding what the disassembly actually says - worth remembering that a weak statistical preference is not a substitute for reading the code.

**Fix**: introduced `drawHeight`, distinct from `landHeight` (the persisted `l885d`-equivalent used for the next column's ceiling/floor comparisons). Defaults to `landHeight` (correct for every case except one). Descend needs no code change at all - `drawHeight` already holds the pre-step value by the time that branch runs, since nothing between its declaration and that branch touches `landHeight`. Climb explicitly refreshes `drawHeight = landHeight` after decrementing, matching its own `dec h`. The final `cpcLandHeightTable[i] = drawHeight` (previously `= landHeight`) is the only table-write change.

**Consequence for the Sprint 14.103 tile/transition validation**: since descend's visible height change now shows up one column later than the transition that caused it (matching the real game), a delta-based tile check would misfire on every real descend. Rewrote it to compare `cpcLandSurfaceTable[i]` against `cpcLandTransitionTable[i]` directly (both set at the same index regardless of the draw-timing shift) instead of re-deriving intent from a height delta.

**Sprint 14.104's reversal-smoothing rule removed entirely** (`landLastSlopeDirection`, `landFlatRunSinceSlope`, `LAND_REVERSAL_MIN_GAP`, and the mode-forcing check) after the user confirmed visually that the draw-timing fix alone resolved the "steep little dump" look - the smoothing rule was compensating for this bug's symptom, not a real CPC behavioural difference. The generator is now an unmodified reproduction of `l9134`/`l9167`/`l9181`'s actual mode dispatch again, with no invented Amiga-only rules.

Verified: clean rebuild, headless autoplay pass with no crash/hang, all HUD-corruption guard counters at 0 throughout.

## Sprint 15 - Wingman: Roadmap

Full ASM-level review (Norwegian "Lederboks" review) of every `wingman*` routine in `HarrierAttackSourceNew2_alt_CRTC_CART16.asm` established that the real CPC wingman is a near-complete second Harrier: its own position/state machine (`wingmantakeoff`, 13 meaningful values incl. death/wreck), own rocket and bomb (infinite bombs - he's a powerup, no ammo stock), object-map collision (id 20 real CPC-side; kept as `HAR_OBJ_WINGMAN=20` on the Amiga port already), enemy-plane interception, ground-target bombing runs sharing the same `enemylandlocationlock` the Maverick system uses, carrier takeoff/landing (including a hard rule that a slow Player-2 human gets left behind and must be recovered as a powerup), and either CPU or human ("PLAYER 2") control selected from the menu (`wingmanon: defb 0`).

**Important source finding, kept as a standing caveat for every sprint below**: the CPC main loop's normal per-frame wingman calls (`erasewingmanwakescrolling`, `drawwingmanplane`) are commented out in this build; only the weapon-update calls and `controlwingmanfunc` (only reached from the landing loop) run live. The state machine, formation math, AI, and weapon logic are all real and fully readable in the disassembly, but whether normal-flight wingman rendering ever actually ran in the shipped CPC+ build is unconfirmed. Decision (per direction given this session): treat the documented states/rules as the intended design and build the Amiga port to that intent, not to the possibly-disabled shipped behaviour - revisit only if a real CPC capture surfaces showing wingman genuinely never appears in flight.

**Hardware constraint that shapes the whole rendering approach**: all 8 Amiga hardware sprite channels are already committed - `copSetSprites()`/`buildGameHudCopper()` wire ch0+1 (player, attached pair), ch2 (bomb/impact), ch3 (enemy plane), ch4 (enemy missile), ch5 (rocket), ch6 (powerup), ch7 (null/reserved terminator). There is no free channel for an attached wingman pair without taking something else away. Wingman is therefore a masked blitter Bob, not a hardware sprite - this codebase has no blitter-Bob compositor yet, so building one is itself part of the work, not a reuse of an existing system.

**Design decisions confirmed for this milestone** (mirroring the CPC data faithfully rather than reinventing it):
- `WingmanControl` (`Off`/`CPU`/`Player 2`) is a distinct setting from the wingman's own flight-mode state - matches the CPC's own separation of `wingmanon` (who's driving) from `wingmantakeoff` (what the plane is doing).
- The 13 raw `wingmantakeoff` values become a named `WingmanMode` enum, not magic numbers, once the state machine sprint is built.
- Enemy missiles need an explicit target identity (`EnemyTarget`: player vs. wingman) before interception can be complete - today's enemy missile only ever aims at the player.
- Ground-target bombing must reuse the existing single `TargetLock`/`enemylandlocationlock`-equivalent, not a second independent search.
- Player 2 uses physical joystick port 1 (`JOY0DAT`, `$DFF00A`) via a port-parametrised `readJoystickPort(port, state)`, not duplicated per-port functions; player 1 keeps port 2 (`JOY1DAT`/`$DFF00C`, already `Joy1Dat()`) untouched.
- Each `WeaponState` (wingman rocket, wingman bomb) gets its own explicit struct instance, not a reused/re-pointed shared block the way CPC's IY-indirection does it - clearer and safer to debug with the memory this port has available.
- Friendly fire (can the player's own weapons hit the wingman?) is explicitly flagged as unverified in the CPC source and must not be assumed either way before a targeted ASM check.

**Sprint sequence** (deliberately finer-grained than the reviewed Fase 1-5, so each one is independently buildable/testable and commit-sized):

| Sprint | Scope | Depends on |
|---|---|---|
| 15.1 | Real `Wingman: Off/CPU/Player 2` menu setting, persisted into `GameState.wingmanControl`. No wingman subsystem reacts yet. | - |
| 15.2 | Generic masked blitter-Bob compositor (new subsystem - alloc/draw/erase against the ring world buffer, independent of any specific object) + wingman flight-graphics conversion from the CPC asset data. | 15.1 |
| 15.3 | **Done, revised.** `WingmanState` added to `GameState`; carrier takeoff timing (spawn once player has passed/launched) and stable CPU formation flight (3-tile offset, above/below choice) rendered via the Bob compositor; registers `HAR_OBJ_WINGMAN` in the object map. Actual movement model turned out simpler than planned here - position is derived directly from the player's own position every frame (row throttled/tile-stepped, column continuously recomputed and rendered pixel-smooth as of 15.4 below) rather than a full 9-direction step-toward-target system; see 15.4's note on why full directional stepping was left as a fallback design instead of built now. | 15.2 |
| 15.4 | **Done, revised scope.** Delivered: pixel-smooth horizontal Bob rendering (`buildWingmanBobShiftedTiles`), and row-based obstacle avoidance (`wingmanFormationRowIsSafe`/`wingmanSafeTargetRow` - walks to the nearest clear row above when the formation-offset row would fly into terrain). This is a deliberately simpler stand-in for CPC's full `checkwingmanradar` (probe one of 9 directions, R-select an alternate of the other 8 if blocked) - it stops the wingman flying into a hill from the front but doesn't laterally dodge. The full 9-direction/R-alternate system (which would also require a real persistent `column` field the wingman steps rather than deriving fresh each frame) remains the documented fallback if this proves insufficient in practice. | 15.3 |
| 15.5 | **Done, resequenced.** Jumped ahead of the roadmap order on explicit request ("wingman doesn't fight yet") straight to interception AI + weapons - see its own write-up below. Original 15.5 scope (joystick port generalisation, manual Player 2 control) not done, remains future work. | 15.3, 15.4 |
| 15.6 | **Done, resequenced/rescoped.** Became a hardware-sprite reallocation pass (wingman body + rocket moved off Bob rendering onto real sprite channels, to eliminate a flutter bug found while testing 15.5) rather than object-map collision/death states - see its own write-up below. Original 15.6 scope (wingman death, `EnemyTarget` friendly-fire selection) not done, remains future work. | 15.5 |
| 15.7 | Wingman weapons: own `WeaponState` rocket and bomb (infinite bomb supply, matching the "he's a powerup" CPC comment), wired for both CPU-fire and Player-2-fire. | 15.5, 15.6 |
| 15.8 | CPU interception AI: leave formation toward an enemy plane, close on its height/lead point, fire, return to formation. | 15.4, 15.7 |
| 15.9 | CPU ground-attack AI: eligibility check against the shared target lock (CPU-only, normal-flight-only, one-shot-per-target, R-gated ~1-in-4 selection matching CPC), scrolling waypoint approach, bomb release, return to formation. | 15.4, 15.7 |
| 15.10 | Wingman powerup revival (`POWERUP_WINGMAN` forced spawn after death, already reserved in the enum but currently downgraded to a health pickup - see `amiga/main.c` around the Sprint 14.96 powerup comments) restores `WingmanState` to formation instead. | 15.6 |
| 15.11 | Landing: CPU dual-waypoint approach with alternate deck slot if player 1 blocks it; manual Player-2 landing (Y=deck height, X within the landing zone); mission-end wait loop holds until both planes are down or wingman is dead, matching CPC's landing-loop behaviour. | 15.5, 15.6 |
| 15.12 | A500 worst-case profiling pass (Bob compositor + AI + both weapons + both planes' collision, all live at once) and finishing pass over the whole feature. | all above |

Each sprint gets its own dated write-up below once implemented, following this document's usual Status/Why/Tasks/Done-checks structure.

## Sprint 15.1 - Real Wingman Off/CPU/Player 2 Menu Setting

First slice of the Sprint 15 roadmap above: give the menu's wingman line a real, persisted, cyclable value, with nothing downstream reacting to it yet - the same "foundation before behaviour" approach the skill/lives settings already went through.

**Menu changes** (`amiga/main.c`): the old `drawMenuRightSettings()` status line ("Wingman: Off", a static label that never did anything - same category of issue the Sprint 14.91-era "Input"/"Controls" toggles had before being cleaned up) is removed. In its place, a new selectable left-column item, `MENU_ITEM_WINGMAN` (index 3, `MENU_ITEM_COUNT` now 4), reuses the row at y=152 that the right-hand status column already occupied - the left column simply hadn't used that row yet, so this needed no layout reflow. `menuItemY()`'s `itemY[]` table extended to `{116, 128, 140, 152}` to match.

**New `WingmanControl` enum** (`WINGMAN_CONTROL_OFF/_CPU/_PLAYER2`, declared next to `TargetLock`/`GameState`) and a new `GameState.wingmanControl` field. `main()` gained a `wingmanControl` local exactly mirroring how `skillLevel`/`livesSetting` already flow from menu input through `drawMenuScreen()`/`updateMenuSelection()`/`menuItemText()` down into `startGameSession()`, which now takes a `wingmanControl` parameter and stores it into `game->wingmanControl` right alongside where `skillLevel`/`livesSetting` are applied post-`initGameState()`. Selecting the item and pressing fire/select cycles Off -> CPU -> Player 2 -> Off, identical in structure to the existing Skill/Lives selection branches.

**Deliberately not done yet** (scoped to later sprints per the roadmap above): no `WingmanState`, no rendering, no joystick-port generalisation, no help text describing Player 2's controls (would be misleading to show before port 1 reading actually exists) - this sprint is the menu/data plumbing only.

Verified: clean rebuild (no new warnings beyond the pre-existing benign LTO ones). Visual check via a live WinUAE screenshot confirms the new "WINGMAN: OFF" row renders correctly in the left column beneath "LIVES: 3", and the right-hand status column now correctly shows only its original 3 lines (Rocket range/Input/Maverick) with no leftover duplicate.

## Sprint 15.2/15.3 - Bob Compositor + CPU Formation Flight (combined)

Delivered together rather than as separate sprints: a compositor with no caller, and a WingmanState with nothing to render, would each have been a half-finished commit on their own - "a visible, testable wingman flying formation" (the roadmap's own Fase-1-equivalent goal) is the smallest slice that's actually whole.

**Research first**: before writing any compositor code, dispatched a research pass over `amiga/main.c`'s world-buffer/rendering architecture, since the world buffer is a genuine ring buffer (`GAME_WORLD_BUFFER_WIDTH`=1040px/130 tiles, physically reused every `GAME_WORLD_SCROLL_PAGE_BYTES`=86 tiles via `ringWorldTileXForColumn()`'s `worldColumn mod 86` mapping), not a simple scrolling screen. Key finding that shaped the whole design: already-visible, already-drawn columns can be rewritten at any time, off-schedule, by `dirtyRedrawWorldColumn()`/`dirtyRedrawWorldTile()` (weapon impacts, flak spawn/clear, ship/town-block hits - `updateWeapons()`, `trySpawnFlak()`, `updateGameCollisions()`). A classic Amiga Bob's "save background pixels, blit, later restore those exact saved pixels" technique would go stale the instant one of those fires under the Bob's footprint and silently undo a real gameplay change (paint over a fresh crater, restore a flak tile that was just cleared). Also confirmed: no blitter-DMA (`BLTCON0`/`BLTSIZE`/etc.) is used anywhere in this codebase yet - every existing draw, including the one masking precedent (`drawGameScrollTileMasked()`, the carrier/gunship overlay's per-row `(dest&~mask)|(src&mask)` blit), is a plain CPU copy.

**Design decisions this drove**:
- **Erase-by-redraw, not save/restore.** `wingmanBobEraseFootprint()` just calls the ring buffer's own authoritative `renderRingWorldColumn()` on the vacated columns - there's no pixel snapshot to go stale, at the cost of a full column rebuild on each move (acceptable since movement is tile-grid-locked, so erases only happen on an actual row/column change, not every frame).
- **Tile-grid-locked movement, not pixel-smooth.** The wingman's footprint is exactly 2 tile columns wide and 1 tile row tall, and it steps a whole 8px tile at a time. This is not a simplification for convenience - the user's own CPC research explicitly states the real wingman "beveger seg en CPC-tile per oppdatering" (moves one CPC tile per update). Matching that exactly also means the compositor never needs sub-byte pixel shifting (the kind of arbitrary-bit-shift blit a real hardware blitter would handle via its shift/mask registers) - every draw/erase is a whole-tile operation using the exact same byte-per-column addressing the terrain ring buffer already uses.
- **CPU-copy masking, deferred blitter-DMA upgrade.** `drawGameScrollTileMasked()`'s existing 8-row×(4 colour planes+1 mask byte) format is reused as-is for the wingman's own tiles rather than inventing a second format or writing the first `BLTCON0`/`BLTSIZE` code in this codebase blind. A real blitter version (same minterm, offloading the per-frame composite from the 68000) is a reasonable future optimisation for Sprint 15.12's profiling pass, not a change to the Bob model itself.
- **Runtime C conversion, not a Python asset-pipeline change.** `harCpcWingmanFlyingLeftPixels`/`RightPixels` (`amiga/assets/cpc_promoted_assets.h`) were already extracted/promoted/compiled in but had zero consumers. `buildWingmanBobTileHalf()` converts them to the masked-tile format once at runtime (mirroring how `buildAttachedSpriteFromCpcPlusHalves()` already does the same kind of live pen-to-output conversion for the player sprite) using the existing `harCpcPlusPenToGameColor[]` table (`amiga/assets/cpc_promoted_sprite_tiles.h`) - the same CPC-Plus-pen-to-playfield-colour mapping the carrier/gunship art already trusts, so the wingman's grey ramp lands on the same colours as everything else instead of a second guessed mapping. Kept out of `tools/*.py` deliberately, since those files carry unrelated uncommitted changes from parallel work this session that shouldn't be touched.
- **On-deck rendering needed no new work.** The carrier's baked deck art (`amiga/assets/cpc_promoted_sprite_tiles.h`) already shows a static landed wingman painted directly into the carrier tiles. Launch is approximated as "the wingman joins formation the same frame the player clears the deck" (`TAKEOFF_STATE_LIFTING` -> `TAKEOFF_STATE_AIRBORNE`) rather than a separate taxi/climb animation - there's nothing to animate before that point since the static art just scrolls away like the rest of the carrier.

**New `WingmanState`** (`active`, `mode` (`WingmanMode` - only `WINGMAN_ON_DECK`/`WINGMAN_FORMATION` driven this sprint, the rest of the 13 CPC-derived states declared for later sprints), `formationBelow`, `row`, `moveTimer`, footprint-tracking fields), added to `GameState`, reset in `initGameState()`. Formation target: 3 tiles left of the player (screen-relative, recomputed fresh from `scrollX+playerX` every frame - no independent horizontal physics needed since the player's own screen X isn't actually fixed, it drifts with `playerTargetXForSpeedLevel()`), 3 tiles above by default or below if that would go off the top of the screen (`updateWingmanFormationTargetRow()`, matching the CPC rule described this session: "Hvis onsket posisjon kommer utenfor toppen av skjermen, byttes det til formasjon under spilleren"). Row steps toward the target one tile per `WINGMAN_MOVE_FRAME_INTERVAL`(4) frames - 8px/4 frames = 100px/s, matching the player's own `PLAYER_MOVE_SPEED_PIXELS`(2)/frame vertical speed so neither plane looks like it's cheating past the other.

**Gating**: only spawns when `wingmanControl == WINGMAN_CONTROL_CPU` (Player 2 control is Sprint 15.5's own scope - showing a CPU-flown wingman under a "Player 2" selection would misrepresent what's implemented, so it deliberately shows nothing yet).

**Verification method**: a diagnostic build temporarily forced a fixed on-screen test position to confirm the compositor itself (masking, addressing, ring-wraparound handling) worked in isolation from the formation-tracking math, before debugging the real formula - this isolated a first false alarm (the wingman was in fact rendering correctly the whole time; an earlier screenshot inspection had simply not zoomed into the right part of the sky to find a small 16x8px sprite). Confirmed via headless autoplay + zoomed screenshots: the wingman spawns the frame the player clears the deck, renders as a recognisable small grey aircraft silhouette, and tracks the player's altitude with the correct 3-tile offset and above/below switching. One cosmetic oddity noted but not chased: a couple of pixels render with a yellowish tint that the pen-to-colour table (`harCpcPlusPenToGameColor`) cannot actually produce from this sprite's source pen values (max pen 6, which maps to white) - most likely a WinUAE window-scaling artifact (this codebase's testing skill already documents non-integer-scale moire/blending on thin patterns as a known false-alarm class), not a logic bug, but worth a native-resolution re-check if it persists once this is played interactively.

Verified: clean rebuild (no new warnings), headless autoplay pass with the temporary `WINGMAN_CONTROL_CPU`/`HAR_HEADLESS_AUTOPLAY` test flags reverted afterward, no crash/hang across the full run.

## Sprint 15.2/15.3 follow-up - Horizontal Vibration Fix

User report after playing with `Wingman: CPU` selected: the wingman visibly "vibrates" horizontally instead of holding a steady formation position.

**Investigation**: screenshot-based pixel analysis was inconclusive at first (a column-histogram of the wingman's grey pixels showed an inconsistent 12-vs-28-column-wide blob across frames, which looked like two overlapping copies but turned out to just be the wingman's own natural silhouette width sampled at different vertical slices while it happened to be climbing between formation rows). Switched to hard data instead of more screenshots: added a temporary per-frame CSV log (`DH1:wingman_log.csv`, same RAM-buffer-then-flush pattern as `land_log.csv`/`perf_log.csv`) capturing `scrollX`, `playerX`, the computed `worldColumnLeft`, and - critically - that column expressed relative to the display's own current scroll offset (`scrollLocalByteOffset()`), which should be a constant if the wingman is genuinely holding a fixed screen position.

**Root cause, confirmed by the log**: it wasn't constant - it flip-flopped between two adjacent tile values (e.g. 8/9/8/9...) every 2-3 frames. `updateWingmanBob()`'s original formula computed the wingman's world column as a plain `(scrollX + playerX - offset) >> 3`, treating scroll position as an ordinary flat pixel quantity. But this ring buffer's actual coarse/fine scroll split (`scrollPointerPixelX()`) rounds to 16px boundaries with a "-16 when already aligned, else -fine" special case that a naive `>>3` doesn't reproduce. The two formulas agreed most of the time but rounded to different tiles for a frame or two around each boundary crossing before re-agreeing - visible as the wingman flickering back one tile and forward again, repeatedly, exactly matching "vibrating."

**Fix**: `updateWingmanBob()` now computes its world column via `scrollLeftWorldColumnForScroll(game->scrollX)` - the exact same function `serviceRingWorldStream()` already trusts for "what world column is at the screen's left edge" - plus the desired screen-relative tile offset, instead of an independent formula. Reusing the ring buffer's own established reference point guarantees agreement with whatever the display actually shows, rather than hoping two separately-derived formulas stay in sync.

**Verified via the same diagnostic log**: the screen-relative column held constant (e.g. steady at 7) for 330+ consecutive frames post-fix, versus flip-flopping every 2-3 frames before - confirmed again visually via closely-spaced screenshots showing no repeating back-and-forth. All temporary diagnostic code (the CSV logger, its buffer/flush functions, a forward declaration, and the `HAR_DEBUG_WINGMAN_LOG` flag) was fully removed once the fix was confirmed - it existed only for this investigation, not as a permanent tool.

Verified: clean rebuild (no new warnings, all debug/test flags reverted to their shipping state), headless autoplay + interactive-equivalent screenshot checks confirm stable formation tracking.

## Sprint 15.4 - Pixel-Smooth Bob Rendering + Row-Based Obstacle Avoidance

Two follow-ups landed together: a rendering smoothness upgrade the tile-locked vibration fix above made obviously worth doing once the wingman held a stable position, and the roadmap's Sprint 15.4 goal (obstacle avoidance via the object map).

**Pixel-smooth horizontal rendering.** The Sprint 15.2/15.3 Bob was deliberately tile-grid-locked horizontally - correct per CPC's own "moves one tile per update," but visually choppier on Amiga than the same logical motion ever looked on CPC, once the vibration bug was gone and the *remaining* 8px hops became the most noticeable thing about it. `buildWingmanBobShiftedTiles(pixelOffset)` (replacing the old fixed left/right-half tile builder) rebuilds the masked Bob image shifted by `worldPixelLeft & 7` every frame, spanning 2 or 3 playfield cells depending on whether the shift spills into a third one. This is a deliberate Amiga-only smoothing on top of CPC-faithful tile-stepping (documented as such directly on `WingmanState`), not a change to the *logical* one-tile-per-update movement rule - only to how continuously it's *drawn*.

This reintroduced the exact class of bug the previous fix targeted (computing `worldColumnLeft` via a plain `(scrollX+screenOffset)>>3` again, not `scrollLeftWorldColumnForScroll()`) - but no longer causes vibration, because the render no longer makes a discrete tile-rounding decision that can disagree with the display frame-to-frame: `worldPixelLeft` (`scrollX + screenOffsetPixels`) is used at full pixel precision (split into `column`/`pixelOffset` via `>>3`/`&7`, not independently rounded), so any small absolute-alignment difference from the ring buffer's own margin-adjusted reference shows up at most as a fixed few-pixel offset, never as a per-frame flicker. Confirmed via the same screenshot-measurement technique used for the original bug: the wingman's left edge held at column 226 in 5 of 6 rapid-fire screenshots (one single-frame reading at 230, not a repeating pattern) - a solid improvement over the pre-fix 8/9 flip-flop.

Because erasing now needs to happen on essentially every frame (the sub-pixel offset changes continuously, not just every 8th of scroll), `wingmanBobEraseFootprint()` was switched from calling `renderRingWorldColumn()` (a full 25-row rebuild plus the carrier/gunship/town-block overlay pass, per-column) to rebuilding just the one occupied `tileRow` via `buildWorldTileColumn()` directly - the previous approach would have been far too expensive at this new erase frequency. Known, accepted limitation: this lighter erase skips `drawDirectColumnRangeObjects()`'s overlay pass, so erasing while the wingman crosses directly over carrier/gunship/town-block art would briefly show the plain terrain tile instead of the overlay - a narrow, cosmetic edge case, not fixed here.

**Row-based obstacle avoidance** (`wingmanCellIsPassable`/`wingmanFormationRowIsSafe`/`wingmanSafeTargetRow`): a deliberately simpler alternative to the full CPC `checkwingmanradar` mechanism described in the Sprint 15 roadmap (probe one of 9 directions, substitute an R-selected alternate of the remaining 8 if blocked). Real CPC data classifies sky/cloud/flak as passable and everything else (terrain, buildings, ships, other solid objects) as blocking; this port applies that same passable-set check across the wingman's full 16px footprint plus 4 cells of lookahead, and if the formation-offset row isn't clear, walks upward one row at a time until it finds one that is (falling back to row 1 if nothing is ever clear). This prevents the wingman flying straight into a rising hillside from the front - the main practical risk - without implementing full lateral dodging (moving left/right to route around an obstacle while holding altitude). Documented here as an intentional scope reduction, not an oversight: if a hill is wide enough that no nearby row is ever clear within the lookahead, or if lateral obstacle routing turns out to matter in practice, the full 9-direction/R-alternate system remains the documented fallback design.

**Consistency fix**: the wingman's spawn-time row (set the frame the player clears the deck) was still calling `updateWingmanFormationTargetRow()` directly, unwrapped by the new safety check - meaning a wingman could theoretically spawn into an unsafe row for one frame before the next update corrected it. Fixed to compute the spawn-time `worldColumnLeft` the same way `updateWingmanBob()` does and wrap the initial row in `wingmanSafeTargetRow()`.

Verified: clean rebuild (no new warnings), headless autoplay with `Wingman: CPU` ran a full ~94s session with no crash, and rapid-fire screenshot comparisons confirm stable (non-vibrating) formation tracking with the new pixel-smooth rendering.

## Sprint 15.5 - Wingman Interception AI + Weapons

User report after playing with the Sprint 15.4 wingman: "it helps but doesn't shoot yet." Jumped ahead of the roadmap's own sequencing (which had weapons at 15.7/15.8, after collision/death handling) straight to combat, per explicit direction to use the CPC ASM as the reference for how the wingman actually fights rather than inventing a fire model.

**ASM verification** (`HarrierAttackSourceNew2_alt_CRTC_CART16.asm`): `checkfirewingmanmissile` gates *player-controlled* fire on a physical fire button and is Player-2-only; `firewingmanmissile` is the shared fire routine the CPU AI calls directly, bypassing that input gate entirely - so CPU-controlled fire has no button check at all, matching this port's CPU-only scope. `checklaunchbombwingman`/`dolaunchbomb` confirm the wingman's bomb has no ammo count ("he's a powerup"). The real fire condition is `sub c; sub 10; jp c,...` - fire when the horizontal gap to the target is under 10 CPC pixels *and* altitude matches, not a timer or random roll.

**New `maybeStartWingmanIntercept()`**: rolled on every enemy-plane spawn (`spawnEnemyPlane()`), 1-in-2 chance (`WINGMAN_INTERCEPT_CHANCE_MASK`), matching the ASM's `r&1` gate at the equivalent spawn site. Real CPC has two separate interception states (`wingmantrackenemyplanefirstpass`/`wingmantrackenemyplane2ndwaypoint`); merged into one `WINGMAN_INTERCEPT_APPROACH` state here as a documented simplification, since the two-waypoint split exists mainly to manage CPC's own coarser update granularity.

**`updateWingmanIntercept()`**: closes horizontally on the enemy plane every frame (not throttled, unlike formation row-seeking) toward a lead point ahead of the target, adjusts row toward the enemy's altitude at the usual `WINGMAN_MOVE_FRAME_INTERVAL` cadence (reusing `wingmanFormationRowIsSafe()` from 15.4 so it won't step into terrain while intercepting), and fires the wingman's own rocket (a plain `WeaponState`, reusing `buildRocketSprite()` unchanged since the wingman fires the same tile-56 non-Maverick missile as the player) once altitude and horizontal gap both close within range.

**Three bugs found and fixed during testing, in order**:
1. **Closing too slowly.** First pass throttled horizontal closing to the same `WINGMAN_MOVE_FRAME_INTERVAL` as row-seeking; a temporary CSV log (`wingman_ai_log.csv`) showed `interceptScreenX` advancing roughly 4x too slowly to ever catch a departing enemy plane before it despawned. Fixed by closing horizontally every frame.
2. **"It does kamikaze."** The literal CPC fire-range constant (10px) is smaller than this port's actual 16px-wide sprites, so the wingman was firing while still visually overlapping the target. `WINGMAN_INTERCEPT_LEAD_PIXELS`/`WINGMAN_INTERCEPT_FIRE_RANGE_PIXELS` scaled up (6->20, 10->32) to preserve the ASM's real intent (a clean stand-off shot) at this port's actual sprite scale, rather than matching the raw pixel value.
3. **"Jumps back under my wing like a ghost."** Mode flipped `INTERCEPT_APPROACH` -> `FORMATION` instantly on fire or enemy despawn, teleporting the position source. Fixed with a new `returningToFormation` flag and `updateWingmanReturnToFormation()` that steps smoothly back to the formation slot before handing off - which in turn surfaced and fixed a self-caught oscillation bug (naive fixed-step-toward-target movement can overshoot forever if the gap isn't an exact multiple of the step size, so `returningToFormation` could never resolve); both the intercept-chase and return-to-formation steppers now clamp the final step to the target instead of overshooting past it.

**"Flutters at speed" - investigated, not fixed here.** Diagnostic CSV logging (`wingman_log.csv`) proved the wingman's logical position (row/column/pixel-offset) is perfectly stable and predictable at every speed level tested; the flutter is not a logic bug. Root cause: the wingman is a Bob drawn into the single-buffered, actively-scanned-out world bitmap, and its erase+redraw (needed on essentially every frame once sub-tile pixel offset is involved, per 15.4) can race the raster beam - worse the more the wingman is on screen continuously, which for this specific object is "always." Explicitly deferred to Sprint 15.6, which removes the wingman from Bob rendering entirely rather than trying to patch around the race.

Verified: clean rebuild, headless autoplay with `Wingman: CPU` and a temporary forced-fire-chance test hook confirmed end-to-end - the wingman triggers on enemy spawn, closes distance, tracks altitude, fires from a realistic stand-off range, kills the enemy, and smoothly returns to formation. All temporary diagnostic logging/flags fully reverted afterward.

## Sprint 15.6 - Hardware-Sprite Reallocation (Bob-to-Sprite Architecture Flip)

Direct follow-up to 15.5's deferred flutter finding. Since the flutter is an inherent single-buffer Bob-tearing risk rather than a logic bug, and the wingman is the object most continuously present on screen, the fix is architectural: stop rendering the wingman as a Bob at all.

**The blocker**: all 8 hardware sprite channels were already committed (player pair, rocket, enemy plane, enemy missile, bomb/impact, powerup, one spare). Freeing two channels for a wingman body pair meant moving something else off hardware sprites first. User's own proposal, confirmed and refined together: powerup pickups and the bomb/impact effect both already track the scrolling terrain the same way other Bobs do, so convert *those* to Bob rendering instead - freeing channels 2 (bomb/impact), 6 and 7 (powerup + the already-spare channel) for the wingman.

**Final channel layout**: ch0+1 player (attached pair, unchanged), ch2 wingman's own rocket (`buildRocketSprite()`, reused unchanged - the wingman's missile is the same plain tile-56 rocket the player fires), ch3 enemy plane, ch4 enemy missile, ch5 player's own rocket, ch6+7 wingman body (attached pair - needed because the wingman's real grey palette has 5 distinct tones, more than a single non-attached sprite's 3-colour limit, the same reason the player itself uses an attached pair).

**Bomb/impact -> Bob** (`updateBombImpactBob`/`buildBombImpactBobTileIfNeeded`/`drawBombImpactBobAt`/`eraseBombImpactBobFootprint`): masked tiles for the falling bomb (tiles 40/41) and hand-rolled small/large impact-star shapes, tile-row/column-locked footprint tracking. The bomb and impact `WeaponState`s are never simultaneously active (the hit handler clears one the same instant it starts the other), so they share one footprint slot exactly as they previously shared one hardware channel.

**Powerup -> Bob** (`updatePowerupBob`/`buildPowerupBobTileIfNeeded`/`erasePowerupBobFootprint`): 2-tile-wide masked parachute art, per-type colour (wingman/health/rockets/bombs) substituted into the mask the same way the old hardware-sprite version did via copper colour changes. A powerup's `worldX` is fixed for its whole lifetime (only `y` falls), so its erase/redraw is far rarer than the wingman's own Bob ever was - much lower tearing risk even in principle.

**Wingman body and rocket -> hardware sprites** (`updateWingmanSprite`/`updateWingmanRocketSprite`): the body reuses `buildAttachedSpriteFromCpcPlusHalves()`, the exact same conversion path the player's own sprite already uses, so the wingman's grey ramp gets the same attached-pair treatment instead of a bespoke Bob mask. The old Bob-specific machinery (`wingmanEraseBodyFootprint`, `wingmanDrawBodyAt`, `updateWingmanRocketBob`, `buildWingmanBobShiftedTiles`, the wingman rocket tile builder, and all their footprint-tracking fields on `WingmanState`) is deleted outright rather than left dead, since hardware sprites need none of it - position is "for free" via the sprite's own DMA-fetched position registers, with zero interaction with the scrolling world bitmap.

**Generalised naming**: the Bob compositor helpers built in 15.2/15.3 (`wingmanBobEraseFootprint`/`wingmanBobDrawColumnMasked`) are no longer wingman-specific now that bomb/impact and powerup are their only callers, so they're renamed `bobCompositorErase`/`bobCompositorDrawMasked` with doc comments updated accordingly. `copSetSprites()`/`buildGameHudCopper()` were changed from partial/implicit-null-fallback parameter lists to explicit, required parameters for all 8 channels - the old signature's "channel 7 defaults to null unless passed" made the previous powerup/bomb assignment easy to get subtly wrong and no longer matched a layout where every channel is meaningfully used.

**Crash-debris interaction preserved as-is**: `updateWeaponSprites()`'s crash-timer branch still temporarily borrows the rocket/wingman-rocket hardware buffers to show crash debris parts 1/2 - unchanged from before, except `updateWingmanRocketSprite()` now has an explicit `if (game->crashTimer) return;` guard so it doesn't fight the crash-debris code for that same buffer mid-crash.

**Verification**: clean rebuild after rewiring every `main()` call site that referenced the now-renamed/removed `bombSprite`/`powerupSprite`/`pendingPowerupSpriteUpdate` identifiers (a dangling call to the just-deleted `updateWingmanBob()` was also caught and replaced this pass). Two full headless autoplay runs (`Wingman: CPU` forced on, plus a temporary periodic bomb/rocket-fire input hook to exercise the new Bob paths) completed cleanly with **identical** frame-by-frame stats both times (deterministic sim, no timing regression) - fuel/armour/ammo tracked expected wear, `hudGuardHits`/`hudGuard2Hits`/`hudRegHits` (the HUD memory-corruption canaries from the Sprint 14.91.4 investigation) stayed at zero across every sample, and `maxVblDelta` stayed at a steady 3 vblanks post-startup (no stalls/hangs). Both runs exited cleanly back to the AmigaDOS shell rather than a Guru Meditation. Zoomed screenshots (10x nearest-neighbour crops, per this project's screenshot-analysis method) confirmed the player and wingman both render correctly as two distinct, slightly-offset grey jet silhouettes in tight 3-tile formation throughout the flight - at normal screenshot resolution the two overlap closely enough to first read as a single blob, which cost some extra verification time but is correct tight-formation flying, not a rendering bug. All temporary test flags (`HAR_HEADLESS_AUTOPLAY`, `HAR_DEBUG_PERF_LOG`, the forced `Wingman: CPU` default, the periodic fire-input hook) reverted before the final rebuild.

**Deliberately not covered by this pass** (unchanged from before, still backlog): wingman death/collision states, `EnemyTarget` friendly-fire selection, wingman bombing-run trigger (`checkwingmandobombingrun`), Player 2 human control (original roadmap 15.5/15.6 scope) - all remain future sprints.

## Sprint 15.7.4 - Wingman flying contrast adaptation

The palette pipeline, compiled palette bytes, Copper writes, attached-sprite
packing and CPC source pixels were re-audited after the flying Wingman looked
almost white while the landed Wingman on the carrier looked correct. All were
byte/pixel exact to the documented design. A runtime-faithful composed preview
then reproduced the same bird-like white result: this was not a missing half or
palette-register regression, but the exact six-step CPC grey ramp rendered as
sharp 16x8 Amiga pixels instead of WinAPE's softened presentation.

Only the flying Wingman's pen indices are therefore translated while building
sprite 6+7. Two brightness-compression tests still produced white/green pixels.
The actual cause was then isolated to OCS attached-sprite bit significance:
the even sprite supplies the upper colour-bit pair and the odd attached sprite
the lower pair, opposite to the logical ordering assumed by the local mapped
builder. The isolated Wingman table now swaps those bit pairs
(`1..6 -> 4,8,12,1,5,9`) so hardware selects the intended CPC grey palette
entries 1..6. Source assets and the shared `COLOR17-31` CPC+ palette remain
unchanged, so the player, enemy sprites, rockets and every other palette
consumer are unaffected.

The CPC graphics viewer was corrected at the same time: CPC Plus sprite cards
now use `sprite_colours` rather than the screen/tile palette, and composed
Flying Harrier/Wingman cards show the same 8+8-pixel assembly used at runtime.

## Sprint 15.8.0 - Incremental main menu

Main-menu navigation no longer redraws the complete 320x256 bitmap. Up/down
only erase and redraw the old and new menu rows. Enter/Fire and left/right now
share one setting-adjustment path that redraws only the selected row; changing
Lives additionally uses the existing HUD delta renderer to update only its
live value. Left decrements and right increments Skill and Wingman with
wraparound; Lives toggles between 1 and 3 in either direction. Start Game
remains an Enter/Fire action. Shift+D is excluded from right-navigation so the
telemetry shortcut cannot also change a menu value.

## Sprint 15.8.1 - Dedicated flying Wingman greys

The attached-pair remapping experiments were removed after WinUAE showed that
the flying Wingman still resolved predominantly to white. Wingman now uses
normal two-bit sprite channel 6 with three deliberate grey levels; channel 7
is hidden while its body is active. Gameplay Copper colours 29-31 are set to
`$333/$777/$bbb`. This range is safe because the attached player artwork uses
only CPC pens through 12 (COLOR28), while normal sprites 6/7 are the only
consumers of COLOR29-31. Player, weapons, playfield graphics and the carrier's
baked landed Wingman remain unchanged.

## Sprint 15.9.0 - CPC post-landing mission loop

Implemented the continuation after a successful carrier landing from CPC
`landinghoverloop` / `beginlandingapproach`:

- touchdown awards the CPC's literal `&00c8` (200) score;
- `LANDED` remains visible for a short hold;
- difficulty increases by one, capped at level 5;
- fuel, armour and skill-scaled rockets/bombs are replenished and flak damage
  is reset through the normal session initialisation;
- level progress, transient actors, destroyed targets and terrain generation
  reset for the next mission;
- accumulated score, hit count, remaining lives and Wingman selection survive;
- the next mission resumes on the carrier at CPC's `newlevelloop/checkliftoff`
  equivalent, ready for the player to press Up, without replaying the initial
  carrier-entry animation.

CPC waits for an enabled Wingman to finish landing before advancing. The Amiga
port does not yet implement Wingman landing waypoints, so player touchdown is
the completion gate until that dedicated parity work exists.
# Sprint 15.10.0

- Bombens flygende mini-BOB er nå en tydelig 4x4 figur i stedet for tre
  isolerte piksler.
- Bomben får en kort skrå fremdriftsfase og bindes deretter til
  verdenskoordinaten, slik at den følger scrollingen mens den faller.
- Maverick har fått en CPC-ekvivalent nærhetsdetonator ved målsenteret, slik
  at Amiga-portens firepikselsteg ikke kan oscillere rundt et bakkemål.
# Sprint 15.11.0

- Ammo-/powerup-drop tegnes nå på sin faktiske piksel-Y i stedet for
  `y / 8`; eksisterende 2/3-piksel-fall og pickup-regler er uendret.
- Piksel-BOB-en gjenoppretter de berørte bakgrunnsradene fra kartdata og
  tegnes bare på nytt når posisjonen faktisk endres.
- Neste vurderte spritefrigjøring er spillerens rakett: sammenlign en liten
  CPU-maskert piksel-BOB med dagens hardware-sprite. Bombens flygefase er
  allerede piksel-BOB. Impact/eksplosjon forblir tile-BOB inntil videre.
# Sprint 15.12.0

- Spillerens vanlige rakett og Maverick tegnes nå som en pikselpresis,
  CPU-maskert 8x8 BOB med CPC-retningsgrafikken.
- Hardware-sprite kanal 5 er skjult under normal spilling og dermed ledig
  for en senere spritefordeling; den lånes foreløpig fortsatt av én
  Harrier-del under krasjanimasjonen.
- Rakettens fysikk, målstyring, kollisjon og skade er ikke endret.
# Sprint 15.13.0

- Wingmans rakett bruker nå samme pikselpresise CPU-maskerte BOB-rutine som
  spillerens rakett.
- Spiller og Wingman har separate footprints per skjermbuffer, slik at
  erase aldri gjenoppretter bakgrunn lagret av den andre raketten.
- Hardware-sprite kanal 2 er nå også skjult og ledig under normal spilling;
  den lånes fortsatt av krasjanimasjonen.
# Sprint 15.14.0

- CPU-Wingmans normale formasjon følger nå CPC-ens toveis
  `wingmanbelowplayer`-regel.
- Utrygg øvre formasjonsplass bytter til under spilleren; utrygg nedre
  plass bytter tilbake over når øvre plass er fri.
- Gjeldende side beholdes mens den er trygg, og eksisterende gradvise
  radbevegelse brukes gjennom byttet uten teleportering.
- Intercept, bombing og retur-waypoints er fortsatt separate tilstander.
# Sprint 15.15.0

- Rettet CPC-tolkningen av skill-basert ammunisjon:
  `numberofbombs`/`numberofrockets` er skudd per HUD-enhet, ikke direkte
  beholdning.
- Skill 1 starter med 60 bomber og 30 raketter, tilsvarende 15 fulle
  gauge-enheter.
- HUD bruker skill-nivåets fulle beholdning som maksimum.
- Landing og ammo-drops fyller opp til samme skill-baserte kapasitet.
# Sprint 15.16.0

- CPU-Wingman bruker CPC-ens delte bakkemållås og vurderer hvert nytt mål
  én gang med `R & 3` (omtrent ett av fire mål).
- Ved valg forlater Wingman formasjonen, flyr fire tiles over målet,
  slipper sin egen bombe og returnerer gradvis til formasjonen.
- Wingman-bomben har separat `WeaponState` og separat piksel-BOB-footprint,
  bruker ikke spillerens bombelager og kan ikke overskrive spillerbombens
  bakgrunn.
- Treff ødelegger det låste bakkemålet via de eksisterende CPC-rutinene for
  smoke, score og dirty redraw.
# Sprint 15.16.1

- Wingman-radaren ser nå alle faktiske tile-kolonner i brede, prosedyregenererte
  bybygninger, ikke bare en eventuell ankerkolonne.
- Under bakkeangrep valideres neste horisontale steg før X flyttes.

# Sprint 15.17.0

- Wingmans CPC-logikk og terrengradar er fortsatt tilebasert.
- Hardware-spritens Y-posisjon følger den godkjente raden pikselpresist med
  2 piksler per frame, i stedet for synlige hopp på 8 piksler.
- Wingman-rakett og bombe opprettes fra den interpolerte, synlige posisjonen.

# Sprint 15.17.1

- Wingmans rakettavfyring sammenligner nå fiendens høyde med Wingmans faktiske
  interpolerte skjermposisjon, ikke den tilebaserte målraden foran spriten.

# Sprint 15.18.0

- CPU-Wingman avbryter kamp og starter landing når slutt-carrieren går over i
  hoverfasen.
- Wingman flyr pikselpresist til et samlepunkt over den ledige dekksenden og
  går deretter vertikalt ned på dekket.
- Venstre eller høyre dekksplass velges ut fra hvor spilleren befinner seg.
- Oppdraget fullføres først når både spilleren og en aktiv Wingman er landet,
  slik CPC-landingsløkken gjør.

# Sprint 15.19.0

- CPC `checkenemyhit` er kontrollert på nytt: bygningscellen som treffes blir
  røyk 52, med røyk 51 én rad over bare når raden er himmel. Krater 97 brukes
  kun ved direkte treff i land.
- Bygningskolonnen bygges nå komplett på nytt etter bombetreff, slik at den
  brede bygningskompositoren respekterer røyken.
- Bombens kollisjonspunkt følger den faktiske 6x3-grafikken og ligger ikke
  lenger flere piksler under bomben.
- Landing krever nå at Harrierens underside er ved dekknivå; den gamle vide
  toleransen som ga `LANDED` i luften er fjernet.
- Carrier-assets finnes både med og uten parkert Wingman. Når CPU-Wingman tar
  av, tegnes carrieren om uten den innbakte flygrafikken.
- Bombens aktive mini-BOB er endret fra rund 4x4 til en slank 6x3 CPC-lignende
  silhuett; fysikk og skade er uendret.

# Sprint 15.20.0

- Fiendeflyet bruker nå den ledige hardware-spritekanal 2 og en isolert
  rød/oransje/gul `COLOR17-19`-palett. Disse registrene brukes ikke av
  spillerens faktiske CPC-penner, så Harrier og Wingman påvirkes ikke.
- Menyens gamle negative X-forskyvning er fjernet og de lengste valgene er
  forkortet til `Wingman: P2` og `Maverick: L+Fire`; alle kolonner holder seg
  innenfor 320 piksler.
- CPC-tributescrollen er gjenskapt som en Amiga-ticker som flyttes én piksel
  hver andre frame. Teksten kan redigeres direkte i `menuTickerText` i
  `amiga/main.c`.

# Sprint 15.20.1

- Tickerteksten er hentet ordrett fra CPC-kilden:
  `Harrier Attack Reloaded - CPSoft 27.07.2025`.
- Menyblippet stjeler ikke lenger Paula-kanal 3 mens firekanals MOD-musikk
  spiller; menyen fortsetter visuelt uten et hørbart opphold i musikken.
- CPC-skipmissilets `HL=&0d24` er beholdt, men X beregnes fra skipets faktiske
  verdenskolonne og gjeldende piksel-scroll. Dermed starter missilet ved båten
  også under fine-scroll og ved høyere scrollhastighet.
- CPC-koden bekrefter at by–hav-overgangen med en 3x2 solid landblokk før
  `pendata`-piren er tilsiktet og beholdes uendret.
  Wingman holder posisjonen og klatrer til nærmeste sikre rad dersom ruten
  foran er blokkert.
- Toveis over/under-formasjon bruker dermed også korrekt bygningsgeometri.
