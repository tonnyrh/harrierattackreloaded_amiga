#include "support/gcc8_c_support.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <dos/dosextens.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <hardware/custom.h>
#include <hardware/cia.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>
#include <stddef.h>
#include <string.h>
#include "assets/harrier_menu_text.h"

#define HAR_BUILD_LABEL "SPRINT 15.38.0"

#define SCREEN_WIDTH 320
/* 256 lines (not 200) - PAL comfortably supports this (320x256 is a common,
 * unexotic Amiga resolution, well within diwstrt/diwstop's normal range, see
 * screenScan()) and it lets the HUD grow to a full CPC-style label-above-bar
 * gauge layout (HUD_HEIGHT below) without touching HUD_TOP/the gameplay
 * viewport at all - confirmed with the user this genuinely unused scanline
 * budget (not emulator window padding) was worth the modest extra chip-RAM
 * cost for larger HUD-only bitplane buffers. */
#define SCREEN_HEIGHT 256
/* Vertical DIWSTRT start (beam-line units). Was hardcoded to 44 in two
 * places (screenScan() and copWaitDisplayYAt()'s baseline) - correct for the
 * original 200-line display (44..244, comfortably within PAL safe area on
 * both ends) but never re-centered when SCREEN_HEIGHT grew to 256: simply
 * extending 56 more lines downward from the same y=44 start pushes the
 * bottom edge to line 300, which the user found is partly cut off / unsafe
 * on real hardware (ROCKETS/BOMBS - the very bottom of the HUD - reported as
 * "drawn too low", partly below the visible screen). Shifted up to bring the
 * bottom edge back to a safer margin; re-tune this if real-hardware testing
 * still shows clipping. */
#define SCREEN_DIWSTRT_Y 20
/* Standard PAL low-resolution display/fetch origin. Menu alignment belongs
 * in its drawing coordinates, not in DIW/DDF timing. */
#define SCREEN_DIWSTRT_X 129
#define SCREEN_PLANES 5
#define SCREEN_ROW_BYTES (SCREEN_WIDTH / 8)
#define SCREEN_BPL_MOD ((SCREEN_PLANES - 1) * SCREEN_ROW_BYTES)
#define SCREEN_BITMAP_BYTES (SCREEN_HEIGHT * SCREEN_PLANES * SCREEN_ROW_BYTES)
#define COPPER_BYTES 2048
#define HUD_TOP 168
#define HUD_HEIGHT (SCREEN_HEIGHT - HUD_TOP)
/* Max pen actually used by the HUD_COLOR_ and GAME_COLOR_ constants drawn
 * into the HUD is 9 (binary 1001), which fits in 4 bitplanes - see
 * buildGameHudCopper(). */
#define HUD_PLANES 4
/* Sprint 14.94 Part 5: same trick as HUD_PLANES, applied to the scrolling
 * world view itself. Every GAME_COLOR_* constant tops out at 15
 * (GAME_COLOR_SEA) and a byte-scan of every tile in the embedded
 * game_tiles.bpl confirmed the 5th bitplane is entirely zero across all 102
 * tiles - it's genuinely never used for anything the world displays. The
 * world buffer itself stays allocated/written at the full SCREEN_PLANES (5)
 * - see buildGameHudCopper() - only the unused 5th plane's data is never
 * fetched/displayed during the world's own scanlines, same as the HUD. */
#define GAME_WORLD_DISPLAY_PLANES 4

#define HAR_DEBUG_INPUT_OVERLAY 0
#define HAR_DEBUG_OBJECT_MAP_OVERLAY 0
#define HAR_DEBUG_FORCE_STAGE -1
#define HAR_DEBUG_REGISTER_RESOURCES 1
#define HAR_DEBUG_PERF_LOG 0
#define HAR_DEBUG_PERF_OVERLAY 0
#define HAR_DEBUG_LAND_LOG 0
#define HAR_HEADLESS_AUTOPLAY 0
#define HAR_USE_PROMOTED_CPC_PLUS_ASSETS 1
#define RING_WORLD_STREAM_MAX_AHEAD_TILES 64

#define SFX_CHANNEL_COUNT 4
#define SFX_MAX_TYPES 16

#define GAME_TILE_WIDTH 8
#define GAME_TILE_HEIGHT 8
#define GAME_TILE_PLANES SCREEN_PLANES
#define GAME_TILE_BYTES (GAME_TILE_HEIGHT * GAME_TILE_PLANES)
#define GAME_TILE_COUNT 102
#define GAME_MAP_WIDTH 40
#define GAME_MAP_HEIGHT 25
#define GAME_LEVEL_WIDTH_TILES 704
#define GAME_WORLD_WIDTH_TILES 128
#define GAME_WORLD_BUFFER_MARGIN_TILES 2
#define GAME_WORLD_BUFFER_MARGIN_PIXELS (GAME_WORLD_BUFFER_MARGIN_TILES * GAME_TILE_WIDTH)
#define GAME_WORLD_BUFFER_TILES (GAME_WORLD_WIDTH_TILES + GAME_WORLD_BUFFER_MARGIN_TILES)
#define GAME_WORLD_BUFFER_WIDTH (GAME_WORLD_BUFFER_TILES * GAME_TILE_WIDTH)
#define GAME_WORLD_HEIGHT HUD_TOP
#define GAME_WORLD_ROW_BYTES (GAME_WORLD_BUFFER_WIDTH / 8)
#define GAME_WORLD_BITMAP_BYTES (GAME_WORLD_HEIGHT * SCREEN_PLANES * GAME_WORLD_ROW_BYTES)
#define GAME_WORLD_BUFFER_COUNT 1
#define GAME_FETCH_WIDTH (SCREEN_WIDTH + 16)
#define GAME_FETCH_BYTES (GAME_FETCH_WIDTH / 8)
#define GAME_WORLD_MAX_BYTE_OFFSET (GAME_WORLD_BUFFER_TILES - GAME_FETCH_BYTES)
#define GAME_WORLD_SCROLL_PAGE_BYTES ((GAME_WORLD_MAX_BYTE_OFFSET - GAME_WORLD_BUFFER_MARGIN_TILES) & ~1)
#define GAME_WORLD_SCROLL_PAGE_TILES GAME_WORLD_SCROLL_PAGE_BYTES
#define WORLD_RENDER_TOWN_BLOCK_MAX_WIDTH 5
#define WORLD_RENDER_CARRIER_WIDTH_TILES 12
/* The promoted CPC+ gunship is two 16px ASIC sprites side by side:
 * 32 pixels = four Amiga 8px world columns. This must match
 * HAR_GUNSHIP_TILES_WIDE emitted by cpc_promoted_sprites_to_tiles.py. */
#define WORLD_RENDER_GUNSHIP_WIDTH_TILES 4
#define WORLD_RENDER_OBJECT_MIN_TILE_Y 7
#define WORLD_RENDER_OBJECT_MAX_TILE_Y 15
#define PERF_LOG_INTERVAL_FRAMES 500
#define TELEMETRY_SAMPLE_COUNT 64
#define TELEMETRY_INTERVAL_FRAMES 500
#define GAME_SCROLL_MAX_PIXELS ((GAME_LEVEL_WIDTH_TILES - GAME_MAP_WIDTH) * GAME_TILE_WIDTH)
#define GAME_SCROLL_SPEED_MIN_PIXELS 1
#define GAME_SCROLL_SPEED_MAX_PIXELS 4
#define GAME_SPEED_LEVEL_MIN 0
#define GAME_SPEED_LEVEL_MAX 15
#define GAME_SPEED_LEVEL_DEFAULT 1
#define GAME_THROTTLE_REPEAT_FRAMES 5
/* Final carrier starts at world column 667. Begin when its first pixel reaches
 * the 320px right edge, then keep the approach moving until it sits around
 * screen x=144. CPC can move its hardware-sprite carrier independently of
 * scenery; these two scroll positions reproduce that staging for Amiga's
 * world-anchored carrier. */
#define LANDING_APPROACH_SCROLL_X ((667 * GAME_TILE_WIDTH) - SCREEN_WIDTH)
#define LANDING_HOVER_SCROLL_X ((667 * GAME_TILE_WIDTH) - 144)
#define LANDING_SLOWDOWN_REPEAT_FRAMES 3
#define LANDING_STATE_NONE 0
#define LANDING_STATE_SLOWING 1
#define LANDING_STATE_HOVER 2
#define LANDING_COMPLETE_HOLD_FRAMES 100
#define LANDING_SCORE_VALUE 200
/* CPC scrollrightfortakeoffloop moves the landed Harrier nine character
 * positions back along the deck before checkliftoff.  The final native
 * carrier sits at screen X 144 in hover mode; use the same 17px deck offset
 * as the opening carrier's TAKEOFF_PLAYER_DECK_X (81 - 64). */
#define LANDING_RESTART_PLAYER_X 161
#define LANDING_RESTART_SLIDE_PIXELS 1
#define GAME_OBJECT_MAP_WIDTH_TILES GAME_WORLD_BUFFER_TILES
#define GAME_OBJECT_MAP_HEIGHT_TILES GAME_MAP_HEIGHT
#define GAME_OBJECT_MAP_CELL_COUNT (GAME_OBJECT_MAP_WIDTH_TILES * GAME_OBJECT_MAP_HEIGHT_TILES)
#define GAME_DESTROYED_TARGET_MAX 64
/* Sprint 14.95 Part 2: runtime-spawned flak (live at the screen's right
 * edge, see trySpawnFlak()) instead of a precomputed table. Bounded via
 * pruneRuntimeFlakBehindColumn() so a long flight doesn't grow this
 * unboundedly - at most ~GAME_MAP_WIDTH(40) columns are ever visible at
 * once, and observed spawn density is well under one flak per visible
 * column, so this is generously sized. */
#define GAME_RUNTIME_FLAK_MAX 64
#define GAME_DESTROYED_SHIP_CELL_MAX 32
#define GAME_ENEMY_SHIP_GROUP_COUNT 2
#define GAME_SHIP_WRECK_SMOKE_MAX 24
#define GAME_SHIP_WRECK_SMOKE_TILE_A 51
#define GAME_SHIP_WRECK_SMOKE_TILE_B 52
#define GAME_LAND_CRATER_MAX 96
#define GAME_LAND_CRATER_TILE 97
#define GAME_HORIZON_TILE_Y 14
#define GAME_SEA_TOP_TILE_Y 15
#define CPC_LAND_PROCEDURAL_LENGTH 295
#define CPC_LAND_PROCEDURAL_BASELINE 14
#define CPC_LAND_PROCEDURAL_FLOOR 11

#define PLAYER_SPRITE_WIDTH 16
#define PLAYER_SPRITE_HEIGHT 8
#define PLAYER_SPRITE_WORDS (2 + PLAYER_SPRITE_HEIGHT * 2 + 2)
#define PLAYER_START_X 128
/* Was 78 - user reported respawning too close to the ground/terrain after a
 * crash; also used for the very first spawn (same constant, both cases
 * benefit from more clearance). */
#define PLAYER_START_Y 56
#define TAKEOFF_SCROLL_START_PIXELS 96
#define TAKEOFF_SCROLL_STEP_PIXELS 2
#define TAKEOFF_PLAYER_DECK_X 81
#define TAKEOFF_PLAYER_DECK_Y 104
#define CARRIER_DECK_PIXEL_Y 113
#define CARRIER_DECK_PIXEL_HEIGHT 7
/* Was 104 - wider than the carrier composite actually renders
 * (WORLD_RENDER_CARRIER_WIDTH_TILES=12 tiles x GAME_TILE_WIDTH=8px=96px), so
 * the refuel/rearm trigger zone (playerOnNativeCarrierDeckPixels()) reached
 * ~8px past the visible ship's edge. Reconciled to match the actual
 * rendered width per Amiga-Improvement-Plan-23.04.2026.md Part 6. */
#define CARRIER_DECK_PIXEL_WIDTH 96
#define TELEMETRY_GAMEPLAY_SCROLL_MIN_PIXELS 96
#define TAKEOFF_STATE_ROLLING_IN 0
#define TAKEOFF_STATE_READY 1
#define TAKEOFF_STATE_LIFTING 2
#define TAKEOFF_STATE_AIRBORNE 3
#define TAKEOFF_CLEAR_Y 88
#define PLAYER_MIN_X 16
#define PLAYER_MAX_X (SCREEN_WIDTH - PLAYER_SPRITE_WIDTH - 16)
#define PLAYER_MIN_Y 8
#define PLAYER_MAX_Y 144
#define PLAYER_MOVE_SPEED_PIXELS 2
#define PLAYER_SPEED_ANCHOR_X 96
#define PLAYER_SPEED_ANCHOR_STEP_PIXELS 6
#define PLAYER_START_LIVES 3
#define PLAYER_RESPAWN_SAFE_FRAMES 90
#define PLAYER_CRASH_FRAMES 64
#define PLAYER_CRASH_PART_COUNT 3
#define PLAYER_OBJECT_COLLISION_SAFE 0

/* Sprint 15.3: CPC's wingman formation offset is "3 tiles left, 3 tiles
 * above or below" the player. WINGMAN_MOVE_FRAME_INTERVAL throttles the
 * wingman's one-tile (8px) row steps to roughly match the player's own
 * PLAYER_MOVE_SPEED_PIXELS(2)/frame - 8px every 4 frames is 100px/s either
 * way, so neither plane looks like it's cheating past the other vertically. */
#define WINGMAN_FORMATION_COLUMNS_BEHIND 3
#define WINGMAN_FORMATION_ROWS_OFFSET 3
#define WINGMAN_TAKEOFF_DECK_X 137
#define WINGMAN_TAKEOFF_DECK_Y TAKEOFF_PLAYER_DECK_Y
#define WINGMAN_TAKEOFF_MOVE_PIXELS 2
#define WINGMAN_MOVE_FRAME_INTERVAL 4
#define WINGMAN_VISUAL_MOVE_PIXELS 2
#define WINGMAN_MAX_ROW ((GAME_WORLD_HEIGHT / GAME_TILE_HEIGHT) - 1)

/* Sprint 15.5: interception AI constants, matching
 * HarrierAttackSourceNew2...asm's wingman intercept routines:
 * - WINGMAN_INTERCEPT_CHANCE_MASK: asm:6513-6516 rolls `r & 1` on every new
 *   enemy-plane spawn to decide whether to break formation and intercept.
 *   Real CPC also *forces* this roll when the enemy plane specifically
 *   targeted the wingman (asm:6504-6509, missiletargetwingman!=1) - not
 *   implemented here yet, since this port has no enemy-target-selection
 *   system (that's Sprint 15.6's EnemyTarget work); only the unconditional
 *   50/50 chance is applied for now.
 * - WINGMAN_INTERCEPT_LEAD_PIXELS/WINGMAN_INTERCEPT_FIRE_RANGE_PIXELS:
 *   asm:2481-2486 and :2470-2474 use 6px/10px respectively - deliberately
 *   NOT copied verbatim here. Both sprites in this port are
 *   ENEMY_SPRITE_WIDTH(16)px wide, so a 10px gap between their X origins
 *   means they're still visually overlapping (confirmed by testing: the
 *   wingman appeared to ram the enemy plane rather than shoot it from a
 *   stand-off distance). The ASM's own comment on the lead offset -
 *   "ONLY TRACK LOCATION IN FRONT OF PLANE SO WE DON'T COLLIDE WITH IT" -
 *   makes the *intent* explicit: fire from clear of the target, not
 *   point-blank. Scaled up here to actually deliver that intent at this
 *   port's sprite sizes rather than the literal CPC pixel count.
 * - WINGMAN_INTERCEPT_ROW_TOLERANCE: CPC compares wingman/enemy height as
 *   equal bytes (asm:2467-2469); this port's enemyPlane.y is pixel-precise
 *   while the wingman's own row is tile-quantised (8px), so an exact match
 *   would rarely happen - a tolerance window approximates "same altitude." */
#define WINGMAN_INTERCEPT_CHANCE_MASK 1
#define WINGMAN_INTERCEPT_LEAD_PIXELS 20
#define WINGMAN_INTERCEPT_FIRE_RANGE_PIXELS 32
#define WINGMAN_INTERCEPT_ROW_TOLERANCE 5
#define WINGMAN_INTERCEPT_MOVE_PIXELS 4
#define WINGMAN_BOMB_CHANCE_MASK 3
#define WINGMAN_BOMB_HEIGHT_TILES 4
#define WINGMAN_BOMB_LEAD_TILES 5
#define WINGMAN_LANDING_WAYPOINT_Y 72
#define WINGMAN_LANDING_MOVE_PIXELS 2
#define WINGMAN_LANDING_DESCEND_PIXELS 1
#define PLAYER_OBJECT_COLLISION_FATAL 1
#define PLAYER_OBJECT_COLLISION_FLAK 2
#define PLAYER_FRIGATE_STATUS_CLEAR 0
#define PLAYER_FRIGATE_STATUS_HIT 1
#define PLAYER_FRIGATE_STATUS_SERVICED 2

#define ENGINE_CHANNEL 3
#define ENGINE_BUFFER_BYTES 2048
#define ENGINE_MUTATE_BYTES 48

#define WEAPON_SPRITE_HEIGHT 8
#define AUXILIARY_SPRITE_WORDS (2 + WEAPON_SPRITE_HEIGHT * 2 + 2)
#define ROCKET_SPEED_PIXELS 7
#define MAVERICK_GUIDANCE_DELAY_PIXELS 64
#define MAVERICK_GUIDED_SPEED_PIXELS 4
#define ROCKET_SHOT_STANDARD 0
#define ROCKET_SHOT_MAVERICK_LAUNCH 1
#define ROCKET_SHOT_MAVERICK_GUIDED 2
#define MAVERICK_DIRECTION_NONE 0
#define MAVERICK_DIRECTION_UP 1
#define MAVERICK_DIRECTION_UP_RIGHT 2
#define MAVERICK_DIRECTION_RIGHT 3
#define MAVERICK_DIRECTION_DOWN_RIGHT 4
#define MAVERICK_DIRECTION_DOWN 5
#define MAVERICK_DIRECTION_DOWN_LEFT 6
#define MAVERICK_DIRECTION_LEFT 7
#define MAVERICK_DIRECTION_UP_LEFT 8
#define BOMB_SPEED_X_PIXELS 1
#define BOMB_SPEED_Y_PIXELS 1
/* Three vertical pixels per two frames (1.5px/frame): request to fall
 * "about 10% faster" than the previous four-pixels-per-three-frames pace
 * (1.333px/frame) - 2 is the closest interval that keeps this a plain
 * extra-pixel-every-Nth-frame integer step rather than a fractional-pixel
 * accumulator, landing at +12.5% rather than exactly +10%. */
#define BOMB_EXTRA_FALL_INTERVAL 2
/* CPC dolaunchbomb performs one immediate horizontal move, then consumes
 * bombmomentum=3 over three more horizontal updates. Only after that does
 * playerbombstatus change to the descending state. */
#define BOMB_FORWARD_MOMENTUM_FRAMES 4
#define BOMB_LAUNCH_COOLDOWN_FRAMES 18
#define BOMB_IMPACT_SFX_GRACE_FRAMES 8
#define IMPACT_FRAMES 12
#define SEA_SURFACE_Y 121

/* Sprint 14.96: descending powerups (CPC's wingmanpowerup system).
 * Real CPC spawns these from launchenemyplane at tile 38/h=0..3, lets them
 * scroll left with the world and fall one tile every ~6 tile-scrolls, and
 * awards health / rockets / bombs / wingman on contact. The Amiga port
 * renders this as a separate 16x16 hardware sprite on sprite channel 6
 * (the first free slot - 0/1=player attached, 2=bomb, 3=enemy plane,
 * 4=enemy missile, 5=rocket, 6=powerup, 7=unused) rather than baking it
 * into the scrolling world ring buffer, since it's a single short-lived
 * dynamic object that would otherwise force dirty redraws every frame.
 * Powerup weapon refills use CPC's literal &10 (16) independently of the
 * Amiga session's smaller starting inventory. */
#define POWERUP_SPRITE_WIDTH 16
#define POWERUP_SPRITE_HEIGHT 8
#define POWERUP_SPRITE_WORDS (2 + POWERUP_SPRITE_HEIGHT * 2 + 2)
#define POWERUP_COLLISION_WIDTH 8
/* Smoothed half-speed equivalent of one CPC 8px tile per 12 frames:
 * add 2/3 pixel each frame instead of jumping a complete tile. */
#define POWERUP_FALL_PHASE_ADD 2
#define POWERUP_FALL_PHASE_PIXEL 3
#define POWERUP_SPAWN_COLUMN 38
#define POWERUP_SPAWN_MAX_ROW 3
#define POWERUP_DESPAWN_LEFT_X (-16)
#define POWERUP_ALTITUDE_FLOOR_BASE 11
#define POWERUP_ROCKET_REFILL 16
#define POWERUP_BOMB_REFILL 16
#define POWERUP_SPAWN_ROLL_MASK 0x0f
#define POWERUP_PICKUP_SCORE_VALUE 0

#define ENEMY_SPRITE_WIDTH 16
#define ENEMY_SPRITE_HEIGHT 8
#define ENEMY_SPRITE_WORDS (2 + ENEMY_SPRITE_HEIGHT * 2 + 2)
#define ENEMY_SPEED_PIXELS 1
#define ENEMY_RESPAWN_FRAMES 72
/* Real CPC point values (explosionnoise() callers, HarrierAttackSourceNew2...
 * asm:1206-8239) - raw A register values there are in tens (convwordtostr
 * appends a literal "0" digit, :4359, "SCORE IS MULTIPLES OF TEN"), so the
 * displayed values below are already *10: enemy plane 75->750,
 * ground/destructible object 10->100 (350 in town, not modelled here),
 * enemy ship 50->500, enemy missile/flak 1->10. */
#define ENEMY_SCORE_VALUE 750
#define ENEMY_MISSILE_SPRITE_WIDTH 16
#define ENEMY_MISSILE_SPRITE_HEIGHT 8
#define ENEMY_MISSILE_SPRITE_WORDS (2 + ENEMY_MISSILE_SPRITE_HEIGHT * 2 + 2)
#define ENEMY_MISSILE_SPEED_X_PIXELS 2
#define ENEMY_TARGET_NONE 0
#define ENEMY_TARGET_PLAYER 1
#define ENEMY_TARGET_WINGMAN 2
#define ENEMY_MISSILE_FIRE_RANGE_PIXELS 80
#define ENEMY_MISSILE_FIRE_FALLBACK_FRAME 42
#define ENEMY_SHIP_MISSILE_START_X 288
#define ENEMY_SHIP_MISSILE_START_Y 104
#define ENEMY_MISSILE_SCORE_VALUE 10
#define GROUND_TARGET_SCORE_VALUE 100
#define ENEMY_SHIP_SCORE_VALUE 500
/* CPC awards 35 internal score units for a destroyed town building tile,
 * displayed as x10 = 350 (Amiga-Improvement-Plan-23.04.2026.md Part 7,
 * "They award 35 internal units, displayed as 350 points"). */
#define TOWN_BLOCK_SCORE_VALUE 350

#define HUD_BITMAP_BYTES (HUD_HEIGHT * SCREEN_PLANES * SCREEN_ROW_BYTES)
/* Was 2 (double-buffered, swapped via copper pointer patch on each hudDirty
 * update). The old text-only HUD's small/fast redraws apparently never
 * exposed it, but the larger gauge-bar redraws (Sprint 14.91.2) take long
 * enough that the buffer swap can occasionally race the display reading the
 * same buffer mid-draw, producing visible noise/tearing in a gauge - the
 * menu screen's single, non-swapped buffer never showed this even with the
 * same tick-mark drawing code, which is what pointed at the swap itself
 * rather than the drawing logic. Score no longer accruing from distance
 * (same sprint) means hudDirty now fires rarely, so single-buffering here
 * is very unlikely to show tearing either. */
#define HUD_BUFFER_COUNT 1
#define HUD_COLOR_BACKGROUND GAME_COLOR_SKY
#define HUD_COLOR_LABEL GAME_COLOR_WHITE
#define HUD_COLOR_VALUE GAME_COLOR_YELLOW
#define HUD_COLOR_SAFE GAME_COLOR_LAND
#define HUD_COLOR_WARN 9
#define HUD_COLOR_POWERUP_HEALTH GAME_COLOR_YELLOW
#define HUD_COLOR_POWERUP_ROCKETS GAME_COLOR_SEA
#define HUD_COLOR_POWERUP_BOMBS GAME_COLOR_LAND

#define FONT_WIDTH 8
#define FONT_HEIGHT 8
#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR 127
#define TELEMETRY_FONT_WIDTH 4
#define TELEMETRY_FONT_HEIGHT 5

#define MENU_ITEM_START 0
#define MENU_ITEM_SKILL 1
/* Was "Redefine keys" (a stub - "COMES IN SPRINT 3" notice, no real
 * functionality). Repurposed for the CPC-authenticity lives toggle since the
 * menu screen is already pixel-tight to its 200px height (gauges reach the
 * bottom edge). Redefine keys remains a real backlog item for whenever
 * there's a free slot or a redesigned menu layout. */
#define MENU_ITEM_LIVES 2
/* Sprint 15.1: real Off/CPU/PLAYER 2 wingman control setting, replacing the
 * old static "Wingman: Off" status line in drawMenuRightSettings() (which
 * never did anything). Uses the row the right-hand status column already
 * reserves at y=152 (see itemY[] and drawMenuRightSettings()'s own 4-row
 * layout below) - the left column just wasn't using it yet, so this doesn't
 * reflow anything. */
#define MENU_ITEM_WINGMAN 3
#define MENU_ITEM_COUNT 4
#define MENU_CONTENT_X_OFFSET 0
/* Menu review: "Input: Joystick/Keyboard" used to be a 4th selectable item
 * here, but ReadInput() always reads joystick/keyboard/mouse simultaneously
 * regardless of it - the toggle only ever changed its own displayed text,
 * never any real input behaviour. Removed as a selectable choice entirely
 * rather than wired up to actually gate input sources (per user direction) -
 * see the "Input: All" status line in drawMenuRightSettings() instead. */

#define MENU_COLOR_BACKGROUND 3
#define MENU_COLOR_PANEL 0
#define MENU_COLOR_WHITE 1
#define MENU_COLOR_RED 2
#define MENU_COLOR_GREEN 4
#define MENU_COLOR_YELLOW 5
#define MENU_COLOR_SHADOW 6
#define MENU_COLOR_ORANGE 10
#define MENU_COLOR_CYAN 11
#define MENU_TICKER_Y 8
#define MENU_TICKER_GAP_PIXELS 48
#define MENU_TICKER_FRAME_DIVIDER 1
#define MENU_TICKER_MARGIN_PIXELS 16
#define MENU_TICKER_SOURCE_WIDTH 2048
#define MENU_TICKER_ROW_BYTES (MENU_TICKER_SOURCE_WIDTH / 8)
#define MENU_TICKER_BITMAP_BYTES \
	(FONT_HEIGHT * SCREEN_PLANES * MENU_TICKER_ROW_BYTES)

/* Complete CPC tribute text extracted from AMSTRADFONT3.asm. Keep the alias
 * here so an Amiga-specific message can still be substituted in one place
 * without touching the smooth-scrolling implementation. The source bitmap
 * is wide enough for the 201-character cycle plus the visible wraparound. */
static const char menuTickerText[] = HAR_TEXT_TRIBUTE;
static UWORD menuTickerScrollX = 0;
static UBYTE menuTickerFrameDivider = 0;
static UBYTE* menuTickerBitmap = 0;

/* CPC writecharf() keeps pen 1 on rows 0-1, maps it to pen 0 on
 * rows 2-4 and to pen 3 on rows 5-7. These Amiga palette slots hold the
 * equivalent colours for each screen area. */
#define CPC_FONT_GREEN_TOP 1
#define CPC_FONT_GREEN_MIDDLE 5
#define CPC_FONT_GREEN_BOTTOM 4
#define CPC_FONT_BLUE_TOP 11
#define CPC_FONT_BLUE_MIDDLE 7
#define CPC_FONT_BLUE_BOTTOM 6
#define CPC_FONT_HUD_TOP 1
#define CPC_FONT_HUD_MIDDLE 12
#define CPC_FONT_HUD_BOTTOM 9

#define GAME_COLOR_SKY 0
#define GAME_COLOR_WHITE 1
#define GAME_COLOR_LIGHT_GREY 2
#define GAME_COLOR_MID_GREY 3
#define GAME_COLOR_DARK_GREY 4
#define GAME_COLOR_LAND 5
#define GAME_COLOR_YELLOW 6
#define GAME_COLOR_RED 9
#define GAME_COLOR_BLACK 10
#define GAME_COLOR_SEA 15
/* Real CPC Mode 1 per-band copper palette (re-derived after the game tile
 * assets were found to have been extracted as Mode 0 instead of Mode 1 and
 * re-extracted correctly) - COLOR00/15 change across the 4 screen bands:
 * upper sky, mid sky, lower play area, instrument panel. COLOR05 (land) and
 * COLOR10 (black) are NOT overridden per-band despite the CPC reference
 * table specifying panel-band values for them: both double as HUD gauge
 * semantics here (HUD_COLOR_BACKGROUND/HUD_COLOR_SAFE) that retinting them
 * away from their bulk-loaded game_palette.pal values would break - see the
 * comment at the panel-band writes in buildGameHudCopper(). COLOR15 doubles
 * as "clouds" (white) in the sky bands and "sea" (blue) once the lower play
 * area starts, then a third accent value for the panel band (not currently
 * visible in any HUD graphics, kept only for parity with the reference
 * table). */
#define GAME_SKY_TOP_RGB 0x058d
#define GAME_SKY_MID_RGB 0x069e
#define GAME_SKY_LOW_RGB 0x07af
#define GAME_SKY_TOP_CLOUD_RGB 0x0fff
#define GAME_SKY_LOW_SEA_RGB 0x0009
#define GAME_HUD_PANEL_SEA_RGB GAME_SKY_LOW_SEA_RGB
#define GAME_SKY_MID_Y 56
#define GAME_SKY_LOW_Y 112
/* Normal hardware sprites 6/7 use COLOR29-31. The attached player art only
 * reaches CPC pen 12 (COLOR28), so these three registers can safely give the
 * flying Wingman a stable, dedicated grey ramp. */
#define WINGMAN_SPRITE_DARK_RGB 0x0333
#define WINGMAN_SPRITE_MID_RGB 0x0777
#define WINGMAN_SPRITE_LIGHT_RGB 0x0bbb
/* Channel 2 is free in normal play after Wingman's rocket became a BOB.
 * COLOR17-19 are unused by the attached player art (its source pens begin
 * at 6 -> COLOR22), so the enemy plane gets a dedicated hostile ramp. */
#define ENEMY_PLANE_DARK_RGB 0x0500
#define ENEMY_PLANE_MID_RGB 0x0d30
#define ENEMY_PLANE_LIGHT_RGB 0x0fc0

/* Sprint 14.95 Part 5: town-entry palette fade (day -> dusk), per real CPC's
 * startpalettefade - the "red flash" the review originally flagged as a
 * possible rendering bug turned out to be this missing, intentional effect:
 * CPC steps through 5 palette transitions (~13 VBlanks/~0.26s each at PAL,
 * ~1.3s total) the instant the terrain flattens back to row 14 and the town
 * stage begins, ending on a stable dusk palette that holds for the whole
 * town section, then restores day colours on the way out. Scoped to COLOR00
 * (sky)/COLOR15 (clouds/sea/panel accent) only - the same two registers this
 * port's existing per-band gradient already dedicates to atmosphere; COLOR05/
 * COLOR10 are deliberately never touched here either, for the same HUD-gauge-
 * sharing reason the per-band gradient above already avoids them. The CPC
 * dusk palette value previously copied into the Amiga COLOR15 register as
 * 0x0ff0 made every sea pixel bright yellow. COLOR15 is our shared cloud/sea
 * pen, so the lower playfield and HUD variants must remain blue while only
 * the atmospheric bands fade. The sky's intermediate/target hues are this
 * port's own approximation of the
 * described purple -> red -> orange progression (CPC's own intermediate
 * palette-table entries weren't available to copy exactly) - worth a visual
 * tuning pass once seen in motion. */
#define CITY_FADE_STEP_COUNT 5
#define CITY_FADE_STEP_FRAMES 13
#define GAME_SKY_TOP_DUSK_RGB 0x0624
#define GAME_SKY_MID_DUSK_RGB 0x0836
#define GAME_SKY_LOW_DUSK_RGB 0x0a41
#define GAME_SKY_TOP_CLOUD_DUSK_RGB 0x0fa6
#define GAME_SKY_LOW_SEA_DUSK_RGB GAME_SKY_LOW_SEA_RGB
#define GAME_HUD_PANEL_SEA_DUSK_RGB GAME_HUD_PANEL_SEA_RGB

#define RAWKEY_E 0x12
#define RAWKEY_W 0x11
#define RAWKEY_A 0x20
#define RAWKEY_S 0x21
#define RAWKEY_D 0x22
#define RAWKEY_P 0x19
#define RAWKEY_R 0x13
#define RAWKEY_B 0x35
#define RAWKEY_SPACE 0x40
#define RAWKEY_RETURN 0x44
#define RAWKEY_ESCAPE 0x45
#define RAWKEY_UP 0x4c
#define RAWKEY_DOWN 0x4d
#define RAWKEY_RIGHT 0x4e
#define RAWKEY_LEFT 0x4f
#define RAWKEY_CONTROL 0x63
#define RAWKEY_LEFT_SHIFT 0x60
#define RAWKEY_RIGHT_SHIFT 0x61
#define RAWKEY_LEFT_ALT 0x64
#define RAWKEY_RIGHT_ALT 0x65

struct ExecBase *SysBase;
volatile struct Custom *custom;
volatile struct CIA *ciaa;
struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;

static UWORD SystemInts;
static UWORD SystemDMA;
static UWORD SystemADKCON;
static volatile APTR VBR = 0;
static APTR SystemIrq;
static struct View *ActiView;

volatile UWORD frameCounter = 0;
static UWORD* activeCopperBplcon1 = 0;
static UWORD* activeCopperPlaneHigh[SCREEN_PLANES];
static UWORD* activeCopperPlaneLow[SCREEN_PLANES];
static UWORD* activeCopperHudPlaneHigh[SCREEN_PLANES];
static UWORD* activeCopperHudPlaneLow[SCREEN_PLANES];
static UWORD* activeMenuTickerPlaneHigh[SCREEN_PLANES];
static UWORD* activeMenuTickerPlaneLow[SCREEN_PLANES];
static UWORD* activeMenuTickerBplcon1 = 0;

/* City fade (see the CITY_FADE_STEP_COUNT comment above) - pointers to the
 * specific copper color-value words written by copSetGameSkyGradient()/
 * buildGameHudCopper()'s panel-band write, captured at build time exactly
 * like activeCopperPlaneHigh/Low above, so applyCityFadeStep() can patch
 * just those words later without rebuilding the copper list (which, unlike
 * the scroll-pointer patching this mirrors, is only ever built once per
 * session, not every frame). current*Rgb hold what's currently baked into
 * those words - reset to the day values in resetCityFade() so a session
 * that starts partway through a previous session's dusk state doesn't bake
 * stale dusk colours into the fresh copper list. */
static UWORD* activeCopperSkyTopColor = 0;
static UWORD* activeCopperSkyMidColor = 0;
static UWORD* activeCopperSkyLowColor = 0;
static UWORD* activeCopperCloudTopColor = 0;
static UWORD* activeCopperSeaLowColor = 0;
static UWORD* activeCopperPanelSeaColor = 0;
static UWORD currentSkyTopRgb = GAME_SKY_TOP_RGB;
static UWORD currentSkyMidRgb = GAME_SKY_MID_RGB;
static UWORD currentSkyLowRgb = GAME_SKY_LOW_RGB;
static UWORD currentCloudTopRgb = GAME_SKY_TOP_CLOUD_RGB;
static UWORD currentSeaLowRgb = GAME_SKY_LOW_SEA_RGB;
static UWORD currentPanelSeaRgb = GAME_HUD_PANEL_SEA_RGB;

#define COPPER_TRACK_NONE 0
#define COPPER_TRACK_SCROLL 1
#define COPPER_TRACK_HUD 2

typedef struct InputState {
	UBYTE up;
	UBYTE down;
	UBYTE left;
	UBYTE right;
	UBYTE fire;
	UBYTE bomb;
	UBYTE eject;
	UBYTE shift;
	UBYTE control;
	UBYTE space;
	UBYTE d;
	UBYTE p;
	UBYTE r;
	UBYTE menuPrev;
	UBYTE menuNext;
	UBYTE select;
	UBYTE cancel;
	UBYTE any;
	UBYTE lastRawKey;
} InputState;

typedef struct TelemetrySample {
	UWORD frame;
	UWORD scrollX;
	UBYTE speedLevel;
	UWORD loops;
	UWORD minFps;
	UWORD maxFps;
	UWORD avgFps;
	UWORD hitches;
	UWORD maxVblDelta;
	UWORD maxVblScrollX;
	UWORD worldOrigin;
	UWORD bytesToPage;
	UWORD desiredOrigin;
	UWORD readyOrigin;
	UWORD nextOrigin;
	UWORD buffer0Origin;
	UWORD buffer1Origin;
	UWORD scrollMisses;
	UWORD scrollWaitFrames;
	UWORD maxScrollWait;
	UWORD currentScrollWait;
	UWORD renderBoostFrames;
	UWORD renderBoostSteps;
	UWORD renderTileX;
	UBYTE renderActive;
	UBYTE renderStage;
	UWORD tileColumns;
	UWORD objectColumns;
	UWORD pages;
	UBYTE event0Code;
	UBYTE event0Buffer;
	UWORD event0Origin;
	UWORD event0Scroll;
	UWORD event0X;
	UBYTE event1Code;
	UBYTE event1Buffer;
	UWORD event1Origin;
	UWORD event1Scroll;
	UWORD event1X;
	UWORD hitchScroll;
	UWORD hitchDelta;
	UWORD hitchRenderOrigin;
	UWORD hitchRenderX;
	UBYTE hitchRenderStage;
	UBYTE hitchRenderActive;
	UBYTE visibleLandMinY;
	UBYTE visibleLandMaxY;
	UBYTE bufferLandMinY;
	UBYTE bufferLandMaxY;
	UBYTE playerY;
	UBYTE playerMinY;
	UBYTE playerMaxY;
	UWORD fuel;
	UWORD armour;
	UBYTE rockets;
	UBYTE bombs;
} TelemetrySample;

typedef struct WeaponState {
	UBYTE active;
	UBYTE timer;
	UBYTE worldAnchored;
	UBYTE type;
	UBYTE direction;
	UWORD guidanceDistance;
	WORD x;
	WORD y;
	LONG worldX;
	LONG targetWorldX;
	WORD targetY;
	WORD dx;
	WORD dy;
} WeaponState;

/* Sprint 14.96: a single descending powerup. worldX is an absolute world
 * pixel coordinate (so it scrolls left naturally as game->scrollX grows),
 * y is a screen-space pixel coordinate (the powerup doesn't scroll
 * vertically with the world - only falls under its own slow gravity).
 * fallCounter is a 2/3-pixel fixed-point phase accumulator, giving the
 * requested half-speed version of CPC's tile descent without visible jumps.
 * spawnId tracks position in
 * the deterministic 1..6 type-rotation sequence (1=health, 2=rockets,
 * 3=bombs, 4=rockets, 5=bombs, 6=skip-and-spawn-enemy-plane). */
typedef struct {
	UBYTE active;
	UBYTE type;
	LONG worldX;
	WORD y;
	UBYTE fallCounter;
	UBYTE spawnId;
	UWORD lastSpawnCheckColumn;
} PowerupState;

/* Mirrors CPC's single enemylandlocationlock. The lock is not replaced while
 * a player rocket is in flight, so a launched Maverick cannot change target. */
typedef struct TargetLock {
	UBYTE active;
	LONG worldX;
	WORD y;
	UBYTE targetType;
} TargetLock;

/* Sprint 15.1: CPC's menu-time wingmanon setting (0=Off, 1=CPU, 2=PLAYER 2).
 * Menu-selectable and persisted into GameState only for now - no wingman
 * actually flies yet. Later Wingman sprints (WingmanState, Bob rendering,
 * CPU formation/AI, a second joystick port for PLAYER 2) read this to decide
 * whether/how a wingman subsystem runs. */
typedef enum WingmanControl {
	WINGMAN_CONTROL_OFF = 0,
	WINGMAN_CONTROL_CPU = 1,
	WINGMAN_CONTROL_PLAYER2 = 2
} WingmanControl;

/* Sprint 15.2/15.3: named form of CPC's raw wingmantakeoff state values (see
 * the Sprint 15 roadmap in AMIGA_PORT_PLAN.md for the full ASM-side mapping).
 * Raw CPC value 4 ("documented as landed, but the code normally resets it to
 * 0") is deliberately not represented - CPC itself never leaves it set. Only
 * WINGMAN_ON_DECK and WINGMAN_FORMATION are driven by real behaviour this
 * sprint; the rest are declared now so later sprints (obstacle avoidance,
 * interception, ground-attack, landing) don't need to renumber anything. */
typedef enum WingmanMode {
	WINGMAN_ON_DECK = 0,
	WINGMAN_FORMATION = 1,
	WINGMAN_LANDING_APPROACH = 2,
	WINGMAN_LANDING_DECK = 3,
	WINGMAN_INTERCEPT_APPROACH = 4,
	WINGMAN_INTERCEPT_TRACK = 5,
	WINGMAN_INTERCEPT_FIRE = 6,
	WINGMAN_WAYPOINT = 7,
	WINGMAN_WAYPOINT_REACHED = 8,
	WINGMAN_BOMB_APPROACH = 9,
	WINGMAN_BOMB_DROP = 10,
	WINGMAN_DESTROYED = 11,
	WINGMAN_WRECK = 12,
	WINGMAN_TAKEOFF = 13
} WingmanMode;

/* Sprint 15.3: the wingman's own flight state. Deliberately holds only what
 * CPU formation flight needs so far - weapons (own WeaponState rocket,
 * per the roadmap's "explicit struct instances, not a reused/re-pointed
 * shared block" decision) and AI/landing fields arrive with the sprints that
 * actually use them, not ahead of time.
 *
 * Movement is tile-grid-locked (row steps by one 8px GAME_TILE_HEIGHT at a
 * time) - matching CPC's own "Flyet beveger seg en CPC-tile per
 * oppdatering" (the wingman moves one CPC tile per update).
 *
 * The Wingman body uses hardware sprite channel 6, while its missile and
 * bomb use their own pixel-BOB footprints. */
typedef struct WingmanState {
	UBYTE active;               /* 1 once launched (CPU only so far), until destroyed/landed */
	UBYTE destroyed;            /* CPC wingmantakeoff 254: eligible for a later Wingman powerup */
	UBYTE mode;                 /* WingmanMode */
	UBYTE formationBelow;       /* 0 = trails above the player, 1 = below - CPC switches to
	                             * below whenever an above target would go off the top of
	                             * the screen (see updateWingmanFormationTarget()) */
	WORD row;                   /* current tile-row, 0..(GAME_WORLD_HEIGHT/GAME_TILE_HEIGHT)-1 */
	WORD screenY;               /* pixel-precise presentation position; follows row smoothly */
	UBYTE moveTimer;             /* throttles row movement to roughly the player's own vertical speed */

	/* Sprint 15.5: interception AI (real CPC wingmantrackenemyplanefirstpass/
	 * wingmantrackenemyplane2ndwaypoint, asm:2491-2512/2455-2487). Only used
	 * while mode==WINGMAN_INTERCEPT_APPROACH - screenOffsetX is the wingman's
	 * own screen-space X during the chase (formation mode never needs this,
	 * its X is always derived fresh from the player's position). CPC's two
	 * separate states (5=first-pass fixed waypoint, 6=live enemy tracking)
	 * are merged into one continuous "chase the enemy's lead point" state
	 * here - a deliberate simplification, not a fidelity gap in the actual
	 * chase/fire logic itself. */
	WORD interceptScreenX;
	WeaponState rocket;          /* wingman's own missile - real CPC always fires the
	                              * plain (non-Maverick) missile from interception,
	                              * infinite supply since he's a powerup */
	WeaponState bomb;
	LONG bombTargetWorldX;
	WORD bombTargetY;
	LONG lastBombTargetColumn;
	WORD landingTargetX;
	UBYTE landingTargetSet;

	/* Sprint 15.5 follow-up: when the chase ends (fired, or the enemy plane
	 * went away), mode doesn't snap straight back to WINGMAN_FORMATION -
	 * that would instantly swap the render's position source from the
	 * tracked interceptScreenX/row to the player-derived formation
	 * position, which are rarely the same place, reading as the wingman
	 * teleporting ("jumps back like a ghost"). Instead mode stays
	 * WINGMAN_INTERCEPT_APPROACH with this flag set, so updateWingmanIntercept()
	 * keeps stepping interceptScreenX/row - now toward the formation slot
	 * instead of the enemy - until they actually coincide, only then
	 * handing off to WINGMAN_FORMATION's normal derived positioning. */
	UBYTE returningToFormation;
} WingmanState;

typedef struct GameState {
	UWORD scrollX;
	WORD playerX;
	WORD playerY;
	UBYTE speedLevel;
	ULONG score;
	ULONG bonusScore;
	UWORD fuel;
	UWORD armour;
	UBYTE gameOver;
	UBYTE missionComplete;
	UWORD missionCompleteTimer;
	UBYTE postLandingSlide;
	UBYTE landingState;
	UBYTE takeoffState;
	UBYTE lives;
	UBYTE respawnSafeTimer;
	UBYTE flakDamageCount;
	UBYTE playerFrigateStatus;
	UBYTE rockets;
	UBYTE bombs;
	WeaponState rocketShot;
	WeaponState bombShot;
	WeaponState impact;
	WeaponState enemyPlane;
	WeaponState enemyMissile;
	WeaponState crashPart[PLAYER_CRASH_PART_COUNT];
	UBYTE enemyRespawnTimer;
	UBYTE enemySpawnIndex;
	UBYTE enemyTriggerIndex;
	UBYTE enemyShipMissileTriggerIndex;
	UBYTE enemyMissileFromShip;
	UBYTE enemyMissileTarget;   /* 1 = player, 2 = Wingman; CPC missiletargetwingman */
	UBYTE crashTimer;
	UBYTE ejectState;           /* 0=inactive, 1=ejector seat, 2=parachute */
	UBYTE ejectTimer;
	WORD ejectX;
	WORD ejectY;
	UBYTE throttleRepeatTimer;
	UBYTE bombLaunchCooldown;
	UBYTE skillLevel;
	UBYTE cityFadeStep;
	UBYTE cityFadeTimer;
	UWORD hitsCount;
	TargetLock targetLock;
	PowerupState powerup;
	UBYTE wingmanControl;
	WingmanState wingman;
} GameState;

typedef struct ObjectCell {
	UBYTE id;
	UBYTE tile;
	UBYTE flags;
	UBYTE hp;
} ObjectCell;

typedef struct LevelSegmentDef {
	WORD startColumn;
	WORD endColumn;
	UBYTE stage;
	UBYTE terrainKind;
} LevelSegmentDef;

typedef struct LevelObjectDef {
	WORD column;
	signed char row;
	UBYTE rowMode;
	UBYTE id;
	UBYTE tile;
	UBYTE flags;
	UBYTE hp;
} LevelObjectDef;

typedef struct SfxSample {
	const UBYTE* data;
	UWORD byteLength;
	UWORD period;
	UWORD volume;
	UBYTE priority;
	UBYTE pan;
	UBYTE frames;
} SfxSample;

#define SFX_PAN_ANY 0
#define SFX_PAN_LEFT 1
#define SFX_PAN_RIGHT 2
#define SFX_PRIORITY_AMBIENT 1
#define SFX_PRIORITY_WEAPON 2
#define SFX_PRIORITY_IMPACT 3
#define SFX_PRIORITY_PLAYER 4
#define SFX_POSITION_CENTER (SCREEN_WIDTH / 2)
#define SFX_SAMPLE_RATE 11025UL
#define SFX_PAULA_PERIOD 322
#define CARRIER_IDLE_MIN_DELAY_FRAMES 750
#define CARRIER_IDLE_DELAY_SPREAD_FRAMES 251
#define CARRIER_IDLE_FADE_FRAMES 25
#define CARRIER_IDLE_VOLUME 34
#define CARRIER_IDLE_DECODE_BUFFER_BYTES 44100
#define CARRIER_IDLE_PCM_EDGE_FADE_SAMPLES 256
/* One frame beyond the rounded-up natural duration gives Paula time to
 * reload the queued silent word before software disables the channel. */
#define SFX_FRAMES_FOR_BYTES(byteCount) \
	((UBYTE)((((ULONG)(byteCount) * 50UL) + SFX_SAMPLE_RATE - 1UL) / \
		SFX_SAMPLE_RATE + 1UL))

typedef struct EnemyShipGroupDef {
	UWORD startColumn;
	UWORD endColumn;
} EnemyShipGroupDef;

static UBYTE keyboardDown[128];
static UBYTE lastKeyboardRawKey = 0xff;
static UBYTE sfxChannelFrames[SFX_CHANNEL_COUNT];
static UBYTE sfxChannelStartDelay[SFX_CHANNEL_COUNT];
static UBYTE sfxChannelSilenceQueueDelay[SFX_CHANNEL_COUNT];
static UBYTE sfxChannelPendingId[SFX_CHANNEL_COUNT];
static UBYTE sfxChannelCurrentId[SFX_CHANNEL_COUNT];
static UBYTE sfxChannelPriority[SFX_CHANNEL_COUNT];
static UWORD sfxChannelPendingPeriod[SFX_CHANNEL_COUNT];
static UWORD sfxChannelPendingVolume[SFX_CHANNEL_COUNT];
static UBYTE sfxRetriggerGuard[SFX_MAX_TYPES];
static ULONG sfxChannelSequence[SFX_CHANNEL_COUNT];
static ULONG sfxVoiceSequence = 0;
static const SfxSample* sfxPendingSample[SFX_CHANNEL_COUNT];
static UWORD carrierIdleRandomState = 0x6d2b;
static UWORD carrierIdleDelayFrames = CARRIER_IDLE_MIN_DELAY_FRAMES;
static UBYTE carrierIdleLastVariant = 0xff;
static UBYTE carrierIdleChannel = 0xff;
static UBYTE carrierIdleAge = 0;
static UBYTE carrierIdleForcedFade = 0;
static UBYTE* carrierIdleDecodeBuffer = 0;
static SfxSample carrierIdlePlaybackSample;
static UBYTE modPlaying = 0;
static UWORD ringWorldLastStreamedColumn = 0;
static LONG ringStreamColumn = -1;
static UWORD ringStreamRow = 0;
static UWORD destroyedTargetColumns[GAME_DESTROYED_TARGET_MAX];
static UBYTE destroyedTargetCount = 0;
static UWORD runtimeFlakColumns[GAME_RUNTIME_FLAK_MAX];
static UBYTE runtimeFlakRows[GAME_RUNTIME_FLAK_MAX];
static UBYTE runtimeFlakTiles[GAME_RUNTIME_FLAK_MAX];
static UBYTE runtimeFlakCount = 0;
static UWORD destroyedShipCellColumns[GAME_DESTROYED_SHIP_CELL_MAX];
static UBYTE destroyedShipCellRows[GAME_DESTROYED_SHIP_CELL_MAX];
static UBYTE destroyedShipCellCount = 0;
static UWORD shipWreckSmokeColumns[GAME_SHIP_WRECK_SMOKE_MAX];
static UBYTE shipWreckSmokeRows[GAME_SHIP_WRECK_SMOKE_MAX];
static UBYTE shipWreckSmokeTiles[GAME_SHIP_WRECK_SMOKE_MAX];
static UBYTE shipWreckSmokeCount = 0;
static UWORD landCraterColumns[GAME_LAND_CRATER_MAX];
static UBYTE landCraterRows[GAME_LAND_CRATER_MAX];
static UBYTE landCraterCount = 0;
static UBYTE* engineBuffer = 0;
static UWORD engineLfsr = 0xace1;
static UWORD engineWriteOffset = 0;
static UBYTE engineActive = 0;
static UBYTE engineLastSpeed = 0xff;
static TelemetrySample* telemetrySamples = 0;
static UBYTE telemetryAvailable = 0;
static UBYTE telemetryEnabled = 0;
static UBYTE debugInfiniteLives = 0;
static UBYTE debugInfiniteBombs = 0;
static UBYTE debugInfiniteRockets = 0;
static UBYTE debugInfiniteFuel = 0;
static UBYTE telemetryIndex = 0;
static UBYTE telemetryCount = 0;
static UWORD telemetryIntervalStartFrame = 0;
static UWORD telemetryLastLoopFrame = 0;
static UWORD telemetryLoopFrames = 0;
static UWORD telemetryMinFps = 999;
static UWORD telemetryMaxFps = 0;
static UWORD telemetryHitches = 0;
static UWORD telemetryMaxVblDelta = 0;
static UWORD telemetryMaxVblScrollX = 0;
static UWORD telemetryWorldTileColumns = 0;
static UWORD telemetryWorldObjectColumns = 0;
static UWORD telemetryWorldPages = 0;
static UWORD telemetrySessionMinFps = 999;
static UWORD telemetrySessionMinFpsScrollX = 0;
static UWORD telemetrySessionMaxFps = 0;
static UWORD telemetrySessionMaxFpsScrollX = 0;
static UWORD telemetrySessionMaxVblDelta = 0;
static UWORD telemetrySessionMaxVblScrollX = 0;
static UBYTE telemetrySessionPlayerMinY = 255;
static UBYTE telemetrySessionPlayerMaxY = 0;
static UWORD telemetryScrollMisses = 0;
static UWORD telemetryScrollWaitFrames = 0;
static UWORD telemetryMaxScrollWait = 0;
static UWORD telemetryCurrentScrollWait = 0;
static UWORD telemetryLastDesiredOrigin = 0;
static UWORD telemetryLastReadyOrigin = 0xffff;
static UWORD telemetryRenderBoostFrames = 0;
static UWORD telemetryRenderBoostSteps = 0;
static UBYTE telemetryEvent0Code = 0;
static UBYTE telemetryEvent0Buffer = 0;
static UWORD telemetryEvent0Origin = 0;
static UWORD telemetryEvent0Scroll = 0;
static UWORD telemetryEvent0X = 0;
static UBYTE telemetryEvent1Code = 0;
static UBYTE telemetryEvent1Buffer = 0;
static UWORD telemetryEvent1Origin = 0;
static UWORD telemetryEvent1Scroll = 0;
static UWORD telemetryEvent1X = 0;
static UWORD telemetryLastHitchScroll = 0;
static UWORD telemetryLastHitchDelta = 0;
static UWORD telemetryLastHitchRenderOrigin = 0;
static UWORD telemetryLastHitchRenderX = 0;
static UBYTE telemetryLastHitchRenderStage = 0;
static UBYTE telemetryLastHitchRenderActive = 0;
/* HUD call/change counters - declared unconditionally (not just under
 * HAR_DEBUG_PERF_LOG) so drawHudValues() can increment them for free
 * without needing #if guards scattered through it; the perf log (below)
 * just reads and resets them once per 10s interval when enabled. Added to
 * investigate a user report that the HUD "updates more than needed" -
 * hudDrawCalls is how many times drawHudValues() actually ran in the
 * interval, and the per-field counters are how many of those calls found
 * that specific field changed (see the delta-draw branches). */
static UWORD hudDrawCalls = 0;
static UWORD hudArmourChanges = 0;
static UWORD hudFuelChanges = 0;
static UWORD hudScoreChanges = 0;
static UWORD hudSpeedChanges = 0;
static UWORD hudRocketsChanges = 0;
static UWORD hudBombsChanges = 0;
/* How many times the updateGameCollisions() path (replenishPlayerFromFrigate,
 * kill events, crash-start) set hudDirty this interval - to see how much of
 * the previously-observed gap between hudDrawCalls and the sum of the
 * per-field *Changes counters (during ordinary flight, no crash, no weapon
 * fire from the player) traces back here vs somewhere still unaccounted
 * for. */
static UWORD hudReplenishFires = 0;

/* Pointers into the copper PROGRAM's own memory (normal chip RAM, safely
 * readable - unlike custom->bplcon0/ddfstrt/etc, which are write-only
 * hardware registers that return bus noise on readback, not what was
 * written) at the exact HUD-section operand words, so a diagnostic can
 * verify buildGameHudCopper() actually wrote what it intended. */
static USHORT* hudCopBplcon0OperandPtr = 0;
static USHORT* hudCopDdfstrtOperandPtr = 0;
static USHORT* hudCopDdfstopOperandPtr = 0;

#if HAR_DEBUG_PERF_LOG
static UWORD perfWorldTileColumns = 0;
static UWORD perfWorldObjectColumns = 0;
static UWORD perfWorldPages = 0;
static UWORD perfIntervalStartFrame = 0;
static UWORD perfLastLoopFrame = 0;
static UWORD perfLoopFrames = 0;
static UWORD perfMinFps = 999;
static UWORD perfMaxFps = 0;
static UWORD perfHitches = 0;
static UWORD perfMaxVblDelta = 0;
#define PERF_LOG_BUFFER_BYTES 4096
static char perfLogBuffer[PERF_LOG_BUFFER_BYTES];
static UWORD perfLogBufferUsed = 0;

/* Sprint 14.103: dumps the generated land height/tile/transition table to
 * disk, for a "terrain looks too abrupt" report where the tile graphics and
 * their anchor checked out fine in isolation (see AMIGA_PORT_PLAN.md) but
 * the user wants to see the actual generated sequence from a real play
 * session, not just static analysis. Same RAM-buffer-then-flush-at-shutdown
 * pattern as perfLogBuffer above, for the same Forbid()/Disable() deadlock
 * reason - see the comment on perfLogAppend(). Unlike perf_log.csv (which
 * accumulates over the whole session), this only ever holds the CURRENT
 * table - landLogBuild() resets it and rebuilds fully each time, so a
 * mid-session restart (game over -> new game) doesn't append a second,
 * confusing copy alongside the first. */
#define LAND_LOG_BUFFER_BYTES 6144
static char landLogBuffer[LAND_LOG_BUFFER_BYTES];
static UWORD landLogBufferUsed = 0;

static void landLogAppend(const char* data, UWORD len) {
	if (landLogBufferUsed + len > LAND_LOG_BUFFER_BYTES)
		return;
	for (UWORD i = 0; i < len; i++)
		landLogBuffer[landLogBufferUsed++] = data[i];
}

static void landLogFlushToDisk(void) {
	BPTR file = Open((CONST_STRPTR)"DH1:land_log.csv", MODE_NEWFILE);
	if (!file)
		return;
	Write(file, (APTR)landLogBuffer, landLogBufferUsed);
	Close(file);
}

/* Memory-overlap canary: investigating a user report that HUD corruption
 * visually "follows terrain movement" - the leading theory being that the
 * scrolling world buffer's ring-stream renderer overflows past its own
 * allocation (worldBuffers[0]) into the very next chip-RAM allocation
 * (hudBuffer, allocated immediately after it in main()). Rows 0-3 of the HUD
 * buffer (the solid white top border drawn once by drawHudStatic(), plus
 * background-only rows before the first label at y=4) are never written
 * again after the initial full redraw at session start - so any deviation
 * from the snapshot taken right after that initial draw can only mean
 * something *outside* the normal HUD draw path touched this memory. */
#define HUD_GUARD_BYTES 800
static UBYTE hudGuardBaseline[HUD_GUARD_BYTES];
static UBYTE* hudGuardBufferPtr = 0;
static UBYTE hudGuardArmed = 0;
static UWORD hudGuardHitFrames = 0;
static UBYTE hudGuardDetailLogged = 0;
static ULONG hudGuardWorldToHudGapBytes = 0;

/* Copper-program verification: the two software canaries above proved the
 * HUD BUFFER's stored bytes are always correct, even while the ghost is
 * visible - so if the bug is register-level, it must be in what the copper
 * PROGRAM actually contains for the HUD section, not the buffer contents.
 * (An earlier version of this diagnostic tried reading custom->bplcon0 /
 * ->ddfstrt / ->bplpt[] directly - those are WRITE-ONLY hardware registers
 * on real OCS/ECS chipsets; reading them back returns bus noise, not what
 * was written, which is exactly why that attempt showed nonsense like
 * 0xFFFFFFFF. This version instead reads the operand words directly out of
 * the copper program's own (normal, readable) chip-RAM buffer via pointers
 * captured at build time in buildGameHudCopper().) */
static UWORD hudRegExpectedBplcon0 = 0;
static ULONG hudRegExpectedBpl5pt = 0;
static UWORD hudRegExpectedDdfstrt = 0;
static UWORD hudRegExpectedDdfstop = 0;
static UWORD hudRegMismatchFrames = 0;
static UBYTE hudRegDetailLogged = 0;
static UWORD hudRegLastBplcon0 = 0;
static ULONG hudRegLastBpl5pt = 0;
static UWORD hudRegLastDdfstrt = 0;
static UWORD hudRegLastDdfstop = 0;

/* Second guard: the very last HUD scanline (row HUD_HEIGHT-1 = 87), where the
 * user visually reports pixels moving at the bottom of the ROCKETS/BOMBS
 * gauges even with the game completely frozen (landed, no scroll, no value
 * changes). That row is the bottom BORDER pixel of both gauges - drawn once
 * by drawHudGaugeBar() and never touched again unless fillColor changes
 * (which it doesn't for rockets/bombs). If this byte range ever deviates
 * from its snapshot while nothing gameplay-relevant changed, the buffer
 * itself is being written by something outside the normal HUD draw path; if
 * it stays clean while the visual artifact is still reported, the bug is
 * purely a display/copper-timing artifact, not corrupted buffer content. */
#define HUD_GUARD2_BYTES (SCREEN_PLANES * SCREEN_ROW_BYTES)
static UBYTE hudGuard2Baseline[HUD_GUARD2_BYTES];
static UBYTE* hudGuard2BufferPtr = 0;
static UWORD hudGuard2HitFrames = 0;
static UBYTE hudGuard2DetailLogged = 0;
#endif

static const EnemyShipGroupDef enemyShipGroups[GAME_ENEMY_SHIP_GROUP_COUNT] = {
	{ 50, 53 },
	{ 629, 632 }
};

enum HarObjectId {
	HAR_OBJ_CLOUD = 0,
	HAR_OBJ_SKY = 1,
	HAR_OBJ_SEA = 2,
	HAR_OBJ_LAND = 3,
	HAR_OBJ_OWN_FRIGATE = 4,
	HAR_OBJ_PLAYER_WEAPON = 6,
	HAR_OBJ_ENEMY_SHIP = 7,
	HAR_OBJ_GROUND_TARGET = 8,
	HAR_OBJ_TOWN_BLOCK = 9,
	HAR_OBJ_FLAK = 10,
	/* Sprint 14.95 Part 2: real CPC ties smoke and flak to one shared object
	 * ID (10), which the review flags as worth splitting apart on the Amiga
	 * port so collision/weapon/sound code can't accidentally confuse a wreck
	 * smoke cell for a live flak hazard (or vice versa) - see
	 * AMIGA_PORT_PLAN.md. Ship-wreck/frigate/town-block hit smoke now reports
	 * this id; HAR_OBJ_FLAK is reserved for the real, player-indestructible
	 * ground-fire hazard. */
	HAR_OBJ_SMOKE = 11,
	HAR_OBJ_PLAYER_PLANE = 12,
	HAR_OBJ_ENEMY_PLANE = 13,
	HAR_OBJ_GUNSHIP = 14,
	HAR_OBJ_WINGMAN = 20,
	HAR_OBJ_POWERUP = 21
};

enum HarLevelStage {
	HAR_STAGE_START = 0,
	HAR_STAGE_START_ENEMY_SHIP = 1,
	HAR_STAGE_ENEMY_SHIP_FIRED_MISSILE = 2,
	HAR_STAGE_DO_LAND = 3,
	HAR_STAGE_DISPLAY_ENEMY = 4,
	HAR_STAGE_DESCEND_MOUNTAINS = 5,
	HAR_STAGE_FLAT_TOWNLAND = 6,
	HAR_STAGE_GENERATE_BUILDING = 7,
	HAR_STAGE_START_PIER = 8,
	HAR_STAGE_END_PIER = 9,
	HAR_STAGE_SECOND_SHIP_MISSILE = 10,
	HAR_STAGE_START_FRIGATE = 11,
	HAR_STAGE_END_FRIGATE = 12,
	HAR_STAGE_LANDING_ON_FRIGATE = 13,
	HAR_STAGE_OPEN_SEA = 14
};

enum HarObjectFlags {
	HAR_OBJECT_FLAG_NATIVE_CARRIER = 1,
	HAR_OBJECT_FLAG_NATIVE_DECK = 2,
	HAR_OBJECT_FLAG_CPC_GUNSHIP = 4,
	HAR_OBJECT_FLAG_CPC_TOWN_BLOCK = 8,
	/* CPC's endfrigatesprite is explicitly commented "FRIGATE REVERSED, SO IT
	 * CAN COME IN SCREEN FROM OPPOSITE SIDE" - the landing/end carrier faces
	 * the opposite way from the start carrier. Combined with (not instead
	 * of) HAR_OBJECT_FLAG_NATIVE_CARRIER on the end carrier's harLevelObjects
	 * entry so every existing NATIVE_CARRIER match still finds it. */
	HAR_OBJECT_FLAG_NATIVE_CARRIER_REVERSED = 16
};

enum HarTerrainKind {
	HAR_TERRAIN_SEA = 0,
	HAR_TERRAIN_COAST_RISE = 1,
	HAR_TERRAIN_MOUNTAINS = 2,
	HAR_TERRAIN_TOWN = 3,
	HAR_TERRAIN_COAST_FALL = 4,
	HAR_TERRAIN_CPC_RANDOM_LAND = 5,
	HAR_TERRAIN_CPC_DESCEND_TO_TOWN = 6
};

enum HarObjectRowMode {
	HAR_ROW_ABSOLUTE = 0,
	HAR_ROW_TERRAIN_RELATIVE = 1
};

/* Sprint 14.96: CPC's wingmanpowerupstatus values (0=none, 1=wingman,
 * 2=health, 3=rockets, 4=bombs). Sprint 15.26: trySpawnPowerup() now forces
 * this type whenever game->wingman.destroyed is set, and activatePowerup()
 * revives the wingman on pickup instead of falling back to health. */
enum PowerupType {
	POWERUP_NONE = 0,
	POWERUP_WINGMAN = 1,
	POWERUP_HEALTH = 2,
	POWERUP_ROCKETS = 3,
	POWERUP_BOMBS = 4
};

#include "assets/level_route.h"
#include "assets/cpc_promoted_assets.h"
#include "assets/cpc_promoted_sprite_tiles.h"
#if WORLD_RENDER_GUNSHIP_WIDTH_TILES != HAR_GUNSHIP_TILES_WIDE
#error "Runtime gunship width must match the promoted CPC+ composite"
#endif

#define HAR_LEVEL_OBJECT_COUNT (sizeof(harLevelObjects) / sizeof(harLevelObjects[0]))
#define HAR_LEVEL_OBJECT_COLUMN_INDEX_NONE 0xff

/* Sprint 14.94 Part 3: harLevelObjects (95 entries) was linearly scanned in
 * full by objectCellForWorldColumnTile() (once per row) and again by
 * drawDirectColumnRangeObjects() (once per column, for the promoted-asset
 * overlay) - for the vast majority of columns, which have zero or one
 * matching entry, that's 95 comparisons to find nothing or one thing. Build
 * two small indexes once, lazily, instead:
 *  - harLevelObjectColumnHead[]/harLevelObjectNext[]: a per-column linked
 *    list for exact-column lookups (flak/targets/most entries) - O(1) to
 *    find the first (if any) match for a column, O(matches) to walk them.
 *  - harWideObjectIndex[]: a short list of just the few entries that span
 *    MULTIPLE columns (native-carrier frigate, gunship, town blocks) since
 *    those need a range check, not an exact-column one, and can't use the
 *    per-column index above.
 * harLevelObjects itself and its column values are untouched - these are
 * pure lookup accelerators built from it, not a replacement for it. The data
 * isn't guaranteed sorted by column (e.g. the gunship entry at column 621
 * appears in the array after pier entries up to column 628), so this can't
 * just binary-search the existing array - hence the small index instead. */
static UBYTE harLevelObjectColumnHead[GAME_LEVEL_WIDTH_TILES + 16];
static UBYTE harLevelObjectNext[HAR_LEVEL_OBJECT_COUNT];
static UBYTE harWideObjectIndex[HAR_LEVEL_OBJECT_COUNT];
static UBYTE harWideObjectCount;
static UBYTE harLevelObjectIndexReady = 0;

/* Sprint 14.95 Part 5: town blocks are no longer harLevelObjects entries at
 * all (see generateCpcTownBlockTable()), so this only ever matches the
 * carrier/gunship now. */
static UBYTE isWideLevelObject(const LevelObjectDef* object) {
	if (object->id == HAR_OBJ_OWN_FRIGATE && (object->flags & HAR_OBJECT_FLAG_NATIVE_CARRIER))
		return 1;
	if (object->id == HAR_OBJ_GUNSHIP && (object->flags & HAR_OBJECT_FLAG_CPC_GUNSHIP))
		return 1;
	return 0;
}

static void buildHarLevelObjectIndex(void) {
	memset(harLevelObjectColumnHead, HAR_LEVEL_OBJECT_COLUMN_INDEX_NONE, sizeof(harLevelObjectColumnHead));
	harWideObjectCount = 0;
	for (UBYTE index = 0; index < HAR_LEVEL_OBJECT_COUNT; index++) {
		const LevelObjectDef* object = &harLevelObjects[index];
		if (object->column >= 0 && object->column < (WORD)sizeof(harLevelObjectColumnHead)) {
			/* Prepend - reverses relative order among entries that share the
			 * exact same column, which only matters if two of them also
			 * resolve to the exact same row (a data-authoring collision that
			 * would already be order-dependent/underspecified in the
			 * original per-row scan too). */
			harLevelObjectNext[index] = harLevelObjectColumnHead[object->column];
			harLevelObjectColumnHead[object->column] = index;
		}
		if (isWideLevelObject(object) && harWideObjectCount < HAR_LEVEL_OBJECT_COUNT)
			harWideObjectIndex[harWideObjectCount++] = index;
	}
	harLevelObjectIndexReady = 1;
}

static UBYTE harLevelObjectFirstIndexForColumn(LONG worldColumn) {
	if (!harLevelObjectIndexReady)
		buildHarLevelObjectIndex();
	if (worldColumn < 0 || worldColumn >= (LONG)sizeof(harLevelObjectColumnHead))
		return HAR_LEVEL_OBJECT_COLUMN_INDEX_NONE;
	return harLevelObjectColumnHead[worldColumn];
}

static UBYTE objectCellForWorldColumnTile(LONG worldColumn, WORD tileY, ObjectCell* outCell);
static UBYTE shipWreckSmokeTileAtColumnRow(LONG worldColumn, WORD tileY);
static UBYTE runtimeFlakTileAtColumnRow(LONG worldColumn, WORD tileY);
static UBYTE cpcRStateForWorldColumn(LONG worldColumn);
static void resetDestroyedTargets(void);
static void resetRuntimeFlak(void);
static void resetTargetLock(void);
static void resetCpcRandomSequence(void);
static void ammoForSkill(UBYTE skillLevel, UBYTE* bombs, UBYTE* rockets);
static void resetDestroyedShipColumns(void);
static void resetLandCraters(void);
static void resetCityFade(GameState* game);
static void resetPowerup(GameState* game);
static const LevelSegmentDef* levelSegmentForWorldColumn(LONG worldColumn);
static UBYTE stageForWorldColumn(LONG worldColumn, const LevelSegmentDef* segment);
static void dirtyRedrawWorldColumn(UBYTE** worldBuffers, LONG worldColumn);
static LONG scrollPointerPixelX(UWORD scrollX);
static void serviceRingWorldStream(UBYTE* bitmap, const GameState* game);
static UBYTE useFixedTakeoffWorldWindow(const GameState* game);
static void startPlayerCrash(GameState* game, WORD x, WORD y);
static void modRestoreChannelAfterSfx(UBYTE channel);

#ifdef __INTELLISENSE__
EMBED_CHIP loadingScreen[] = { 0 };
EMBED loadingPalette[] = { 0, 0 };
EMBED cpcFont8x8[] = { 0 };
EMBED gameTiles[] = { 0 };
EMBED gameSceneMap[] = { 0 };
EMBED gamePalette[] = { 0, 0 };
#else
EMBED_CHIP loadingScreen[] = {
	#embed "assets/loading_screen.bpl"
};
EMBED loadingPalette[] = {
	#embed "assets/loading_screen.pal"
};
EMBED cpcFont8x8[] = {
	#embed "assets/cpc_font8x8.bin"
};
EMBED gameTiles[] = {
	#embed "assets/game_tiles.bpl"
};
EMBED gameSceneMap[] = {
	#embed "assets/game_scene.map"
};
EMBED gamePalette[] = {
	#embed "assets/game_palette.pal"
};
#endif

#ifdef __INTELLISENSE__
EMBED_CHIP sfxFireSample[] = { 0, 0 };
EMBED_CHIP sfxBombSample[] = { 0, 0 };
EMBED_CHIP sfxImpactSample[] = { 0, 0 };
EMBED_CHIP sfxHitSample[] = { 0, 0 };
EMBED_CHIP sfxEjectSample[] = { 0, 0 };
EMBED_CHIP sfxPickupPowerupSample[] = { 0, 0 };
EMBED_CHIP sfxFlakGun1Sample[] = { 0, 0 };
EMBED_CHIP sfxFlakGun2Sample[] = { 0, 0 };
EMBED_CHIP sfxGroundTargetHit1Sample[] = { 0, 0 };
EMBED_CHIP sfxGroundTargetHit2Sample[] = { 0, 0 };
EMBED_CHIP sfxGroundTargetHit3Sample[] = { 0, 0 };
EMBED_CHIP sfxGroundTargetHit4Sample[] = { 0, 0 };
EMBED sfxCarrierIdle1Sample[] = { 0, 0, 0, 2, 0 };
EMBED sfxCarrierIdle2Sample[] = { 0, 0, 0, 2, 0 };
EMBED_CHIP menuMusicMod[] = { 0, 0 };
EMBED_CHIP gameOverMusicMod[] = { 0, 0 };
#else
EMBED_CHIP sfxFireSample[] = {
	#embed "assets/sfx/fire.raw"
};
EMBED_CHIP sfxBombSample[] = {
	#embed "assets/sfx/bomb.raw"
};
EMBED_CHIP sfxImpactSample[] = {
	#embed "assets/sfx/impact.raw"
};
EMBED_CHIP sfxHitSample[] = {
	#embed "assets/sfx/hit.raw"
};
EMBED_CHIP sfxEjectSample[] = {
	#embed "assets/sfx/eject.raw"
};
EMBED_CHIP sfxPickupPowerupSample[] = {
	#embed "assets/sfx/pickup_powerup.raw"
};
EMBED_CHIP sfxFlakGun1Sample[] = {
	#embed "assets/sfx/flak_gun_1.raw"
};
EMBED_CHIP sfxFlakGun2Sample[] = {
	#embed "assets/sfx/flak_gun_2.raw"
};
EMBED_CHIP sfxGroundTargetHit1Sample[] = {
	#embed "assets/sfx/ground_target_hit_1.raw"
};
EMBED_CHIP sfxGroundTargetHit2Sample[] = {
	#embed "assets/sfx/ground_target_hit_2.raw"
};
EMBED_CHIP sfxGroundTargetHit3Sample[] = {
	#embed "assets/sfx/ground_target_hit_3.raw"
};
EMBED_CHIP sfxGroundTargetHit4Sample[] = {
	#embed "assets/sfx/ground_target_hit_4.raw"
};
EMBED sfxCarrierIdle1Sample[] = {
	#embed "assets/sfx/carrier_idle_1.adpcm"
};
EMBED sfxCarrierIdle2Sample[] = {
	#embed "assets/sfx/carrier_idle_2.adpcm"
};
/* Standard 4-channel/31-instrument ProTracker "M.K." MOD - see
 * assets/music/README.md for provenance (Thaxted/"I Vow to Thee, My
 * Country", the real CPC menu tune, re-arranged to 4 independent voices). */
EMBED_CHIP menuMusicMod[] = {
	#embed "assets/music/harrier_menu_fixed.mod"
};
EMBED_CHIP gameOverMusicMod[] = {
	#embed "assets/music/raf_game_over.mod"
};
EMBED_CHIP carrierLandingMusicMod[] = {
	#embed "assets/music/carrier_landing_fanfare.mod"
};
#endif

/* Permanently-silent 1-word loop buffer for one-shot SFX playback - see
 * startPendingSfxChannel()'s use of it below. */
EMBED_CHIP sfxSilenceLoop[] = { 0, 0 };

enum {
	SFX_FIRE = 0,
	SFX_BOMB,
	SFX_IMPACT,
	SFX_HIT,
	SFX_EJECT,
	SFX_PICKUP_POWERUP,
	SFX_FLAK_GUN_1,
	SFX_FLAK_GUN_2,
	SFX_GROUND_TARGET_HIT_1,
	SFX_GROUND_TARGET_HIT_2,
	SFX_GROUND_TARGET_HIT_3,
	SFX_GROUND_TARGET_HIT_4,
	SFX_CARRIER_IDLE_1,
	SFX_CARRIER_IDLE_2,
	SFX_COUNT
};

static const SfxSample sfxSamples[SFX_COUNT] = {
	/* ~700 ms: short ignition pop followed by the rocket exhaust hiss. */
	[SFX_FIRE] = { sfxFireSample, sizeof(sfxFireSample), SFX_PAULA_PERIOD, 48,
		SFX_PRIORITY_WEAPON, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxFireSample)) },
	[SFX_BOMB] = { sfxBombSample, sizeof(sfxBombSample), SFX_PAULA_PERIOD, 44,
		SFX_PRIORITY_WEAPON, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxBombSample)) },
	[SFX_IMPACT] = { sfxImpactSample, sizeof(sfxImpactSample), SFX_PAULA_PERIOD, 58,
		SFX_PRIORITY_IMPACT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxImpactSample)) },
	[SFX_HIT] = { sfxHitSample, sizeof(sfxHitSample), SFX_PAULA_PERIOD, 50,
		SFX_PRIORITY_PLAYER, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxHitSample)) },
	[SFX_EJECT] = { sfxEjectSample, sizeof(sfxEjectSample), SFX_PAULA_PERIOD, 54,
		SFX_PRIORITY_PLAYER, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxEjectSample)) },
	[SFX_PICKUP_POWERUP] = { sfxPickupPowerupSample,
		sizeof(sfxPickupPowerupSample), SFX_PAULA_PERIOD, 52,
		SFX_PRIORITY_PLAYER, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxPickupPowerupSample)) },
	[SFX_FLAK_GUN_1] = { sfxFlakGun1Sample, sizeof(sfxFlakGun1Sample), SFX_PAULA_PERIOD, 56,
		SFX_PRIORITY_AMBIENT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxFlakGun1Sample)) },
	[SFX_FLAK_GUN_2] = { sfxFlakGun2Sample, sizeof(sfxFlakGun2Sample), SFX_PAULA_PERIOD, 56,
		SFX_PRIORITY_AMBIENT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxFlakGun2Sample)) },
	[SFX_GROUND_TARGET_HIT_1] = { sfxGroundTargetHit1Sample, sizeof(sfxGroundTargetHit1Sample), SFX_PAULA_PERIOD, 58,
		SFX_PRIORITY_IMPACT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxGroundTargetHit1Sample)) },
	[SFX_GROUND_TARGET_HIT_2] = { sfxGroundTargetHit2Sample, sizeof(sfxGroundTargetHit2Sample), SFX_PAULA_PERIOD, 58,
		SFX_PRIORITY_IMPACT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxGroundTargetHit2Sample)) },
	[SFX_GROUND_TARGET_HIT_3] = { sfxGroundTargetHit3Sample, sizeof(sfxGroundTargetHit3Sample), SFX_PAULA_PERIOD, 58,
		SFX_PRIORITY_IMPACT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxGroundTargetHit3Sample)) },
	[SFX_GROUND_TARGET_HIT_4] = { sfxGroundTargetHit4Sample, sizeof(sfxGroundTargetHit4Sample), SFX_PAULA_PERIOD, 58,
		SFX_PRIORITY_IMPACT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxGroundTargetHit4Sample)) },
	[SFX_CARRIER_IDLE_1] = { sfxCarrierIdle1Sample, sizeof(sfxCarrierIdle1Sample), SFX_PAULA_PERIOD, 34,
		SFX_PRIORITY_AMBIENT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxCarrierIdle1Sample)) },
	[SFX_CARRIER_IDLE_2] = { sfxCarrierIdle2Sample, sizeof(sfxCarrierIdle2Sample), SFX_PAULA_PERIOD, 34,
		SFX_PRIORITY_AMBIENT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxCarrierIdle2Sample)) },
};

static const UWORD menuPalette[32] = {
	0x000, 0xffa, 0xf22, 0x026, 0x0f0, 0xaf0, 0x05f, 0x0af,
	0x444, 0xf00, 0xf80, 0x0ff, 0xfa0, 0x0a0, 0xa00, 0x00a,
	0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000
};

static const UBYTE menuFont8x8[64][FONT_HEIGHT] = {
	['!' - 32] = { 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00 },
	['*' - 32] = { 0x00, 0x24, 0x18, 0x7e, 0x18, 0x24, 0x00, 0x00 },
	['+' - 32] = { 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00 },
	[',' - 32] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30 },
	['-' - 32] = { 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00 },
	['.' - 32] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00 },
	['/' - 32] = { 0x02, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x40, 0x00 },
	['0' - 32] = { 0x3c, 0x66, 0x6e, 0x76, 0x66, 0x66, 0x3c, 0x00 },
	['1' - 32] = { 0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00 },
	['2' - 32] = { 0x3c, 0x66, 0x06, 0x1c, 0x30, 0x60, 0x7e, 0x00 },
	['3' - 32] = { 0x3c, 0x66, 0x06, 0x1c, 0x06, 0x66, 0x3c, 0x00 },
	['4' - 32] = { 0x0c, 0x1c, 0x3c, 0x6c, 0x7e, 0x0c, 0x0c, 0x00 },
	['5' - 32] = { 0x7e, 0x60, 0x7c, 0x06, 0x06, 0x66, 0x3c, 0x00 },
	['6' - 32] = { 0x1c, 0x30, 0x60, 0x7c, 0x66, 0x66, 0x3c, 0x00 },
	['7' - 32] = { 0x7e, 0x66, 0x0c, 0x18, 0x18, 0x18, 0x18, 0x00 },
	['8' - 32] = { 0x3c, 0x66, 0x66, 0x3c, 0x66, 0x66, 0x3c, 0x00 },
	['9' - 32] = { 0x3c, 0x66, 0x66, 0x3e, 0x06, 0x0c, 0x38, 0x00 },
	[':' - 32] = { 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00 },
	[';' - 32] = { 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30 },
	['<' - 32] = { 0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x00 },
	['=' - 32] = { 0x00, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00 },
	['>' - 32] = { 0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x00 },
	['?' - 32] = { 0x3c, 0x66, 0x06, 0x0c, 0x18, 0x00, 0x18, 0x00 },
	['A' - 32] = { 0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x00 },
	['B' - 32] = { 0x7c, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x7c, 0x00 },
	['C' - 32] = { 0x3c, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3c, 0x00 },
	['D' - 32] = { 0x78, 0x6c, 0x66, 0x66, 0x66, 0x6c, 0x78, 0x00 },
	['E' - 32] = { 0x7e, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x7e, 0x00 },
	['F' - 32] = { 0x7e, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x60, 0x00 },
	['G' - 32] = { 0x3c, 0x66, 0x60, 0x6e, 0x66, 0x66, 0x3c, 0x00 },
	['H' - 32] = { 0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00 },
	['I' - 32] = { 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00 },
	['J' - 32] = { 0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x6c, 0x38, 0x00 },
	['K' - 32] = { 0x66, 0x6c, 0x78, 0x70, 0x78, 0x6c, 0x66, 0x00 },
	['L' - 32] = { 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7e, 0x00 },
	['M' - 32] = { 0x63, 0x77, 0x7f, 0x6b, 0x63, 0x63, 0x63, 0x00 },
	['N' - 32] = { 0x66, 0x76, 0x7e, 0x7e, 0x6e, 0x66, 0x66, 0x00 },
	['O' - 32] = { 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00 },
	['P' - 32] = { 0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0x00 },
	['Q' - 32] = { 0x3c, 0x66, 0x66, 0x66, 0x6a, 0x6c, 0x36, 0x00 },
	['R' - 32] = { 0x7c, 0x66, 0x66, 0x7c, 0x78, 0x6c, 0x66, 0x00 },
	['S' - 32] = { 0x3c, 0x66, 0x60, 0x3c, 0x06, 0x66, 0x3c, 0x00 },
	['T' - 32] = { 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 },
	['U' - 32] = { 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00 },
	['V' - 32] = { 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00 },
	['W' - 32] = { 0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00 },
	['X' - 32] = { 0x66, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0x66, 0x00 },
	['Y' - 32] = { 0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x00 },
	['Z' - 32] = { 0x7e, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x7e, 0x00 },
};

static __attribute__((interrupt)) void SupervisorGetVBR() {
	__asm__ volatile(".short 0x4e7a, 0x0801");
}

static APTR GetVBR(void) {
	APTR vbr = 0;
	if (SysBase->AttnFlags & AFF_68010)
		vbr = (APTR)Supervisor((ULONG (*)())SupervisorGetVBR);
	return vbr;
}

static void SetInterruptHandler(APTR interrupt) {
	*(volatile APTR*)(((UBYTE*)VBR) + 0x6c) = interrupt;
}

static APTR GetInterruptHandler(void) {
	return *(volatile APTR*)(((UBYTE*)VBR) + 0x6c);
}

static void WaitVbl(void) {
	debug_start_idle();
	while (1) {
		volatile ULONG vpos = *(volatile ULONG*)0xDFF004;
		vpos &= 0x1ff00;
		if (vpos != (311 << 8))
			break;
	}
	while (1) {
		volatile ULONG vpos = *(volatile ULONG*)0xDFF004;
		vpos &= 0x1ff00;
		if (vpos == (311 << 8))
			break;
	}
	debug_stop_idle();
}

static __attribute__((always_inline)) inline void WaitBlt(void) {
	UWORD tst = *(volatile UWORD*)&custom->dmaconr;
	(void)tst;
	while (*(volatile UWORD*)&custom->dmaconr & (1 << 14)) {}
}

static UWORD sfxDmaBit(UBYTE channel) {
	return (UWORD)(DMAF_AUD0 << channel);
}

static void stopSfxChannel(UBYTE channel) {
	if (channel >= SFX_CHANNEL_COUNT)
		return;

	custom->dmacon = sfxDmaBit(channel);
	custom->aud[channel].ac_vol = 0;
	sfxChannelFrames[channel] = 0;
	sfxChannelStartDelay[channel] = 0;
	sfxChannelSilenceQueueDelay[channel] = 0;
	sfxPendingSample[channel] = 0;
	sfxChannelPendingId[channel] = 0xff;
	sfxChannelPendingPeriod[channel] = 0;
	sfxChannelPendingVolume[channel] = 0;
	sfxChannelCurrentId[channel] = 0xff;
	sfxChannelPriority[channel] = 0;
	if (channel == ENGINE_CHANNEL) {
		engineActive = 0;
		engineLastSpeed = 0xff;
	}
}

static void stopAllSfx(void) {
	custom->dmacon = DMAF_AUDIO;
	for (UBYTE channel = 0; channel < SFX_CHANNEL_COUNT; channel++) {
		custom->aud[channel].ac_vol = 0;
		sfxChannelFrames[channel] = 0;
		sfxChannelStartDelay[channel] = 0;
		sfxChannelSilenceQueueDelay[channel] = 0;
		sfxChannelPendingId[channel] = 0xff;
		sfxChannelPendingPeriod[channel] = 0;
		sfxChannelPendingVolume[channel] = 0;
		sfxChannelCurrentId[channel] = 0xff;
		sfxChannelPriority[channel] = 0;
		sfxChannelSequence[channel] = 0;
		sfxPendingSample[channel] = 0;
	}
	memset(sfxRetriggerGuard, 0, sizeof(sfxRetriggerGuard));
	sfxVoiceSequence = 0;
	engineActive = 0;
	engineLastSpeed = 0xff;
	carrierIdleChannel = 0xff;
	carrierIdleAge = 0;
	carrierIdleForcedFade = 0;
	carrierIdleDelayFrames = CARRIER_IDLE_MIN_DELAY_FRAMES;
}

static UBYTE sfxRetriggerGuardFrames(UBYTE sfxId) {
	switch (sfxId) {
		case SFX_FIRE:
			return 8;
		case SFX_BOMB:
			return 18;
		case SFX_IMPACT:
			return 24;
		case SFX_HIT:
			return 30;
		case SFX_EJECT:
			return 40;
		case SFX_PICKUP_POWERUP:
			return 16;
		case SFX_FLAK_GUN_1:
		case SFX_FLAK_GUN_2:
			return 12;
		case SFX_GROUND_TARGET_HIT_1:
		case SFX_GROUND_TARGET_HIT_2:
		case SFX_GROUND_TARGET_HIT_3:
		case SFX_GROUND_TARGET_HIT_4:
			return 18;
		case SFX_CARRIER_IDLE_1:
		case SFX_CARRIER_IDLE_2:
			return 1;
		default:
			return 6;
	}
}

static void initSfx(void) {
	custom->adkcon = 0x7fff;
	stopAllSfx();
}

static UBYTE sfxChannelIsLeft(UBYTE channel) {
	return channel == 0 || channel == 3;
}

static UBYTE selectSfxChannel(const SfxSample* sample, WORD screenX) {
	UBYTE desiredPan = sample->pan;
	if (desiredPan == SFX_PAN_ANY)
		desiredPan = screenX < SFX_POSITION_CENTER ? SFX_PAN_LEFT : SFX_PAN_RIGHT;

	UBYTE bestChannel = 0xff;
	WORD bestScore = -32767;
	for (UBYTE channel = 0; channel < SFX_CHANNEL_COUNT; channel++) {
		if (channel == ENGINE_CHANNEL && engineActive)
			continue;

		UBYTE occupied = sfxChannelFrames[channel] != 0 ||
			sfxChannelStartDelay[channel] != 0 ||
			sfxPendingSample[channel] != 0;
		if (occupied && sfxChannelPriority[channel] >= sample->priority)
			continue;

		UBYTE channelPan = sfxChannelIsLeft(channel) ?
			SFX_PAN_LEFT : SFX_PAN_RIGHT;
		WORD score = occupied ? 0 : 1000;
		if (channelPan == desiredPan)
			score += 100;
		/* The persistent engine occupies the left pair's channel 3. Prefer
		 * the two right channels for centred sounds to keep the full mix
		 * balanced without software mixing. */
		if (engineActive && channelPan == SFX_PAN_RIGHT)
			score += 10;
		if (occupied)
			score += (WORD)(sample->priority - sfxChannelPriority[channel]) * 20;

		if (bestChannel == 0xff || score > bestScore ||
			(score == bestScore &&
			 sfxChannelSequence[channel] < sfxChannelSequence[bestChannel])) {
			bestChannel = channel;
			bestScore = score;
		}
	}
	return bestChannel;
}

static const WORD imaStepTable[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
	34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
	143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
	494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
	1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
	4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
	11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
	27086, 29794, 32767
};
static const BYTE imaIndexTable[16] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8
};

/* Decode one of the long carrier cues into a single reusable chip-RAM
 * buffer. The packed masters stay in ordinary memory and never occupy DMA
 * memory. Decode happens only while stationary on deck, not during flight. */
static UWORD decodeCarrierIdleSample(const UBYTE* encoded,
	ULONG encodedLength) {
	if (!carrierIdleDecodeBuffer || encodedLength < 5)
		return 0;
	ULONG decodedLength = ((ULONG)encoded[0] << 24) |
		((ULONG)encoded[1] << 16) | ((ULONG)encoded[2] << 8) | encoded[3];
	if (decodedLength < 2 ||
		decodedLength > CARRIER_IDLE_DECODE_BUFFER_BYTES ||
		4 + ((decodedLength + 1) >> 1) > encodedLength)
		return 0;

	LONG predictor = 0;
	WORD stepIndex = 0;
	ULONG outputIndex = 0;
	for (ULONG inputIndex = 4;
		inputIndex < encodedLength && outputIndex < decodedLength;
		inputIndex++) {
		UBYTE packed = encoded[inputIndex];
		for (UBYTE half = 0; half < 2 && outputIndex < decodedLength; half++) {
			UBYTE nibble = half == 0 ? packed >> 4 : packed & 15;
			LONG step = imaStepTable[stepIndex];
			LONG difference = step >> 3;
			if (nibble & 1) difference += step >> 2;
			if (nibble & 2) difference += step >> 1;
			if (nibble & 4) difference += step;
			predictor += (nibble & 8) ? -difference : difference;
			if (predictor > 32767) predictor = 32767;
			if (predictor < -32768) predictor = -32768;
			stepIndex += imaIndexTable[nibble];
			if (stepIndex < 0) stepIndex = 0;
			if (stepIndex > 88) stepIndex = 88;
			carrierIdleDecodeBuffer[outputIndex++] =
				(UBYTE)((BYTE)(predictor >> 8));
		}
	}
	if (outputIndex & 1)
		carrierIdleDecodeBuffer[outputIndex++] = 0;

	/* IMA quantisation can reintroduce a small non-zero edge even though the
	 * WAV master was faded. Smooth the decoded bytes themselves as well as
	 * Paula's slower volume envelope, preventing a DMA-start click. */
	ULONG fadeSamples = outputIndex / 2;
	if (fadeSamples > CARRIER_IDLE_PCM_EDGE_FADE_SAMPLES)
		fadeSamples = CARRIER_IDLE_PCM_EDGE_FADE_SAMPLES;
	for (ULONG index = 0; index < fadeSamples; index++) {
		LONG gain = (LONG)index;
		LONG first = (BYTE)carrierIdleDecodeBuffer[index];
		LONG last = (BYTE)carrierIdleDecodeBuffer[outputIndex - 1 - index];
		carrierIdleDecodeBuffer[index] =
			(UBYTE)((BYTE)((first * gain) / (LONG)fadeSamples));
		carrierIdleDecodeBuffer[outputIndex - 1 - index] =
			(UBYTE)((BYTE)((last * gain) / (LONG)fadeSamples));
	}
	carrierIdleDecodeBuffer[0] = 0;
	carrierIdleDecodeBuffer[outputIndex - 1] = 0;
	return (UWORD)outputIndex;
}

static void playSfxAtTuned(UBYTE sfxId, WORD screenX, UWORD volume,
	UWORD period) {
	if (sfxId >= SFX_COUNT)
		return;
	/* Menu music owns all four Paula channels exclusively. Gameplay stops
	 * the MOD before any one-shot effect can be requested. */
	if (modPlaying)
		return;

	const SfxSample* sample = &sfxSamples[sfxId];
	if (sample->byteLength < 2 || sfxRetriggerGuard[sfxId] > 0)
		return;
	UBYTE channel = selectSfxChannel(sample, screenX);
	if (channel >= SFX_CHANNEL_COUNT)
		return;

	if (sfxId >= SFX_CARRIER_IDLE_1 && sfxId <= SFX_CARRIER_IDLE_2) {
		UWORD decodedLength = decodeCarrierIdleSample(sample->data,
			sample->byteLength);
		if (!decodedLength)
			return;
		carrierIdlePlaybackSample = *sample;
		carrierIdlePlaybackSample.data = carrierIdleDecodeBuffer;
		carrierIdlePlaybackSample.byteLength = decodedLength;
		carrierIdlePlaybackSample.frames = (UBYTE)(
			(((ULONG)decodedLength * 50UL) + SFX_SAMPLE_RATE - 1UL) /
			SFX_SAMPLE_RATE + 1UL);
		sample = &carrierIdlePlaybackSample;
	}

	stopSfxChannel(channel);
	sfxPendingSample[channel] = sample;
	sfxChannelCurrentId[channel] = sfxId;
	sfxChannelPriority[channel] = sample->priority;
	sfxChannelSequence[channel] = ++sfxVoiceSequence;
	sfxChannelPendingId[channel] = sfxId;
	sfxChannelPendingVolume[channel] = volume > 64 ? 64 : volume;
	sfxChannelPendingPeriod[channel] = period;
	sfxRetriggerGuard[sfxId] = sfxRetriggerGuardFrames(sfxId);
	sfxChannelStartDelay[channel] = 1;
}

static void playSfxAt(UBYTE sfxId, WORD screenX) {
	if (sfxId >= SFX_COUNT)
		return;
	playSfxAtTuned(sfxId, screenX, sfxSamples[sfxId].volume,
		sfxSamples[sfxId].period);
}

static void playSfx(UBYTE sfxId) {
	playSfxAt(sfxId, SFX_POSITION_CENTER);
}

static UWORD flakSfxRandomState = 0x4a31;
static UWORD groundHitSfxRandomState = 0x73b9;
static UBYTE flakSfxLastVariant = 0xff;
static UBYTE groundHitSfxLastVariant = 0xff;

static UBYTE nextSfxVariant(UWORD* state, UWORD entropy, UBYTE* lastVariant,
	UBYTE variantCount) {
	/* Tiny 16-bit Galois LFSR: cheap on 68000, but unlike the old
	 * column-derived hash it advances on every actual sound event. Avoid
	 * the immediately previous choice as well; GroundTargetHit 1 and 2 are
	 * currently identical source WAVs, so repeated spatially deterministic
	 * choices otherwise make the new bank sound like one unchanged sample. */
	UWORD value = (UWORD)(*state ^ entropy);
	for (UBYTE bit = 0; bit < 3; bit++)
		value = (UWORD)((value >> 1) ^ (-(value & 1) & 0xb400));
	if (value == 0)
		value = 0xace1;
	*state = value;

	UBYTE variant = (UBYTE)((value ^ (value >> 5) ^ entropy) % variantCount);
	if (variant == *lastVariant)
		variant = (UBYTE)((variant + 1 + ((value >> 9) & 1)) % variantCount);
	*lastVariant = variant;
	return variant;
}

static void scheduleNextCarrierIdle(void) {
	carrierIdleRandomState = (UWORD)((carrierIdleRandomState >> 1) ^
		(-(carrierIdleRandomState & 1) & 0xb400) ^ frameCounter);
	if (!carrierIdleRandomState)
		carrierIdleRandomState = 0x6d2b;
	carrierIdleDelayFrames = (UWORD)(CARRIER_IDLE_MIN_DELAY_FRAMES +
		(carrierIdleRandomState % CARRIER_IDLE_DELAY_SPREAD_FRAMES));
	carrierIdleChannel = 0xff;
	carrierIdleAge = 0;
	carrierIdleForcedFade = 0;
}

/* Sparse carrier-deck ambience. The WAV masters retain their own authored
 * shape, while Paula volume supplies a clearly controlled 0.5-second fade
 * at both edges. Starting the engine, entering flight, or starting a MOD
 * initiates the same short fade-out instead of cutting the sample. */
static void updateCarrierIdleSfx(UBYTE eligible) {
	if (carrierIdleChannel < SFX_CHANNEL_COUNT) {
		UBYTE currentId = sfxChannelCurrentId[carrierIdleChannel];
		if (currentId < SFX_CARRIER_IDLE_1 ||
			currentId > SFX_CARRIER_IDLE_2) {
			scheduleNextCarrierIdle();
			return;
		}

		if (!eligible && carrierIdleForcedFade == 0)
			carrierIdleForcedFade = CARRIER_IDLE_FADE_FRAMES;

		UWORD volume;
		if (carrierIdleForcedFade) {
			carrierIdleForcedFade--;
			volume = (UWORD)((CARRIER_IDLE_VOLUME *
				carrierIdleForcedFade) / CARRIER_IDLE_FADE_FRAMES);
			if (!carrierIdleForcedFade) {
				stopSfxChannel(carrierIdleChannel);
				scheduleNextCarrierIdle();
				return;
			}
		} else if (carrierIdleAge < CARRIER_IDLE_FADE_FRAMES) {
			volume = (UWORD)((CARRIER_IDLE_VOLUME * carrierIdleAge) /
				CARRIER_IDLE_FADE_FRAMES);
		} else if (sfxChannelFrames[carrierIdleChannel] <
			CARRIER_IDLE_FADE_FRAMES) {
			volume = (UWORD)((CARRIER_IDLE_VOLUME *
				sfxChannelFrames[carrierIdleChannel]) /
				CARRIER_IDLE_FADE_FRAMES);
		} else {
			volume = CARRIER_IDLE_VOLUME;
		}

		if (sfxChannelStartDelay[carrierIdleChannel])
			sfxChannelPendingVolume[carrierIdleChannel] = volume;
		else
			custom->aud[carrierIdleChannel].ac_vol = volume;
		if (carrierIdleAge < 255)
			carrierIdleAge++;
		return;
	}

	if (!eligible) {
		/* Do not let time spent in music/flight make an ambience sample fire
		 * immediately on the first deck frame. */
		if (carrierIdleDelayFrames < CARRIER_IDLE_MIN_DELAY_FRAMES)
			scheduleNextCarrierIdle();
		return;
	}
	if (carrierIdleDelayFrames > 0) {
		carrierIdleDelayFrames--;
		return;
	}

	UBYTE variant = nextSfxVariant(&carrierIdleRandomState, frameCounter,
		&carrierIdleLastVariant, 2);
	UBYTE sfxId = (UBYTE)(SFX_CARRIER_IDLE_1 + variant);
	playSfxAtTuned(sfxId, SFX_POSITION_CENTER, 0, SFX_PAULA_PERIOD);
	for (UBYTE channel = 0; channel < SFX_CHANNEL_COUNT; channel++) {
		if (sfxChannelCurrentId[channel] == sfxId) {
			carrierIdleChannel = channel;
			carrierIdleAge = 0;
			carrierIdleForcedFade = 0;
			return;
		}
	}
	scheduleNextCarrierIdle();
}

static void playFlakGunSfx(UWORD entropy, WORD screenX, WORD flakY,
	WORD playerY) {
	WORD distance = flakY - playerY;
	if (distance < 0)
		distance = -distance;
	/* Current full level is 56. Fall off smoothly with vertical distance,
	 * but retain a quiet floor so off-height flak remains audible. */
	UWORD volumeReduction = (UWORD)(distance / 3);
	UWORD volume = volumeReduction >= 38 ? 18 : (UWORD)(56 - volumeReduction);

	UBYTE variant = nextSfxVariant(&flakSfxRandomState, entropy,
		&flakSfxLastVariant, 2);
	/* Most shots keep the authored pitch. Roughly one quarter get a subtle
	 * +/- period offset (about two percent) to reduce mechanical repetition. */
	UWORD period = SFX_PAULA_PERIOD;
	switch ((flakSfxRandomState >> 5) & 7) {
		case 0: period = SFX_PAULA_PERIOD - 6; break;
		case 1: period = SFX_PAULA_PERIOD + 6; break;
		default: break;
	}
	playSfxAtTuned((UBYTE)(SFX_FLAK_GUN_1 + variant), screenX, volume,
		period);
}

static void playGroundTargetHitSfx(UWORD entropy, WORD screenX) {
	playSfxAt((UBYTE)(SFX_GROUND_TARGET_HIT_1 +
		nextSfxVariant(&groundHitSfxRandomState, entropy,
			&groundHitSfxLastVariant, 4)), screenX);
}

/* User-reported bug: bomb/rocket/hit sounds sometimes audibly repeat. Root
 * cause found by computing each SFX's real playback duration against its
 * software stop timer (sfxChannelFrames, decremented once/frame in
 * updateSfx()): SFX_FIRE's sample plays for ~342ms but its stop timer was
 * 360ms, and SFX_HIT's ~346ms against the same 360ms timer - both genuinely
 * finish playing and Paula loops back to the START of the real sample data
 * for the remaining ~14-18ms before the software stop finally cuts DMA,
 * audibly retriggering the very start of the sound. (Other sounds -
 * SFX_BOMB/IMPACT/MENU/GAME_OVER - all had a large enough margin that this
 * never happened for them; a separate, unrelated bug already fixed this
 * session was the actual cause of BOMB specifically repeating - see
 * ReadJoyFire1Debounced().)
 *
 * Fix: play the real sample at its natural length, then queue a permanently
 * silent 1-word loop. Do not replace AUDxLC/AUDxLEN in the same CPU burst as
 * DMA is enabled: on OCS the audio DMA state has not necessarily latched the
 * sample registers yet, so the immediate rewrite can make Paula start from
 * the queued word instead of the sample. updateSfx() waits one full frame
 * after the DMA enable before changing the reload registers. That is far
 * shorter than the shortest sample, but safely beyond the first audio DMA
 * fetch. The new values are therefore used only at the natural loop wrap. */
static void startPendingSfxChannel(UBYTE channel) {
	const SfxSample* sample = sfxPendingSample[channel];
	if (!sample)
		return;

	custom->aud[channel].ac_ptr = (volatile UWORD*)sample->data;
	custom->aud[channel].ac_len = sample->byteLength >> 1;
	custom->aud[channel].ac_per = sfxChannelPendingPeriod[channel];
	custom->aud[channel].ac_vol = sfxChannelPendingVolume[channel];
	sfxChannelFrames[channel] = sample->frames;
	sfxPendingSample[channel] = 0;
	sfxChannelPendingId[channel] = 0xff;
	custom->dmacon = DMAF_SETCLR | sfxDmaBit(channel);
	sfxChannelSilenceQueueDelay[channel] = 1;
}

static void updateSfx(void) {
	for (UBYTE sfxId = 0; sfxId < SFX_COUNT; sfxId++) {
		if (sfxRetriggerGuard[sfxId] > 0)
			sfxRetriggerGuard[sfxId]--;
	}
	for (UBYTE channel = 0; channel < SFX_CHANNEL_COUNT; channel++) {
		if (sfxChannelStartDelay[channel] > 0) {
			sfxChannelStartDelay[channel]--;
			if (sfxChannelStartDelay[channel] == 0)
				startPendingSfxChannel(channel);
			continue;
		}

		if (sfxChannelSilenceQueueDelay[channel] > 0) {
			sfxChannelSilenceQueueDelay[channel]--;
			if (sfxChannelSilenceQueueDelay[channel] == 0) {
				custom->aud[channel].ac_ptr = (volatile UWORD*)sfxSilenceLoop;
				custom->aud[channel].ac_len = 1;
			}
		}

		if (sfxChannelFrames[channel] > 0) {
			sfxChannelFrames[channel]--;
			if (sfxChannelFrames[channel] == 0) {
				stopSfxChannel(channel);
				modRestoreChannelAfterSfx(channel);
			}
		}
	}
}

/* --- Menu screen background music (ProTracker MOD playback) ---
 * Minimal replay routine for the 4-channel/31-instrument "M.K." MOD format,
 * driven from the main loop's 50Hz VBlank tick (modTick(), called once per
 * loop iteration while the menu is showing) rather than a CIA-timer
 * interrupt - this project doesn't use CIA timers for anything else, and a
 * VBlank-driven tick is exactly it what real replayers reduce to at the
 * ProTracker-default tempo of 125 (see the accumulator math in modTick()).
 *
 * Only implements the effects amiga/assets/music/harrier_menu_fixed.mod
 * actually uses (verified by scanning its pattern data): 0xC (set volume),
 * 0xD (pattern break), 0xF (set speed if param<32, set tempo/BPM if
 * param>=32). Any other effect command is silently ignored rather than
 * misinterpreted, so a future re-export of the same tune with minor edits
 * degrades gracefully instead of desyncing. No arpeggio/portamento/vibrato -
 * this tune doesn't use them. Samples honour the MOD repeat offset/length:
 * sustained instruments switch to their repeat region after the initial
 * body, while percussion without a valid repeat region then becomes silent.
 *
 * Shares Paula's 4 hardware channels with the SFX system (see sfxDmaBit()
 * above) - the two are never active at once (music stops before gameplay
 * starts, see startModMusic()/stopModMusic() call sites). */
#define MOD_SAMPLE_COUNT 31
#define MOD_CHANNEL_COUNT 4
#define MOD_ROWS_PER_PATTERN 64
#define MOD_DEFAULT_SPEED 6
#define MOD_DEFAULT_TEMPO 125

typedef struct ModSample {
	const UBYTE* data;
	UWORD lengthBytes;
	UWORD repeatOffsetBytes;
	UWORD repeatLengthBytes;
	UBYTE volume;
} ModSample;

typedef struct ModChannelState {
	const ModSample* sample;
	UWORD period;
	UBYTE volume;
} ModChannelState;

static ModSample modSamples[MOD_SAMPLE_COUNT];
static UBYTE modOrder[128];
static UBYTE modSongLength;
static UBYTE modRestartPosition;
static const UBYTE* modPatternData;
static UBYTE modNumPatterns;
static ModChannelState modChannel[MOD_CHANNEL_COUNT];

static UBYTE modOrderIndex;
static UBYTE modRow;
static UWORD modTickAccum;
static UBYTE modTicksThisRow;
static UBYTE modSpeed;
static UBYTE modTempo;
static UBYTE modBreakPending;
static UBYTE modBreakRow;
static UBYTE modLoopEnabled;
static UBYTE modTempoPercent;

static void modParseHeader(const UBYTE* data) {
	UWORD off = 20;
	for (UBYTE i = 0; i < MOD_SAMPLE_COUNT; i++) {
		UWORD length = (UWORD)((((UWORD)data[off + 22] << 8) | data[off + 23]) * 2);
		UBYTE volume = data[off + 25];
		UWORD repeatOffset = (UWORD)((((UWORD)data[off + 26] << 8) |
			data[off + 27]) * 2);
		UWORD repeatLength = (UWORD)((((UWORD)data[off + 28] << 8) |
			data[off + 29]) * 2);
		modSamples[i].lengthBytes = length;
		modSamples[i].repeatOffsetBytes = repeatOffset;
		modSamples[i].repeatLengthBytes = repeatLength;
		modSamples[i].volume = volume > 64 ? 64 : volume;
		modSamples[i].data = 0;
		off = (UWORD)(off + 30);
	}

	modSongLength = data[off]; off++;
	modRestartPosition = data[off]; off++;
	UBYTE maxOrder = 0;
	for (UBYTE i = 0; i < 128; i++) {
		modOrder[i] = data[off + i];
		/* Only the first modSongLength entries are actually used - bytes
		 * past that are unused padding and not guaranteed to be zero (some
		 * tools leave garbage there), so they must not influence how many
		 * patterns we think exist or sampleData's computed start offset
		 * would be wrong. */
		if (i < modSongLength && modOrder[i] > maxOrder)
			maxOrder = modOrder[i];
	}
	off = (UWORD)(off + 128);
	off = (UWORD)(off + 4); /* "M.K." signature */
	modNumPatterns = (UBYTE)(maxOrder + 1);
	modPatternData = data + off;

	const UBYTE* sampleData = modPatternData + (ULONG)modNumPatterns * MOD_ROWS_PER_PATTERN * MOD_CHANNEL_COUNT * 4;
	for (UBYTE i = 0; i < MOD_SAMPLE_COUNT; i++) {
		modSamples[i].data = sampleData;
		sampleData += modSamples[i].lengthBytes;
	}
}

static UBYTE modChannelDmaOffPending[MOD_CHANNEL_COUNT];
static UBYTE modChannelLoopQueueDelay[MOD_CHANNEL_COUNT];

/* Paula only reloads its internal pointer/length from AUDxLC/AUDxLEN on an
 * off->on DMA transition (or a natural loop wrap) - if this channel's DMA
 * was already enabled from the previous note, writing new AUDxLC/AUDxLEN
 * without first clearing DMA is a no-op as far as the hardware's fetch state
 * goes. Clearing and immediately re-setting DMACON within the same function
 * call isn't a safe fix on its own though - Paula's DMA slot for this channel
 * may not have had a chance to actually latch the "off" state yet before the
 * "on" write lands, especially at full CPU speed, so a genuine off->on edge
 * isn't guaranteed. Split into two phases instead, exactly like this file's
 * own SFX system already does for the same shared channels
 * (stopSfxChannel() clears now, startPendingSfxChannel() writes the new
 * registers and re-enables a full VBlank later via sfxChannelStartDelay):
 * clear DMA now and mark the channel pending, then finish the retrigger
 * (write ac_ptr/ac_len/ac_per/ac_vol and set DMA) at the very start of the
 * *next* frame, via modCompletePendingRetriggers(). The one-frame gap is a
 * fixed, uniform delay applied identically to every note, so relative timing
 * between notes is unaffected - only the absolute clock position needed to
 * satisfy the hardware shifts, which is inaudible. */
static void modBeginRetrigger(UBYTE channel) {
	ModChannelState* ch = &modChannel[channel];
	if (!ch->sample || ch->sample->lengthBytes < 2)
		return;
	custom->dmacon = sfxDmaBit(channel);
	modChannelDmaOffPending[channel] = 1;
}

static void modCompletePendingRetriggers(void) {
	if (!modPlaying)
		return;
	for (UBYTE channel = 0; channel < MOD_CHANNEL_COUNT; channel++) {
		if (modChannelLoopQueueDelay[channel] > 0) {
			modChannelLoopQueueDelay[channel]--;
			if (modChannelLoopQueueDelay[channel] == 0) {
				ModChannelState* loopCh = &modChannel[channel];
				const ModSample* loopSample = loopCh->sample;
				if (loopSample &&
					loopSample->repeatLengthBytes >= 2 &&
					(ULONG)loopSample->repeatOffsetBytes +
						loopSample->repeatLengthBytes <=
						loopSample->lengthBytes) {
					custom->aud[channel].ac_ptr = (volatile UWORD*)
						(loopSample->data +
						 loopSample->repeatOffsetBytes);
					custom->aud[channel].ac_len =
						loopSample->repeatLengthBytes >> 1;
				} else {
					custom->aud[channel].ac_ptr =
						(volatile UWORD*)sfxSilenceLoop;
					custom->aud[channel].ac_len = 1;
				}
			}
		}
		if (!modChannelDmaOffPending[channel])
			continue;
		modChannelDmaOffPending[channel] = 0;
		ModChannelState* ch = &modChannel[channel];
		if (!ch->sample || ch->sample->lengthBytes < 2)
			continue;
		custom->aud[channel].ac_ptr = (volatile UWORD*)ch->sample->data;
		custom->aud[channel].ac_len = ch->sample->lengthBytes >> 1;
		custom->aud[channel].ac_per = ch->period;
		custom->aud[channel].ac_vol = ch->volume;
		custom->dmacon = DMAF_SETCLR | sfxDmaBit(channel);
		/* Paula has one VBlank to latch the complete initial sample before
		 * these registers become its repeat/silence reload source. */
		modChannelLoopQueueDelay[channel] = 1;
	}
}

static void modStopChannels(void) {
	modPlaying = 0;
	for (UBYTE channel = 0; channel < MOD_CHANNEL_COUNT; channel++) {
		custom->dmacon = sfxDmaBit(channel);
		custom->aud[channel].ac_vol = 0;
		modChannelDmaOffPending[channel] = 0;
		modChannelLoopQueueDelay[channel] = 0;
	}
}

static void modAdvanceRow(void) {
	const UBYTE* pattern = modPatternData + (ULONG)modOrder[modOrderIndex] * MOD_ROWS_PER_PATTERN * MOD_CHANNEL_COUNT * 4;
	const UBYTE* cellBase = pattern + (ULONG)modRow * MOD_CHANNEL_COUNT * 4;

	for (UBYTE channel = 0; channel < MOD_CHANNEL_COUNT; channel++) {
		const UBYTE* cell = cellBase + channel * 4;
		UBYTE sampleNum = (UBYTE)((cell[0] & 0xf0) | (cell[2] >> 4));
		UWORD period = (UWORD)(((cell[0] & 0x0f) << 8) | cell[1]);
		UBYTE effNum = (UBYTE)(cell[2] & 0x0f);
		UBYTE effParam = cell[3];
		ModChannelState* ch = &modChannel[channel];
		UBYTE retrigger = 0;

		if (sampleNum != 0 && sampleNum <= MOD_SAMPLE_COUNT) {
			ch->sample = &modSamples[sampleNum - 1];
			ch->volume = ch->sample->volume;
		}
		if (period != 0) {
			ch->period = period;
			retrigger = 1;
		}

		switch (effNum) {
			case 0x0: /* arpeggio - only the "no-op" (param 0) form is used here */
				break;
			case 0xC:
				ch->volume = effParam > 64 ? 64 : effParam;
				break;
			case 0xD:
				modBreakPending = 1;
				modBreakRow = (UBYTE)((effParam >> 4) * 10 + (effParam & 0x0f));
				break;
			case 0xF:
				if (effParam == 0)
					break;
				if (effParam < 32)
					modSpeed = effParam;
				else
					modTempo = effParam;
				break;
			default:
				break;
		}

		if (retrigger)
			modBeginRetrigger(channel);
		else if (ch->sample)
			custom->aud[channel].ac_vol = ch->volume;
	}

	modRow++;
	if (modBreakPending || modRow >= MOD_ROWS_PER_PATTERN) {
		modRow = modBreakPending ? modBreakRow : 0;
		modBreakPending = 0;
		modOrderIndex++;
		if (modOrderIndex >= modSongLength) {
			if (!modLoopEnabled) {
				modStopChannels();
				return;
			}
			modOrderIndex = modRestartPosition < modSongLength
				? modRestartPosition : 0;
		}
	}
}

/* Defensive recovery if a gameplay SFX ever overlaps a menu transition. In
 * normal operation the menu MOD and gameplay effects are mutually exclusive. */
static void modRestoreChannelAfterSfx(UBYTE channel) {
	if (!modPlaying || channel >= MOD_CHANNEL_COUNT)
		return;
	modBeginRetrigger(channel);
}

static void modTick(void) {
	if (!modPlaying)
		return;

	modTickAccum = (UWORD)(modTickAccum +
		((ULONG)modTempo * modTempoPercent + 50) / 100);
	while (modTickAccum >= 125) {
		modTickAccum = (UWORD)(modTickAccum - 125);
		modTicksThisRow++;
		if (modTicksThisRow >= modSpeed) {
			modTicksThisRow = 0;
			modAdvanceRow();
			if (!modPlaying)
				break;
		}
	}
}

static void startModTrack(const UBYTE* modData, UBYTE loopEnabled,
	UBYTE tempoPercent) {
	modParseHeader(modData);
	for (UBYTE channel = 0; channel < MOD_CHANNEL_COUNT; channel++) {
		modChannel[channel].sample = 0;
		modChannel[channel].period = 0;
		modChannel[channel].volume = 0;
		modChannelDmaOffPending[channel] = 0;
		modChannelLoopQueueDelay[channel] = 0;
	}
	modOrderIndex = 0;
	modRow = 0;
	modTickAccum = 0;
	modTicksThisRow = 0;
	modSpeed = MOD_DEFAULT_SPEED;
	modTempo = MOD_DEFAULT_TEMPO;
	modBreakPending = 0;
	modLoopEnabled = loopEnabled;
	modTempoPercent = tempoPercent;
	modPlaying = 1;
	/* Play row 0 immediately instead of waiting modSpeed ticks (~120ms at the
	 * default speed=6/tempo=125 before this tune's own row-0 F-effects even
	 * take hold) - a normal ProTracker replayer processes the first row on
	 * the very first interrupt, not after a full row's worth of silence. */
	modAdvanceRow();
}

static void startModMusic(void) {
	startModTrack(menuMusicMod, 1, 100);
}

static void startGameOverMusic(void) {
	startModTrack(gameOverMusicMod, 0, 120);
}

static void startCarrierLandingMusic(void) {
	startModTrack(carrierLandingMusicMod, 0, 100);
}

static void stopModMusic(void) {
	if (!modPlaying)
		return;
	/* Also drops a retrigger pending from the track's last frame, preventing
	 * stale MOD data from re-enabling DMA after gameplay takes ownership. */
	modStopChannels();
}

static UBYTE nextEngineNoiseByte(UBYTE speed) {
	UWORD bit = (UWORD)(((engineLfsr >> 0) ^ (engineLfsr >> 2) ^ (engineLfsr >> 3) ^ (engineLfsr >> 5)) & 1);
	BYTE white;
	BYTE rumble;
	BYTE mixed;

	engineLfsr = (UWORD)((engineLfsr >> 1) | (bit << 15));
	white = (BYTE)((engineLfsr & 0x1f) - 16);
	rumble = (BYTE)(((engineLfsr >> 8) & 0x0f) - 8);
	mixed = (BYTE)((white + rumble + (BYTE)(speed * 2)) << 1);
	return (UBYTE)mixed;
}

static UWORD enginePeriodForSpeed(UBYTE speed) {
	static const UWORD periods[GAME_SCROLL_SPEED_MAX_PIXELS + 1] = {
		0, 430, 360, 300, 250
	};
	if (speed < GAME_SCROLL_SPEED_MIN_PIXELS)
		speed = GAME_SCROLL_SPEED_MIN_PIXELS;
	if (speed > GAME_SCROLL_SPEED_MAX_PIXELS)
		speed = GAME_SCROLL_SPEED_MAX_PIXELS;
	return periods[speed] - (frameCounter & 3);
}

static UWORD engineVolumeForSpeed(UBYTE speed) {
	return (UWORD)(12 + speed * 4);
}

#if HAR_DEBUG_PERF_LOG
static char* appendUnsignedLong(char* out, ULONG value) {
	char temp[10];
	UBYTE count = 0;
	if (value == 0) {
		*out++ = '0';
		return out;
	}
	while (value > 0 && count < sizeof(temp)) {
		temp[count++] = (char)('0' + (value % 10));
		value /= 10;
	}
	while (count > 0)
		*out++ = temp[--count];
	return out;
}

static void perfLogResetInterval(void);

/* Appends to an in-RAM buffer only - no DOS calls here. TakeSystem() Forbid()s
 * task switching and Disable()s interrupts for the whole game session, so any
 * blocking dos.library call (Open/Write/Read) made after that point deadlocks
 * the machine (AmigaDOS shows "Software error - task held - finish all disk
 * activity" because the filesystem handler task can never be scheduled).
 * The buffer is only flushed to disk once, after FreeSystem() has restored
 * normal multitasking. */
static void perfLogAppend(const char* data, UWORD len) {
	if (perfLogBufferUsed + len > PERF_LOG_BUFFER_BYTES)
		return;
	for (UWORD i = 0; i < len; i++)
		perfLogBuffer[perfLogBufferUsed++] = data[i];
}

static void perfLogOpen(void) {
	KPrintF("Perf console every 10s: f/sec/loops fps(min,max,avg) hitch/maxVbl scroll/speed/origin render fuel/arm/rkt/bmb\n");
#if HAR_DEBUG_PERF_OVERLAY
	debug_text(12, 14, "HAR PERF: waiting 10s", 0x00ffff00);
#endif
	{
		static const char header[] =
			"frame,seconds,loops,minFps,maxFps,avgFps,hitches,maxVblDelta,scroll,speed,origin,job,stage,tileX,tileCols,objCols,pages,fuel,armour,rockets,bombs,hudCalls,hudArmChg,hudFuelChg,hudScoreChg,hudSpdChg,hudRktChg,hudBmbChg,hudGuardHits,hudGuard2Hits,hudRegHits,hudCollisionFires,livBplcon0,expBplcon0,livDdfstrt,expDdfstrt,livDdfstop,expDdfstop,livBpl5pt,expBpl5pt\n";
		perfLogAppend(header, sizeof(header) - 1);
	}
	perfLastLoopFrame = frameCounter;
	perfLogResetInterval();
}

static void perfLogFlushToDisk(void) {
	BPTR file = Open((CONST_STRPTR)"DH1:perf_log.csv", MODE_NEWFILE);
	if (!file)
		return;
	Write(file, (APTR)perfLogBuffer, perfLogBufferUsed);
	Close(file);
}

static void perfLogResetInterval(void) {
	perfIntervalStartFrame = frameCounter;
	perfLoopFrames = 0;
	perfMinFps = 999;
	perfMaxFps = 0;
	perfHitches = 0;
	perfMaxVblDelta = 0;
	perfWorldTileColumns = 0;
	perfWorldObjectColumns = 0;
	perfWorldPages = 0;
}

static UWORD perfFpsForVblDelta(UWORD delta) {
	if (delta == 0)
		return 50;
	if (delta > 50)
		return 1;
	return (UWORD)(50 / delta);
}

static void perfHudGuardArm(UBYTE* hudBuffer, const UBYTE* worldBufferEnd) {
	hudGuardBufferPtr = hudBuffer;
	hudGuardWorldToHudGapBytes = (ULONG)hudBuffer - (ULONG)worldBufferEnd;
	for (UWORD i = 0; i < HUD_GUARD_BYTES; i++)
		hudGuardBaseline[i] = hudBuffer[i];
	hudGuardArmed = 1;
	hudGuardHitFrames = 0;
	hudGuardDetailLogged = 0;

	hudGuard2BufferPtr = hudBuffer + (HUD_HEIGHT - 1) * SCREEN_PLANES * SCREEN_ROW_BYTES;
	for (UWORD i = 0; i < HUD_GUARD2_BYTES; i++)
		hudGuard2Baseline[i] = hudGuard2BufferPtr[i];
	hudGuard2HitFrames = 0;
	hudGuard2DetailLogged = 0;

	hudRegExpectedBplcon0 = (HUD_PLANES << 12) | (1 << 9);
	hudRegExpectedBpl5pt = (ULONG)(hudBuffer + SCREEN_ROW_BYTES * 4);
	hudRegExpectedDdfstrt = (129 >> 1) - 8;
	hudRegExpectedDdfstop = (UWORD)(hudRegExpectedDdfstrt + (((SCREEN_WIDTH >> 4) - 1) << 3));
	hudRegMismatchFrames = 0;
	hudRegDetailLogged = 0;
	{
		char line[64];
		char* out = line;
		*out++ = 'g'; *out++ = 'a'; *out++ = 'p'; *out++ = '=';
		out = appendUnsignedLong(out, hudGuardWorldToHudGapBytes);
		*out++ = '\n';
		*out = 0;
		KPrintF(line);
	}
}

static void perfHudGuardCheck(void) {
	if (!hudGuardArmed)
		return;
	UWORD mismatch = 0;
	UWORD firstOffset = 0;
	for (UWORD i = 0; i < HUD_GUARD_BYTES; i++) {
		if (hudGuardBufferPtr[i] != hudGuardBaseline[i]) {
			if (mismatch == 0)
				firstOffset = i;
			mismatch++;
		}
	}
	if (mismatch > 0) {
		hudGuardHitFrames++;
		if (!hudGuardDetailLogged) {
			hudGuardDetailLogged = 1;
			char line[64];
			char* out = line;
			*out++ = 'H'; *out++ = 'U'; *out++ = 'D'; *out++ = 'G'; *out++ = 'U'; *out++ = 'A'; *out++ = 'R'; *out++ = 'D'; *out++ = ' ';
			*out++ = 'o'; *out++ = 'f'; *out++ = 'f'; *out++ = '=';
			out = appendUnsignedLong(out, firstOffset);
			*out++ = ' '; *out++ = 'n'; *out++ = '=';
			out = appendUnsignedLong(out, mismatch);
			*out++ = '\n';
			*out = 0;
			KPrintF(line);
		}
	}

	UWORD mismatch2 = 0;
	UWORD firstOffset2 = 0;
	for (UWORD i = 0; i < HUD_GUARD2_BYTES; i++) {
		if (hudGuard2BufferPtr[i] != hudGuard2Baseline[i]) {
			if (mismatch2 == 0)
				firstOffset2 = i;
			mismatch2++;
		}
	}
	if (mismatch2 > 0) {
		hudGuard2HitFrames++;
		if (!hudGuard2DetailLogged) {
			hudGuard2DetailLogged = 1;
			char line[64];
			char* out = line;
			*out++ = 'H'; *out++ = 'U'; *out++ = 'D'; *out++ = 'G'; *out++ = 'U'; *out++ = 'A'; *out++ = 'R'; *out++ = 'D'; *out++ = '2'; *out++ = ' ';
			*out++ = 'o'; *out++ = 'f'; *out++ = 'f'; *out++ = '=';
			out = appendUnsignedLong(out, firstOffset2);
			*out++ = ' '; *out++ = 'n'; *out++ = '=';
			out = appendUnsignedLong(out, mismatch2);
			*out++ = '\n';
			*out = 0;
			KPrintF(line);
		}
	}

	UWORD liveBplcon0 = hudCopBplcon0OperandPtr ? *hudCopBplcon0OperandPtr : 0xffff;
	ULONG liveBpl5pt = 0xffffffff;
	if (activeCopperHudPlaneHigh[4] && activeCopperHudPlaneLow[4])
		liveBpl5pt = ((ULONG)*activeCopperHudPlaneHigh[4] << 16) | *activeCopperHudPlaneLow[4];
	UWORD liveDdfstrt = hudCopDdfstrtOperandPtr ? *hudCopDdfstrtOperandPtr : 0xffff;
	UWORD liveDdfstop = hudCopDdfstopOperandPtr ? *hudCopDdfstopOperandPtr : 0xffff;
	hudRegLastBplcon0 = liveBplcon0;
	hudRegLastBpl5pt = liveBpl5pt;
	hudRegLastDdfstrt = liveDdfstrt;
	hudRegLastDdfstop = liveDdfstop;
	if (liveBplcon0 != hudRegExpectedBplcon0 ||
		liveDdfstrt != hudRegExpectedDdfstrt || liveDdfstop != hudRegExpectedDdfstop) {
		hudRegMismatchFrames++;
		if (!hudRegDetailLogged) {
			hudRegDetailLogged = 1;
			char line[100];
			char* out = line;
			*out++ = 'H'; *out++ = 'R'; *out++ = 'E'; *out++ = 'G'; *out++ = ' ';
			*out++ = 'c'; *out++ = '0'; *out++ = '=';
			out = appendUnsignedLong(out, liveBplcon0);
			*out++ = '/'; out = appendUnsignedLong(out, hudRegExpectedBplcon0);
			*out++ = ' '; *out++ = 'p'; *out++ = '5'; *out++ = '=';
			out = appendUnsignedLong(out, liveBpl5pt);
			*out++ = '/'; out = appendUnsignedLong(out, hudRegExpectedBpl5pt);
			*out++ = ' '; *out++ = 's'; *out++ = 't'; *out++ = 'r'; *out++ = '=';
			out = appendUnsignedLong(out, liveDdfstrt);
			*out++ = '/'; out = appendUnsignedLong(out, hudRegExpectedDdfstrt);
			*out++ = ' '; *out++ = 's'; *out++ = 't'; *out++ = 'p'; *out++ = '=';
			out = appendUnsignedLong(out, liveDdfstop);
			*out++ = '/'; out = appendUnsignedLong(out, hudRegExpectedDdfstop);
			*out++ = '\n';
			*out = 0;
			KPrintF(line);
		}
	}
}

static void perfLogFrame(const GameState* game, UBYTE activeWorldBuffer) {
	perfHudGuardCheck();
	UWORD now = frameCounter;
	UWORD delta = (UWORD)(now - perfLastLoopFrame);
	UWORD elapsed = (UWORD)(now - perfIntervalStartFrame);
	UWORD instantFps = perfFpsForVblDelta(delta);

	perfLastLoopFrame = now;
	perfLoopFrames++;
	if (instantFps < perfMinFps)
		perfMinFps = instantFps;
	if (instantFps > perfMaxFps)
		perfMaxFps = instantFps;
	if (delta > perfMaxVblDelta)
		perfMaxVblDelta = delta;
	if (delta > 1)
		perfHitches++;

	if (elapsed < PERF_LOG_INTERVAL_FRAMES)
		return;

	char line[128];
	char* out = line;
	UWORD avgFps = elapsed ? (UWORD)(((ULONG)perfLoopFrames * 50) / elapsed) : 0;
	out = appendUnsignedLong(out, now);
	*out++ = '/';
	out = appendUnsignedLong(out, now / 50);
	*out++ = '/';
	out = appendUnsignedLong(out, perfLoopFrames);
	*out++ = ' ';
	*out++ = 'f';
	*out++ = 'p';
	*out++ = 's';
	*out++ = '=';
	out = appendUnsignedLong(out, perfMinFps == 999 ? 0 : perfMinFps);
	*out++ = '/';
	out = appendUnsignedLong(out, perfMaxFps);
	*out++ = '/';
	out = appendUnsignedLong(out, avgFps);
	*out++ = ' ';
	*out++ = 'h';
	*out++ = '=';
	out = appendUnsignedLong(out, perfHitches);
	*out++ = '/';
	out = appendUnsignedLong(out, perfMaxVblDelta);
	*out++ = ' ';
	*out++ = 'x';
	*out++ = '=';
	out = appendUnsignedLong(out, game->scrollX);
	*out++ = ' ';
	*out++ = 's';
	*out++ = 'p';
	*out++ = '=';
	out = appendUnsignedLong(out, game->speedLevel);
	*out++ = ' ';
	*out++ = 'o';
	*out++ = '=';
	out = appendUnsignedLong(out, ringWorldLastStreamedColumn);
	*out++ = '\n';
	*out = 0;
	KPrintF(line);
#if HAR_DEBUG_PERF_OVERLAY
	debug_text(12, 14, line, 0x00ffff00);
#endif

	out = line;
	*out++ = 'r';
	*out++ = '=';
	out = appendUnsignedLong(out, ringStreamColumn >= 0);
	*out++ = '/';
	out = appendUnsignedLong(out, 0);
	*out++ = '/';
	out = appendUnsignedLong(out, ringStreamRow);
	*out++ = ' ';
	*out++ = 'c';
	*out++ = '=';
	out = appendUnsignedLong(out, perfWorldTileColumns);
	*out++ = '/';
	out = appendUnsignedLong(out, perfWorldObjectColumns);
	*out++ = '/';
	out = appendUnsignedLong(out, perfWorldPages);
	*out++ = ' ';
	*out++ = 'f';
	*out++ = 'u';
	*out++ = '=';
	out = appendUnsignedLong(out, game->fuel);
	*out++ = ' ';
	*out++ = 'a';
	*out++ = 'r';
	*out++ = 'm';
	*out++ = '=';
	out = appendUnsignedLong(out, game->armour);
	*out++ = ' ';
	*out++ = 'w';
	*out++ = '=';
	out = appendUnsignedLong(out, game->rockets);
	*out++ = '/';
	out = appendUnsignedLong(out, game->bombs);
	*out++ = '\n';
	*out = 0;
	KPrintF(line);
#if HAR_DEBUG_PERF_OVERLAY
	debug_text(12, 28, line, 0x00ffff00);
#endif
	{
		char csv[300];
		char* c = csv;
		c = appendUnsignedLong(c, now);
		*c++ = ',';
		c = appendUnsignedLong(c, now / 50);
		*c++ = ',';
		c = appendUnsignedLong(c, perfLoopFrames);
		*c++ = ',';
		c = appendUnsignedLong(c, perfMinFps == 999 ? 0 : perfMinFps);
		*c++ = ',';
		c = appendUnsignedLong(c, perfMaxFps);
		*c++ = ',';
		c = appendUnsignedLong(c, avgFps);
		*c++ = ',';
		c = appendUnsignedLong(c, perfHitches);
		*c++ = ',';
		c = appendUnsignedLong(c, perfMaxVblDelta);
		*c++ = ',';
		c = appendUnsignedLong(c, game->scrollX);
		*c++ = ',';
		c = appendUnsignedLong(c, game->speedLevel);
		*c++ = ',';
		c = appendUnsignedLong(c, ringWorldLastStreamedColumn);
		*c++ = ',';
		c = appendUnsignedLong(c, ringStreamColumn >= 0);
		*c++ = ',';
		c = appendUnsignedLong(c, 0);
		*c++ = ',';
		c = appendUnsignedLong(c, ringStreamRow);
		*c++ = ',';
		c = appendUnsignedLong(c, perfWorldTileColumns);
		*c++ = ',';
		c = appendUnsignedLong(c, perfWorldObjectColumns);
		*c++ = ',';
		c = appendUnsignedLong(c, perfWorldPages);
		*c++ = ',';
		c = appendUnsignedLong(c, game->fuel);
		*c++ = ',';
		c = appendUnsignedLong(c, game->armour);
		*c++ = ',';
		c = appendUnsignedLong(c, game->rockets);
		*c++ = ',';
		c = appendUnsignedLong(c, game->bombs);
		*c++ = ',';
		c = appendUnsignedLong(c, hudDrawCalls);
		*c++ = ',';
		c = appendUnsignedLong(c, hudArmourChanges);
		*c++ = ',';
		c = appendUnsignedLong(c, hudFuelChanges);
		*c++ = ',';
		c = appendUnsignedLong(c, hudScoreChanges);
		*c++ = ',';
		c = appendUnsignedLong(c, hudSpeedChanges);
		*c++ = ',';
		c = appendUnsignedLong(c, hudRocketsChanges);
		*c++ = ',';
		c = appendUnsignedLong(c, hudBombsChanges);
		*c++ = ',';
		c = appendUnsignedLong(c, hudGuardHitFrames);
		*c++ = ',';
		c = appendUnsignedLong(c, hudGuard2HitFrames);
		*c++ = ',';
		c = appendUnsignedLong(c, hudRegMismatchFrames);
		*c++ = ',';
		c = appendUnsignedLong(c, hudReplenishFires);
		*c++ = ',';
		c = appendUnsignedLong(c, hudRegLastBplcon0);
		*c++ = ',';
		c = appendUnsignedLong(c, hudRegExpectedBplcon0);
		*c++ = ',';
		c = appendUnsignedLong(c, hudRegLastDdfstrt);
		*c++ = ',';
		c = appendUnsignedLong(c, hudRegExpectedDdfstrt);
		*c++ = ',';
		c = appendUnsignedLong(c, hudRegLastDdfstop);
		*c++ = ',';
		c = appendUnsignedLong(c, hudRegExpectedDdfstop);
		*c++ = ',';
		c = appendUnsignedLong(c, hudRegLastBpl5pt);
		*c++ = ',';
		c = appendUnsignedLong(c, hudRegExpectedBpl5pt);
		*c++ = '\n';
		perfLogAppend(csv, (UWORD)(c - csv));
	}
	hudDrawCalls = 0;
	hudArmourChanges = 0;
	hudFuelChanges = 0;
	hudScoreChanges = 0;
	hudSpeedChanges = 0;
	hudRocketsChanges = 0;
	hudBombsChanges = 0;
	hudGuardHitFrames = 0;
	hudGuard2HitFrames = 0;
	hudRegMismatchFrames = 0;
	hudReplenishFires = 0;
	perfLogResetInterval();
}
#endif

static void fillEngineBuffer(UBYTE speed) {
	if (!engineBuffer)
		return;

	for (UWORD i = 0; i < ENGINE_BUFFER_BYTES; i++)
		engineBuffer[i] = nextEngineNoiseByte(speed);
}

static void mutateEngineBuffer(UBYTE speed) {
	if (!engineBuffer)
		return;

	for (UWORD i = 0; i < ENGINE_MUTATE_BYTES; i++) {
		engineBuffer[engineWriteOffset] = nextEngineNoiseByte(speed);
		engineWriteOffset++;
		if (engineWriteOffset >= ENGINE_BUFFER_BYTES)
			engineWriteOffset = 0;
	}
}

static void startEngineSound(UBYTE speed) {
	if (!engineBuffer)
		return;

	fillEngineBuffer(speed);
	engineWriteOffset = 0;
	stopSfxChannel(ENGINE_CHANNEL);
	custom->aud[ENGINE_CHANNEL].ac_ptr = (volatile UWORD*)engineBuffer;
	custom->aud[ENGINE_CHANNEL].ac_len = ENGINE_BUFFER_BYTES >> 1;
	custom->aud[ENGINE_CHANNEL].ac_per = enginePeriodForSpeed(speed);
	custom->aud[ENGINE_CHANNEL].ac_vol = engineVolumeForSpeed(speed);
	custom->dmacon = DMAF_SETCLR | sfxDmaBit(ENGINE_CHANNEL);
	engineActive = 1;
	engineLastSpeed = speed;
}

static void updateEngineSound(UBYTE speed) {
	if (!engineBuffer)
		return;
	if (!engineActive) {
		startEngineSound(speed);
		return;
	}

	mutateEngineBuffer(speed);
	if (speed != engineLastSpeed || (frameCounter & 7) == 0) {
		custom->aud[ENGINE_CHANNEL].ac_per = enginePeriodForSpeed(speed);
		custom->aud[ENGINE_CHANNEL].ac_vol = engineVolumeForSpeed(speed);
		engineLastSpeed = speed;
	}
}

static void TakeSystem(void) {
	Forbid();
	SystemADKCON = custom->adkconr;
	SystemInts = custom->intenar;
	SystemDMA = custom->dmaconr;
	ActiView = GfxBase->ActiView;

	LoadView(0);
	WaitTOF();
	WaitTOF();
	WaitVbl();
	WaitVbl();

	OwnBlitter();
	WaitBlit();
	Disable();

	custom->intena = 0x7fff;
	custom->intreq = 0x7fff;
	custom->dmacon = 0x7fff;

	for (int i = 0; i < 32; i++)
		custom->color[i] = 0;

	WaitVbl();
	WaitVbl();

	VBR = GetVBR();
	SystemIrq = GetInterruptHandler();
}

static void FreeSystem(void) {
	WaitVbl();
	WaitBlit();
	custom->intena = 0x7fff;
	custom->intreq = 0x7fff;
	custom->dmacon = 0x7fff;

	SetInterruptHandler(SystemIrq);

	custom->cop1lc = (ULONG)GfxBase->copinit;
	custom->cop2lc = (ULONG)GfxBase->LOFlist;
	custom->copjmp1 = 0x7fff;

	custom->intena = SystemInts | 0x8000;
	custom->dmacon = SystemDMA | 0x8000;
	custom->adkcon = SystemADKCON | 0x8000;

	WaitBlit();
	DisownBlitter();
	Enable();

	LoadView(ActiView);
	WaitTOF();
	WaitTOF();
	Permit();
}

static __attribute__((always_inline)) inline short MouseLeft(void) {
	return !((*(volatile UBYTE*)0xbfe001) & 64);
}

static __attribute__((always_inline)) inline short MouseRight(void) {
	return !((*(volatile UWORD*)0xdff016) & (1 << 10));
}

static __attribute__((always_inline)) inline short JoyFire(void) {
	return !((*(volatile UBYTE*)0xbfe001) & 128);
}

/* Joystick port 2's second fire button - POTGOR/POTINP ($DFF016) bit 14,
 * active low, confirmed against the Amiga Hardware Reference Manual and
 * independently re-confirmed by the user. Uses custom->potinp (this
 * project's header names the same register potinp, not potgor) rather than
 * a raw pointer cast, matching the rest of this file's hardware-register
 * access style. A first attempt at this (raw pointer cast, same bit) hung
 * the game before it ever reached the menu; re-trying with this cleaner
 * access form since the bit/register itself checks out - see the headless
 * verification pass this was re-added with before being trusted again. */
static __attribute__((always_inline)) inline short JoyFire1(void) {
	return !(custom->potinp & (1 << 14));
}

/* User-reported bug: bomb/rocket sounds sometimes repeat several times for
 * one press. Most likely cause: the POT line (or a mechanical 2-button
 * joystick's real switch contacts) can flicker between reads, and Pressed()
 * (a plain now&&!previous edge check, one frame of history) interprets each
 * flicker as a brand new press. Requires JoyFire1() to read the same state
 * for 2 consecutive frames (40ms) before it's trusted - long enough to ride
 * out contact bounce, short enough not to add perceptible input lag. Reset
 * in InitInput() alongside the POTGO pull-up fix. */
static UBYTE joyFire1Stable;
static UBYTE joyFire1LastRaw;
static UBYTE joyFire1StableFrames;

static UBYTE ReadJoyFire1Debounced(void) {
	UBYTE raw = (UBYTE)JoyFire1();

	if (raw == joyFire1LastRaw) {
		if (joyFire1StableFrames < 2)
			joyFire1StableFrames++;
	} else {
		joyFire1LastRaw = raw;
		joyFire1StableFrames = 0;
	}

	if (joyFire1StableFrames >= 2)
		joyFire1Stable = raw;

	return joyFire1Stable;
}

static __attribute__((always_inline)) inline UWORD Joy1Dat(void) {
	return *(volatile UWORD*)0xdff00c;
}

static short JoyRight(void) {
	UWORD joy = Joy1Dat();
	return (joy & 0x0002) != 0;
}

static short JoyLeft(void) {
	UWORD joy = Joy1Dat();
	return (joy & 0x0200) != 0;
}

static short JoyDown(void) {
	UWORD joy = Joy1Dat();
	return ((joy & 0x0001) != 0) ^ ((joy & 0x0002) != 0);
}

static short JoyUp(void) {
	UWORD joy = Joy1Dat();
	return ((joy & 0x0100) != 0) ^ ((joy & 0x0200) != 0);
}

static void KeyboardAck(void) {
	ciaa->ciasdr = 0;
	ciaa->ciacra |= CIACRAF_SPMODE;
	for (volatile UWORD delay = 0; delay < 700; delay++) {}
	ciaa->ciacra &= (UBYTE)~CIACRAF_SPMODE;
}

static UBYTE DecodeKeyboardRaw(UBYTE serialData) {
	UBYTE rotated = (UBYTE)((serialData >> 1) | (serialData << 7));
	return (UBYTE)~rotated;
}

static void InitInput(void) {
	memclr(keyboardDown, sizeof(keyboardDown));
	lastKeyboardRawKey = 0xff;
	ciaa->ciacra &= (UBYTE)~CIACRAF_SPMODE;
	(void)ciaa->ciaicr;

	/* Drive the POT lines high (pull-up) so an unpressed second fire
	 * button reads as released rather than floating low/"pressed". Without
	 * this, JoyFire1() (POTINP bit 14) reads as permanently pressed, which
	 * makes input->bomb/input->any permanently true and hangs forever in
	 * WaitForInputRelease() right after this call, before the menu ever
	 * shows - this was the actual root cause of the JoyFire1 boot hang, not
	 * the read itself (the bit/register were always correct). */
	custom->potgo = 0xff00;

	joyFire1Stable = 0;
	joyFire1LastRaw = 0;
	joyFire1StableFrames = 0;
}

static void PollKeyboard(void) {
	for (short i = 0; i < 4; i++) {
		UBYTE icr = ciaa->ciaicr;
		if (!(icr & CIAICRF_SP))
			return;

		UBYTE raw = DecodeKeyboardRaw(ciaa->ciasdr);
		UBYTE code = raw & 0x7f;
		if (code < sizeof(keyboardDown))
			keyboardDown[code] = (raw & 0x80) ? 0 : 1;
		lastKeyboardRawKey = raw;
		KeyboardAck();
	}
}

static UBYTE KeyDown(UBYTE rawKey) {
	return keyboardDown[rawKey & 0x7f] != 0;
}

static void ReadInput(InputState* input) {
	PollKeyboard();
	memset(input, 0, sizeof(*input));

	UBYTE keyUp = KeyDown(RAWKEY_UP) || KeyDown(RAWKEY_W);
	UBYTE keyDown = KeyDown(RAWKEY_DOWN) || KeyDown(RAWKEY_S);
	UBYTE keyLeft = KeyDown(RAWKEY_LEFT) || KeyDown(RAWKEY_A);
	UBYTE keyRight = KeyDown(RAWKEY_RIGHT) || KeyDown(RAWKEY_D);
	UBYTE keyFire = KeyDown(RAWKEY_RETURN) || KeyDown(RAWKEY_CONTROL);
	/* Space moved here from keyFire - real Amstrad joystick behaviour is
	 * button0=bomb/button1=rocket, and this port deliberately swaps that
	 * (button0/JoyFire=rocket, button1/JoyFire2=bomb, see below) so the
	 * primary button matches the primary weapon. On keyboard there's no
	 * separate "button 2", so Space is the equivalent second key - keeping
	 * it out of keyFire too so one press doesn't fire both weapons at once. */
	UBYTE keyBomb = KeyDown(RAWKEY_B) || KeyDown(RAWKEY_LEFT_ALT) || KeyDown(RAWKEY_RIGHT_ALT) || KeyDown(RAWKEY_SPACE);
	/* Keep eject unambiguous: W remains the keyboard alternative for Up. */
	UBYTE keyEject = KeyDown(RAWKEY_E);
	UBYTE keyShift = KeyDown(RAWKEY_LEFT_SHIFT) || KeyDown(RAWKEY_RIGHT_SHIFT);
	UBYTE keyControl = KeyDown(RAWKEY_CONTROL);
	UBYTE keySpace = KeyDown(RAWKEY_SPACE);
	UBYTE keyD = KeyDown(RAWKEY_D);
	UBYTE keyP = KeyDown(RAWKEY_P);
	UBYTE keyR = KeyDown(RAWKEY_R);

	input->up = JoyUp() || keyUp;
	input->down = JoyDown() || keyDown;
	input->left = JoyLeft() || keyLeft;
	input->right = JoyRight() || keyRight;
	input->fire = JoyFire() || MouseLeft() || keyFire;
	/* JoyFire1() (joystick port 2's second button) gives a real 2-button
	 * joystick/pad bomb access, matching the real Amstrad's button0=bomb/
	 * button1=rocket pairing on the other button (JoyFire()/button0 stays
	 * rocket here - see keyFire's comment above). Right mouse button matches
	 * the existing left-mouse=fire pairing; keyBomb (B/Alt/Space) covers
	 * keyboard. A single-fire-button joystick still can't reach bomb this
	 * way - same limitation the real Amstrad's own single-button mode has. */
	input->bomb = keyBomb || MouseRight() || ReadJoyFire1Debounced();
	input->eject = keyEject;
	input->shift = keyShift;
	input->control = keyControl;
	input->space = keySpace;
	input->d = keyD;
	input->p = keyP;
	input->r = keyR;
	input->menuPrev = input->up;
	input->menuNext = input->down || MouseRight();
	input->select = input->fire;
	input->cancel = KeyDown(RAWKEY_ESCAPE);
	input->any = input->up || input->down || input->left || input->right || input->fire || input->bomb || input->eject || MouseRight();
	input->lastRawKey = lastKeyboardRawKey;
}

static UBYTE Pressed(UBYTE now, UBYTE previous) {
	return now && !previous;
}

static UBYTE InputMask(const InputState* input) {
	return (input->up ? 0x01 : 0)
		| (input->down ? 0x02 : 0)
		| (input->left ? 0x04 : 0)
		| (input->right ? 0x08 : 0)
		| (input->fire ? 0x10 : 0)
		| (input->bomb ? 0x20 : 0)
		| (input->eject ? 0x40 : 0);
}

static WORD landSurfaceYForWorldColumn(LONG worldColumn);

static void WaitFramesOrSelect(UWORD frames) {
	InputState input;
	UWORD start = frameCounter;
	while ((UWORD)(frameCounter - start) < frames) {
		ReadInput(&input);
		if (input.select)
			break;
		WaitVbl();
	}
}

static void WaitForInputRelease(void) {
	InputState input;
	do {
		ReadInput(&input);
		WaitVbl();
	} while (input.any);
}

static __attribute__((always_inline)) inline USHORT* copSetPlanesEx(UBYTE bplPtrStart, USHORT* copListEnd, const UBYTE** planes, int numPlanes, UBYTE trackMode) {
	for (USHORT i = 0; i < numPlanes; i++) {
		ULONG addr = (ULONG)planes[i];
		*copListEnd++ = offsetof(struct Custom, bplpt[0]) + (i + bplPtrStart) * sizeof(APTR);
		if (trackMode == COPPER_TRACK_SCROLL)
			activeCopperPlaneHigh[i] = copListEnd;
		else if (trackMode == COPPER_TRACK_HUD)
			activeCopperHudPlaneHigh[i] = copListEnd;
		*copListEnd++ = (UWORD)(addr >> 16);
		*copListEnd++ = offsetof(struct Custom, bplpt[0]) + (i + bplPtrStart) * sizeof(APTR) + 2;
		if (trackMode == COPPER_TRACK_SCROLL)
			activeCopperPlaneLow[i] = copListEnd;
		else if (trackMode == COPPER_TRACK_HUD)
			activeCopperHudPlaneLow[i] = copListEnd;
		*copListEnd++ = (UWORD)addr;
	}
	return copListEnd;
}

static __attribute__((always_inline)) inline USHORT* copSetPlanes(UBYTE bplPtrStart, USHORT* copListEnd, const UBYTE** planes, int numPlanes) {
	return copSetPlanesEx(bplPtrStart, copListEnd, planes, numPlanes, COPPER_TRACK_SCROLL);
}

/* Channel 1 is the player's attached companion and channels 2+3 are the
 * enemy plane's attached pair. Channel 5 is normally hidden and is borrowed
 * for crash debris. Wingman uses channel 6 as a normal three-colour sprite;
 * channel 7 is currently unused. Every channel takes an explicit argument
 * (no more
 * implicit "anything unmatched falls back to nullSprite" - all 8 channels
 * are spoken for, callers pass nullSprite themselves where that's what they
 * want, e.g. the menu screen). */
static USHORT* copSetSprites(USHORT* copListEnd, const UWORD* sprite0, const UWORD* sprite1, const UWORD* sprite2, const UWORD* sprite3, const UWORD* sprite4, const UWORD* sprite5, const UWORD* sprite6, const UWORD* sprite7) {
	for (USHORT i = 0; i < 8; i++) {
		const UWORD* sprite;
		if (i == 0)
			sprite = sprite0;
		else if (i == 1)
			sprite = sprite1;
		else if (i == 2)
			sprite = sprite2;
		else if (i == 3)
			sprite = sprite3;
		else if (i == 4)
			sprite = sprite4;
		else if (i == 5)
			sprite = sprite5;
		else if (i == 6)
			sprite = sprite6;
		else
			sprite = sprite7;
		ULONG addr = (ULONG)sprite;
		*copListEnd++ = offsetof(struct Custom, sprpt[0]) + i * sizeof(APTR);
		*copListEnd++ = (UWORD)(addr >> 16);
		*copListEnd++ = offsetof(struct Custom, sprpt[0]) + i * sizeof(APTR) + 2;
		*copListEnd++ = (UWORD)addr;
	}
	return copListEnd;
}

static __attribute__((always_inline)) inline USHORT* copSetColor(USHORT* copListCurrent, USHORT index, USHORT color) {
	*copListCurrent++ = offsetof(struct Custom, color) + sizeof(UWORD) * index;
	*copListCurrent++ = color;
	return copListCurrent;
}

static USHORT* screenScan(USHORT* copListEnd, USHORT fetchWidth, UBYTE fetchExtraWordLeft) {
	const USHORT x = SCREEN_DIWSTRT_X;
	const USHORT y = SCREEN_DIWSTRT_Y;
	const USHORT width = SCREEN_WIDTH;
	const USHORT height = SCREEN_HEIGHT;
	const USHORT res = 8;
	const USHORT xstop = x + width;
	const USHORT ystop = y + height;
	const USHORT fw = (x >> 1) - res - (fetchExtraWordLeft ? 8 : 0);

	*copListEnd++ = offsetof(struct Custom, ddfstrt);
	*copListEnd++ = fw;
	*copListEnd++ = offsetof(struct Custom, ddfstop);
	*copListEnd++ = fw + (((fetchWidth >> 4) - 1) << 3);
	*copListEnd++ = offsetof(struct Custom, diwstrt);
	*copListEnd++ = x + (y << 8);
	*copListEnd++ = offsetof(struct Custom, diwstop);
	/* NOTE: a DIWSTOP vertical byte that reads identical to DIWSTRT's (since
	 * (y+256)&0xff always equals y) was suspected as a source of display
	 * instability, but empirically fixing it (subtracting 1 line) did not
	 * resolve the reported HUD ghosting and cost a visible pixel row off
	 * the bottom gauges (ROCKETS/BOMBS border) - reverted pending a
	 * confirmed root cause. */
	*copListEnd++ = ((ystop & 0xff) << 8) | ((xstop - 256) & 0xff);
	return copListEnd;
}

static USHORT* copWaitDisplayYAt(USHORT* copListEnd, USHORT displayY, UBYTE hpos) {
	USHORT beamY = (USHORT)(SCREEN_DIWSTRT_Y + displayY);
	*copListEnd++ = (beamY << 8) | (hpos & 0xfe) | 0x01;
	*copListEnd++ = 0xfffe;
	return copListEnd;
}

static USHORT* copWaitDisplayY(USHORT* copListEnd, USHORT displayY) {
	return copWaitDisplayYAt(copListEnd, displayY, 0);
}

static USHORT* copSetGameSkyGradient(USHORT* copListEnd, const UWORD* palette) {
	activeCopperSkyTopColor = (UWORD*)(copListEnd + 1);
	copListEnd = copSetColor(copListEnd, GAME_COLOR_SKY, currentSkyTopRgb);
	activeCopperCloudTopColor = (UWORD*)(copListEnd + 1);
	copListEnd = copSetColor(copListEnd, GAME_COLOR_SEA, currentCloudTopRgb);
	copListEnd = copWaitDisplayY(copListEnd, GAME_SKY_MID_Y);
	activeCopperSkyMidColor = (UWORD*)(copListEnd + 1);
	copListEnd = copSetColor(copListEnd, GAME_COLOR_SKY, currentSkyMidRgb);
	copListEnd = copWaitDisplayY(copListEnd, GAME_SKY_LOW_Y);
	activeCopperSkyLowColor = (UWORD*)(copListEnd + 1);
	copListEnd = copSetColor(copListEnd, GAME_COLOR_SKY, currentSkyLowRgb);
	activeCopperSeaLowColor = (UWORD*)(copListEnd + 1);
	copListEnd = copSetColor(copListEnd, GAME_COLOR_SEA, currentSeaLowRgb);
	(void)palette;
	return copListEnd;
}

static USHORT* copSetFetch(USHORT* copListEnd, USHORT fetchWidth, UBYTE fetchExtraWordLeft) {
	const USHORT x = SCREEN_DIWSTRT_X;
	const USHORT res = 8;
	const USHORT fw = (x >> 1) - res - (fetchExtraWordLeft ? 8 : 0);

	*copListEnd++ = offsetof(struct Custom, ddfstrt);
	*copListEnd++ = fw;
	*copListEnd++ = offsetof(struct Custom, ddfstop);
	*copListEnd++ = fw + (((fetchWidth >> 4) - 1) << 3);
	return copListEnd;
}

static USHORT* copSetBplcon1(USHORT* copListEnd, UWORD value, UBYTE trackForScroll) {
	*copListEnd++ = offsetof(struct Custom, bplcon1);
	if (trackForScroll)
		activeCopperBplcon1 = copListEnd;
	*copListEnd++ = value;
	return copListEnd;
}

static USHORT* copSetModulo(USHORT* copListEnd, USHORT rowBytes, USHORT fetchWidth) {
	USHORT fetchBytes = fetchWidth >> 3;
	UWORD modulo = SCREEN_PLANES * rowBytes - fetchBytes;

	*copListEnd++ = offsetof(struct Custom, bpl1mod);
	*copListEnd++ = modulo;
	*copListEnd++ = offsetof(struct Custom, bpl2mod);
	*copListEnd++ = modulo;
	return copListEnd;
}

static __attribute__((interrupt)) void interruptHandler(void) {
	custom->intreq = (1 << INTB_VERTB);
	custom->intreq = (1 << INTB_VERTB);
	frameCounter++;
}

static UWORD horizontalScrollDelayToBplcon1(UWORD scrollDelay) {
	UWORD value = scrollDelay & 0x0f;
	return value | (value << 4);
}

static void setCopperPlanePointers(const UBYTE* screen, USHORT rowBytes, USHORT byteOffset) {
	for (int plane = 0; plane < SCREEN_PLANES; plane++) {
		ULONG addr = (ULONG)(screen + byteOffset + rowBytes * plane);
		if (activeCopperPlaneHigh[plane])
			*activeCopperPlaneHigh[plane] = (UWORD)(addr >> 16);
		if (activeCopperPlaneLow[plane])
			*activeCopperPlaneLow[plane] = (UWORD)addr;
	}
}

/* setCopperHudPointers() (patched activeCopperHudPlaneHigh/Low on a HUD
 * buffer swap) removed along with HUD double-buffering itself - see
 * HUD_BUFFER_COUNT. The single hud buffer's copper pointers are set once by
 * buildGameHudCopper() and never need patching afterward. */

static void setCopperFineScroll(UWORD bplcon1) {
	if (activeCopperBplcon1)
		*activeCopperBplcon1 = bplcon1;
}

static void buildDisplayCopperEx(USHORT* copper, const UBYTE* screen, const UWORD* palette, USHORT rowBytes, USHORT fetchWidth, UWORD bplcon1, UBYTE fetchExtraWordLeft, USHORT byteOffset, const UWORD* sprite0, const UWORD* sprite1, const UWORD* sprite2, const UWORD* sprite3, const UWORD* sprite4, const UWORD* sprite5, const UWORD* sprite6, const UWORD* sprite7) {
	USHORT* copPtr = copper;
	const UBYTE* planes[SCREEN_PLANES];

	copPtr = screenScan(copPtr, fetchWidth, fetchExtraWordLeft);

	*copPtr++ = offsetof(struct Custom, bplcon0);
	*copPtr++ = (SCREEN_PLANES << 12) | (1 << 9);
	copPtr = copSetBplcon1(copPtr, bplcon1, 1);
	*copPtr++ = offsetof(struct Custom, bplcon2);
	*copPtr++ = 0;

	copPtr = copSetModulo(copPtr, rowBytes, fetchWidth);

	for (int plane = 0; plane < SCREEN_PLANES; plane++)
		planes[plane] = screen + byteOffset + rowBytes * plane;
	copPtr = copSetPlanes(0, copPtr, planes, SCREEN_PLANES);
	copPtr = copSetSprites(copPtr, sprite0, sprite1, sprite2, sprite3, sprite4, sprite5, sprite6, sprite7);

	for (int color = 0; color < 32; color++)
		copPtr = copSetColor(copPtr, color, palette[color]);

	*copPtr++ = 0xffff;
	*copPtr++ = 0xfffe;
}

static void buildDisplayCopper(USHORT* copper, const UBYTE* screen, const UWORD* palette, const UWORD* nullSprite) {
	buildDisplayCopperEx(copper, screen, palette, SCREEN_ROW_BYTES, SCREEN_WIDTH, 0, 0, 0, nullSprite, nullSprite, nullSprite, nullSprite, nullSprite, nullSprite, nullSprite, nullSprite);
}

/*
 * Demo-style menu scroller: Copper switches to an independent, pre-rendered
 * CHIP playfield for eight raster lines, then restores the stationary menu.
 * Runtime scrolling only patches five pointers and BPLCON1.
 */
static void buildMenuCopper(USHORT* copper, const UBYTE* screen,
	const UBYTE* ticker, const UWORD* palette, const UWORD* nullSprite) {
	USHORT* copPtr = copper;
	const UBYTE* planes[SCREEN_PLANES];
	const UWORD tickerFetchWidth = SCREEN_WIDTH + 16;

	copPtr = screenScan(copPtr, SCREEN_WIDTH, 0);
	*copPtr++ = offsetof(struct Custom, bplcon0);
	*copPtr++ = (SCREEN_PLANES << 12) | (1 << 9);
	copPtr = copSetBplcon1(copPtr, 0, 0);
	*copPtr++ = offsetof(struct Custom, bplcon2);
	*copPtr++ = 0;
	copPtr = copSetModulo(copPtr, SCREEN_ROW_BYTES, SCREEN_WIDTH);
	for (int plane = 0; plane < SCREEN_PLANES; plane++)
		planes[plane] = screen + SCREEN_ROW_BYTES * plane;
	copPtr = copSetPlanesEx(0, copPtr, planes, SCREEN_PLANES,
		COPPER_TRACK_NONE);
	copPtr = copSetSprites(copPtr, nullSprite, nullSprite, nullSprite,
		nullSprite, nullSprite, nullSprite, nullSprite, nullSprite);
	for (int color = 0; color < 32; color++)
		copPtr = copSetColor(copPtr, color, palette[color]);

	/* Program the ticker after line 7's fetch, safely before line 8. */
	copPtr = copWaitDisplayYAt(copPtr, MENU_TICKER_Y - 1, 0xd2);
	copPtr = copSetFetch(copPtr, tickerFetchWidth, 1);
	*copPtr++ = offsetof(struct Custom, bplcon1);
	activeMenuTickerBplcon1 = copPtr;
	*copPtr++ = 0;
	copPtr = copSetModulo(copPtr, MENU_TICKER_ROW_BYTES,
		tickerFetchWidth);
	for (int plane = 0; plane < SCREEN_PLANES; plane++) {
		ULONG addr = (ULONG)(ticker + MENU_TICKER_ROW_BYTES * plane);
		*copPtr++ = offsetof(struct Custom, bplpt[0]) +
			plane * sizeof(APTR);
		activeMenuTickerPlaneHigh[plane] = copPtr;
		*copPtr++ = (UWORD)(addr >> 16);
		*copPtr++ = offsetof(struct Custom, bplpt[0]) +
			plane * sizeof(APTR) + 2;
		activeMenuTickerPlaneLow[plane] = copPtr;
		*copPtr++ = (UWORD)addr;
	}

	/* Restore menu fetch geometry and its exact line-16 addresses. */
	copPtr = copWaitDisplayYAt(copPtr,
		MENU_TICKER_Y + FONT_HEIGHT - 1, 0xd2);
	copPtr = copSetFetch(copPtr, SCREEN_WIDTH, 0);
	copPtr = copSetBplcon1(copPtr, 0, 0);
	copPtr = copSetModulo(copPtr, SCREEN_ROW_BYTES, SCREEN_WIDTH);
	for (int plane = 0; plane < SCREEN_PLANES; plane++)
		planes[plane] = screen +
			(MENU_TICKER_Y + FONT_HEIGHT) *
				SCREEN_PLANES * SCREEN_ROW_BYTES +
			SCREEN_ROW_BYTES * plane;
	copPtr = copSetPlanesEx(0, copPtr, planes, SCREEN_PLANES,
		COPPER_TRACK_NONE);

	*copPtr++ = 0xffff;
	*copPtr++ = 0xfffe;
}

static void buildGameHudCopper(USHORT* copper, const UBYTE* world, const UBYTE* hud, const UWORD* palette, UWORD scrollDelay, USHORT byteOffset, const UWORD* playerSprite, const UWORD* playerAttachSprite, const UWORD* crashPart1Sprite, const UWORD* enemyAttachSprite, const UWORD* enemySprite, const UWORD* enemyMissileSprite, const UWORD* wingmanSprite, const UWORD* unusedSprite7) {
	USHORT* copPtr = copper;
	const UBYTE* planes[SCREEN_PLANES];

	copPtr = screenScan(copPtr, GAME_FETCH_WIDTH, 1);

	*copPtr++ = offsetof(struct Custom, bplcon0);
	*copPtr++ = (GAME_WORLD_DISPLAY_PLANES << 12) | (1 << 9);
	copPtr = copSetBplcon1(copPtr, horizontalScrollDelayToBplcon1(scrollDelay), 1);
	*copPtr++ = offsetof(struct Custom, bplcon2);
	*copPtr++ = 0;

	/* copSetModulo() always computes its stride from SCREEN_PLANES (the
	 * buffer's real storage layout, unchanged) regardless of how many planes
	 * are actually being fetched/displayed here - correct as-is, same as the
	 * HUD split below already relies on. */
	copPtr = copSetModulo(copPtr, GAME_WORLD_ROW_BYTES, GAME_FETCH_WIDTH);
	for (int plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++)
		planes[plane] = world + byteOffset + GAME_WORLD_ROW_BYTES * plane;
	copPtr = copSetPlanesEx(0, copPtr, planes, GAME_WORLD_DISPLAY_PLANES, COPPER_TRACK_SCROLL);
	/* The menu/loading-screen copper build (buildDisplayCopperEx()) still
	 * uses the full SCREEN_PLANES(5) via these same activeCopperPlaneHigh/Low
	 * tracking arrays (COPPER_TRACK_SCROLL) - without this, switching from
	 * menu to gameplay would leave index 4 holding a stale pointer into the
	 * menu's own copper buffer, and updateGameScrollCopper()'s per-frame
	 * setCopperPlanePointers() (which null-guards before writing) would
	 * happily keep writing scroll data into that stale, out-of-scope
	 * location every frame. Explicitly clear anything beyond the planes this
	 * build actually wrote. */
	for (int plane = GAME_WORLD_DISPLAY_PLANES; plane < SCREEN_PLANES; plane++) {
		activeCopperPlaneHigh[plane] = 0;
		activeCopperPlaneLow[plane] = 0;
	}
	/* Channels 2+3 form an attached pair for the CPC+ enemy plane. Channel 3
	 * is still borrowed for player crash part 2 while the enemy is hidden. */
	copPtr = copSetSprites(copPtr, playerSprite, playerAttachSprite,
		enemySprite, enemyAttachSprite, enemyMissileSprite, crashPart1Sprite,
		wingmanSprite, unusedSprite7);

	for (int color = 0; color < 32; color++)
		copPtr = copSetColor(copPtr, color, palette[color]);
	copPtr = copSetColor(copPtr, 29, WINGMAN_SPRITE_DARK_RGB);
	copPtr = copSetColor(copPtr, 30, WINGMAN_SPRITE_MID_RGB);
	copPtr = copSetColor(copPtr, 31, WINGMAN_SPRITE_LIGHT_RGB);

	copPtr = copSetGameSkyGradient(copPtr, palette);
	/* This WAIT's horizontal position matters a lot: everything from here
	 * down to the HUD's own DDFSTRT (line 168, position 56) must execute
	 * inside the gap between the world's own DDFSTOP (position 208, for its
	 * wide 336px scroll-prefetch fetch) and that point. The 17 MOVE
	 * instructions in this transition (colour, ddfstrt/stop, bplcon1,
	 * modulo, bplcon0, 5 plane-pointer pairs) take a minimum of ~68 colour
	 * clocks; waiting until 0xe0(224) here left only ~59 clocks available
	 * (227-224 to end of this line, +56 into the next) - a real shortfall
	 * even before accounting for audio/disk DMA contention, meaning the
	 * copper could still be mid-transition (most at risk: the LAST
	 * instructions in program order, i.e. the plane pointers) when line 168
	 * already needs them, reading stale/scrolling world-buffer pointers for
	 * a variable number of HUD scanlines - matching the reported ghost's
	 * scroll-synced behaviour. Triggering right after DDFSTOP(208) instead
	 * reclaims that wasted slack without cutting into the world's own
	 * fetch. */
	copPtr = copWaitDisplayYAt(copPtr, HUD_TOP - 1, 0xd2);
	/* GAME_COLOR_SKY doubles as HUD_COLOR_BACKGROUND - the whole panel's
	 * fill colour (drawHudStatic() does a full-panel fillRect() with it) -
	 * so this stays plain black rather than the CPC reference table's
	 * instrument-panel sky value ($FA0/orange): that value washed the
	 * entire panel background orange instead of accenting anything, since
	 * our custom HUD wasn't designed around the CPC's own raster panel
	 * layout, just reusing the same register numbers for its own scheme. */
	copPtr = copSetColor(copPtr, GAME_COLOR_SKY, 0x000);
	/* copSetFetch() writes [ddfstrt_addr, ddfstrt_val, ddfstop_addr,
	 * ddfstop_val] - capture the two value-word slots directly rather than
	 * threading a tracking flag through the shared helper. */
	hudCopDdfstrtOperandPtr = copPtr + 1;
	hudCopDdfstopOperandPtr = copPtr + 3;
	copPtr = copSetFetch(copPtr, SCREEN_WIDTH, 0);
	copPtr = copSetBplcon1(copPtr, 0, 0);
	copPtr = copSetModulo(copPtr, SCREEN_ROW_BYTES, SCREEN_WIDTH);
	/* Growing HUD_HEIGHT from 32 to 88 lines (Sprint 14.91.2) quadrupled the
	 * HUD split's bitplane-fetch DMA cost, stealing enough extra cycles
	 * from the CPU every frame to make world scrolling stutter/burst. The
	 * HUD only ever uses pens 0/1/5/6/9 (HUD_COLOR_*), which fit in 4
	 * bitplanes - drop BPLCON0 to HUD_PLANES here (only for this split;
	 * next frame's copper list re-sets it to SCREEN_PLANES at the top) to
	 * cut that added DMA cost by 20% with no colour loss. The HUD buffer
	 * itself stays allocated/drawn at the full SCREEN_PLANES - only the
	 * unused 5th plane's data is simply never fetched/displayed.
	 *
	 * BPL5PT was previously also explicitly repointed here on a theory that
	 * it was left stale from the world section - tested and it made no
	 * difference (see git history), so reverted to only writing the
	 * HUD_PLANES(4) pointers actually active per BPLCON0 - this also saves
	 * 2 MOVE instructions (~8 colour clocks) in an already-tight transition
	 * window, see the WAIT comment above. */
	*copPtr++ = offsetof(struct Custom, bplcon0);
	hudCopBplcon0OperandPtr = copPtr;
	*copPtr++ = (HUD_PLANES << 12) | (1 << 9);
	for (int plane = 0; plane < HUD_PLANES; plane++)
		planes[plane] = hud + SCREEN_ROW_BYTES * plane;
	copPtr = copSetPlanesEx(0, copPtr, planes, HUD_PLANES, COPPER_TRACK_HUD);

	/* Sea colour for the instrument panel band - deliberately placed here
	 * rather than alongside the sky colour right after the WAIT above: that
	 * transition's timing budget is already tight (see the comment on it),
	 * so any register write added there risks reintroducing the Sprint
	 * 14.91.4 HUD "ghost" bug. By the time the HUD's own bitplane pointers
	 * are set (just above), the tight transition is fully complete and
	 * there's no further timing pressure before end of frame. GAME_COLOR_SEA
	 * isn't one of the pens (0/1/5/6/9) the HUD's own graphics use, so this
	 * has no visible effect today - kept only for parity with the reference
	 * table. GAME_COLOR_LAND is NOT overridden here (unlike the reference
	 * table's instrument-panel value) - it doubles as HUD_COLOR_SAFE (the
	 * FUEL/LIVES "ok" green), and retinting it away from green broke that
	 * distinction from HUD_COLOR_VALUE's yellow. COLOR10 (black) isn't
	 * written here either - identical to the bulk-loaded value at every
	 * band, never actually changes. */
	activeCopperPanelSeaColor = (UWORD*)(copPtr + 1);
	copPtr = copSetColor(copPtr, GAME_COLOR_SEA, currentPanelSeaRgb);

	*copPtr++ = 0xffff;
	*copPtr++ = 0xfffe;
}

static void fillScreen(UBYTE* bitmap, UBYTE color) {
	for (int y = 0; y < SCREEN_HEIGHT; y++) {
		UBYTE* row = bitmap + y * SCREEN_PLANES * SCREEN_ROW_BYTES;
		for (int plane = 0; plane < SCREEN_PLANES; plane++)
			memset(row + plane * SCREEN_ROW_BYTES, (color & (1 << plane)) ? 0xff : 0x00, SCREEN_ROW_BYTES);
	}
}

static void putPixel(UBYTE* bitmap, short x, short y, UBYTE color) {
	if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
		return;

	UBYTE bit = 0x80 >> (x & 7);
	UBYTE mask = (UBYTE)~bit;
	UBYTE* row = bitmap + y * SCREEN_PLANES * SCREEN_ROW_BYTES + (x >> 3);
	for (int plane = 0; plane < SCREEN_PLANES; plane++) {
		UBYTE* target = row + plane * SCREEN_ROW_BYTES;
		if (color & (1 << plane))
			*target |= bit;
		else
			*target &= mask;
	}
}

static void fillRect(UBYTE* bitmap, short x, short y, short width, short height, UBYTE color) {
	if (width <= 0 || height <= 0)
		return;

	if (((x | width) & 7) == 0) {
		short byteX = x >> 3;
		short byteWidth = width >> 3;
		for (short py = 0; py < height; py++) {
			UBYTE* row = bitmap + (y + py) * SCREEN_PLANES * SCREEN_ROW_BYTES + byteX;
			for (short plane = 0; plane < SCREEN_PLANES; plane++) {
				UBYTE fill = (color & (1 << plane)) ? 0xff : 0x00;
				memset(row + plane * SCREEN_ROW_BYTES, fill, byteWidth);
			}
		}
		return;
	}

	for (short py = 0; py < height; py++) {
		for (short px = 0; px < width; px++)
			putPixel(bitmap, x + px, y + py, color);
	}
}

static void putPixelScroll(UBYTE* bitmap, short x, short y, UBYTE color) {
	if (x < 0 || x >= GAME_WORLD_BUFFER_WIDTH || y < 0 || y >= GAME_WORLD_HEIGHT)
		return;

	UBYTE bit = 0x80 >> (x & 7);
	UBYTE mask = (UBYTE)~bit;
	UBYTE* row = bitmap + y * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + (x >> 3);
	for (int plane = 0; plane < SCREEN_PLANES; plane++) {
		UBYTE* target = row + plane * GAME_WORLD_ROW_BYTES;
		if (color & (1 << plane))
			*target |= bit;
		else
			*target &= mask;
	}
}

static void fillRectScroll(UBYTE* bitmap, short x, short y, short width, short height, UBYTE color) {
	if (width <= 0 || height <= 0)
		return;

	if (x < 0) {
		width += x;
		x = 0;
	}
	if (y < 0) {
		height += y;
		y = 0;
	}
	if (x + width > GAME_WORLD_BUFFER_WIDTH)
		width = GAME_WORLD_BUFFER_WIDTH - x;
	if (y + height > GAME_WORLD_HEIGHT)
		height = GAME_WORLD_HEIGHT - y;
	if (width <= 0 || height <= 0)
		return;

	if (((x | width) & 7) == 0) {
		short byteX = x >> 3;
		short byteWidth = width >> 3;
		for (short py = 0; py < height; py++) {
			UBYTE* row = bitmap + (y + py) * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + byteX;
			for (short plane = 0; plane < SCREEN_PLANES; plane++) {
				UBYTE fill = (color & (1 << plane)) ? 0xff : 0x00;
				memset(row + plane * GAME_WORLD_ROW_BYTES, fill, byteWidth);
			}
		}
		return;
	}

	for (short py = 0; py < height; py++) {
		for (short px = 0; px < width; px++)
			putPixelScroll(bitmap, x + px, y + py, color);
	}
}

static short glyphHasPixels(const UBYTE* glyph) {
	for (short row = 0; row < FONT_HEIGHT; row++) {
		if (glyph[row])
			return 1;
	}
	return 0;
}

static const UBYTE* getMenuGlyph(char ch) {
	unsigned char value = (unsigned char)ch;
	if (value >= 'a' && value <= 'z')
		value -= ('a' - 'A');

	/* Prefer the original CPC Mode 1 font exported as an 8x8 1bpp mask. */
	if (value >= FONT_FIRST_CHAR && value < FONT_LAST_CHAR) {
		const UBYTE* glyph = cpcFont8x8 + (value - FONT_FIRST_CHAR) * FONT_HEIGHT;
		if (value == ' ' || glyphHasPixels(glyph))
			return glyph;
	}

	/* Keep the hand-authored font as a fallback for absent CPC glyphs. */
	if (value >= 32 && value < 96) {
		const UBYTE* glyph = &menuFont8x8[value - 32][0];
		if (value == ' ' || glyphHasPixels(glyph))
			return glyph;
	}

	return &menuFont8x8[0][0];
}

static void drawChar(UBYTE* bitmap, short x, short y, char ch, UBYTE color) {
	const UBYTE* glyph = getMenuGlyph(ch);
	for (short row = 0; row < FONT_HEIGHT; row++) {
		UBYTE bits = glyph[row];
		for (short col = 0; col < FONT_WIDTH; col++) {
			if (bits & (0x80 >> col))
				putPixel(bitmap, x + col, y + row, color);
		}
	}
}

typedef enum FontStyle {
	FONT_STYLE_PLAIN,
	FONT_STYLE_CPC_GREEN,
	FONT_STYLE_CPC_BLUE,
	FONT_STYLE_CPC_HUD
} FontStyle;

static UBYTE cpcFontRowColor(FontStyle style, short row, UBYTE plainColor) {
	if (style == FONT_STYLE_PLAIN)
		return plainColor;

	if (style == FONT_STYLE_CPC_GREEN)
		return row < 2 ? CPC_FONT_GREEN_TOP : (row < 5 ? CPC_FONT_GREEN_MIDDLE : CPC_FONT_GREEN_BOTTOM);
	if (style == FONT_STYLE_CPC_BLUE)
		return row < 2 ? CPC_FONT_BLUE_TOP : (row < 5 ? CPC_FONT_BLUE_MIDDLE : CPC_FONT_BLUE_BOTTOM);
	return row < 2 ? CPC_FONT_HUD_TOP : (row < 5 ? CPC_FONT_HUD_MIDDLE : CPC_FONT_HUD_BOTTOM);
}

static void drawCharStyled(UBYTE* bitmap, short x, short y, char ch, FontStyle style, UBYTE plainColor) {
	const UBYTE* glyph = getMenuGlyph(ch);
	for (short row = 0; row < FONT_HEIGHT; row++) {
		UBYTE bits = glyph[row];
		UBYTE color = cpcFontRowColor(style, row, plainColor);
		for (short col = 0; col < FONT_WIDTH; col++) {
			if (bits & (0x80 >> col))
				putPixel(bitmap, x + col, y + row, color);
		}
	}
}

static void drawText(UBYTE* bitmap, short x, short y, const char* text, UBYTE color) {
	while (*text) {
		drawChar(bitmap, x, y, *text, color);
		x += FONT_WIDTH;
		text++;
	}
}

static void drawTextStyled(UBYTE* bitmap, short x, short y, const char* text, FontStyle style) {
	while (*text) {
		drawCharStyled(bitmap, x, y, *text, style, 0);
		x += FONT_WIDTH;
		text++;
	}
}

static void drawTextCentered(UBYTE* bitmap, short y, const char* text, UBYTE color) {
	short width = (short)(strlen(text) * FONT_WIDTH);
	short x = (SCREEN_WIDTH - width) / 2;
	drawText(bitmap, x, y, text, color);
}

static void drawTextCenteredStyled(UBYTE* bitmap, short y, const char* text, FontStyle style) {
	short width = (short)(strlen(text) * FONT_WIDTH);
	short x = (SCREEN_WIDTH - width) / 2;
	drawTextStyled(bitmap, x, y, text, style);
}

static const UBYTE telemetryFont4x5Digits[10][TELEMETRY_FONT_HEIGHT] = {
	{ 0x6, 0x9, 0x9, 0x9, 0x6 },
	{ 0x2, 0x6, 0x2, 0x2, 0x7 },
	{ 0x6, 0x9, 0x2, 0x4, 0xf },
	{ 0xe, 0x1, 0x6, 0x1, 0xe },
	{ 0x9, 0x9, 0xf, 0x1, 0x1 },
	{ 0xf, 0x8, 0xe, 0x1, 0xe },
	{ 0x7, 0x8, 0xe, 0x9, 0x6 },
	{ 0xf, 0x1, 0x2, 0x4, 0x4 },
	{ 0x6, 0x9, 0x6, 0x9, 0x6 },
	{ 0x6, 0x9, 0x7, 0x1, 0xe }
};

static const UBYTE telemetryFont4x5Letters[26][TELEMETRY_FONT_HEIGHT] = {
	{ 0x6, 0x9, 0xf, 0x9, 0x9 }, // A
	{ 0xe, 0x9, 0xe, 0x9, 0xe }, // B
	{ 0x7, 0x8, 0x8, 0x8, 0x7 }, // C
	{ 0xe, 0x9, 0x9, 0x9, 0xe }, // D
	{ 0xf, 0x8, 0xe, 0x8, 0xf }, // E
	{ 0xf, 0x8, 0xe, 0x8, 0x8 }, // F
	{ 0x7, 0x8, 0xb, 0x9, 0x7 }, // G
	{ 0x9, 0x9, 0xf, 0x9, 0x9 }, // H
	{ 0x7, 0x2, 0x2, 0x2, 0x7 }, // I
	{ 0x1, 0x1, 0x1, 0x9, 0x6 }, // J
	{ 0x9, 0xa, 0xc, 0xa, 0x9 }, // K
	{ 0x8, 0x8, 0x8, 0x8, 0xf }, // L
	{ 0x9, 0xf, 0xf, 0x9, 0x9 }, // M
	{ 0x9, 0xd, 0xb, 0x9, 0x9 }, // N
	{ 0x6, 0x9, 0x9, 0x9, 0x6 }, // O
	{ 0xe, 0x9, 0xe, 0x8, 0x8 }, // P
	{ 0x6, 0x9, 0x9, 0xb, 0x7 }, // Q
	{ 0xe, 0x9, 0xe, 0xa, 0x9 }, // R
	{ 0x7, 0x8, 0x6, 0x1, 0xe }, // S
	{ 0xf, 0x2, 0x2, 0x2, 0x2 }, // T
	{ 0x9, 0x9, 0x9, 0x9, 0x6 }, // U
	{ 0x9, 0x9, 0x9, 0x6, 0x6 }, // V
	{ 0x9, 0x9, 0xf, 0xf, 0x9 }, // W
	{ 0x9, 0x9, 0x6, 0x9, 0x9 }, // X
	{ 0x9, 0x9, 0x6, 0x2, 0x2 }, // Y
	{ 0xf, 0x1, 0x6, 0x8, 0xf }  // Z
};

static const UBYTE* telemetryGlyph4x5(char ch) {
	static const UBYTE glyphSpace[TELEMETRY_FONT_HEIGHT] = { 0, 0, 0, 0, 0 };
	static const UBYTE glyphDash[TELEMETRY_FONT_HEIGHT] = { 0, 0, 0xf, 0, 0 };
	static const UBYTE glyphAt[TELEMETRY_FONT_HEIGHT] = { 0x6, 0xb, 0xb, 0x8, 0x7 };
	static const UBYTE glyphSlash[TELEMETRY_FONT_HEIGHT] = { 0x1, 0x2, 0x4, 0x8, 0x8 };
	static const UBYTE glyphColon[TELEMETRY_FONT_HEIGHT] = { 0, 0x2, 0, 0x2, 0 };
	static const UBYTE glyphDot[TELEMETRY_FONT_HEIGHT] = { 0, 0, 0, 0, 0x2 };
	static const UBYTE glyphQuestion[TELEMETRY_FONT_HEIGHT] = { 0x6, 0x9, 0x2, 0, 0x2 };
	if (ch >= '0' && ch <= '9')
		return telemetryFont4x5Digits[ch - '0'];
	if (ch >= 'A' && ch <= 'Z')
		return telemetryFont4x5Letters[ch - 'A'];
	if (ch >= 'a' && ch <= 'z')
		return telemetryFont4x5Letters[ch - 'a'];
	if (ch == '-')
		return glyphDash;
	if (ch == '@')
		return glyphAt;
	if (ch == '/')
		return glyphSlash;
	if (ch == ':')
		return glyphColon;
	if (ch == '.')
		return glyphDot;
	if (ch == ' ')
		return glyphSpace;
	return glyphQuestion;
}

static void drawTelemetryChar(UBYTE* bitmap, short x, short y, char ch, UBYTE color) {
	const UBYTE* glyph = telemetryGlyph4x5(ch);
	for (short row = 0; row < TELEMETRY_FONT_HEIGHT; row++) {
		UBYTE bits = glyph[row];
		for (short col = 0; col < TELEMETRY_FONT_WIDTH; col++) {
			if (bits & (0x8 >> col))
				putPixel(bitmap, (short)(x + col), (short)(y + row), color);
		}
	}
}

static void drawTelemetryText(UBYTE* bitmap, short x, short y, const char* text, UBYTE color) {
	while (*text) {
		drawTelemetryChar(bitmap, x, y, *text, color);
		x += TELEMETRY_FONT_WIDTH + 1;
		text++;
	}
}

static void drawTelemetryTextCentered(UBYTE* bitmap, short y, const char* text, UBYTE color) {
	short width = (short)(strlen(text) * (TELEMETRY_FONT_WIDTH + 1));
	short x = (SCREEN_WIDTH - width) / 2;
	drawTelemetryText(bitmap, x, y, text, color);
}

static void drawTelemetryUnsignedPadded(UBYTE* bitmap, short x, short y, ULONG value, UBYTE digits, UBYTE color) {
	char text[12];
	text[digits] = 0;
	for (short i = digits - 1; i >= 0; i--) {
		text[i] = (char)('0' + (value % 10));
		value /= 10;
	}
	drawTelemetryText(bitmap, x, y, text, color);
}

static void drawMenuNotice(UBYTE* bitmap, const char* text, UBYTE color) {
	fillRect(bitmap, 44, 18, 232, 9, MENU_COLOR_PANEL);
	if (text && *text)
		drawTextCentered(bitmap, 18, text, color);
}

static void putMenuTickerPixel(UBYTE* bitmap, WORD x, WORD y,
	UBYTE color) {
	if (x < 0 || x >= MENU_TICKER_SOURCE_WIDTH ||
		y < 0 || y >= FONT_HEIGHT)
		return;
	UBYTE mask = (UBYTE)(0x80 >> (x & 7));
	UBYTE* row = bitmap +
		y * SCREEN_PLANES * MENU_TICKER_ROW_BYTES + (x >> 3);
	for (WORD plane = 0; plane < SCREEN_PLANES; plane++) {
		UBYTE* pixelByte = row + plane * MENU_TICKER_ROW_BYTES;
		if (color & (1 << plane))
			*pixelByte |= mask;
		else
			*pixelByte &= (UBYTE)~mask;
	}
}

static void drawMenuTickerText(UBYTE* bitmap, WORD x, const char* text) {
	while (*text) {
		const UBYTE* glyph = getMenuGlyph(*text++);
		for (WORD row = 0; row < FONT_HEIGHT; row++) {
			UBYTE bits = glyph[row];
			UBYTE color = cpcFontRowColor(FONT_STYLE_CPC_GREEN, row, 0);
			for (WORD col = 0; col < FONT_WIDTH; col++) {
				if (bits & (0x80 >> col))
					putMenuTickerPixel(bitmap, x + col, row, color);
			}
		}
		x += FONT_WIDTH;
	}
}

static void initMenuTicker(UBYTE* bitmap) {
	WORD cycleWidth = (WORD)(strlen(menuTickerText) * FONT_WIDTH +
		MENU_TICKER_GAP_PIXELS);
	memset(bitmap, 0, MENU_TICKER_BITMAP_BYTES);
	drawMenuTickerText(bitmap, MENU_TICKER_MARGIN_PIXELS,
		menuTickerText);
	drawMenuTickerText(bitmap, MENU_TICKER_MARGIN_PIXELS + cycleWidth,
		menuTickerText);
	menuTickerScrollX = 0;
	menuTickerFrameDivider = 0;
}

static void drawMenuTicker(UBYTE* bitmap) {
	/* Copper supplies this band from menuTickerBitmap. */
	fillRect(bitmap, 0, MENU_TICKER_Y, SCREEN_WIDTH, FONT_HEIGHT,
		MENU_COLOR_PANEL);
}

static void updateMenuTicker(void) {
	menuTickerFrameDivider++;
	if (menuTickerFrameDivider < MENU_TICKER_FRAME_DIVIDER)
		return;
	menuTickerFrameDivider = 0;
	UWORD cycleWidth = (UWORD)(strlen(menuTickerText) * FONT_WIDTH +
		MENU_TICKER_GAP_PIXELS);
	UWORD fine;
	LONG pointerPixelX;
	USHORT byteOffset;
	UWORD delay;

	menuTickerScrollX++;
	if (menuTickerScrollX >= cycleWidth)
		menuTickerScrollX = 0;
	fine = menuTickerScrollX & 15;
	pointerPixelX = fine == 0
		? (LONG)menuTickerScrollX - 16
		: (LONG)(menuTickerScrollX - fine);
	pointerPixelX += MENU_TICKER_MARGIN_PIXELS;
	if (pointerPixelX < 0)
		pointerPixelX = 0;
	byteOffset = (USHORT)(pointerPixelX >> 3);
	delay = fine == 0 ? 0 : 16 - fine;

	if (activeMenuTickerBplcon1)
		*activeMenuTickerBplcon1 =
			horizontalScrollDelayToBplcon1(delay);
	for (WORD plane = 0; plane < SCREEN_PLANES; plane++) {
		ULONG addr = (ULONG)(menuTickerBitmap + byteOffset +
			MENU_TICKER_ROW_BYTES * plane);
		if (activeMenuTickerPlaneHigh[plane])
			*activeMenuTickerPlaneHigh[plane] = (UWORD)(addr >> 16);
		if (activeMenuTickerPlaneLow[plane])
			*activeMenuTickerPlaneLow[plane] = (UWORD)addr;
	}
}

static char hexDigit(UBYTE value) {
	value &= 0x0f;
	return value < 10 ? (char)('0' + value) : (char)('A' + value - 10);
}

static void drawInputFlag(UBYTE* bitmap, short x, short y, const char* label, UBYTE active) {
	drawText(bitmap, x, y, label, active ? MENU_COLOR_YELLOW : MENU_COLOR_SHADOW);
}

static void drawInputDebug(UBYTE* bitmap, const InputState* input, short y, UBYTE background) {
	char rawText[8] = { 'R', 'A', 'W', ':', '-', '-', 0, 0 };
	if (input->lastRawKey != 0xff) {
		rawText[4] = hexDigit(input->lastRawKey >> 4);
		rawText[5] = hexDigit(input->lastRawKey);
	}

	fillRect(bitmap, 48, y - 2, 248, 12, background);
	drawText(bitmap, 56, y, "INPUT:", MENU_COLOR_WHITE);
	drawInputFlag(bitmap, 112, y, "U", input->up);
	drawInputFlag(bitmap, 128, y, "D", input->down);
	drawInputFlag(bitmap, 144, y, "L", input->left);
	drawInputFlag(bitmap, 160, y, "R", input->right);
	drawInputFlag(bitmap, 184, y, "F", input->fire);
	drawInputFlag(bitmap, 200, y, "B", input->bomb);
	drawInputFlag(bitmap, 216, y, "E", input->eject);
	drawText(bitmap, 240, y, rawText, MENU_COLOR_GREEN);
}

static void drawInputDebugIfEnabled(UBYTE* bitmap, const InputState* input, short y, UBYTE background) {
	if (HAR_DEBUG_INPUT_OVERLAY)
		drawInputDebug(bitmap, input, y, background);
	else {
		(void)bitmap;
		(void)input;
		(void)y;
		(void)background;
	}
}

static void drawUnsignedPadded(UBYTE* bitmap, short x, short y, ULONG value, UBYTE digits, UBYTE color) {
	char text[11];
	if (digits > 10)
		digits = 10;
	text[digits] = 0;

	for (short index = digits - 1; index >= 0; index--) {
		text[index] = (char)('0' + (value % 10));
		value /= 10;
	}
	drawText(bitmap, x, y, text, color);
}

static void drawUnsignedPaddedStyled(UBYTE* bitmap, short x, short y, ULONG value, UBYTE digits, FontStyle style) {
	char text[11];
	if (digits > 10)
		digits = 10;
	text[digits] = 0;
	for (short index = digits - 1; index >= 0; index--) {
		text[index] = (char)('0' + (value % 10));
		value /= 10;
	}
	drawTextStyled(bitmap, x, y, text, style);
}

static short menuItemY(short item) {
	static const short itemY[MENU_ITEM_COUNT] = { 116, 128, 140, 152 };
	return itemY[item];
}

static void copyMenuText(char* dest, const char* src) {
	while (*src)
		*dest++ = *src++;
	*dest = 0;
}

static void menuItemText(short item, short skillLevel, short livesSetting, short wingmanControl, char* text) {
	switch (item) {
		case MENU_ITEM_START:
			copyMenuText(text, HAR_TEXT_START_GAME);
			break;
		case MENU_ITEM_SKILL:
			copyMenuText(text, "Skill level: 1");
			text[13] = (char)('0' + skillLevel);
			break;
		case MENU_ITEM_LIVES:
			copyMenuText(text, "Lives: 1");
			text[7] = (char)('0' + livesSetting);
			break;
		case MENU_ITEM_WINGMAN:
			switch (wingmanControl) {
				case WINGMAN_CONTROL_CPU:
					copyMenuText(text, "Wingman: CPU");
					break;
				case WINGMAN_CONTROL_PLAYER2:
					copyMenuText(text, "Wingman: P2");
					break;
				default:
					copyMenuText(text, "Wingman: Off");
					break;
			}
			break;
		default:
			text[0] = 0;
			break;
	}
}

static void drawMenuOption(UBYTE* bitmap, short selected, short y, const char* text) {
	fillRect(bitmap, 34 + MENU_CONTENT_X_OFFSET, y - 1, 142, 10, MENU_COLOR_PANEL);
	if (selected)
		drawTextStyled(bitmap, 42 + MENU_CONTENT_X_OFFSET, y, ">", FONT_STYLE_CPC_BLUE);
	drawTextStyled(bitmap, 58 + MENU_CONTENT_X_OFFSET, y, text, FONT_STYLE_CPC_BLUE);
}

static void drawMenuItem(UBYTE* bitmap, short item, short selected, short skillLevel, short livesSetting, short wingmanControl) {
	char text[24];
	menuItemText(item, skillLevel, livesSetting, wingmanControl, text);
	drawMenuOption(bitmap, selected, menuItemY(item), text);
}

static void drawMenuItems(UBYTE* bitmap, short selected, short skillLevel, short livesSetting, short wingmanControl) {
	for (short item = 0; item < MENU_ITEM_COUNT; item++)
		drawMenuItem(bitmap, item, selected == item, skillLevel, livesSetting, wingmanControl);
}

static void drawMenuCursor(UBYTE* bitmap, short item, UBYTE visible) {
	short x = 42 + MENU_CONTENT_X_OFFSET;
	short y = menuItemY(item);
	/* The menu text never changes when selection moves. Touch only the
	 * cursor's single 8x8 character cell, avoiding two row clears plus two
	 * complete font redraws on every up/down press. */
	fillRect(bitmap, x, y, FONT_WIDTH, FONT_HEIGHT, MENU_COLOR_PANEL);
	if (visible)
		drawTextStyled(bitmap, x, y, ">", FONT_STYLE_CPC_BLUE);
}

/* Menu review: these read as selectable settings (same MENU_COLOR_CYAN as
 * an unselected left-column menu item) despite none of them being
 * interactive - only "Lock height" reflects something the game actually
 * does (rocketShot tracking the player's Y, see updateWeapons()); the rest
 * are just informational text. MENU_COLOR_SHADOW is this menu's existing
 * "dim/inactive" colour (see the debug-overlay label above) - reused here
 * so this column reads as status at a glance instead of unimplemented
 * options. "Controls: Off" (previously another non-functional text-only
 * token, same issue as the old Input toggle) replaced with "Input: All",
 * matching what ReadInput() actually does - always read every source at
 * once. The former 4th line here ("Wingman: Off") is gone - Sprint 15.1
 * turned it into the real selectable MENU_ITEM_WINGMAN in the left column
 * instead of leaving it as dead status text. */
static void drawMenuRightSettings(UBYTE* bitmap) {
	drawTextStyled(bitmap, 184 + MENU_CONTENT_X_OFFSET, 116, "Rocket range: 10", FONT_STYLE_CPC_BLUE);
	drawTextStyled(bitmap, 184 + MENU_CONTENT_X_OFFSET, 128, "Input: All", FONT_STYLE_CPC_BLUE);
	drawTextStyled(bitmap, 184 + MENU_CONTENT_X_OFFSET, 140, "Maverick: L+Fire", FONT_STYLE_CPC_BLUE);
}


#define HIGH_SCORE_ENTRY_COUNT 7
#define HIGH_SCORE_NAME_LENGTH 7
#define HIGH_SCORE_SAVE_PATH "PROGDIR:harrier_scores.dat"

typedef struct HighScoreEntry {
	char name[HIGH_SCORE_NAME_LENGTH];
	UBYTE level;
	UWORD hits;
	ULONG score;
} HighScoreEntry;

static HighScoreEntry highScoreTable[HIGH_SCORE_ENTRY_COUNT];

/* Menu review: LEVEL/HITS previously always displayed as 0, and only row 0
 * ever showed a real score - the other 6 rows were permanently the same
 * placeholder names/100-point score. Defaults here match what a fresh
 * install looked like before (same names, same 100 placeholder score) so
 * nothing changes visually until a real run actually beats one. */
static void resetHighScoreTableToDefaults(void) {
	static const char* const defaultNames[HIGH_SCORE_ENTRY_COUNT] = {
		"CPSOFT", "AMSOFT", "DURELL", "CPSOFT", "AMSOFT", "DURELL", "CPSOFT"
	};
	for (UBYTE i = 0; i < HIGH_SCORE_ENTRY_COUNT; i++) {
		copyMenuText(highScoreTable[i].name, defaultNames[i]);
		highScoreTable[i].level = 0;
		highScoreTable[i].hits = 0;
		highScoreTable[i].score = 100;
	}
}

/* AmigaDOS pops a blocking "Please insert volume X in any drive" system
 * requester from Open() whenever the target path's volume can't be resolved
 * (confirmed live - PROGDIR: doesn't resolve in the headless test harness,
 * which doesn't launch the program through a normal AmigaDOS process
 * that would have that assign set up, and froze the whole run waiting on a
 * dialog no automated test can click). pr_WindowPtr=-1 is the standard
 * AmigaOS way to suppress that class of requester for the current process -
 * Open() then just fails and returns 0 instead, which the callers below
 * already handle. Scoped tightly around just the Open() call and restored
 * immediately after, so it doesn't suppress unrelated requesters elsewhere
 * in the program. */
static APTR suppressDosRequesters(void) {
	struct Process* thisProcess = (struct Process*)SysBase->ThisTask;
	APTR oldWindowPtr = thisProcess->pr_WindowPtr;
	thisProcess->pr_WindowPtr = (APTR)-1L;
	return oldWindowPtr;
}

static void restoreDosRequesters(APTR oldWindowPtr) {
	struct Process* thisProcess = (struct Process*)SysBase->ThisTask;
	thisProcess->pr_WindowPtr = oldWindowPtr;
}

static void saveHighScoreTable(void) {
	APTR oldWindowPtr = suppressDosRequesters();
	BPTR file = Open((CONST_STRPTR)HIGH_SCORE_SAVE_PATH, MODE_NEWFILE);
	if (file) {
		Write(file, (APTR)highScoreTable, sizeof(highScoreTable));
		Close(file);
	}
	restoreDosRequesters(oldWindowPtr);
}

/* Loaded once at program startup (see main()). A missing or short-read file
 * (first run, or a save from a differently-sized table) falls back to the
 * same defaults resetHighScoreTableToDefaults() always starts from. */
static void loadHighScoreTable(void) {
	resetHighScoreTableToDefaults();
	APTR oldWindowPtr = suppressDosRequesters();
	BPTR file = Open((CONST_STRPTR)HIGH_SCORE_SAVE_PATH, MODE_OLDFILE);
	UBYTE loaded = 0;
	if (file) {
		LONG bytesRead = Read(file, (APTR)highScoreTable, sizeof(highScoreTable));
		Close(file);
		loaded = bytesRead == (LONG)sizeof(highScoreTable);
	}
	restoreDosRequesters(oldWindowPtr);
	if (!loaded)
		resetHighScoreTableToDefaults();
}

static void drawMenuHighScore(UBYTE* bitmap) {
	drawTextStyled(bitmap, 40 + MENU_CONTENT_X_OFFSET, 44, "NAME", FONT_STYLE_CPC_GREEN);
	drawTextStyled(bitmap, 128 + MENU_CONTENT_X_OFFSET, 44, "LEVEL", FONT_STYLE_CPC_GREEN);
	drawTextStyled(bitmap, 212 + MENU_CONTENT_X_OFFSET, 44, "HITS", FONT_STYLE_CPC_GREEN);
	drawTextStyled(bitmap, 268 + MENU_CONTENT_X_OFFSET, 44, "SCORE", FONT_STYLE_CPC_GREEN);

	for (short row = 0; row < HIGH_SCORE_ENTRY_COUNT; row++) {
		short y = (short)(58 + row * 8);
		drawTextStyled(bitmap, 40 + MENU_CONTENT_X_OFFSET, y, highScoreTable[row].name, FONT_STYLE_CPC_GREEN);
		drawUnsignedPaddedStyled(bitmap, 136 + MENU_CONTENT_X_OFFSET, y, highScoreTable[row].level, 2, FONT_STYLE_CPC_GREEN);
		drawUnsignedPaddedStyled(bitmap, 212 + MENU_CONTENT_X_OFFSET, y, highScoreTable[row].hits, 5, FONT_STYLE_CPC_GREEN);
		drawUnsignedPaddedStyled(bitmap, 268 + MENU_CONTENT_X_OFFSET, y, highScoreTable[row].score, 6, FONT_STYLE_CPC_GREEN);
	}
}

static void initGameState(GameState* game) {
	resetDestroyedTargets();
	resetCpcRandomSequence();
	resetRuntimeFlak();
	resetTargetLock();
	resetDestroyedShipColumns();
	resetLandCraters();
	resetCityFade(game);
	resetPowerup(game);
	game->scrollX = 0;
	game->playerX = PLAYER_START_X;
	game->playerY = PLAYER_START_Y;
	game->speedLevel = GAME_SPEED_LEVEL_DEFAULT;
	game->score = 0;
	game->bonusScore = 0;
	game->hitsCount = 0;
	game->targetLock.active = 0;
	game->targetLock.worldX = 0;
	game->targetLock.y = 0;
	game->targetLock.targetType = 0; /* matches CPC_LAND_TARGET_NONE, defined later in the file */
	game->fuel = 999;
	game->armour = 100;
	game->gameOver = 0;
	game->missionComplete = 0;
	game->missionCompleteTimer = 0;
	game->postLandingSlide = 0;
	game->landingState = LANDING_STATE_NONE;
	game->takeoffState = TAKEOFF_STATE_AIRBORNE;
	game->lives = PLAYER_START_LIVES;
	game->respawnSafeTimer = 0;
	game->flakDamageCount = 0;
	game->playerFrigateStatus = PLAYER_FRIGATE_STATUS_CLEAR;
	game->rockets = 12;
	game->bombs = 6;
	memset(&game->rocketShot, 0, sizeof(game->rocketShot));
	memset(&game->bombShot, 0, sizeof(game->bombShot));
	memset(&game->impact, 0, sizeof(game->impact));
	memset(&game->enemyPlane, 0, sizeof(game->enemyPlane));
	memset(&game->enemyMissile, 0, sizeof(game->enemyMissile));
	memset(game->crashPart, 0, sizeof(game->crashPart));
	memset(&game->wingman, 0, sizeof(game->wingman));
	game->wingman.lastBombTargetColumn = -1;
	game->enemyRespawnTimer = 0;
	game->enemySpawnIndex = 0;
	game->enemyTriggerIndex = 0;
	game->enemyShipMissileTriggerIndex = 0;
	game->enemyMissileFromShip = 0;
	game->enemyMissileTarget = ENEMY_TARGET_NONE;
	game->crashTimer = 0;
	game->ejectState = 0;
	game->ejectTimer = 0;
	game->ejectX = 0;
	game->ejectY = 0;
	game->throttleRepeatTimer = 0;
}

static WORD takeoffPlayerXForScroll(UWORD scrollX) {
	return (WORD)(TAKEOFF_PLAYER_DECK_X - (WORD)scrollX);
}

static void setTakeoffDeckPosition(GameState* game) {
	game->playerX = takeoffPlayerXForScroll(game->scrollX);
	game->playerY = TAKEOFF_PLAYER_DECK_Y;
}

static UBYTE updateHudValues(GameState* game) {
	ULONG oldScore = game->score;
	UWORD oldFuel = game->fuel;
	UWORD usedFuel = game->scrollX >> 6;

	/* Real CPC only awards score via explosionnoise() (HarrierAttackSourceNew2...
	 * asm:8250-8266), called exclusively on actual hits/kills - there is no
	 * distance/survival term anywhere in its scoring. game->bonusScore
	 * already accumulates only from hit events (see the *_SCORE_VALUE
	 * constants below); the previous `+ (scrollX >> 4)` term here awarded
	 * points for sheer distance flown, which doesn't exist on the real
	 * hardware and also meant the HUD redrew the score field constantly
	 * during normal flight instead of only on actual hits. */
	game->score = game->bonusScore;
	game->fuel = debugInfiniteFuel ? 999 :
		(usedFuel < 999 ? (UWORD)(999 - usedFuel) : 0);
	return oldScore != game->score || oldFuel != game->fuel;
}

/* LEVEL is the CPC's own gamelevelprogress-equivalent stage (HarLevelStage)
 * reached by the run's final scroll position - the world only ever scrolls
 * forward (no reverse-scroll mechanic), so scrollX at game-end is always
 * the furthest point reached; no separate "furthest position" tracking
 * needed. */
static UBYTE highScoreLevelForWorldColumn(LONG worldColumn) {
	const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
	return stageForWorldColumn(worldColumn, segment);
}

static UBYTE updateHighScore(ULONG* highScore, const GameState* game) {
	UBYTE changed = 0;
	if (game->score > *highScore) {
		*highScore = game->score;
		changed = 1;
	}

	UBYTE lowestIndex = 0;
	for (UBYTE i = 1; i < HIGH_SCORE_ENTRY_COUNT; i++) {
		if (highScoreTable[i].score < highScoreTable[lowestIndex].score)
			lowestIndex = i;
	}
	if (game->score > highScoreTable[lowestIndex].score) {
		/* No name-entry UI (out of scope for this pass) - real runs are
		 * simply tagged "PLAYER", displacing the CPSOFT/AMSOFT/DURELL
		 * placeholder rows one at a time as they're actually beaten. */
		copyMenuText(highScoreTable[lowestIndex].name, "PLAYER");
		highScoreTable[lowestIndex].level = highScoreLevelForWorldColumn((LONG)(game->scrollX >> 3));
		highScoreTable[lowestIndex].hits = game->hitsCount;
		highScoreTable[lowestIndex].score = game->score;

		/* Small fixed table, re-sorted once per game-end (not per frame) -
		 * a plain selection sort is simplest and plenty fast enough here. */
		for (UBYTE i = 0; i < HIGH_SCORE_ENTRY_COUNT - 1; i++) {
			UBYTE maxIndex = i;
			for (UBYTE j = (UBYTE)(i + 1); j < HIGH_SCORE_ENTRY_COUNT; j++) {
				if (highScoreTable[j].score > highScoreTable[maxIndex].score)
					maxIndex = j;
			}
			if (maxIndex != i) {
				HighScoreEntry temp = highScoreTable[i];
				highScoreTable[i] = highScoreTable[maxIndex];
				highScoreTable[maxIndex] = temp;
			}
		}

		saveHighScoreTable();
		changed = 1;
	}
	return changed;
}

static UBYTE scrollPixelsForSpeedLevel(UBYTE speedLevel) {
	if (speedLevel > GAME_SPEED_LEVEL_MAX)
		speedLevel = GAME_SPEED_LEVEL_MAX;
	if (speedLevel == 0)
		return 1;
	if (speedLevel < 5)
		return 2;
	if (speedLevel < 9)
		return 3;
	return 4;
}

static WORD playerTargetXForSpeedLevel(UBYTE speedLevel) {
	if (speedLevel > GAME_SPEED_LEVEL_MAX)
		speedLevel = GAME_SPEED_LEVEL_MAX;
	return (WORD)(PLAYER_SPEED_ANCHOR_X + (speedLevel * PLAYER_SPEED_ANCHOR_STEP_PIXELS));
}

static UBYTE updateThrottle(GameState* game, const InputState* input) {
	if (game->throttleRepeatTimer > 0)
		game->throttleRepeatTimer--;

	if (game->throttleRepeatTimer == 0) {
		if (input->right && !input->left && game->speedLevel < GAME_SPEED_LEVEL_MAX) {
			game->speedLevel++;
			game->throttleRepeatTimer = GAME_THROTTLE_REPEAT_FRAMES;
			return 1;
		}
		if (input->left && !input->right && game->speedLevel > GAME_SPEED_LEVEL_MIN) {
			game->speedLevel--;
			game->throttleRepeatTimer = GAME_THROTTLE_REPEAT_FRAMES;
			return 1;
		}
	}

	return 0;
}

/* CPC checkplayerspeed forces a decrement from gamelevelprogress 11 onward
 * while the final friendly carrier is intact. Once speed reaches zero,
 * gamelevelprogress 13 leaves the scrolling game loop for landinghoverloop.
 *
 * CPC scrolls the final carrier as separate hardware sprites while scenery
 * slows. The Amiga carrier is world-anchored, so begin at the point where it
 * has entered the screen (the existing 640-column landing threshold) rather
 * than when its first off-screen column is generated. */
static UBYTE updateLandingApproach(GameState* game) {
	if (game->missionComplete || game->gameOver || game->crashTimer)
		return 0;
	if (game->playerFrigateStatus != PLAYER_FRIGATE_STATUS_CLEAR)
		return 0;

	if (game->landingState == LANDING_STATE_NONE) {
		if (game->scrollX < LANDING_APPROACH_SCROLL_X)
			return 0;
		game->landingState = LANDING_STATE_SLOWING;
		game->throttleRepeatTimer = 0;
	}

	if (game->landingState != LANDING_STATE_SLOWING)
		return 0;

	if (game->throttleRepeatTimer > 0) {
		game->throttleRepeatTimer--;
		return 0;
	}

	if (game->speedLevel > 0) {
		game->speedLevel--;
		game->throttleRepeatTimer = LANDING_SLOWDOWN_REPEAT_FRAMES;
	}
	if (game->speedLevel == 0 && game->scrollX >= LANDING_HOVER_SCROLL_X) {
		game->landingState = LANDING_STATE_HOVER;
		/* CPC jumps out of the combat/scroll loop at state 13. Remove
		 * transient combat actors instead of letting them keep updating
		 * inside the Amiga hover phase. */
		game->rocketShot.active = 0;
		game->bombShot.active = 0;
		game->enemyPlane.active = 0;
		game->enemyMissile.active = 0;
		game->enemyMissileFromShip = 0;
		game->powerup.active = 0;
		if (game->wingman.active) {
			game->wingman.mode = WINGMAN_LANDING_APPROACH;
			game->wingman.interceptScreenX = (WORD)(game->playerX -
				WINGMAN_FORMATION_COLUMNS_BEHIND * GAME_TILE_WIDTH);
			game->wingman.landingTargetSet = 0;
			game->wingman.rocket.active = 0;
			game->wingman.bomb.active = 0;
		}
	}
	return 1;
}

/* Real CPC in-game HUD draws SPEED/FUEL/ROCKETS/BOMBS as tick-segmented
 * gauge bars (drawgauge, HarrierAttackSourceNew2_alt_CRTC_CART16.asm:5249-5261
 * - 15 "empty gauge" tiles plus one "marker" tile) and ARMOUR as a bar that
 * erases one segment per hit (updatehealth, :2963-2985), not plain numbers -
 * this Amiga port's gameplay HUD had been left as placeholder digit readouts
 * since Sprint 14.7.1 ("gauge-style in-game HUD parity... later dedicated
 * HUD sprint"). The menu screen already built a matching gauge-bar look
 * (drawMenuGaugeBar) as the CPC-style reference; reused here with a live
 * fill fraction instead of the menu's fixed/decorative full bar. SCORE and
 * LIV stay plain numbers - CPC's score is just a number too, and LIV (lives)
 * has no CPC equivalent at all (single-life game, see Sprint 14.91). */
static void drawHudGaugeBar(UBYTE* hud, short x, short y, short width, short height, UBYTE fillColor, UWORD value, UWORD maxValue) {
	short innerWidth = (short)(width - 2);
	short filled = maxValue ? (short)(((ULONG)value * innerWidth) / maxValue) : 0;
	if (filled > innerWidth)
		filled = innerWidth;
	if (filled < 0)
		filled = 0;

	fillRect(hud, x, y, width, height, HUD_COLOR_LABEL);
	fillRect(hud, x + 1, y + 1, innerWidth, (short)(height - 2), HUD_COLOR_BACKGROUND);
	if (filled > 0)
		fillRect(hud, x + 1, y + 1, filled, (short)(height - 2), fillColor);
	/* 12px spacing matches the menu's drawMenuGaugeBar (the established
	 * CPC-style reference) - an earlier 6px spacing here made the bar look
	 * like a busy hatched texture rather than a clean segmented gauge. */
	for (short tick = (short)(x + 6); tick < x + width - 2; tick = (short)(tick + 12))
		fillRect(hud, tick, (short)(y + 1), 1, (short)(height - 2), HUD_COLOR_BACKGROUND);
}

/* Delta-only version: redraws just the pixel span between the old and new
 * fill widths, not the whole bar every time. If fillColor itself changed
 * since the last draw (e.g. armour/fuel crossing into their WARN colour),
 * falls back to a full drawHudGaugeBar() - that only happens at rare
 * threshold crossings, not every frame. Returns 1 if anything was drawn. */
static UBYTE drawHudGaugeBarDelta(UBYTE* hud, short x, short y, short width, short height, UBYTE fillColor, UBYTE oldFillColor, UWORD oldValue, UWORD newValue, UWORD maxValue) {
	short innerWidth = (short)(width - 2);
	short oldFilled = maxValue ? (short)(((ULONG)oldValue * innerWidth) / maxValue) : 0;
	short newFilled = maxValue ? (short)(((ULONG)newValue * innerWidth) / maxValue) : 0;
	if (oldFilled > innerWidth)
		oldFilled = innerWidth;
	if (newFilled > innerWidth)
		newFilled = innerWidth;
	if (oldFilled < 0)
		oldFilled = 0;
	if (newFilled < 0)
		newFilled = 0;

	if (fillColor != oldFillColor) {
		drawHudGaugeBar(hud, x, y, width, height, fillColor, newValue, maxValue);
		return 1;
	}
	if (oldFilled == newFilled)
		return 0;

	short deltaStart = oldFilled < newFilled ? oldFilled : newFilled;
	short deltaEnd = oldFilled < newFilled ? newFilled : oldFilled;
	short deltaWidth = (short)(deltaEnd - deltaStart);
	UBYTE growing = (UBYTE)(newFilled > oldFilled);
	UBYTE deltaColor = growing ? fillColor : HUD_COLOR_BACKGROUND;

	fillRect(hud, (short)(x + 1 + deltaStart), (short)(y + 1), deltaWidth, (short)(height - 2), deltaColor);
	if (growing) {
		/* Restore the thin background tick gaps inside the newly-filled
		 * span, matching drawHudGaugeBar's segmented look (12px spacing). */
		for (short tick = (short)(x + 6); tick < x + width - 2; tick = (short)(tick + 12)) {
			short tickLocal = (short)(tick - (x + 1));
			if (tickLocal >= deltaStart && tickLocal < deltaEnd)
				fillRect(hud, tick, (short)(y + 1), 1, (short)(height - 2), HUD_COLOR_BACKGROUND);
		}
	}
	return 1;
}

/* Delta-only version of drawUnsignedPadded(): only blanks+redraws the
 * specific 8px-wide digit cells that actually differ between old and new,
 * not the whole padded field every time. */
static void drawUnsignedPaddedDelta(UBYTE* hud, short x, short y, ULONG oldValue, ULONG newValue, UBYTE digits, FontStyle style) {
	char oldText[11];
	char newText[11];
	ULONG oldTmp = oldValue;
	ULONG newTmp = newValue;
	if (digits > 10)
		digits = 10;
	oldText[digits] = 0;
	newText[digits] = 0;

	for (short index = (short)(digits - 1); index >= 0; index--) {
		oldText[index] = (char)('0' + (oldTmp % 10));
		oldTmp /= 10;
		newText[index] = (char)('0' + (newTmp % 10));
		newTmp /= 10;
	}
	for (UBYTE i = 0; i < digits; i++) {
		if (oldText[i] != newText[i]) {
			short cellX = (short)(x + i * FONT_WIDTH);
			char ch[2];
			ch[0] = newText[i];
			ch[1] = 0;
			fillRect(hud, cellX, y, FONT_WIDTH, FONT_HEIGHT, HUD_COLOR_BACKGROUND);
			drawTextStyled(hud, cellX, y, ch, style);
		}
	}
}

/* Full label-above-bar layout matching a WinAPE reference capture of the
 * real in-game HUD and the user's own ASCII mockup:
 *   SCORE ######   HIGH SCORE ######            (row 1, LIV - Amiga-only,
 *       ARMOUR  ---------------------------      no CPC equivalent - tucked
 *   SPEED             FUEL                       in at the end of row 1)
 *   OOOOOOOOOO        OOOOOOOOOO
 *   ROCKETS           BOMBS
 *   OOOOOOOOOO        OOOOOOOOOO
 * Needed HUD_HEIGHT growing from 32 to 88 (SCREEN_HEIGHT 200->256) to fit -
 * confirmed with the user this uses genuinely unused PAL scanline budget
 * (320x256 is a standard resolution) rather than shrinking HUD_TOP/the
 * gameplay viewport. */
static void drawHudStatic(UBYTE* hud) {
	fillRect(hud, 0, 0, SCREEN_WIDTH, HUD_HEIGHT, HUD_COLOR_BACKGROUND);
	fillRect(hud, 0, 0, SCREEN_WIDTH, 1, GAME_COLOR_WHITE);

	drawTextStyled(hud, 8, 4, "SCORE", FONT_STYLE_CPC_HUD);
	drawTextStyled(hud, 112, 4, "HIGH SCORE", FONT_STYLE_CPC_HUD);
	drawTextStyled(hud, 256, 4, "LIV", FONT_STYLE_CPC_HUD);

	drawTextStyled(hud, 8, 19, "ARMOUR", FONT_STYLE_CPC_HUD);

	drawTextStyled(hud, 66, 36, "SPEED", FONT_STYLE_CPC_HUD);
	drawTextStyled(hud, 218, 36, "FUEL", FONT_STYLE_CPC_HUD);

	drawTextStyled(hud, 58, 66, "ROCKETS", FONT_STYLE_CPC_HUD);
	drawTextStyled(hud, 214, 66, "BOMBS", FONT_STYLE_CPC_HUD);
}

/* Per-buffer tracked "last drawn" state, so drawHudValues() can diff against
 * what THIS specific physical HUD buffer actually still shows (the two
 * buffers alternate and are drawn on different frames, so a single shared
 * "last value" wouldn't match either one) and only touch pixels that
 * changed - see drawHudGaugeBarDelta()/drawUnsignedPaddedDelta() above. */
typedef struct HudRenderState {
	UBYTE valid;
	ULONG score;
	ULONG highScore;
	UWORD armour;
	UBYTE armourColor;
	UWORD speedLevel;
	UWORD fuel;
	UBYTE fuelColor;
	UWORD rockets;
	UWORD bombs;
	UBYTE lives;
	UBYTE livesColor;
	UBYTE overlayMode; /* 0 = none, 1 = game over, 2 = mission complete */
} HudRenderState;

/* +1 slot for the menu screen's own HUD render-state tracking (see
 * drawMenuDemoHud()) - a separate physical buffer/screen region from the
 * gameplay HUD, so it needs its own delta-tracking slot, not index 0. */
#define MENU_HUD_STATE_INDEX HUD_BUFFER_COUNT
static HudRenderState hudRenderState[HUD_BUFFER_COUNT + 1];

static void drawHudValues(UBYTE* hud, const GameState* game, ULONG highScore, UBYTE hudBufferIndex) {
	HudRenderState* state = &hudRenderState[hudBufferIndex];
	UBYTE fullBombs;
	UBYTE fullRockets;
	ammoForSkill(game->skillLevel, &fullBombs, &fullRockets);
	UBYTE armourColor = (UBYTE)(game->armour == 0 ? HUD_COLOR_WARN : HUD_COLOR_VALUE);
	UBYTE fuelColor = (UBYTE)(game->fuel < 100 ? HUD_COLOR_WARN :
		HUD_COLOR_POWERUP_HEALTH);
	UBYTE livesColor = (UBYTE)(game->lives == 0 ? HUD_COLOR_WARN : HUD_COLOR_SAFE);
	UBYTE overlayMode = (UBYTE)(game->gameOver ? 1 : (game->missionComplete ? 2 : 0));

	hudDrawCalls++;
	if (state->valid) {
		if (state->score != game->score)
			hudScoreChanges++;
		if (state->fuel != game->fuel)
			hudFuelChanges++;
		if (state->armour != game->armour)
			hudArmourChanges++;
		if (state->speedLevel != game->speedLevel)
			hudSpeedChanges++;
		if (state->rockets != game->rockets)
			hudRocketsChanges++;
		if (state->bombs != game->bombs)
			hudBombsChanges++;
	}

	if (!state->valid) {
		fillRect(hud, 56, 4, 48, 8, HUD_COLOR_BACKGROUND);
		drawUnsignedPaddedStyled(hud, 56, 4, game->score, 6, FONT_STYLE_CPC_HUD);
		fillRect(hud, 200, 4, 48, 8, HUD_COLOR_BACKGROUND);
		drawUnsignedPaddedStyled(hud, 200, 4, highScore, 6, FONT_STYLE_CPC_HUD);
		fillRect(hud, 284, 4, 16, 8, HUD_COLOR_BACKGROUND);
		drawUnsignedPadded(hud, 284, 4, game->lives, 2, livesColor);
		drawHudGaugeBar(hud, 64, 17, 232, 10, armourColor, game->armour, 100);
		drawHudGaugeBar(hud, 16, 50, 140, 10, HUD_COLOR_VALUE, game->speedLevel, GAME_SPEED_LEVEL_MAX);
		drawHudGaugeBar(hud, 164, 50, 140, 10, fuelColor, game->fuel, 999);
		drawHudGaugeBar(hud, 16, 80, 140, 8,
			HUD_COLOR_POWERUP_ROCKETS, game->rockets, fullRockets);
		drawHudGaugeBar(hud, 164, 80, 140, 8,
			HUD_COLOR_POWERUP_BOMBS, game->bombs, fullBombs);
	} else {
		if (state->score != game->score)
			drawUnsignedPaddedDelta(hud, 56, 4, state->score, game->score, 6, FONT_STYLE_CPC_HUD);
		if (state->highScore != highScore)
			drawUnsignedPaddedDelta(hud, 200, 4, state->highScore, highScore, 6, FONT_STYLE_CPC_HUD);
		if (state->lives != game->lives || state->livesColor != livesColor) {
			fillRect(hud, 284, 4, 16, 8, HUD_COLOR_BACKGROUND);
			drawUnsignedPadded(hud, 284, 4, game->lives, 2, livesColor);
		}
		drawHudGaugeBarDelta(hud, 64, 17, 232, 10, armourColor, state->armourColor, state->armour, game->armour, 100);
		/* SPEED/FUEL sit under the GAME OVER/LANDED overlay rect - skip
		 * updating them while it's showing, no point drawing what the
		 * overlay immediately covers; restored when the overlay clears. */
		if (overlayMode == 0) {
			drawHudGaugeBarDelta(hud, 16, 50, 140, 10, HUD_COLOR_VALUE, HUD_COLOR_VALUE, state->speedLevel, game->speedLevel, GAME_SPEED_LEVEL_MAX);
			drawHudGaugeBarDelta(hud, 164, 50, 140, 10, fuelColor, state->fuelColor, state->fuel, game->fuel, 999);
		}
		drawHudGaugeBarDelta(hud, 16, 80, 140, 8,
			HUD_COLOR_POWERUP_ROCKETS, HUD_COLOR_POWERUP_ROCKETS,
			state->rockets, game->rockets, fullRockets);
		drawHudGaugeBarDelta(hud, 164, 80, 140, 8,
			HUD_COLOR_POWERUP_BOMBS, HUD_COLOR_POWERUP_BOMBS,
			state->bombs, game->bombs, fullBombs);
	}

	if (state->overlayMode != overlayMode) {
		/* Height capped at 36 (not 38) so this clear never reaches row 66,
		 * where the ROCKETS/BOMBS labels start - a taller rect used to chop
		 * the top 2 pixel rows off those glyphs every time the overlay
		 * appeared or cleared, corrupting them until the next full
		 * drawHudStatic(). */
		fillRect(hud, 32, 30, 256, 36, HUD_COLOR_BACKGROUND);
		if (overlayMode == 1) {
			/* High-contrast CPC-style status panel instead of loose text
			 * floating over the normal SPEED/FUEL area. Outer border capped
			 * at 35 tall for the same row-66 reason as the clear above. */
			fillRect(hud, 42, 31, 236, 35, GAME_COLOR_RED);
			fillRect(hud, 44, 33, 232, 32, HUD_COLOR_VALUE);
			fillRect(hud, 46, 35, 228, 28, HUD_COLOR_BACKGROUND);
			drawTextCentered(hud, 38, "GAME OVER", GAME_COLOR_RED);
			drawTextCentered(hud, 52, "FIRE RETRY  ESC MENU",
				HUD_COLOR_SAFE);
		} else if (overlayMode == 2) {
			drawTextCentered(hud, 44, "LANDED", HUD_COLOR_SAFE);
		} else {
			/* Overlay just cleared - restore what's normally there. */
			drawTextStyled(hud, 66, 36, "SPEED", FONT_STYLE_CPC_HUD);
			drawTextStyled(hud, 218, 36, "FUEL", FONT_STYLE_CPC_HUD);
			drawHudGaugeBar(hud, 16, 50, 140, 10, HUD_COLOR_VALUE, game->speedLevel, GAME_SPEED_LEVEL_MAX);
			drawHudGaugeBar(hud, 164, 50, 140, 10, fuelColor, game->fuel, 999);
		}
	}

	state->valid = 1;
	state->score = game->score;
	state->highScore = highScore;
	state->armour = game->armour;
	state->armourColor = armourColor;
	state->speedLevel = game->speedLevel;
	state->fuel = game->fuel;
	state->fuelColor = fuelColor;
	state->rockets = game->rockets;
	state->bombs = game->bombs;
	state->lives = game->lives;
	state->livesColor = livesColor;
	state->overlayMode = overlayMode;
}

static void drawHudBuffer(UBYTE* hud, const GameState* game, ULONG highScore, UBYTE hudBufferIndex) {
	hudRenderState[hudBufferIndex].valid = 0;
	/* drawHudStatic() below always draws the plain, no-overlay layout, so
	 * the tracked overlay mode must agree with that - otherwise a stale
	 * mode left over from a previous GAME OVER/LANDED screen (this buffer's
	 * state isn't reset anywhere else) makes drawHudValues() think the
	 * overlay just changed and immediately re-clears/redraws over the
	 * labels drawHudStatic() just correctly drew, corrupting them (the
	 * report was "rockets/bombs partially overwritten after landing and a
	 * new mission" - exactly this sequence, since that's the one place a
	 * LANDED->normal transition coincides with a full HUD reset). */
	hudRenderState[hudBufferIndex].overlayMode = 0;
	drawHudStatic(hud);
	drawHudValues(hud, game, highScore, hudBufferIndex);
}

/* Renders the real in-game HUD - the exact same drawHudBuffer()/drawHudStatic()/
 * drawHudValues() gameplay uses, not a separate decorative copy - onto the
 * bottom of the menu screen (same SCREEN_WIDTH x HUD_HEIGHT region, at
 * HUD_TOP, that the HUD split occupies during actual play), so any future
 * change to the gameplay HUD's layout/values is automatically reflected here
 * too. Shows a full/ready demo state rather than real gameplay values -
 * livesSetting comes straight from the menu's own Lives option so that
 * setting's effect is visible immediately. */
static void drawMenuDemoHud(UBYTE* bitmap, short skillLevel,
	short livesSetting, ULONG highScore) {
	GameState demoGame;
	memset(&demoGame, 0, sizeof(demoGame));
	demoGame.armour = 100;
	demoGame.fuel = 999;
	demoGame.speedLevel = GAME_SPEED_LEVEL_DEFAULT;
	demoGame.skillLevel = (UBYTE)skillLevel;
	ammoForSkill(demoGame.skillLevel, &demoGame.bombs,
		&demoGame.rockets);
	demoGame.lives = (UBYTE)livesSetting;

	drawHudBuffer(bitmap + HUD_TOP * SCREEN_PLANES * SCREEN_ROW_BYTES, &demoGame, highScore, MENU_HUD_STATE_INDEX);
}

/* Menu setting changes must not rebuild the whole 320x256 screen. Reuse the
 * HUD delta renderer so changing Lives only touches its value, just as it
 * would during gameplay. */
static void updateMenuDemoHudValues(UBYTE* bitmap, short skillLevel,
	short livesSetting, ULONG highScore) {
	GameState demoGame;
	memset(&demoGame, 0, sizeof(demoGame));
	demoGame.armour = 100;
	demoGame.fuel = 999;
	demoGame.speedLevel = GAME_SPEED_LEVEL_DEFAULT;
	demoGame.skillLevel = (UBYTE)skillLevel;
	ammoForSkill(demoGame.skillLevel, &demoGame.bombs,
		&demoGame.rockets);
	demoGame.lives = (UBYTE)livesSetting;

	drawHudValues(bitmap + HUD_TOP * SCREEN_PLANES * SCREEN_ROW_BYTES,
		&demoGame, highScore, MENU_HUD_STATE_INDEX);
}

static void drawMenuScreen(UBYTE* bitmap, short selected, short skillLevel, short livesSetting, short wingmanControl, ULONG highScore) {
	fillScreen(bitmap, MENU_COLOR_PANEL);

	drawMenuTicker(bitmap);
	drawMenuNotice(bitmap, "", MENU_COLOR_WHITE);
	drawTextCenteredStyled(bitmap, 28, HAR_TEXT_TITLE, FONT_STYLE_CPC_GREEN);
	drawMenuHighScore(bitmap);
	drawMenuItems(bitmap, selected, skillLevel, livesSetting, wingmanControl);
	drawMenuRightSettings(bitmap);
	drawMenuDemoHud(bitmap, skillLevel, livesSetting, highScore);
}

static void drawTelemetryMenuIndicator(UBYTE* bitmap) {
	if (!telemetryEnabled)
		return;
	drawText(bitmap, 304, 18, "D", MENU_COLOR_RED);
}

static UWORD telemetryFpsFromVblDelta(UWORD delta) {
	if (delta == 0)
		delta = 1;
	return (UWORD)(50 / delta);
}

static void telemetryLogRenderEvent(UBYTE code, UBYTE buffer, UWORD origin, UWORD scrollX, UWORD x) {
	telemetryEvent1Code = telemetryEvent0Code;
	telemetryEvent1Buffer = telemetryEvent0Buffer;
	telemetryEvent1Origin = telemetryEvent0Origin;
	telemetryEvent1Scroll = telemetryEvent0Scroll;
	telemetryEvent1X = telemetryEvent0X;
	telemetryEvent0Code = code;
	telemetryEvent0Buffer = buffer;
	telemetryEvent0Origin = origin;
	telemetryEvent0Scroll = scrollX;
	telemetryEvent0X = x;
}

static void telemetryResetInterval(void) {
	telemetryIntervalStartFrame = frameCounter;
	telemetryLastLoopFrame = frameCounter;
	telemetryLoopFrames = 0;
	telemetryMinFps = 999;
	telemetryMaxFps = 0;
	telemetryHitches = 0;
	telemetryMaxVblDelta = 0;
	telemetryMaxVblScrollX = 0;
	telemetryWorldTileColumns = 0;
	telemetryWorldObjectColumns = 0;
	telemetryWorldPages = 0;
}

static void telemetryReset(void) {
	telemetryIndex = 0;
	telemetryCount = 0;
	if (telemetrySamples)
		memset(telemetrySamples, 0, sizeof(TelemetrySample) * TELEMETRY_SAMPLE_COUNT);
	telemetrySessionMinFps = 999;
	telemetrySessionMinFpsScrollX = 0;
	telemetrySessionMaxFps = 0;
	telemetrySessionMaxFpsScrollX = 0;
	telemetrySessionMaxVblDelta = 0;
	telemetrySessionMaxVblScrollX = 0;
	telemetrySessionPlayerMinY = 255;
	telemetrySessionPlayerMaxY = 0;
	telemetryScrollMisses = 0;
	telemetryScrollWaitFrames = 0;
	telemetryMaxScrollWait = 0;
	telemetryCurrentScrollWait = 0;
	telemetryLastDesiredOrigin = 0;
	telemetryLastReadyOrigin = 0xffff;
	telemetryRenderBoostFrames = 0;
	telemetryRenderBoostSteps = 0;
	telemetryEvent0Code = 0;
	telemetryEvent1Code = 0;
	telemetryLastHitchScroll = 0;
	telemetryLastHitchDelta = 0;
	telemetryLastHitchRenderOrigin = 0;
	telemetryLastHitchRenderX = 0;
	telemetryLastHitchRenderStage = 0;
	telemetryLastHitchRenderActive = 0;
	telemetryResetInterval();
}

static void telemetryTerrainRange(LONG startColumn, UWORD columns, UBYTE* minY, UBYTE* maxY) {
	UBYTE found = 0;
	UBYTE localMin = 255;
	UBYTE localMax = 0;

	for (UWORD i = 0; i < columns; i++) {
		WORD y = landSurfaceYForWorldColumn(startColumn + (LONG)i);
		if (y < 0)
			continue;
		if ((UBYTE)y < localMin)
			localMin = (UBYTE)y;
		if ((UBYTE)y > localMax)
			localMax = (UBYTE)y;
		found = 1;
	}

	if (!found) {
		*minY = 255;
		*maxY = 255;
	} else {
		*minY = localMin;
		*maxY = localMax;
	}
}

static void telemetryUpdate(const GameState* game, UBYTE activeWorldBuffer) {
	if (!telemetryEnabled || !telemetryAvailable || !telemetrySamples)
		return;

	UWORD now = frameCounter;
	UWORD delta = (UWORD)(now - telemetryLastLoopFrame);
	UWORD fps = telemetryFpsFromVblDelta(delta);
	telemetryLastLoopFrame = now;
	telemetryLoopFrames++;
	if (fps < telemetryMinFps)
		telemetryMinFps = fps;
	if (fps > telemetryMaxFps)
		telemetryMaxFps = fps;
	UBYTE countGameplayFps = game->takeoffState == TAKEOFF_STATE_AIRBORNE
		&& !game->crashTimer
		&& !game->gameOver
		&& game->scrollX > TELEMETRY_GAMEPLAY_SCROLL_MIN_PIXELS;
	if (countGameplayFps) {
		UBYTE playerY = game->playerY < 0 ? 0 : (game->playerY > 255 ? 255 : (UBYTE)game->playerY);
		if (playerY < telemetrySessionPlayerMinY)
			telemetrySessionPlayerMinY = playerY;
		if (playerY > telemetrySessionPlayerMaxY)
			telemetrySessionPlayerMaxY = playerY;
		if (fps < telemetrySessionMinFps) {
			telemetrySessionMinFps = fps;
			telemetrySessionMinFpsScrollX = game->scrollX;
		}
		if (fps > telemetrySessionMaxFps) {
			telemetrySessionMaxFps = fps;
			telemetrySessionMaxFpsScrollX = game->scrollX;
		}
	}
	if (delta > 1) {
		telemetryHitches++;
		telemetryLastHitchScroll = game->scrollX;
		telemetryLastHitchDelta = delta;
		telemetryLastHitchRenderOrigin = ringWorldLastStreamedColumn;
		telemetryLastHitchRenderX = ringStreamRow;
		telemetryLastHitchRenderStage = 0;
		telemetryLastHitchRenderActive = ringStreamColumn >= 0;
	}
	if (delta > telemetryMaxVblDelta) {
		telemetryMaxVblDelta = delta;
		telemetryMaxVblScrollX = game->scrollX;
	}
	if (countGameplayFps && delta > telemetrySessionMaxVblDelta) {
		telemetrySessionMaxVblDelta = delta;
		telemetrySessionMaxVblScrollX = game->scrollX;
	}

	UWORD elapsed = (UWORD)(now - telemetryIntervalStartFrame);
	if (elapsed < TELEMETRY_INTERVAL_FRAMES)
		return;
	if (elapsed == 0)
		elapsed = 1;

	TelemetrySample* sample = &telemetrySamples[telemetryIndex];
	sample->frame = now;
	sample->scrollX = game->scrollX;
	sample->speedLevel = game->speedLevel;
	sample->loops = telemetryLoopFrames;
	sample->minFps = telemetryMinFps == 999 ? 0 : telemetryMinFps;
	sample->maxFps = telemetryMaxFps;
	sample->avgFps = (UWORD)(((ULONG)telemetryLoopFrames * 50UL) / elapsed);
	sample->hitches = telemetryHitches;
	sample->maxVblDelta = telemetryMaxVblDelta;
	sample->maxVblScrollX = telemetryMaxVblScrollX;
	(void)activeWorldBuffer;
	sample->worldOrigin = ringWorldLastStreamedColumn;
	sample->bytesToPage = 0;
	sample->desiredOrigin = 0;
	sample->readyOrigin = 0;
	sample->nextOrigin = 0;
	sample->buffer0Origin = ringWorldLastStreamedColumn;
	sample->buffer1Origin = 0;
	sample->scrollMisses = 0;
	sample->scrollWaitFrames = 0;
	sample->maxScrollWait = 0;
	sample->currentScrollWait = 0;
	sample->renderBoostFrames = telemetryRenderBoostFrames;
	sample->renderBoostSteps = telemetryRenderBoostSteps;
	sample->renderTileX = ringStreamRow;
	sample->renderActive = ringStreamColumn >= 0;
	sample->renderStage = 0;
	sample->tileColumns = telemetryWorldTileColumns;
	sample->objectColumns = telemetryWorldObjectColumns;
	sample->pages = telemetryWorldPages;
	sample->event0Code = telemetryEvent0Code;
	sample->event0Buffer = telemetryEvent0Buffer;
	sample->event0Origin = telemetryEvent0Origin;
	sample->event0Scroll = telemetryEvent0Scroll;
	sample->event0X = telemetryEvent0X;
	sample->event1Code = telemetryEvent1Code;
	sample->event1Buffer = telemetryEvent1Buffer;
	sample->event1Origin = telemetryEvent1Origin;
	sample->event1Scroll = telemetryEvent1Scroll;
	sample->event1X = telemetryEvent1X;
	sample->hitchScroll = telemetryLastHitchScroll;
	sample->hitchDelta = telemetryLastHitchDelta;
	sample->hitchRenderOrigin = telemetryLastHitchRenderOrigin;
	sample->hitchRenderX = telemetryLastHitchRenderX;
	sample->hitchRenderStage = telemetryLastHitchRenderStage;
	sample->hitchRenderActive = telemetryLastHitchRenderActive;
	telemetryTerrainRange((LONG)(game->scrollX >> 3), GAME_MAP_WIDTH, &sample->visibleLandMinY, &sample->visibleLandMaxY);
	telemetryTerrainRange((LONG)sample->worldOrigin - GAME_WORLD_BUFFER_MARGIN_TILES, GAME_WORLD_BUFFER_TILES, &sample->bufferLandMinY, &sample->bufferLandMaxY);
	sample->playerY = game->playerY < 0 ? 0 : (game->playerY > 255 ? 255 : (UBYTE)game->playerY);
	sample->playerMinY = telemetrySessionPlayerMinY == 255 ? 0 : telemetrySessionPlayerMinY;
	sample->playerMaxY = telemetrySessionPlayerMaxY;
	sample->fuel = game->fuel;
	sample->armour = game->armour;
	sample->rockets = game->rockets;
	sample->bombs = game->bombs;

	telemetryIndex++;
	if (telemetryIndex >= TELEMETRY_SAMPLE_COUNT)
		telemetryIndex = 0;
	if (telemetryCount < TELEMETRY_SAMPLE_COUNT)
		telemetryCount++;
	telemetryResetInterval();
}

static const TelemetrySample* latestTelemetrySample(void) {
	if (!telemetrySamples || telemetryCount == 0)
		return 0;
	UBYTE index = telemetryIndex == 0 ? TELEMETRY_SAMPLE_COUNT - 1 : telemetryIndex - 1;
	return &telemetrySamples[index];
}

static void drawTelemetryStatsScreen(UBYTE* bitmap) {
	fillScreen(bitmap, MENU_COLOR_PANEL);
	drawTelemetryTextCentered(bitmap, 6, "TELEMETRY", MENU_COLOR_GREEN);
	drawTelemetryTextCentered(bitmap, 16, HAR_BUILD_LABEL, MENU_COLOR_CYAN);
	drawTelemetryTextCentered(bitmap, 188, "SPACE BACK   R RESET", MENU_COLOR_YELLOW);

	if (!telemetryAvailable || !telemetrySamples) {
		drawTelemetryTextCentered(bitmap, 88, "NO EXTENDED MEMORY BUFFER", MENU_COLOR_RED);
		return;
	}
	if (!telemetryEnabled) {
		drawTelemetryTextCentered(bitmap, 88, "SHIFT+D IN MENU ENABLES DEBUG", MENU_COLOR_CYAN);
		return;
	}

	const TelemetrySample* s = latestTelemetrySample();
	if (!s) {
		drawTelemetryTextCentered(bitmap, 88, "WAITING FOR FIRST 10S SAMPLE", MENU_COLOR_CYAN);
		return;
	}

	drawTelemetryText(bitmap, 12, 32, "FRAME", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 70, 32, s->frame, 5, MENU_COLOR_YELLOW);
	drawTelemetryText(bitmap, 160, 32, "SAMP", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 210, 32, telemetryCount, 2, MENU_COLOR_YELLOW);
	drawTelemetryText(bitmap, 244, 32, "PFH", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 284, 32, GAME_WORLD_HEIGHT, 3, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 12, 44, "FPS MIN MAX AVG", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 150, 44, s->minFps, 2, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 190, 44, s->maxFps, 2, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 230, 44, s->avgFps, 2, MENU_COLOR_YELLOW);
	drawTelemetryText(bitmap, 260, 44, "LOOP", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 302, 44, s->loops, 3, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 12, 56, "HITCH CNT MAX @", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 150, 56, s->hitches, 3, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 190, 56, s->maxVblDelta, 2, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 230, 56, s->maxVblScrollX, 5, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 12, 68, "SESS MAXVBL @", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 150, 68, telemetrySessionMaxVblDelta, 2, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 190, 68, telemetrySessionMaxVblScrollX, 5, MENU_COLOR_YELLOW);
	drawTelemetryText(bitmap, 250, 68, "MINFPS", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 302, 68, telemetrySessionMinFps == 999 ? 0 : telemetrySessionMinFps, 2, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 12, 80, "FPSMAP MIN@ MAX@", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 150, 80, telemetrySessionMinFpsScrollX, 5, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 220, 80, telemetrySessionMaxFps, 2, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 250, 80, telemetrySessionMaxFpsScrollX, 5, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 12, 92, "MISS WAIT MAX CUR", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 150, 92, s->scrollMisses, 3, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 190, 92, s->scrollWaitFrames, 3, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 230, 92, s->maxScrollWait, 2, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 270, 92, s->currentScrollWait, 2, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 12, 104, "SCROLL SPD BTP", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 150, 104, s->scrollX, 5, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 220, 104, s->speedLevel, 2, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 260, 104, s->bytesToPage, 3, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 12, 116, "DES RDY ACT NXT", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 150, 116, s->desiredOrigin, 4, MENU_COLOR_YELLOW);
	if (s->readyOrigin == 0xffff)
		drawTelemetryText(bitmap, 200, 116, "----", MENU_COLOR_YELLOW);
	else
		drawTelemetryUnsignedPadded(bitmap, 200, 116, s->readyOrigin, 4, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 250, 116, s->worldOrigin, 4, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 290, 116, s->nextOrigin, 4, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 12, 128, "A X B0 B1", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 150, 128, s->renderActive, 1, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 170, 128, s->renderTileX, 3, MENU_COLOR_YELLOW);
	if (s->buffer0Origin == 0xffff)
		drawTelemetryText(bitmap, 205, 128, "----", MENU_COLOR_YELLOW);
	else
		drawTelemetryUnsignedPadded(bitmap, 205, 128, s->buffer0Origin, 4, MENU_COLOR_YELLOW);
	if (s->buffer1Origin == 0xffff)
		drawTelemetryText(bitmap, 250, 128, "----", MENU_COLOR_YELLOW);
	else
		drawTelemetryUnsignedPadded(bitmap, 250, 128, s->buffer1Origin, 4, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 12, 140, "COL TILE OBJ PAGE", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 150, 140, s->tileColumns, 4, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 210, 140, s->objectColumns, 4, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 270, 140, s->pages, 2, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 12, 152, "E0 C B ORG SCR X", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 110, 152, s->event0Code, 2, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 145, 152, s->event0Buffer, 1, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 175, 152, s->event0Origin, 4, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 230, 152, s->event0Scroll, 5, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 295, 152, s->event0X, 3, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 12, 164, "HCH D A S ORG SCR X", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 92, 164, s->hitchDelta, 2, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 125, 164, s->hitchRenderActive, 1, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 155, 164, s->hitchRenderStage, 1, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 185, 164, s->hitchRenderOrigin, 4, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 235, 164, s->hitchScroll, 5, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 295, 164, s->hitchRenderX, 3, MENU_COLOR_YELLOW);
}

static void drawPauseHudOverlay(UBYTE* hud, UBYTE visible) {
	/* Temporarily replace only the HUD's bottom row. The playfield, Copper
	 * split and hardware sprites remain untouched while paused. */
	fillRect(hud, 0, 64, SCREEN_WIDTH, HUD_HEIGHT - 64,
		HUD_COLOR_BACKGROUND);
	if (visible)
		drawTextCentered(hud, 72, "PAUSED - SPACE TO CONTINUE",
			HUD_COLOR_VALUE);
}

static void updateMenuSelection(UBYTE* bitmap, short oldSelected, short newSelected, short skillLevel, short livesSetting, short wingmanControl) {
	if (oldSelected == newSelected)
		return;

	(void)skillLevel;
	(void)livesSetting;
	(void)wingmanControl;
	drawMenuCursor(bitmap, oldSelected, 0);
	drawMenuCursor(bitmap, newSelected, 1);
}

/* direction: -1 for left, +1 for right/Select. Returns 1 only when a setting
 * changed. The selected row and (for Lives) the one live HUD value are the
 * only pixels redrawn. */
static UBYTE adjustSelectedMenuOption(UBYTE* bitmap, short selected, short direction,
	short* skillLevel, short* livesSetting, short* wingmanControl,
	ULONG highScore) {
	if (selected == MENU_ITEM_SKILL) {
		*skillLevel += direction;
		if (*skillLevel < 1)
			*skillLevel = 5;
		else if (*skillLevel > 5)
			*skillLevel = 1;
	} else if (selected == MENU_ITEM_LIVES) {
		*livesSetting = (*livesSetting == 3) ? 1 : 3;
	} else if (selected == MENU_ITEM_WINGMAN) {
		*wingmanControl += direction;
		if (*wingmanControl < WINGMAN_CONTROL_OFF)
			*wingmanControl = WINGMAN_CONTROL_PLAYER2;
		else if (*wingmanControl > WINGMAN_CONTROL_PLAYER2)
			*wingmanControl = WINGMAN_CONTROL_OFF;
	} else {
		return 0;
	}

	drawMenuItem(bitmap, selected, 1, *skillLevel, *livesSetting, *wingmanControl);
	if (selected == MENU_ITEM_LIVES || selected == MENU_ITEM_SKILL)
		updateMenuDemoHudValues(bitmap, *skillLevel, *livesSetting,
			highScore);
	return 1;
}

static void drawGameTileWithStride(UBYTE* bitmap, short rowBytes, short tileX, short tileY, UBYTE tileId) {
	if (tileId >= GAME_TILE_COUNT)
		tileId = 0;

	const UBYTE* tile = gameTiles + tileId * GAME_TILE_BYTES;
	short screenXByte = tileX;
	short screenY = tileY * GAME_TILE_HEIGHT;
	short maxHeight = rowBytes == GAME_WORLD_ROW_BYTES ? GAME_WORLD_HEIGHT : SCREEN_HEIGHT;

	for (short row = 0; row < GAME_TILE_HEIGHT; row++) {
		if (screenY + row >= maxHeight)
			break;
		UBYTE* dest = bitmap + (screenY + row) * SCREEN_PLANES * rowBytes + screenXByte;
		const UBYTE* src = tile + row * GAME_TILE_PLANES;
		for (short plane = 0; plane < GAME_TILE_PLANES; plane++)
			dest[plane * rowBytes] = src[plane];
	}
}

static void drawGameTile(UBYTE* bitmap, short tileX, short tileY, UBYTE tileId) {
	drawGameTileWithStride(bitmap, SCREEN_ROW_BYTES, tileX, tileY, tileId);
}

static void drawGameScrollTile(UBYTE* bitmap, short tileX, short tileY, UBYTE tileId) {
	drawGameTileWithStride(bitmap, GAME_WORLD_ROW_BYTES, tileX, tileY, tileId);
}

/* Sprint 14.94 Part 6: like drawGameScrollTile(), but for pre-masked tiles
 * (tools/cpc_promoted_sprites_to_tiles.py's output - 8 rows x [4 colour-plane
 * bytes + 1 opacity-mask byte], the same 40-byte footprint as a regular game
 * tile) rather than plain opaque ones. Unlike a flat overwrite, this only
 * replaces the pixels the mask marks as opaque (bit set = draw this pixel's
 * colour, bit clear = leave whatever's already in the buffer) - needed
 * because unlike terrain tiles, the carrier/gunship art has real
 * transparency (background sea/sky showing through) mixed within individual
 * 8x8 cells, confirmed by scanning the source pixel data directly before
 * writing the conversion script. A whole-row skip (mask==0) still short
 * circuits to a no-op, same cost as the plain blit for empty rows. */
static void drawGameScrollTileMasked(UBYTE* bitmap, short tileX, short tileY, const UBYTE* tile) {
	short screenY = (short)(tileY * GAME_TILE_HEIGHT);

	for (short row = 0; row < GAME_TILE_HEIGHT; row++) {
		if (screenY + row >= GAME_WORLD_HEIGHT)
			break;
		const UBYTE* src = tile + row * (GAME_WORLD_DISPLAY_PLANES + 1);
		UBYTE mask = src[GAME_WORLD_DISPLAY_PLANES];
		if (!mask)
			continue;
		UBYTE* dest = bitmap + (screenY + row) * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + tileX;
		for (short plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++)
			dest[plane * GAME_WORLD_ROW_BYTES] = (UBYTE)((dest[plane * GAME_WORLD_ROW_BYTES] & ~mask) | (src[plane] & mask));
	}
}

/* Sprint 15.2, generalised in 15.6: shared byte footprint for one masked-Bob
 * tile row-set (matches drawGameScrollTileMasked()'s own input format: 8
 * rows x (4 colour-plane bytes + 1 opacity-mask byte)). Originally sized
 * for the wingman's own Bob tiles; now shared by the bomb/impact and
 * powerup Bob tiles below. */
#define BOB_TILE_BYTES (GAME_TILE_HEIGHT * (GAME_WORLD_DISPLAY_PLANES + 1))

static void drawGameTileMap(UBYTE* bitmap, const UBYTE* map) {
	for (short tileY = 0; tileY < GAME_MAP_HEIGHT; tileY++) {
		for (short tileX = 0; tileX < GAME_MAP_WIDTH; tileX++)
			drawGameTile(bitmap, tileX, tileY, map[tileY * GAME_MAP_WIDTH + tileX]);
	}
}

static UBYTE gameTilePixelColor(UBYTE tileId, short x, short y) {
	if (tileId >= GAME_TILE_COUNT)
		return GAME_COLOR_SKY;

	const UBYTE* row = gameTiles + tileId * GAME_TILE_BYTES + y * GAME_TILE_PLANES;
	UBYTE bit = 0x80 >> (x & 7);
	UBYTE color = 0;
	for (short plane = 0; plane < GAME_TILE_PLANES; plane++) {
		if (row[plane] & bit)
			color |= 1 << plane;
	}
	return color;
}

/* Sprint 15.6: bomb (falling, tiles 40/41) and impact (hand-rolled star
 * shapes) Bob tiles, moved off hardware sprite channel 2 to free it for the
 * wingman's own rocket (see AMIGA_PORT_PLAN.md's Sprint 15 roadmap). Both
 * share one tile buffer, rebuilt whenever the requested "kind" changes:
 * 0/1 = bomb tiles 40/41 (the same two-frame falling animation
 * buildBombSprite() used to draw as a hardware sprite), 2/3 = impact
 * small/large star. Treats GAME_COLOR_SKY as transparent for the bomb
 * tiles, exactly like the old gameColorToBombSpriteColor() did. The impact
 * shapes were hand-rolled hardware-sprite bitplane rows fed to both planes
 * at once (a single solid colour, no shading) - GAME_COLOR_WHITE here is a
 * reasonable flash colour, not a preserved exact value (the sprite's own
 * hardware colour register for that channel was never inspected/mattered
 * before this conversion). */
#define BOMB_IMPACT_BOB_KIND_FALLING_A 0
#define BOMB_IMPACT_BOB_KIND_FALLING_B 1
#define BOMB_IMPACT_BOB_KIND_IMPACT_SMALL 2
#define BOMB_IMPACT_BOB_KIND_IMPACT_LARGE 3
static UBYTE bombImpactBobTile[BOB_TILE_BYTES];
static UBYTE bombImpactBobTileKind = 0xFF;

/* Sprint 15.7: the flying bomb is deliberately not part of the tile Bob
 * compositor below.  It is only four pixels wide and five pixels high, so a
 * Blitter setup would cost more than the handful of byte masks required to
 * draw it on a 68000.  Keep an independent saved background for every world
 * buffer: if GAME_WORLD_BUFFER_COUNT is raised again, restoring buffer N can
 * never use pixels captured from buffer M.
 *
 * A four-pixel shape can straddle at most two bytes.  The ring seam can make
 * the same world pixel visible at both the primary and duplicate positions,
 * hence two saved placements per buffer.  Only the four displayed world
 * planes are touched; the allocated-but-unfetched fifth plane is unchanged. */
#define BOMB_SHOT_PIXEL_BOB_WIDTH 4
#define BOMB_SHOT_PIXEL_BOB_HEIGHT 3
#define BOMB_SHOT_PIXEL_BOB_MAX_PLACEMENTS 2
#define BOMB_SHOT_PIXEL_BOB_MAX_BYTES_PER_ROW 2

typedef struct BombShotFootprint {
	UBYTE valid;
	UBYTE placementCount;
	WORD y;
	UWORD byteX[BOMB_SHOT_PIXEL_BOB_MAX_PLACEMENTS];
	UBYTE byteCount[BOMB_SHOT_PIXEL_BOB_MAX_PLACEMENTS];
	UBYTE background[BOMB_SHOT_PIXEL_BOB_MAX_PLACEMENTS]
		[BOMB_SHOT_PIXEL_BOB_HEIGHT]
		[GAME_WORLD_DISPLAY_PLANES]
		[BOMB_SHOT_PIXEL_BOB_MAX_BYTES_PER_ROW];
} BombShotFootprint;

static BombShotFootprint bombShotFootprints[GAME_WORLD_BUFFER_COUNT];
static BombShotFootprint wingmanBombFootprints[GAME_WORLD_BUFFER_COUNT];
static UBYTE carrierParkedWingmanVisible = 0;

#define ROCKET_PIXEL_BOB_WIDTH 8
#define ROCKET_PIXEL_BOB_HEIGHT 8
#define ROCKET_PIXEL_BOB_MAX_PLACEMENTS 2
#define ROCKET_PIXEL_BOB_MAX_BYTES_PER_ROW 2

typedef struct RocketShotFootprint {
	UBYTE valid;
	UBYTE placementCount;
	WORD y;
	UWORD byteX[ROCKET_PIXEL_BOB_MAX_PLACEMENTS];
	UBYTE byteCount[ROCKET_PIXEL_BOB_MAX_PLACEMENTS];
	UBYTE background[ROCKET_PIXEL_BOB_MAX_PLACEMENTS]
		[ROCKET_PIXEL_BOB_HEIGHT]
		[GAME_WORLD_DISPLAY_PLANES]
		[ROCKET_PIXEL_BOB_MAX_BYTES_PER_ROW];
} RocketShotFootprint;

static RocketShotFootprint rocketShotFootprints[GAME_WORLD_BUFFER_COUNT];
static RocketShotFootprint wingmanRocketFootprints[GAME_WORLD_BUFFER_COUNT];

static void buildBombImpactBobTileIfNeeded(UBYTE kind) {
	if (bombImpactBobTileKind == kind)
		return;
	memset(bombImpactBobTile, 0, sizeof(bombImpactBobTile));
	if (kind == BOMB_IMPACT_BOB_KIND_FALLING_A || kind == BOMB_IMPACT_BOB_KIND_FALLING_B) {
		UBYTE tileId = (kind == BOMB_IMPACT_BOB_KIND_FALLING_A) ? 40 : 41;
		for (UBYTE row = 0; row < GAME_TILE_HEIGHT; row++) {
			UBYTE* dest = bombImpactBobTile + row * (GAME_WORLD_DISPLAY_PLANES + 1);
			UBYTE mask = 0;
			for (UBYTE col = 0; col < GAME_TILE_WIDTH; col++) {
				UBYTE color = gameTilePixelColor(tileId, (short)col, (short)row);
				if (color == GAME_COLOR_SKY)
					continue;
				UBYTE bit = (UBYTE)(0x80 >> col);
				mask |= bit;
				for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++) {
					if (color & (1 << plane))
						dest[plane] |= bit;
				}
			}
			dest[GAME_WORLD_DISPLAY_PLANES] = mask;
		}
	} else {
		static const UWORD small[GAME_TILE_HEIGHT] = {
			0x0000, 0x1800, 0x2400, 0x5a00, 0x2400, 0x1800, 0x0000, 0x0000
		};
		static const UWORD large[GAME_TILE_HEIGHT] = {
			0x8100, 0x4200, 0x2400, 0x7e00, 0x2400, 0x4200, 0x8100, 0x0000
		};
		const UWORD* shape = (kind == BOMB_IMPACT_BOB_KIND_IMPACT_LARGE) ? large : small;
		for (UBYTE row = 0; row < GAME_TILE_HEIGHT; row++) {
			UBYTE* dest = bombImpactBobTile + row * (GAME_WORLD_DISPLAY_PLANES + 1);
			UBYTE mask = (UBYTE)(shape[row] >> 8);
			if (mask) {
				for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++) {
					if (GAME_COLOR_WHITE & (1 << plane))
						dest[plane] = mask;
				}
			}
			dest[GAME_WORLD_DISPLAY_PLANES] = mask;
		}
	}
	bombImpactBobTileKind = kind;
}

/* Powerup pickup Bob tiles (a 16px-wide parachute, spanning two playfield
 * cells), moved off hardware sprite channel 6 for the Wingman body.
 * Channel 7 remains unused. The old hardware-sprite version recoloured per powerup type
 * at runtime via the copper (writing hardware colour 29 directly - see
 * updatePowerupSprite()'s own removed comment) since a sprite's bitplane
 * data never needed to change, only its palette. A Bob has no such
 * separate palette register to repoint - the type's colour has to be baked
 * directly into the tile's own plane bits, so this rebuilds the tile
 * whenever the type actually changes instead. CPC pen 15 (the canopy/type
 * area) maps to the requested type colour; pen 1 (the dark rigging lines)
 * stays fixed, matching the old hardcoded hardware-colour-30 override. */
static UBYTE powerupBobTileLeft[BOB_TILE_BYTES];
static UBYTE powerupBobTileRight[BOB_TILE_BYTES];
static UBYTE powerupBobTileType = 0xFF;

static void buildPowerupBobTileIfNeeded(UBYTE type) {
	if (powerupBobTileType == type)
		return;
	/* CPC's per-type palette colours (matches updatePowerupSprite()'s old
	 * powerupTypeColor[] table's intent - red/yellow/blue/green - translated
	 * to the closest existing playfield colours since a Bob can't repoint a
	 * hardware colour register per pixel the way a sprite could). */
	static const UBYTE typeColor[5] = {
		GAME_COLOR_SKY,    /* NONE (unused) */
		GAME_COLOR_RED,    /* WINGMAN */
		GAME_COLOR_YELLOW, /* HEALTH */
		GAME_COLOR_SEA,    /* ROCKETS (closest existing blue) */
		GAME_COLOR_LAND    /* BOMBS (closest existing green) */
	};
	UBYTE canopyColor = typeColor[type < 5 ? type : 0];
	memset(powerupBobTileLeft, 0, sizeof(powerupBobTileLeft));
	memset(powerupBobTileRight, 0, sizeof(powerupBobTileRight));
	for (UBYTE row = 0; row < HAR_CPC_PARACHUTE_HEIGHT; row++) {
		for (UBYTE col = 0; col < HAR_CPC_PARACHUTE_WIDTH; col++) {
			UBYTE pen = harCpcParachutePixels[row * HAR_CPC_PARACHUTE_WIDTH + col];
			if (pen != 15 && pen != 1)
				continue;
			UBYTE pixelColor = (pen == 15) ? canopyColor : GAME_COLOR_BLACK;
			UBYTE* dest = (col < 8 ? powerupBobTileLeft : powerupBobTileRight) +
				row * (GAME_WORLD_DISPLAY_PLANES + 1);
			UBYTE bit = (UBYTE)(0x80 >> (col & 7));
			for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++) {
				if (pixelColor & (1 << plane))
					dest[plane] |= bit;
			}
			dest[GAME_WORLD_DISPLAY_PLANES] |= bit;
		}
	}
	powerupBobTileType = type;
}

static UBYTE gameColorToPlayerSpriteColor(UBYTE color) {
	if (color == GAME_COLOR_SKY)
		return 0;
	if (color == GAME_COLOR_BLACK)
		return 1;
	if (color == GAME_COLOR_LAND)
		return 2;
	return 3;
}

static UBYTE gameColorToHostileSpriteColor(UBYTE color) {
	if (color == GAME_COLOR_SKY)
		return 0;
	if (color == GAME_COLOR_BLACK)
		return 1;
	if (color == GAME_COLOR_RED)
		return 3;
	return 2;
}

static UBYTE cpcPlusPenToHostileHardwareColor(UBYTE pen) {
	if (!pen)
		return 0;
	if (pen <= 2)
		return 1;
	if (pen <= 4)
		return 2;
	return 3;
}

static void hideHardwareSprite(UWORD* sprite) {
	sprite[0] = 0;
	sprite[1] = 0;
}

static void setHardwareSpritePosition(UWORD* sprite, UWORD height, WORD x, WORD y) {
	/* Keep hardware sprites aligned with the shifted playfield window. */
	UWORD hstart = (UWORD)(x + SCREEN_DIWSTRT_X - 1);
	/* Was hardcoded 0x2c(44) - the display's own DIWSTRT vertical baseline
	 * used to be hardcoded to the same 44 in screenScan()/copWaitDisplayYAt(),
	 * so this matched by coincidence. Repositioning the display window up
	 * (SCREEN_DIWSTRT_Y 44->20, Sprint 14.91.4, to fix ROCKETS/BOMBS being
	 * drawn partly off-screen) moved the playfield/HUD but left this sprite
	 * baseline behind, so every hardware sprite (player, rocket, bomb, enemy,
	 * enemy missile) rendered 24px too low relative to the world - looking
	 * like the plane sat under the carrier and sank into the ground/sea
	 * before collision (which uses logical, not screen, coordinates and was
	 * never actually wrong) caught up. */
	UWORD vstart = (UWORD)(y + SCREEN_DIWSTRT_Y);
	UWORD vstop = (UWORD)(vstart + height);

	sprite[0] = (UWORD)((vstart << 8) | ((hstart >> 1) & 0xff));
	sprite[1] = (UWORD)(((vstop & 0xff) << 8)
		| ((vstart & 0x100) >> 6)
		| ((vstop & 0x100) >> 7)
		| (hstart & 1));
}

static void setPlayerSpritePosition(UWORD* sprite, WORD x, WORD y) {
	setHardwareSpritePosition(sprite, PLAYER_SPRITE_HEIGHT, x, y);
}

#define HW_SPRITE_ATTACH_BIT 0x0080

/* Builds an attached 4-bitplane, 15-colour sprite pair from raw CPC+ pen
 * values (0-15) instead of reducing to a 2-bitplane, 3-colour sprite. The
 * low 2 bits go to the normal sprite channel (planes 0-1), the high 2 bits
 * go to the paired "attach" channel (planes 2-3), which must be the very
 * next odd-numbered hardware sprite. Both halves need matching vstart/vstop
 * (set here identically); only the even sprite's hstart is used for
 * horizontal position once attached, per the OCS/ECS attach mechanism. */
static void buildAttachedSpriteFromCpcPlusHalvesMapped(UWORD* sprite,
	UWORD* attachSprite, UWORD height, WORD x, WORD y,
	const UBYTE* leftPixels, const UBYTE* rightPixels,
	const UBYTE* penMap) {
	setHardwareSpritePosition(sprite, height, x, y);
	setHardwareSpritePosition(attachSprite, height, x, y);
	attachSprite[1] |= HW_SPRITE_ATTACH_BIT;

	for (short row = 0; row < height; row++) {
		UWORD plane0 = 0;
		UWORD plane1 = 0;
		UWORD plane2 = 0;
		UWORD plane3 = 0;
		for (short col = 0; col < 16; col++) {
			const UBYTE* source = col < 8 ? leftPixels : rightPixels;
			UBYTE sourceX = (UBYTE)(col & 7);
			UBYTE pen = source[row * 16 + sourceX];
			if (penMap)
				pen = penMap[pen & 0x0f];
			UWORD bit = 0x8000 >> col;
			if (pen & 1)
				plane0 |= bit;
			if (pen & 2)
				plane1 |= bit;
			if (pen & 4)
				plane2 |= bit;
			if (pen & 8)
				plane3 |= bit;
		}
		sprite[2 + row * 2] = plane0;
		sprite[3 + row * 2] = plane1;
		attachSprite[2 + row * 2] = plane2;
		attachSprite[3 + row * 2] = plane3;
	}
	sprite[2 + height * 2] = 0;
	sprite[3 + height * 2] = 0;
	attachSprite[2 + height * 2] = 0;
	attachSprite[3 + height * 2] = 0;
}

static void buildAttachedSpriteFromCpcPlusHalves(UWORD* sprite,
	UWORD* attachSprite, UWORD height, WORD x, WORD y,
	const UBYTE* leftPixels, const UBYTE* rightPixels) {
	buildAttachedSpriteFromCpcPlusHalvesMapped(sprite, attachSprite, height,
		x, y, leftPixels, rightPixels, 0);
}

static void buildSpriteFromCpcPlusHalves(UWORD* sprite, UWORD height, WORD x, WORD y, const UBYTE* leftPixels, const UBYTE* rightPixels, UBYTE (*mapPen)(UBYTE)) {
	setHardwareSpritePosition(sprite, height, x, y);
	for (short row = 0; row < height; row++) {
		UWORD plane0 = 0;
		UWORD plane1 = 0;
		for (short col = 0; col < 16; col++) {
			const UBYTE* source = col < 8 ? leftPixels : rightPixels;
			UBYTE sourceX = (UBYTE)(col & 7);
			UBYTE pen = source[row * 16 + sourceX];
			UBYTE spriteColor = mapPen(pen);
			UWORD bit = 0x8000 >> col;
			if (spriteColor & 1)
				plane0 |= bit;
			if (spriteColor & 2)
				plane1 |= bit;
		}
		sprite[2 + row * 2] = plane0;
		sprite[3 + row * 2] = plane1;
	}
	sprite[2 + height * 2] = 0;
	sprite[3 + height * 2] = 0;
}

static void buildSpriteFromGameTile(UWORD* sprite, UWORD height, WORD x, WORD y, UBYTE tileId, UBYTE xOffset, UBYTE (*mapColor)(UBYTE)) {
	setHardwareSpritePosition(sprite, height, x, y);
	for (short row = 0; row < height; row++) {
		UWORD plane0 = 0;
		UWORD plane1 = 0;
		for (short col = 0; col < 8; col++) {
			UBYTE color = gameTilePixelColor(tileId, col, row);
			UBYTE spriteColor = mapColor(color);
			UWORD bit = 0x8000 >> (col + xOffset);
			if (spriteColor & 1)
				plane0 |= bit;
			if (spriteColor & 2)
				plane1 |= bit;
		}
		sprite[2 + row * 2] = plane0;
		sprite[3 + row * 2] = plane1;
	}
	sprite[2 + height * 2] = 0;
	sprite[3 + height * 2] = 0;
}

static void buildPlayerSprite(UWORD* sprite, UWORD* attachSprite, WORD x, WORD y) {
	buildAttachedSpriteFromCpcPlusHalves(sprite, attachSprite, PLAYER_SPRITE_HEIGHT, x, y, harCpcHarrierFlyingLeftPixels, harCpcHarrierFlyingRightPixels);
}

static void buildPlayerLandingSprite(UWORD* sprite, UWORD* attachSprite, WORD x, WORD y) {
	buildAttachedSpriteFromCpcPlusHalves(sprite, attachSprite, PLAYER_SPRITE_HEIGHT, x, y, harCpcHarrierLandingLeftPixels, harCpcHarrierLandingRightPixels);
}

static void buildPlayerCrashPartSprite(UWORD* sprite, WORD x, WORD y, UBYTE part) {
	buildSpriteFromGameTile(sprite, PLAYER_SPRITE_HEIGHT, x, y, (UBYTE)(67 + part), 4, gameColorToPlayerSpriteColor);
}

static void buildEjectSprite(UWORD* sprite, const GameState* game);

static void updatePlayerSprite(UWORD* sprite, UWORD* attachSprite, const GameState* game) {
	if (game->gameOver) {
		hideHardwareSprite(sprite);
		hideHardwareSprite(attachSprite);
		return;
	}

	/* Eject owns the player's normal sprite channel. Channel 7 is paired
	 * with Wingman sprite 6 and uses that pair's palette/attach state, which
	 * made the parachute invisible or white depending on Wingman's frame. */
	if (game->ejectState) {
		hideHardwareSprite(attachSprite);
		buildEjectSprite(sprite, game);
		return;
	}

	if (game->crashTimer && game->crashPart[0].active) {
		hideHardwareSprite(attachSprite);
		buildPlayerCrashPartSprite(sprite, game->crashPart[0].x, game->crashPart[0].y, 0);
		return;
	}

	/* CPC beginlandingapproach calls setlandingsprite immediately, loading
	 * sprite_pixel_data4/3 (the Harrier with its undercarriage down), and
	 * keeps that artwork throughout landinghoverloop.  The Amiga port
	 * already had the correctly extracted attached-sprite pair, but only
	 * selected it while parked on the opening carrier. */
	if (game->takeoffState == TAKEOFF_STATE_ROLLING_IN ||
		game->takeoffState == TAKEOFF_STATE_READY ||
		game->landingState != LANDING_STATE_NONE)
		buildPlayerLandingSprite(sprite, attachSprite, game->playerX, game->playerY);
	else
		buildPlayerSprite(sprite, attachSprite, game->playerX, game->playerY);
}

static void buildEjectSprite(UWORD* sprite, const GameState* game) {
	if (game->ejectState == 1) {
		buildSpriteFromGameTile(sprite, HAR_CPC_PARACHUTE_HEIGHT,
			game->ejectX, game->ejectY, 47, 4,
			gameColorToPlayerSpriteColor);
		return;
	}

	setHardwareSpritePosition(sprite, HAR_CPC_PARACHUTE_HEIGHT,
		game->ejectX, game->ejectY);
	for (UBYTE row = 0; row < HAR_CPC_PARACHUTE_HEIGHT; row++) {
		UWORD plane0 = 0;
		UWORD plane1 = 0;
		for (UBYTE col = 0; col < HAR_CPC_PARACHUTE_WIDTH; col++) {
			UBYTE pen = harCpcParachutePixels[
				row * HAR_CPC_PARACHUTE_WIDTH + col];
			UWORD bit = (UWORD)(0x8000 >> col);
			if (pen == 15)
				plane0 |= bit;
			else if (pen != 0)
				plane1 |= bit;
		}
		sprite[2 + row * 2] = plane0;
		sprite[3 + row * 2] = plane1;
	}
	sprite[2 + HAR_CPC_PARACHUTE_HEIGHT * 2] = 0;
	sprite[3 + HAR_CPC_PARACHUTE_HEIGHT * 2] = 0;
}

static void buildSpriteFromRows(UWORD* sprite, UWORD height, WORD x, WORD y, const UWORD* plane0Rows, const UWORD* plane1Rows) {
	setHardwareSpritePosition(sprite, height, x, y);
	for (UWORD row = 0; row < height; row++) {
		sprite[2 + row * 2] = plane0Rows[row];
		sprite[3 + row * 2] = plane1Rows[row];
	}
	sprite[2 + height * 2] = 0;
	sprite[3 + height * 2] = 0;
}

static UBYTE rocketTileForState(const WeaponState* rocket) {
	if (rocket->type != ROCKET_SHOT_MAVERICK_GUIDED)
		return 56;

	switch (rocket->direction) {
		case MAVERICK_DIRECTION_UP: return 101;
		case MAVERICK_DIRECTION_UP_RIGHT: return 98;
		case MAVERICK_DIRECTION_DOWN_RIGHT: return 99;
		case MAVERICK_DIRECTION_DOWN: return 100;
		case MAVERICK_DIRECTION_DOWN_LEFT:
		case MAVERICK_DIRECTION_LEFT: return 55;
		case MAVERICK_DIRECTION_UP_LEFT: return 53;
		default: return 56;
	}
}

static void buildEnemyPlaneSprite(UWORD* sprite, UWORD* attachSprite, WORD x, WORD y) {
	/* The CPC+ enemy uses more than three pens.  Reducing it to one normal
	 * Amiga sprite made it mostly white, and channel 2 actually reads
	 * COLOR21-23 (not COLOR17-19).  Use channels 2+3 as an attached pair,
	 * like the player, so the original CPC pen values
	 * reach the shared 16-colour sprite palette unchanged. */
	buildAttachedSpriteFromCpcPlusHalves(sprite, attachSprite,
		ENEMY_SPRITE_HEIGHT, x, y,
		harCpcEnemyPlaneFlyingLeftPixels,
		harCpcEnemyPlaneFlyingRightPixels);
}

static void buildEnemyMissileSprite(UWORD* sprite, WORD x, WORD y) {
	buildSpriteFromGameTile(sprite, ENEMY_MISSILE_SPRITE_HEIGHT, x, y, 55, 4, gameColorToHostileSpriteColor);
}

static UBYTE cpcPlusPenToWingmanHardwareColor(UBYTE pen) {
	pen &= 15;
	if (pen == 0)
		return 0;
	if (pen <= 2)
		return 1;
	if (pen <= 4)
		return 2;
	return 3;
}

static UBYTE cpcPlusPenToGameColor(UBYTE pen) {
	/* Generated with the promoted tiles from the converter's single source
	 * table, keeping runtime and offline colour conversion identical. */
	return harCpcPlusPenToGameColor[pen & 15];
}

static void drawCpcPlusSpriteScroll(UBYTE* bitmap, short x, short y, const UBYTE* pixels, UBYTE width, UBYTE height, UBYTE xScale) {
	if (!xScale)
		xScale = 1;

	for (short py = 0; py < height; py++) {
		const UBYTE* src = pixels + py * width;
		for (short px = 0; px < width; px++) {
			UBYTE pen = src[px] & 15;
			if (!pen)
				continue;
			UBYTE color = cpcPlusPenToGameColor(pen);
			for (short sx = 0; sx < xScale; sx++)
				putPixelScroll(bitmap, (short)(x + px * xScale + sx), (short)(y + py), color);
		}
	}
}

static void drawPromotedCpcCarrierAt(UBYTE* bitmap, short x, short y) {
	// Mirrors the CPC drawnewcarrier composition:
	// back/front/top are drawn as normal Plus sprites, while body blocks used
	// the CPC mode0-wide path. Stretch those body chunks horizontally here so
	// the deck reads as one continuous carrier instead of separated tiles.
	drawCpcPlusSpriteScroll(bitmap, (short)(x + 0), (short)(y + 32), harCpcCarrierBackPixels, HAR_CPC_CARRIER_BACK_WIDTH, HAR_CPC_CARRIER_BACK_HEIGHT, 1);
	drawCpcPlusSpriteScroll(bitmap, (short)(x + 16), (short)(y + 32), harCpcCarrierBodyPixels, HAR_CPC_CARRIER_BODY_WIDTH, HAR_CPC_CARRIER_BODY_HEIGHT, 2);
	drawCpcPlusSpriteScroll(bitmap, (short)(x + 48), (short)(y + 32), harCpcCarrierBodyPixels, HAR_CPC_CARRIER_BODY_WIDTH, HAR_CPC_CARRIER_BODY_HEIGHT, 2);
	drawCpcPlusSpriteScroll(bitmap, (short)(x + 40), (short)(y + 16), harCpcCarrierTopPixels, HAR_CPC_CARRIER_TOP_WIDTH, HAR_CPC_CARRIER_TOP_HEIGHT, 1);
	drawCpcPlusSpriteScroll(bitmap, (short)(x + 56), (short)(y + 16), harCpcCarrierTop2Pixels, HAR_CPC_CARRIER_TOP_2_WIDTH, HAR_CPC_CARRIER_TOP_2_HEIGHT, 1);
	drawCpcPlusSpriteScroll(bitmap, (short)(x + 80), (short)(y + 32), harCpcCarrierFrontPixels, HAR_CPC_CARRIER_FRONT_WIDTH, HAR_CPC_CARRIER_FRONT_HEIGHT, 1);
}

/* Sprint 14.94 Part 6: replaces 6 per-pixel drawCpcPlusSpriteScrollRange()
 * calls (each looping every opaque pixel of its own piece, converting CPC
 * pen->colour and doing a bit-level read-modify-write per pixel) with up to
 * HAR_CARRIER_TILES_TALL(3) pre-masked tile blits - the 6 source pieces
 * (back/body x2/top/top2/front) were composited into one HAR_CARRIER_TILES_
 * WIDE x HAR_CARRIER_TILES_TALL tile grid once, offline, by
 * tools/cpc_promoted_sprites_to_tiles.py (which also bakes in the body
 * piece's xScale=2 stretch, done live before). Always called for exactly one
 * buffer column (physicalTileX) at a time - see the call site in
 * drawDirectColumnRangeObjects() - so compositeColumn (0..WIDE-1) directly
 * selects which column of the pre-built grid belongs at that buffer
 * position; no per-call clipping math needed since there's only ever one
 * column to draw. */
/* reversed selects the horizontally-mirrored tile set (harCarrierReversedTile
 * Data/Skip, baked offline by tools/cpc_promoted_sprites_to_tiles.py's
 * Canvas.mirrored()) for the end/landing carrier - CPC's endfrigatesprite is
 * explicitly commented as reversed so it can approach from the opposite
 * side. Both tile sets share the same grid layout/size, so compositeColumn
 * still selects the same column position in either - only the pixel content
 * underneath differs, already pre-flipped, no extra index math needed here. */
static void drawPromotedCpcCarrierRangeAt(UBYTE* bitmap, UWORD physicalTileX, UWORD compositeColumn, UBYTE reversed) {
	if (compositeColumn >= HAR_CARRIER_TILES_WIDE)
		return;
	const UBYTE* tileData;
	const UBYTE* tileSkip;
	if (carrierParkedWingmanVisible) {
		tileData = reversed ? harCarrierReversedTileData : harCarrierTileData;
		tileSkip = reversed ? harCarrierReversedTileSkip : harCarrierTileSkip;
	} else {
		tileData = reversed ? harCarrierReversedWithoutWingmanTileData :
			harCarrierWithoutWingmanTileData;
		tileSkip = reversed ? harCarrierReversedWithoutWingmanTileSkip :
			harCarrierWithoutWingmanTileSkip;
	}
	for (UBYTE row = 0; row < HAR_CARRIER_TILES_TALL; row++) {
		UWORD gridIndex = (UWORD)(row * HAR_CARRIER_TILES_WIDE + compositeColumn);
		if (tileSkip[gridIndex])
			continue;
		/* Canvas row 0 in the generator corresponds to world tile row 12
		 * (pixel Y 96 = the caller's old fixed y=80 base + the 16px/2-tile
		 * shift the generator applied so its own canvas starts at row 0 -
		 * see tools/cpc_promoted_sprites_to_tiles.py's carrier canvas
		 * comment). Only ever called with that same fixed base today. */
		drawGameScrollTileMasked(bitmap, (short)physicalTileX, (short)(12 + row), tileData + (ULONG)gridIndex * HAR_CARRIER_TILE_BYTES);
	}
}

static void drawPromotedCpcGunshipAt(UBYTE* bitmap, short x, short y) {
	drawCpcPlusSpriteScroll(bitmap, x, y, harCpcGunshipLeftPixels, HAR_CPC_GUNSHIP_LEFT_WIDTH, HAR_CPC_GUNSHIP_LEFT_HEIGHT, 1);
	drawCpcPlusSpriteScroll(bitmap, (short)(x + 16), y, harCpcGunshipRightPixels, HAR_CPC_GUNSHIP_RIGHT_WIDTH, HAR_CPC_GUNSHIP_RIGHT_HEIGHT, 1);
}

/* Shift+D debug hub -------------------------------------------------------
 * This is deliberately a normal planar screen, not an overlay on the menu:
 * it cannot corrupt the ticker's independently-scrolled bitplane pointers,
 * and graphics/SFX inspection never needs gameplay buffers or sprite DMA.
 * The graphics catalogue includes every converted 8x8 game tile followed by
 * every promoted CPC Plus sprite component. Composite objects can therefore
 * be audited down to the exact source piece that supplied their pixels. */
typedef enum DebugHubPage {
	DEBUG_HUB_CLOSED = 0,
	DEBUG_HUB_OPTIONS,
	DEBUG_HUB_GRAPHICS,
	DEBUG_HUB_SOUNDS,
	DEBUG_HUB_MUSIC
} DebugHubPage;

enum {
	DEBUG_ITEM_TELEMETRY = 0,
	DEBUG_ITEM_INFINITE_LIVES,
	DEBUG_ITEM_INFINITE_BOMBS,
	DEBUG_ITEM_INFINITE_ROCKETS,
	DEBUG_ITEM_INFINITE_FUEL,
	DEBUG_ITEM_GRAPHICS,
	DEBUG_ITEM_SOUNDS,
	DEBUG_ITEM_MUSIC,
	DEBUG_ITEM_BACK,
	DEBUG_ITEM_COUNT
};

typedef struct DebugGraphicAsset {
	const char* name;
	const UBYTE* pixels;
	UBYTE width;
	UBYTE height;
} DebugGraphicAsset;

static const DebugGraphicAsset debugPromotedGraphics[] = {
	{ "HARRIER FLYING LEFT", harCpcHarrierFlyingLeftPixels, HAR_CPC_HARRIER_FLYING_LEFT_WIDTH, HAR_CPC_HARRIER_FLYING_LEFT_HEIGHT },
	{ "HARRIER FLYING RIGHT", harCpcHarrierFlyingRightPixels, HAR_CPC_HARRIER_FLYING_RIGHT_WIDTH, HAR_CPC_HARRIER_FLYING_RIGHT_HEIGHT },
	{ "HARRIER LANDING LEFT", harCpcHarrierLandingLeftPixels, HAR_CPC_HARRIER_LANDING_LEFT_WIDTH, HAR_CPC_HARRIER_LANDING_LEFT_HEIGHT },
	{ "HARRIER LANDING RIGHT", harCpcHarrierLandingRightPixels, HAR_CPC_HARRIER_LANDING_RIGHT_WIDTH, HAR_CPC_HARRIER_LANDING_RIGHT_HEIGHT },
	{ "ENEMY FLYING LEFT", harCpcEnemyPlaneFlyingLeftPixels, HAR_CPC_ENEMY_PLANE_FLYING_LEFT_WIDTH, HAR_CPC_ENEMY_PLANE_FLYING_LEFT_HEIGHT },
	{ "ENEMY FLYING RIGHT", harCpcEnemyPlaneFlyingRightPixels, HAR_CPC_ENEMY_PLANE_FLYING_RIGHT_WIDTH, HAR_CPC_ENEMY_PLANE_FLYING_RIGHT_HEIGHT },
	{ "ENEMY BROKEN LEFT", harCpcEnemyPlaneBrokenLeftPixels, HAR_CPC_ENEMY_PLANE_BROKEN_LEFT_WIDTH, HAR_CPC_ENEMY_PLANE_BROKEN_LEFT_HEIGHT },
	{ "ENEMY BROKEN RIGHT", harCpcEnemyPlaneBrokenRightPixels, HAR_CPC_ENEMY_PLANE_BROKEN_RIGHT_WIDTH, HAR_CPC_ENEMY_PLANE_BROKEN_RIGHT_HEIGHT },
	{ "CARRIER BODY", harCpcCarrierBodyPixels, HAR_CPC_CARRIER_BODY_WIDTH, HAR_CPC_CARRIER_BODY_HEIGHT },
	{ "CARRIER BACK", harCpcCarrierBackPixels, HAR_CPC_CARRIER_BACK_WIDTH, HAR_CPC_CARRIER_BACK_HEIGHT },
	{ "CARRIER FRONT", harCpcCarrierFrontPixels, HAR_CPC_CARRIER_FRONT_WIDTH, HAR_CPC_CARRIER_FRONT_HEIGHT },
	{ "CARRIER TOP", harCpcCarrierTopPixels, HAR_CPC_CARRIER_TOP_WIDTH, HAR_CPC_CARRIER_TOP_HEIGHT },
	{ "CARRIER TOP 2", harCpcCarrierTop2Pixels, HAR_CPC_CARRIER_TOP_2_WIDTH, HAR_CPC_CARRIER_TOP_2_HEIGHT },
	{ "GUNSHIP LEFT", harCpcGunshipLeftPixels, HAR_CPC_GUNSHIP_LEFT_WIDTH, HAR_CPC_GUNSHIP_LEFT_HEIGHT },
	{ "GUNSHIP RIGHT", harCpcGunshipRightPixels, HAR_CPC_GUNSHIP_RIGHT_WIDTH, HAR_CPC_GUNSHIP_RIGHT_HEIGHT },
	{ "WINGMAN FLYING LEFT", harCpcWingmanFlyingLeftPixels, HAR_CPC_WINGMAN_FLYING_LEFT_WIDTH, HAR_CPC_WINGMAN_FLYING_LEFT_HEIGHT },
	{ "WINGMAN FLYING RIGHT", harCpcWingmanFlyingRightPixels, HAR_CPC_WINGMAN_FLYING_RIGHT_WIDTH, HAR_CPC_WINGMAN_FLYING_RIGHT_HEIGHT },
	{ "WINGMAN LANDED LEFT", harCpcWingmanLandedLeftPixels, HAR_CPC_WINGMAN_LANDED_LEFT_WIDTH, HAR_CPC_WINGMAN_LANDED_LEFT_HEIGHT },
	{ "WINGMAN LANDED RIGHT", harCpcWingmanLandedRightPixels, HAR_CPC_WINGMAN_LANDED_RIGHT_WIDTH, HAR_CPC_WINGMAN_LANDED_RIGHT_HEIGHT },
	{ "PARACHUTE", harCpcParachutePixels, HAR_CPC_PARACHUTE_WIDTH, HAR_CPC_PARACHUTE_HEIGHT }
};

#define DEBUG_PROMOTED_GRAPHIC_COUNT ((UWORD)(sizeof(debugPromotedGraphics) / sizeof(debugPromotedGraphics[0])))
#define DEBUG_GRAPHIC_COUNT ((UWORD)(GAME_TILE_COUNT + DEBUG_PROMOTED_GRAPHIC_COUNT))

static const char* const debugSfxNames[SFX_COUNT] = {
	"ROCKET FIRE",
	"BOMB DROP",
	"IMPACT",
	"PLAYER HIT",
	"EJECT",
	"PICKUP POWERUP",
	"FLAK GUN 1",
	"FLAK GUN 2",
	"GROUND HIT 1",
	"GROUND HIT 2",
	"GROUND HIT 3",
	"GROUND HIT 4",
	"CARRIER IDLE 1",
	"CARRIER IDLE 2"
};

#define DEBUG_MUSIC_COUNT 3
static const char* const debugMusicNames[DEBUG_MUSIC_COUNT] = {
	"MENU - I VOW TO THEE",
	"RAF GAME OVER",
	"CARRIER LANDING FANFARE"
};

static void drawDebugPageHeader(UBYTE* bitmap, const char* title) {
	fillScreen(bitmap, MENU_COLOR_PANEL);
	drawTextCentered(bitmap, 20, "DEBUG HUB", MENU_COLOR_GREEN);
	drawTextCentered(bitmap, 36, HAR_BUILD_LABEL, MENU_COLOR_CYAN);
	drawTextCentered(bitmap, 54, title, MENU_COLOR_WHITE);
}

static void debugToggleText(char* text, const char* label, UBYTE enabled) {
	copyMenuText(text, label);
	char* end = text + strlen(text);
	*end++ = ':';
	*end++ = ' ';
	copyMenuText(end, enabled ? "ON" : "OFF");
}

static void debugHubItemText(UBYTE item, char* text) {
	switch (item) {
		case DEBUG_ITEM_TELEMETRY:
			debugToggleText(text, "TELEMETRY", telemetryEnabled);
			break;
		case DEBUG_ITEM_INFINITE_LIVES:
			debugToggleText(text, "INFINITE LIVES", debugInfiniteLives);
			break;
		case DEBUG_ITEM_INFINITE_BOMBS:
			debugToggleText(text, "INFINITE BOMBS", debugInfiniteBombs);
			break;
		case DEBUG_ITEM_INFINITE_ROCKETS:
			debugToggleText(text, "INFINITE ROCKETS", debugInfiniteRockets);
			break;
		case DEBUG_ITEM_INFINITE_FUEL:
			debugToggleText(text, "INFINITE FUEL", debugInfiniteFuel);
			break;
		case DEBUG_ITEM_GRAPHICS:
			copyMenuText(text, "GRAPHICS BROWSER");
			break;
		case DEBUG_ITEM_SOUNDS:
			copyMenuText(text, "SOUND BROWSER");
			break;
		case DEBUG_ITEM_MUSIC:
			copyMenuText(text, "MUSIC BROWSER");
			break;
		default:
			copyMenuText(text, "BACK TO MAIN MENU");
			break;
	}
}

static short debugHubItemY(UBYTE item) {
	return (short)(78 + item * 16);
}

static void drawDebugHubItem(UBYTE* bitmap, UBYTE item, UBYTE selected) {
	char text[28];
	short y = debugHubItemY(item);
	debugHubItemText(item, text);
	fillRect(bitmap, 48, (short)(y - 2), 224, 12, MENU_COLOR_PANEL);
	if (selected)
		drawText(bitmap, 56, y, ">", MENU_COLOR_YELLOW);
	drawText(bitmap, 72, y, text,
		item <= DEBUG_ITEM_INFINITE_FUEL ? MENU_COLOR_CYAN : MENU_COLOR_WHITE);
}

static void drawDebugHub(UBYTE* bitmap, UBYTE selected) {
	drawDebugPageHeader(bitmap, "OPTIONS");
	for (UBYTE item = 0; item < DEBUG_ITEM_COUNT; item++)
		drawDebugHubItem(bitmap, item, item == selected);
	drawTextCentered(bitmap, 218, "UP/DOWN SELECT  LEFT/RIGHT CHANGE", MENU_COLOR_SHADOW);
	drawTextCentered(bitmap, 232, "SHIFT+D OR ESC BACK", MENU_COLOR_SHADOW);
}

static void drawDebugPromotedGraphicScaled(UBYTE* bitmap,
	const DebugGraphicAsset* asset) {
	UBYTE scale = asset->height > 8 ? 4 : 6;
	short width = (short)(asset->width * scale);
	short height = (short)(asset->height * scale);
	short startX = (short)((SCREEN_WIDTH - width) / 2);
	short startY = (short)(116 - height / 2);
	for (UBYTE y = 0; y < asset->height; y++) {
		for (UBYTE x = 0; x < asset->width; x++) {
			UBYTE pen = asset->pixels[(UWORD)y * asset->width + x] & 15;
			if (!pen)
				continue;
			fillRect(bitmap, (short)(startX + x * scale),
				(short)(startY + y * scale), scale, scale,
				cpcPlusPenToGameColor(pen));
		}
	}
}

static void drawDebugGameTileScaled(UBYTE* bitmap, UBYTE tileId) {
	const UBYTE scale = 8;
	const short startX = (SCREEN_WIDTH - GAME_TILE_WIDTH * scale) / 2;
	const short startY = 84;
	for (UBYTE y = 0; y < GAME_TILE_HEIGHT; y++) {
		for (UBYTE x = 0; x < GAME_TILE_WIDTH; x++)
			fillRect(bitmap, (short)(startX + x * scale),
				(short)(startY + y * scale), scale, scale,
				gameTilePixelColor(tileId, x, y));
	}
}

static void drawDebugGraphicsBrowser(UBYTE* bitmap, UWORD index) {
	drawDebugPageHeader(bitmap, "GRAPHICS BROWSER");
	if (index < GAME_TILE_COUNT) {
		drawTextCentered(bitmap, 68, "CONVERTED GAME TILE", MENU_COLOR_CYAN);
		drawDebugGameTileScaled(bitmap, (UBYTE)index);
		drawText(bitmap, 112, 168, "TILE", MENU_COLOR_WHITE);
		drawUnsignedPadded(bitmap, 160, 168, index, 3, MENU_COLOR_YELLOW);
	} else {
		const DebugGraphicAsset* asset =
			&debugPromotedGraphics[index - GAME_TILE_COUNT];
		drawTextCentered(bitmap, 68, asset->name, MENU_COLOR_CYAN);
		drawDebugPromotedGraphicScaled(bitmap, asset);
		drawTextCentered(bitmap, 174, "PROMOTED CPC SPRITE PART",
			MENU_COLOR_WHITE);
	}
	drawUnsignedPadded(bitmap, 120, 194, (ULONG)(index + 1), 3,
		MENU_COLOR_YELLOW);
	drawText(bitmap, 148, 194, "/", MENU_COLOR_WHITE);
	drawUnsignedPadded(bitmap, 164, 194, DEBUG_GRAPHIC_COUNT, 3,
		MENU_COLOR_YELLOW);
	drawTextCentered(bitmap, 220, "LEFT/RIGHT OBJECT", MENU_COLOR_WHITE);
	drawTextCentered(bitmap, 234, "ESC OR SHIFT+D BACK", MENU_COLOR_SHADOW);
}

static void drawDebugSoundBrowser(UBYTE* bitmap, UBYTE index,
	UBYTE playing) {
	drawDebugPageHeader(bitmap, "SOUND BROWSER");
	drawTextCentered(bitmap, 88, debugSfxNames[index], MENU_COLOR_CYAN);
	drawUnsignedPadded(bitmap, 120, 112, (ULONG)(index + 1), 2,
		MENU_COLOR_YELLOW);
	drawText(bitmap, 144, 112, "/", MENU_COLOR_WHITE);
	drawUnsignedPadded(bitmap, 160, 112, SFX_COUNT, 2, MENU_COLOR_YELLOW);
	drawTextCentered(bitmap, 142, playing ? "PLAYING" : "FIRE TO PLAY",
		playing ? MENU_COLOR_GREEN : MENU_COLOR_WHITE);
	drawTextCentered(bitmap, 158, "PAULA VOICES  0/3 LEFT  1/2 RIGHT",
		MENU_COLOR_SHADOW);
	for (UBYTE channel = 0; channel < SFX_CHANNEL_COUNT; channel++) {
		WORD x = (channel & 1) ? 168 : 16;
		WORD y = (channel < 2) ? 174 : 188;
		char channelLabel[4] = { 'C', 'H', (char)('0' + channel), 0 };
		drawText(bitmap, x, y, channelLabel, MENU_COLOR_WHITE);
		drawText(bitmap, (WORD)(x + 28), y,
			sfxChannelIsLeft(channel) ? "L" : "R", MENU_COLOR_CYAN);
		if (channel == ENGINE_CHANNEL && engineActive) {
			drawText(bitmap, (WORD)(x + 44), y, "ENGINE",
				MENU_COLOR_GREEN);
		} else if (sfxChannelCurrentId[channel] < SFX_COUNT) {
			drawText(bitmap, (WORD)(x + 44), y, "ID", MENU_COLOR_WHITE);
			drawUnsignedPadded(bitmap, (WORD)(x + 64), y,
				sfxChannelCurrentId[channel], 2, MENU_COLOR_YELLOW);
			drawText(bitmap, (WORD)(x + 88), y, "P", MENU_COLOR_WHITE);
			drawUnsignedPadded(bitmap, (WORD)(x + 100), y,
				sfxChannelPriority[channel], 1, MENU_COLOR_YELLOW);
		} else {
			drawText(bitmap, (WORD)(x + 44), y, "ID --",
				MENU_COLOR_SHADOW);
		}
	}
	drawTextCentered(bitmap, 204, "LEFT/RIGHT SOUND", MENU_COLOR_WHITE);
	drawTextCentered(bitmap, 220, "FIRE PLAY SAMPLE", MENU_COLOR_WHITE);
	drawTextCentered(bitmap, 236, "ESC OR SHIFT+D BACK", MENU_COLOR_SHADOW);
}

static void drawDebugMusicBrowser(UBYTE* bitmap, UBYTE index,
	UBYTE playing) {
	drawDebugPageHeader(bitmap, "MUSIC BROWSER");
	drawTextCentered(bitmap, 92, debugMusicNames[index], MENU_COLOR_CYAN);
	drawUnsignedPadded(bitmap, 120, 116, (ULONG)(index + 1), 2,
		MENU_COLOR_YELLOW);
	drawText(bitmap, 144, 116, "/", MENU_COLOR_WHITE);
	drawUnsignedPadded(bitmap, 160, 116, DEBUG_MUSIC_COUNT, 2,
		MENU_COLOR_YELLOW);
	drawTextCentered(bitmap, 148, playing ? "PLAYING" : "FIRE TO PLAY",
		playing ? MENU_COLOR_GREEN : MENU_COLOR_WHITE);
	drawTextCentered(bitmap, 174, "FOUR PAULA CHANNEL MOD",
		MENU_COLOR_SHADOW);
	drawTextCentered(bitmap, 204, "LEFT/RIGHT MUSIC", MENU_COLOR_WHITE);
	drawTextCentered(bitmap, 220, "FIRE PLAY FROM START", MENU_COLOR_WHITE);
	drawTextCentered(bitmap, 236, "ESC OR SHIFT+D BACK", MENU_COLOR_SHADOW);
}

/* Sprint 14.94 Part 6: same idea as drawPromotedCpcCarrierRangeAt() above,
 * for the 2-piece (left/right) gunship. baseTileRow is already in tile units
 * (levelObjectRowForColumnObject()'s result, passed straight through by the
 * caller) - the gunship canvas has no vertical shift baked in (unlike the
 * carrier's +16px/2-tile one), since both pieces share the same y in the
 * original per-pixel calls. */
static void drawPromotedCpcGunshipRangeAt(UBYTE* bitmap, UWORD physicalTileX, UWORD compositeColumn, short baseTileRow) {
	if (compositeColumn >= HAR_GUNSHIP_TILES_WIDE)
		return;
	for (UBYTE row = 0; row < HAR_GUNSHIP_TILES_TALL; row++) {
		UWORD gridIndex = (UWORD)(row * HAR_GUNSHIP_TILES_WIDE + compositeColumn);
		if (harGunshipTileSkip[gridIndex])
			continue;
		drawGameScrollTileMasked(bitmap, (short)physicalTileX, (short)(baseTileRow + row), harGunshipTileData + (ULONG)gridIndex * HAR_CARRIER_TILE_BYTES);
	}
}

static void drawHorizonCarrierAt(UBYTE* bitmap, short x) {
#if HAR_USE_PROMOTED_CPC_PLUS_ASSETS
	drawPromotedCpcCarrierAt(bitmap, x, 80);
#else
	const short deckY = 113;

	fillRectScroll(bitmap, x + 6, deckY, 86, 2, GAME_COLOR_LIGHT_GREY);
	fillRectScroll(bitmap, x + 0, deckY + 2, 102, 3, GAME_COLOR_DARK_GREY);
	fillRectScroll(bitmap, x + 20, deckY + 1, 18, 1, GAME_COLOR_WHITE);
	fillRectScroll(bitmap, x + 46, deckY + 1, 12, 1, GAME_COLOR_WHITE);

	for (short row = 0; row < 8; row++) {
		short left = (short)(x + 8 + row * 3);
		short width = (short)(86 - row * 8);
		UBYTE color = row < 3 ? GAME_COLOR_DARK_GREY : GAME_COLOR_BLACK;
		if (width > 0)
			fillRectScroll(bitmap, left, (short)(deckY + 5 + row), width, 1, color);
	}

	fillRectScroll(bitmap, x + 62, deckY - 12, 8, 12, GAME_COLOR_DARK_GREY);
	fillRectScroll(bitmap, x + 70, deckY - 9, 12, 4, GAME_COLOR_DARK_GREY);
	fillRectScroll(bitmap, x + 66, deckY - 18, 2, 6, GAME_COLOR_DARK_GREY);
	fillRectScroll(bitmap, x + 58, deckY - 2, 20, 2, GAME_COLOR_LIGHT_GREY);
#endif
}

static void drawNativeDeckTileAt(UBYTE* bitmap, short tileX, short tileY) {
	short x = (short)(tileX * GAME_TILE_WIDTH);
	short y = (short)(tileY * GAME_TILE_HEIGHT + 3);

	fillRectScroll(bitmap, x, y, GAME_TILE_WIDTH, 2, GAME_COLOR_LIGHT_GREY);
	fillRectScroll(bitmap, x, (short)(y + 2), GAME_TILE_WIDTH, 3, GAME_COLOR_DARK_GREY);
	if ((tileX & 3) == 0)
		fillRectScroll(bitmap, (short)(x + 3), (short)(y + 5), 2, 5, GAME_COLOR_DARK_GREY);
}

static UBYTE seaTileForColumn(LONG worldColumn, UWORD tileY) {
	/* Sprint 14.97 PRI 9: CPC selects sea surface tile from R:
	 * `ld a,r; and #03; add 3` → tile 3 + (R & 3), range 3-6.
	 * Previously used a worldColumn/row-derived pattern with modulo-11
	 * rules — visually plausible but not CPC's random selection. Now uses
	 * the modeled R state, same as grass/hill/town decisions. Only the
	 * surface row (GAME_SEA_TOP_TILE_Y) uses the R-based variant; deeper
	 * rows stay as the simpler tile 3 fill. */
	if (tileY == GAME_SEA_TOP_TILE_Y) {
		UBYTE rState = cpcRStateForWorldColumn(worldColumn);
		return (UBYTE)(3 + (rState & 3));
	}
	return 3;
}

static const LevelSegmentDef* levelSegmentForWorldColumn(LONG worldColumn) {
	for (UWORD index = 0; index < sizeof(harLevelRoute) / sizeof(harLevelRoute[0]); index++) {
		const LevelSegmentDef* segment = &harLevelRoute[index];
		if (worldColumn >= segment->startColumn && worldColumn <= segment->endColumn)
			return segment;
	}
	return 0;
}

static UBYTE terrainKindForStage(UBYTE stage) {
	switch (stage) {
		case HAR_STAGE_DO_LAND:
			return HAR_TERRAIN_CPC_RANDOM_LAND;
		case HAR_STAGE_DESCEND_MOUNTAINS:
			return HAR_TERRAIN_CPC_DESCEND_TO_TOWN;
		case HAR_STAGE_FLAT_TOWNLAND:
		case HAR_STAGE_GENERATE_BUILDING:
			return HAR_TERRAIN_TOWN;
		default:
			return HAR_TERRAIN_SEA;
	}
}

static UBYTE stageForWorldColumn(LONG worldColumn, const LevelSegmentDef* segment) {
#if HAR_DEBUG_FORCE_STAGE >= 0
	(void)worldColumn;
	(void)segment;
	return (UBYTE)HAR_DEBUG_FORCE_STAGE;
#else
	if (worldColumn < 0 || !segment)
		return HAR_STAGE_OPEN_SEA;
	return segment->stage;
#endif
}

/* Mirrors the CPC's stage-3 land dispatcher (l9134/drawflatterrain/mode 0-3
 * at HarrierAttackSourceNew2_alt_CRTC_CART16.asm:5423-5505): height starts at
 * row CPC_LAND_PROCEDURAL_BASELINE and each column is one of 4 modes: stay
 * flat, step 1 row back toward the baseline, step 1 row up toward
 * CPC_LAND_PROCEDURAL_FLOOR, or (mode 3) insert a ground target - debounced
 * so two mode-3 columns in a row don't both place one
 * (checkinsertenemylandtile, :5499-5505: skips if the previous column was
 * also a target-insert column). Target type is 2 more pseudo-random bits,
 * matching the R-register-driven index into enemylandsprites (:5605-5619).
 *
 * l8859 is not an independent byte or an alias for Z80 R: it immediately
 * follows currtime in memory, while genrandomhl reads and writes a 16-bit
 * word at currtime. It is therefore the high byte of that PRNG state.
 * Expanding genrandomhl's Z80 shifts gives the exact recurrence
 * state = state * 1509 + 0x29 (mod 65536), advanced once per new column.
 * `ld a,(l8859); rra; rra; and 3` then selects state bits 10-11.
 * This is precomputed into tables once, rather than walked live, because
 * callers query arbitrary/non-sequential columns (e.g. neighbour lookahead
 * for slope tiles). */
static UBYTE cpcLandHeightTable[CPC_LAND_PROCEDURAL_LENGTH];
static UBYTE cpcLandTargetTable[CPC_LAND_PROCEDURAL_LENGTH];
/* Sprint 14.95 Part 3/4: the actual tile chosen at generation time for each
 * column, stored directly instead of re-derived later by comparing
 * neighbouring columns' heights (landSurfaceTileForColumn()'s old approach)
 * - that re-derivation could tag two consecutive columns as a slope tile for
 * a single real height change (e.g. heights 14,14,13,13 read as "hill up" at
 * both the transition column and the column after it), which the real CPC
 * never does (only the column where the height dispatcher's mode 1/2 branch
 * actually fires gets a slope tile). */
static UBYTE cpcLandSurfaceTable[CPC_LAND_PROCEDURAL_LENGTH];

/* Sprint 14.103: records which transition the height dispatcher actually
 * took for each column, purely for validation/diagnostics - doesn't change
 * any generation behaviour. Added while investigating a "terrain looks too
 * abrupt, especially uphills" report: the recommended first step was to make
 * the generator's own transition type explicit and checkable against the
 * tile it chose, rather than guessing whether the height algorithm or the
 * tile graphics/anchor is at fault. */
typedef enum CpcLandTransition {
	CPC_LAND_FLAT = 0,
	CPC_LAND_CLIMB = 1,   /* mode 2: height-- (asm hill-up, tiles 24-27) */
	CPC_LAND_DESCEND = 2, /* mode 1: height++ (asm hill-down, tiles 28-31) */
	CPC_LAND_TARGET = 3
} CpcLandTransition;
static UBYTE cpcLandTransitionTable[CPC_LAND_PROCEDURAL_LENGTH];

/* Coverage-ordered fallback used by deterministic non-procedural transitions.
 * CPC-procedural hills select their 24-27/28-31 variant randomly at the
 * actual height-change event, matching `ld a,r; rra; and 3`. */
static const UBYTE hillPhaseByCoverage[4] = { 2, 3, 0, 1 };

#define CPC_LAND_TARGET_NONE 0
#define CPC_LAND_TARGET_RADAR 1
#define CPC_LAND_TARGET_LAUNCHER 2
#define CPC_LAND_TARGET_GUN 3
#define CPC_LAND_TARGET_TANK_FRONT 4
#define CPC_LAND_TARGET_TANK_REAR 5

/* Flak altitude range, expressed as a ROW OFFSET ABOVE THE TERRAIN (the
 * hand-placed level_route.h entries use offsets 5/6, which is where this
 * range's minimum starts) rather than a fixed tile-choice enum - user asked
 * for flak to be able to appear at any altitude up to near the Harrier's own
 * ceiling, not just hug the ground, otherwise players can simply fly high to
 * dodge all of it. Which of the two flak sprites (57/58) to draw is derived
 * from the offset's own parity at spawn time. Sprint 14.95 Part 2: this
 * altitude range is kept as-is (already tuned from user feedback) even
 * though the spawn TRIGGER below was rearchitected from a precomputed
 * per-column lookahead table to a live roll at the screen's right edge,
 * matching real CPC's launchflakattack (:6033-6095), which runs continuously
 * from checkenemyattacks and places flak directly into the currently-visible
 * screen tilemap rather than any lookahead structure. */
#define CPC_LAND_FLAK_MIN_ROW_OFFSET 5
#define CPC_LAND_FLAK_MAX_ROW_OFFSET 20

/* Sprint 14.95 Part 3/4: real CPC difficulty scaling for the land section's
 * maximum height (leveldifficulty, :5423-5429 per the review) - minimum row
 * (highest the terrain can climb) = 12 - skillLevel, so skill 1 caps at row
 * 11 (3 tiles above baseline 14) up to skill 5 capping at row 7 (7 tiles).
 * Previously a fixed CPC_LAND_PROCEDURAL_FLOOR(11) regardless of skill,
 * capping every difficulty to the same 3-tile max. skillLevel itself was
 * already selectable in the menu but had never actually been wired into
 * gameplay anywhere until now. */
static UBYTE cpcLandMinimumRow(UBYTE skillLevel) {
	UBYTE row = (UBYTE)(12 - skillLevel);
	return row < 2 ? 2 : row;
}

/* Set by startGameSession() from the menu's skillLevel before (re-)triggering
 * generation - the lazy cpcLandProceduralXxx() accessors below have no way to
 * pass a parameter through, so the level to generate for has to live here
 * instead. Defaults to skill 1 so a direct/early call before any session has
 * started still produces a sane table. */
static UBYTE cpcLandSkillLevel = 1;

/* One CPC genrandomhl state per generated world column. The table preserves
 * the single sequential RNG while allowing the Amiga ring renderer and
 * collision code to query columns out of order. Terrain, targets and live
 * flak all read different bits from this same per-column state. */
/* Important direction (confirmed with the user): the real Amstrad game does
 * NOT generate the same landscape on every run - comparing multiple real CPC
 * playthroughs shows genuinely different terrain/target/flak layouts each
 * time. So the goal here was never a fixed, reproducible Amiga map either -
 * it's for the GENERATING ALGORITHM to match CPC's (same genrandomhl LCG,
 * same mode dispatch, same gating rules), fed by a seed that varies every
 * session, same as the real machine effectively gets fresh unpredictable
 * state each run. See resetCpcRandomSequence() below for the land generator
 * algorithm itself - terrain, targets, clouds, flak and town all share this
 * one sequence, not separate ones, per CPC's own single genrandomhl source.
 *
 * CPC's genrandomhl recurrence is `state = state * 1509 + 0x29` (verified
 * against a real WinAPE-captured trace during development - not shipped,
 * see AMIGA_PORT_PLAN.md). CPC_RANDOM_INITIAL_STATE is kept only as the
 * specific value that trace was captured with (so the recurrence itself can
 * be re-verified later if needed) - actual runtime seeding reads frameCounter
 * instead, below, so every session's world differs like the real game's. */
#define CPC_RANDOM_INITIAL_STATE 0x2F08
/* Sprint 14.97 PRI 8: updated from 106 to 102 to match the shortened
 * COAST_RISE segment (now 100-101 instead of 100-105). Procedural land
 * now starts 4 columns earlier, and the RNG state for each column is
 * looked up by the correct absolute world column. */
#define CPC_LAND_PROCEDURAL_WORLD_START 102
static UWORD cpcRandomStateByColumn[GAME_LEVEL_WIDTH_TILES];
static UBYTE cpcRandomSequenceReady = 0;
static void resetCpcTownBlockTable(void);

/* Sprint 14.97 PRI 5: Z80 R register state per generated column. R is a 7-bit
 * refresh counter (0-127) that increments once per M1 (opcode fetch) cycle.
 * CPC reads it at specific decision points:
 *   grass variant:    `ld a,r; and 3; add #20`           → tile 32-35 (R & 3)
 *   hill down:        `ld a,r; rra; and 3; add #1c`       → tile 28-31 ((R>>1)&3)
 *   hill up:           `ld a,(l8859); rra; and 3; add #18` → tile 24-27 (l8859>>1 & 3) — uses genrandomhl, NOT R
 *   target type:       `ld a,r; rra; and #0c; rra; rra; ld (l884b),a` → (R>>3)&3
 *   town building:     `ld a,r; rra; rra; and #07`        → (R>>2)&7
 *   sea tile:          `ld a,r; and #03; add 3`            → tile 3-6 (R & 3)
 *   flak gate:         `ld a,r; and #0f`                  → (R & 0x0f)
 * Previously the Amiga mixed genrandomhl bits as a substitute for R, which
 * preserved determinism but broke CPC's correlation: R and l8859 advance
 * independently, so CPC can produce the same grass variant on two columns
 * with very different terrain heights, which a shared-state model can't.
 * Modeled as a linear counter with a path-dependent increment per column —
 * R is inherently a linear counter, not a PRNG, so this is the correct
 * shape. cpcRStateByColumn stores R at the start of the generated column,
 * before that column's modeled opcode-fetch cost is applied. This is the
 * value used by the terrain decision; later CPC reads (notably flak) occur
 * after additional instructions and remain an explicitly documented
 * approximation until their M1 offsets have been derived from the assembly.
 * Calibrated against a real instrumented CPC run (Sprint 14.101 - a LOGGEN
 * cartridge variant that tags every l9134 dispatch with which path it took
 * and captures R immediately before/after that path, so rExit-rEntry mod
 * 128 is the real M1-fetch count for that specific path, no manual
 * instruction counting needed). Measured across two independent play
 * sessions (498 real logged land columns total, ~249-250 each): every path
 * clusters in roughly the same range (individual-session means ~48-79,
 * combined grand mean 64.3, median 60) with no statistically meaningful
 * separation between paths in either sample, because the dominant source
 * of variance turned out to be l914e's own fill-loop cost (which scales
 * with 15-height, the same for every path that reaches it), not the
 * deciding code itself - the second session reproduced the first's shape
 * independently, so this isn't a one-sample fluke. Per the project's own
 * direction here: the goal is reproducing the generating algorithm's
 * statistical behaviour close enough (roughly 95%+), not bit-exact
 * landscape replay, so this collapses to one calibrated constant (63,
 * within noise of both samples) instead of preserving the earlier guessed
 * per-path split the data didn't actually support. Revisit only if a much
 * larger sample later shows a real per-path difference worth modelling.
 * See AMIGA_PORT_PLAN.md Sprint 14.101 for the full writeup. */
static UBYTE cpcRStateByColumn[GAME_LEVEL_WIDTH_TILES];
#define CPC_R_MASK 0x7f
/* Sprint 14.102: sea got its own LOGGEN instrumentation (drawseatiles'
 * single ld a,r, no branching) alongside land's. Measured extremely tight
 * across 100 real sea columns from one session: 99/100 exactly 7, one
 * outlier at 27 (almost certainly an interrupt landing between the rEntry/
 * rExit capture, not a real second code path - drawseatiles has none).
 * CPC_R_COST_SEA is therefore a genuinely calibrated value, unlike
 * CPC_R_COST_DEFAULT below. CPC_R_COST_DEFAULT still only covers town,
 * which the same session's log buffer filled up (250 records shared across
 * land/sea/flak/town) before ever reaching - still an unverified
 * assumption, carried over unchanged from before any calibration work.
 * Do not treat CPC_R_COST_DEFAULT as calibrated until town gets its own
 * dedicated LOGGEN run (its instrumentation already exists in
 * HarrierAttackSourceNew2_alt_CRTC_CART16.asm - buildportstanley - it just
 * hasn't been reached by a session yet without the log filling up first). */
#define CPC_R_COST_SEA 7        /* measured (asm:drawseatiles, n=100, mean 7.2) */
#define CPC_R_COST_DEFAULT 63  /* town only now - UNMEASURED, assumed */
#define CPC_R_COST_FLAT 63     /* land: flat, or a blocked hill/target degenerating to flat */
#define CPC_R_COST_HILL 63     /* land: hill up/down actually stepping */
#define CPC_R_COST_TARGET 63   /* land: successful target insertion */
#define CPC_R_COST_TANK_REAR 63 /* CPC's next-tick continuation of the tank sprite block */

/* CPC clouds are streamed multi-column tile blocks, not moving sprites.
 * Store only the chosen top row and block column for each world column;
 * this preserves the sequential l885a/l885b state while keeping arbitrary
 * ring-buffer redraws deterministic and cheap. */
#define CPC_CLOUD_NONE 0xff
#define CPC_CLOUD_SMALL_WIDTH 5
#define CPC_CLOUD_LARGE_WIDTH 9
#define CPC_CLOUD_SEA_DELAY_COLUMNS 80
static const UBYTE cpcCloudSmall[CPC_CLOUD_SMALL_WIDTH][3] = {
	{ 0, 77, 80 }, { 0, 78, 81 }, { 0, 79, 82 },
	{ 0, 83, 85 }, { 0, 84, 86 }
};
static const UBYTE cpcCloudLarge[CPC_CLOUD_LARGE_WIDTH][3] = {
	{ 0, 77, 80 }, { 0, 78, 81 }, { 0, 79, 82 },
	{ 87, 91, 93 }, { 88, 3, 94 }, { 89, 3, 95 },
	{ 90, 92, 96 }, { 83, 85, 0 }, { 84, 86, 0 }
};
static UBYTE cpcCloudTopRowByColumn[GAME_LEVEL_WIDTH_TILES];
/* Bit 7 selects the large block; bits 0-6 hold its column. */
static UBYTE cpcCloudBlockColumnByColumn[GAME_LEVEL_WIDTH_TILES];

static UBYTE terrainKindForCloudColumn(LONG worldColumn) {
	const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
	UBYTE stage = stageForWorldColumn(worldColumn, segment);
	return segment ? segment->terrainKind : terrainKindForStage(stage);
}

static void generateCpcCloudTable(void) {
	UBYTE active = 0;
	UBYTE topRow = 0;
	UBYTE large = 0;
	UBYTE blockColumn = 0;
	UBYTE seaDelay = 0;
	UBYTE cloudsEnabled = 0;

	memset(cpcCloudTopRowByColumn, CPC_CLOUD_NONE, sizeof(cpcCloudTopRowByColumn));
	memset(cpcCloudBlockColumnByColumn, 0, sizeof(cpcCloudBlockColumnByColumn));

	for (UWORD column = 0; column < GAME_LEVEL_WIDTH_TILES; column++) {
		UBYTE terrainKind = terrainKindForCloudColumn((LONG)column);
		UWORD rng = cpcRandomStateByColumn[column];

		/* CPC resets this on every generated sea column. An in-progress
		 * cloud cannot cross back into a sea section. */
		if (terrainKind == HAR_TERRAIN_SEA) {
			seaDelay = CPC_CLOUD_SEA_DELAY_COLUMNS;
			cloudsEnabled = 0;
			active = 0;
			continue;
		}

		if (!cloudsEnabled) {
			if (seaDelay) {
				seaDelay--;
				/* The column that changes the palette still draws no cloud. */
				if (!seaDelay)
					cloudsEnabled = 1;
				continue;
			}
			cloudsEnabled = 1;
		}

		if (active) {
			UBYTE width = large ? CPC_CLOUD_LARGE_WIDTH : CPC_CLOUD_SMALL_WIDTH;
			/* CPC consumes the 255 terminator on a separate generator
			 * round, guaranteeing a blank column between cloud blocks. */
			if (blockColumn >= width) {
				active = 0;
				continue;
			}
		} else {
			UBYTE currtime = (UBYTE)rng;
			UBYTE l8859 = (UBYTE)(rng >> 8);
			if ((currtime & 0x70) != 0)
				continue;
			topRow = (UBYTE)(l8859 & 3);
			if (!topRow)
				continue;
			large = (UBYTE)((l8859 & 8) ? 0 : 1);
			blockColumn = 0;
			active = 1;
		}

		cpcCloudTopRowByColumn[column] = topRow;
		cpcCloudBlockColumnByColumn[column] =
			(UBYTE)((large ? 0x80 : 0) | blockColumn);
		blockColumn++;
	}
}

static UBYTE cpcTargetTypeForRState(UBYTE rState);

/* Important direction (confirmed with the user): the real Amstrad game does
 * NOT generate the same landscape on every run, so exact reproduction of any
 * one captured trace was never the goal - what matters is that the ALGORITHM
 * matches CPC's, fed by a seed that varies every session. A stricter, later
 * correction narrowed this further: land height/targets must NOT be walked
 * as their own separate, independently-seeded sequence (as an earlier pass
 * of this file did, seeding land from `frameCounter ^ 0x9E17` while clouds/
 * flak/town read the frameCounter-seeded cpcRandomStateByColumn/
 * cpcRStateByColumn tables) - that made the two only superficially similar,
 * since real CPC has exactly one shared genrandomhl/currtime sequence and
 * one shared R counter feeding every subsystem. Land generation is therefore
 * folded into this same single per-world-column walk below, so terrain,
 * targets, clouds, flak and town all read the *same* sequence at the *same*
 * position - same seed now genuinely means same landscape end to end, and a
 * new seed means a new but equally CPC-like one.
 *
 * LOGGEN POLICY: captured LOGGEN rows are observations used to compare the
 * distribution and timing of this algorithm. They must never be imported as
 * a map, a lookup sequence, or any other runtime world data. The Amiga world
 * must always be produced by the reconstructed CPC algorithms below. */

#if HAR_DEBUG_LAND_LOG
static void landLogBuild(void) {
	landLogBufferUsed = 0;
	{
		static const char header[] = "index,height,surfaceTile,transition\n";
		landLogAppend(header, sizeof(header) - 1);
	}
	for (UWORD i = 0; i < CPC_LAND_PROCEDURAL_LENGTH; i++) {
		char line[24];
		char* out = line;
		out = appendUnsignedLong(out, i);
		*out++ = ',';
		out = appendUnsignedLong(out, cpcLandHeightTable[i]);
		*out++ = ',';
		out = appendUnsignedLong(out, cpcLandSurfaceTable[i]);
		*out++ = ',';
		out = appendUnsignedLong(out, cpcLandTransitionTable[i]);
		*out++ = '\n';
		landLogAppend(line, (UWORD)(out - line));
	}
}
#endif
static void resetCpcRandomSequence(void) {
	/* Session-random seed (frameCounter at the moment a new game starts,
	 * itself unpredictable since it depends on how long the player sat at
	 * the menu) - not CPC_RANDOM_INITIAL_STATE, so every playthrough's whole
	 * world (terrain, targets, clouds, flak, town) differs from the last,
	 * same as the real Amstrad's does. */
	UWORD state = frameCounter;
	/* A real Z80's refresh register at mission start depends on the opcode
	 * fetches performed while the player was in the menu. Amiga has no R
	 * register, so derive a varying 7-bit starting point from the same
	 * session seed. Fixed seeds may still be used by validation builds, but
	 * captured LOGGEN values are never runtime input. */
	UBYTE rState = (UBYTE)((state ^ (state >> 7)) & CPC_R_MASK);

	/* Land generator state (asm:5433-5521 l9134, asm:5621-5665
	 * insertenemylandtile) - persists across the whole walk below but only
	 * evolves once `column` reaches CPC_LAND_PROCEDURAL_WORLD_START. See the
	 * (former, now-merged) generateCpcLandHeightTable() history in
	 * AMIGA_PORT_PLAN.md Sprint 14.99/14.100 for the verification this
	 * mode/height/gating logic was checked against. */
	UBYTE landHeight = CPC_LAND_PROCEDURAL_BASELINE;
	UBYTE landFloorRow = cpcLandMinimumRow(cpcLandSkillLevel);
	UBYTE landLastMode = 1;      /* l8861's compiled-in initial value (asm:218) */
	UBYTE landJustInserted = 0;  /* forces flat the column right after any target */
	UBYTE landPendingTankRear = 0;
	/* Sprint 14.104 introduced a deliberate Amiga-only reversal-smoothing
	 * rule here (blocking a climb/descend that would immediately undo the
	 * previous column's slope), adopted after a captured CPC trace showed a
	 * lower reversal rate than this port's generator and a direct visual
	 * comparison confirmed real CPC reads smoother. Sprint 14.105 found and
	 * fixed the actual cause instead - descend was drawing at the wrong
	 * (post-step) height row, one row lower than the real asm:5500-5525
	 * draws it (see drawHeight's own comment below) - and confirmed visually
	 * that this alone resolved the "steep little dump" look. The smoothing
	 * rule was a workaround for that timing bug, not a real CPC difference;
	 * removed now that the actual bug is fixed, restoring the generator to
	 * an unmodified reproduction of l9134/l9167/l9181's real mode dispatch. */

	for (UWORD column = 0; column < GAME_LEVEL_WIDTH_TILES; column++) {
		/* CPC calls genrandomhl before generating the newly revealed column,
		 * unconditionally, every tick regardless of sea/land/town stage. */
		state = (UWORD)(state * 1509U + 0x0029U);
		UBYTE l8859 = (UBYTE)(state >> 8);
		UBYTE rCost = (terrainKindForCloudColumn((LONG)column) == HAR_TERRAIN_SEA)
			? CPC_R_COST_SEA : CPC_R_COST_DEFAULT;

		LONG landLocalColumn = (LONG)column - CPC_LAND_PROCEDURAL_WORLD_START;
		if (landLocalColumn >= 0 && landLocalColumn < CPC_LAND_PROCEDURAL_LENGTH) {
			UWORD i = (UWORD)landLocalColumn;
			UBYTE surface;
			UBYTE transition = CPC_LAND_FLAT;
			/* Sprint 14.105: landHeight is l885d, the *persisted* state read
			 * by the next column's climb/descend comparisons - it is NOT
			 * always the same row this column actually draws at. Verified
			 * directly against asm:5500-5525 (l916a, descend/mode 1): H (the
			 * row l914e draws to) is loaded from l885d *before* the height
			 * is updated and is never touched again - only `a`, which gets
			 * stored back to l885d, changes. So descend draws at the OLD
			 * height and only the *next* column sees the incremented one.
			 * Climb (asm:5527-5559, l9181/mode 2) is different: it does
			 * `dec h` explicitly, so it draws at the NEW height immediately.
			 * This port previously drew both modes at the post-step height,
			 * which for descend put the tile one row lower than the real
			 * game does - exactly the "steep little dump" a direct visual
			 * comparison caught. drawHeight defaults to landHeight (correct
			 * for every other case) and is only set to the OLD value in the
			 * descend branch below. */
			UBYTE drawHeight = landHeight;
			cpcLandTargetTable[i] = CPC_LAND_TARGET_NONE;

			if (landPendingTankRear) {
				/* CPC continuation, not an Amiga invention. The first
				 * insertenemylandtile call draws bytes 00,2d vertically and
				 * stores the advanced DE in l885e. On the next scroll tick
				 * gamelevelprogress=4 resumes at l9206/l91cb and draws
				 * 00,2e at the fresh right edge. The visible tank therefore
				 * spans two world columns even though drawspriteblock3 itself
				 * advances rows within each column. */
				landPendingTankRear = 0;
				cpcLandTargetTable[i] = CPC_LAND_TARGET_TANK_REAR;
				surface = (UBYTE)(32 + (rState & 3));
				rCost = CPC_R_COST_TANK_REAR;
				transition = CPC_LAND_TARGET;
			} else if (i == 0) {
				/* asm:5409-5431 startoffalklandisland: one-shot land-entry
				 * transition column - draws fixed join tiles, not mode
				 * dispatch. */
				landHeight = CPC_LAND_PROCEDURAL_BASELINE;
				drawHeight = landHeight;
				surface = (UBYTE)(32 + (rState & 3));
				rCost = CPC_R_COST_FLAT;
			} else {
				UBYTE mode = (UBYTE)((l8859 >> 2) & 3);
				if (landJustInserted)
					mode = 0;
				landJustInserted = 0;

				if (mode == 1) {
					/* Descend toward baseline. Blocked at the baseline must
					 * draw flat, not a sloped tile with no actual height
					 * change - otherwise the tile geometry contradicts
					 * itself (a slope where the ground didn't move).
					 * drawHeight needs no update here - it still holds the
					 * value landHeight had before this branch ran, which is
					 * exactly the OLD height asm:5500-5525 draws at; only
					 * landHeight itself (the persisted l885d equivalent)
					 * advances, for the next column to read. */
					if (landHeight < CPC_LAND_PROCEDURAL_BASELINE) {
						landHeight++;
						surface = (UBYTE)(28 + ((rState >> 1) & 3));
						rCost = CPC_R_COST_HILL;
						transition = CPC_LAND_DESCEND;
					} else {
						surface = (UBYTE)(32 + (rState & 3));
						rCost = CPC_R_COST_FLAT;
					}
				} else if (mode == 2) {
					/* Climb toward the skill's minimum row - same
					 * blocked-must-be-flat rule as mode 1 above. Unlike
					 * descend, climb's asm (l9181) does `dec h` explicitly,
					 * drawing the new height immediately - drawHeight must
					 * be refreshed here since its default (captured before
					 * this branch ran) is still the old value. */
					if (landHeight > landFloorRow) {
						landHeight--;
						drawHeight = landHeight;
						surface = (UBYTE)(24 + ((l8859 >> 1) & 3));
						rCost = CPC_R_COST_HILL;
						transition = CPC_LAND_CLIMB;
					} else {
						surface = (UBYTE)(32 + (rState & 3));
						rCost = CPC_R_COST_FLAT;
					}
				} else if (mode == 3 && (landLastMode & 1) == 0) {
					/* Insert a ground target - gated on the previous
					 * column's dispatched mode being even (l8861 gating,
					 * asm:5515-5521); confirmed empirically (21/21 real
					 * insertions) that the column right after any
					 * successful insertion is always forced flat too -
					 * landJustInserted enforces that above. */
					UBYTE type = cpcTargetTypeForRState(rState);
					if (type == 3 && i + 1 < CPC_LAND_PROCEDURAL_LENGTH) {
						cpcLandTargetTable[i] = CPC_LAND_TARGET_TANK_FRONT;
						landPendingTankRear = 1;
					} else {
						cpcLandTargetTable[i] = (UBYTE)(CPC_LAND_TARGET_RADAR + type);
					}
					landJustInserted = 1;
					surface = (UBYTE)(32 + (rState & 3));
					rCost = CPC_R_COST_TARGET;
					transition = CPC_LAND_TARGET;
				} else {
					mode = 0;
					surface = (UBYTE)(32 + (rState & 3));
					rCost = CPC_R_COST_FLAT;
				}
				landLastMode = mode;
			}

			cpcLandHeightTable[i] = drawHeight;
			cpcLandSurfaceTable[i] = surface;
			cpcLandTransitionTable[i] = transition;
		}

		cpcRandomStateByColumn[column] = state;
		/* Store the value actually visible to this column's terrain path.
		 * The former ordering stored the post-column value, shifting every
		 * R-based lookup away from the terrain that selected it. */
		cpcRStateByColumn[column] = rState;
		/* R advances independently of genrandomhl, by however much code ran
		 * this tick - see the CPC_R_COST_* comment above. */
		rState = (UBYTE)((rState + rCost) & CPC_R_MASK);
	}
	cpcRandomSequenceReady = 1;
	resetCpcTownBlockTable();
	generateCpcCloudTable();
#if HAR_DEBUG_LAND_LOG
	landLogBuild();
#endif

#if HAR_DEBUG_PERF_LOG
	/* Sanity check: CPC's real height table only ever steps by 1 per
	 * column (hill up/down), so any larger jump means this reconstruction
	 * has a bug - not something that should ever legitimately fire. */
	for (UWORD i = 1; i < CPC_LAND_PROCEDURAL_LENGTH; i++) {
		WORD delta = (WORD)cpcLandHeightTable[i] - (WORD)cpcLandHeightTable[i - 1];
		if (delta > 1 || delta < -1) {
			char line[64];
			char* out = line;
			*out++ = 'l'; *out++ = 'a'; *out++ = 'n'; *out++ = 'd';
			*out++ = ' '; *out++ = 'h'; *out++ = 'e'; *out++ = 'i';
			*out++ = 'g'; *out++ = 'h'; *out++ = 't'; *out++ = ' ';
			*out++ = 'j'; *out++ = 'u'; *out++ = 'm'; *out++ = 'p';
			*out++ = ' '; *out++ = '@'; *out++ = ' ';
			out = appendUnsignedLong(out, i);
			*out++ = ':'; *out++ = ' ';
			out = appendUnsignedLong(out, cpcLandHeightTable[i - 1]);
			*out++ = '-'; *out++ = '>';
			out = appendUnsignedLong(out, cpcLandHeightTable[i]);
			*out++ = '\n';
			*out = 0;
			KPrintF(line);
		}
	}

	/* Sprint 14.103: cross-check the chosen tile against the transition it's
	 * supposed to represent - added while investigating a "terrain looks too
	 * abrupt, especially uphills" report, to rule in/out the height algorithm
	 * itself (this check) before suspecting the converted tile graphics or
	 * their vertical anchor (which this can't detect - only a genuinely
	 * mismatched tile ID for the transition type). Climbs must land in the
	 * 24-27 tile group; descends in 28-31; flat/target columns must never
	 * use either group. None of these should ever fire - if one does, the
	 * generator's mode/tile pairing has an actual bug, not just a
	 * presentation issue.
	 *
	 * Sprint 14.105: this now checks cpcLandTransitionTable[i] directly
	 * instead of re-deriving the transition from a height delta. Once
	 * descend was fixed to draw at the OLD height (matching asm:5500-5525 -
	 * see drawHeight's own comment above), the *visible* height change for a
	 * descend column shows up one column later than the transition that
	 * caused it, so a delta-based check here would misfire on every real
	 * descend. The explicit transition table isn't affected by that timing
	 * shift - it's set at the same index as the tile choice either way. */
	for (UWORD i = 0; i < CPC_LAND_PROCEDURAL_LENGTH; i++) {
		UBYTE transition = cpcLandTransitionTable[i];
		UBYTE tile = cpcLandSurfaceTable[i];
		const char* problem = 0;
		if (transition == CPC_LAND_CLIMB && (tile < 24 || tile > 27))
			problem = "LAND: climb uses wrong tile @ ";
		else if (transition == CPC_LAND_DESCEND && (tile < 28 || tile > 31))
			problem = "LAND: descend uses wrong tile @ ";
		else if ((transition == CPC_LAND_FLAT || transition == CPC_LAND_TARGET) && tile >= 24 && tile <= 31)
			problem = "LAND: flat column uses slope tile @ ";
		if (problem) {
			char line[80];
			char* out = line;
			const char* p = problem;
			while (*p)
				*out++ = *p++;
			out = appendUnsignedLong(out, i);
			*out++ = ' '; *out++ = '('; *out++ = 't'; *out++ = '=';
			out = appendUnsignedLong(out, tile);
			*out++ = ')'; *out++ = '\n';
			*out = 0;
			KPrintF(line);
		}
	}
#endif
}

static UWORD cpcRandomStateForWorldColumn(LONG worldColumn) {
	if (!cpcRandomSequenceReady)
		resetCpcRandomSequence();
	if (worldColumn < 0)
		worldColumn = 0;
	if (worldColumn >= GAME_LEVEL_WIDTH_TILES)
		worldColumn = GAME_LEVEL_WIDTH_TILES - 1;
	return cpcRandomStateByColumn[worldColumn];
}

/* Modeled Z80 R value at the start of the given generated column. This is
 * exact within our reconstructed column walk for early terrain decisions.
 * Consumers reached later in CPC's instruction path currently use it as the
 * nearest deterministic approximation; they must eventually use explicit
 * decision-point offsets, not LOGGEN-derived lookup rows. */
static UBYTE cpcRStateForWorldColumn(LONG worldColumn) {
	if (!cpcRandomSequenceReady)
		resetCpcRandomSequence();
	if (worldColumn < 0)
		worldColumn = 0;
	if (worldColumn >= GAME_LEVEL_WIDTH_TILES)
		worldColumn = GAME_LEVEL_WIDTH_TILES - 1;
	return cpcRStateByColumn[worldColumn];
}

static UBYTE cpcCloudTileAtColumnRow(LONG worldColumn, WORD tileY) {
	UBYTE topRow;
	UBYTE encodedColumn;
	UBYTE blockColumn;
	UBYTE row;
	if (!cpcRandomSequenceReady)
		resetCpcRandomSequence();
	if (worldColumn < 0 || worldColumn >= GAME_LEVEL_WIDTH_TILES)
		return 0;
	topRow = cpcCloudTopRowByColumn[worldColumn];
	if (topRow == CPC_CLOUD_NONE || tileY < topRow || tileY >= topRow + 3)
		return 0;
	encodedColumn = cpcCloudBlockColumnByColumn[worldColumn];
	blockColumn = (UBYTE)(encodedColumn & 0x7f);
	row = (UBYTE)(tileY - topRow);
	if (encodedColumn & 0x80)
		return blockColumn < CPC_CLOUD_LARGE_WIDTH ? cpcCloudLarge[blockColumn][row] : 0;
	return blockColumn < CPC_CLOUD_SMALL_WIDTH ? cpcCloudSmall[blockColumn][row] : 0;
}

/*
 * CPC chooses the ground-target variant from the Z80 R refresh register.
 * Sprint 14.97 PRI 5: now uses the modeled R state (cpcRStateForWorldColumn)
 * instead of mixing genrandomhl bits. CPC's insertenemylandtile
 * (asm:5606-5619): `ld a,r; rra; and #0c; rra; rra; ld (l884b),a` gives
 * target type = (R >> 3) & 3.
 */
static UBYTE cpcTargetTypeForRState(UBYTE rState) {
	return (UBYTE)((rState >> 3) & 3);
}

static UBYTE cpcLandProceduralProfile(UWORD index) {
	if (!cpcRandomSequenceReady)
		resetCpcRandomSequence();
	return cpcLandHeightTable[index];
}

static UBYTE cpcLandProceduralTarget(UWORD index) {
	if (!cpcRandomSequenceReady)
		resetCpcRandomSequence();
	return cpcLandTargetTable[index];
}

static UBYTE cpcLandProceduralSurface(UWORD index) {
	if (!cpcRandomSequenceReady)
		resetCpcRandomSequence();
	return cpcLandSurfaceTable[index];
}

/* Sprint 14.95 Part 5: real CPC builds the town continuously - pick a random
 * block of 8 (townspritestable, `ld a,r; rra; rra; and 7`), draw it column
 * by column, and pick the next one immediately when it's done, repeating
 * until the town stage ends. The Amiga port previously only had 17 hand-
 * placed HAR_OBJ_TOWN_BLOCK harLevelObjects entries in level_route.h -
 * placed back-to-back with real gaps between them (e.g. block widths sum to
 * only 44 of the town segment's 200 columns), reading as sparse and mostly
 * empty rather than a dense city. Precomputed the same way as
 * generateCpcLandHeightTable() - keyed by LOCAL column within the town
 * segment, not world column, since callers query it from both rendering
 * (one column at a time) and collision (arbitrary probe points). */
#define CPC_TOWN_PROCEDURAL_LENGTH 200
/* Small gap before the first building, matching the existing hand-placed
 * data's own first block starting 2 columns into the segment rather than
 * immediately at the coast-to-town seam. */
#define CPC_TOWN_PROCEDURAL_START_MARGIN 2
#define CPC_TOWN_BLOCK_NONE 0xff
static UBYTE townBlockForColumn[CPC_TOWN_PROCEDURAL_LENGTH];
static UBYTE townBlockLocalColumnForColumn[CPC_TOWN_PROCEDURAL_LENGTH];
static UBYTE townBlockTableReady = 0;

static void resetCpcTownBlockTable(void) {
	townBlockTableReady = 0;
}

static void generateCpcTownBlockTable(void) {
	/* Sprint 14.97 PRI 5: CPC selects town buildings from R:
	 * `ld a,r; rra; rra; and #07` → blockId = (R >> 2) & 7.
	 * Previously used a separate local LCG (seed 0x3c91, * 25173 + 13849),
	 * which was Amiga-specific and had no correlation to CPC's R-driven
	 * sequence. Now uses the same modeled R state as all other R-based
	 * decisions, keyed by the world column of each town position. */
	memset(townBlockForColumn, CPC_TOWN_BLOCK_NONE, sizeof(townBlockForColumn));

	/* Town segment starts at column 411 in the current route. The R state
	 * for each town column is looked up by absolute world column, matching
	 * how CPC's R would have advanced by the time the town section's column
	 * generation runs. Bounded approximation, not yet measured: real CPC
	 * only picks a new building once the previous spriteblock has finished
	 * drawing, so the real R value at each block-choice point depends on
	 * how much code the previous block's own drawing cost - a per-world-
	 * column lookup like this can't capture that block-to-block dependency.
	 * This still gives a varied, session-deterministic town layout; the
	 * exact block sequence is an approximation until town's own ld a,r
	 * point gets its own LOGGEN instrumentation (see AMIGA_PORT_PLAN.md
	 * Sprint 14.101's "not yet calibrated" list). */
	const LONG townWorldStart = 411;

	UWORD i = CPC_TOWN_PROCEDURAL_START_MARGIN;
	while (i < CPC_TOWN_PROCEDURAL_LENGTH) {
		UBYTE rState = cpcRStateForWorldColumn(townWorldStart + i);
		UBYTE blockId = (UBYTE)((rState >> 2) & 7);
		UBYTE width = harCpcTownBlockWidths[blockId];
		UWORD remaining = (UWORD)(CPC_TOWN_PROCEDURAL_LENGTH - i);
		/* CPC completes spriteblockptr before it tests the town countdown.
		 * Never emit a partial building at the harbour boundary. */
		if (width > remaining)
			break;
		for (UBYTE col = 0; col < width; col++, i++) {
			townBlockForColumn[i] = blockId;
			townBlockLocalColumnForColumn[i] = col;
		}
	}
	townBlockTableReady = 1;
}

static UBYTE cpcTownProceduralBlockId(UWORD index) {
	if (!townBlockTableReady)
		generateCpcTownBlockTable();
	return townBlockForColumn[index];
}

static UBYTE cpcTownProceduralLocalColumn(UWORD index) {
	if (!townBlockTableReady)
		generateCpcTownBlockTable();
	return townBlockLocalColumnForColumn[index];
}

static UBYTE terrainYForWorldColumn(LONG worldColumn, const LevelSegmentDef* segment, UBYTE terrainKind) {
	LONG localColumn = segment ? worldColumn - segment->startColumn : worldColumn;
	if (localColumn < 0)
		localColumn = 0;

	switch (terrainKind) {
		case HAR_TERRAIN_COAST_RISE:
			/* Sprint 14.97 PRI 8: CPC does a 1+1 coast transition:
			 * one solid tile at row 15, one hill-up at row 14. Was 6
			 * columns (100-105) which was noticeably longer than CPC. */
			if (localColumn == 0)
				return 15;
			return 14;
		case HAR_TERRAIN_MOUNTAINS:
			return (UBYTE)(12 + ((worldColumn + (worldColumn >> 1)) & 3));
		case HAR_TERRAIN_TOWN:
			return 14;
		case HAR_TERRAIN_COAST_FALL:
			/* Sprint 14.97 PRI 6: CPC's setcloudcolourtosea (asm:5790-5804)
			 * draws a 3x2 solid land block (tile 1) at rows 14-15 as the
			 * transition from town to pier, then immediately streams
			 * pendata (JHIJHIJHIJHI). Replaces the old 6-column
			 * approximation. First 3 columns: solid land at row 14
			 * (row 15 is sea fill below). After that: sea (255). */
			if (localColumn < 3)
				return 14;
			return 255;
		case HAR_TERRAIN_CPC_RANDOM_LAND: {
			if (localColumn >= CPC_LAND_PROCEDURAL_LENGTH)
				localColumn = CPC_LAND_PROCEDURAL_LENGTH - 1;
			return cpcLandProceduralProfile((UWORD)localColumn);
		}
		case HAR_TERRAIN_CPC_DESCEND_TO_TOWN: {
			/* Sprint 14.97 PRI 3: CPC continues from the last generated
			 * land height and steps one row down per column until reaching
			 * 14, rather than a fixed 10-column script. The descend length
			 * adapts to how high the terrain was when land ended - skill 5
			 * with row 7 terrain needs 7 descend columns, not the same 3
			 * that skill 1 with row 11 terrain does. */
			UBYTE lastLandHeight = cpcLandProceduralProfile(CPC_LAND_PROCEDURAL_LENGTH - 1);
			UBYTE descendTo = (UBYTE)(lastLandHeight + localColumn);
			if (descendTo > CPC_LAND_PROCEDURAL_BASELINE)
				descendTo = CPC_LAND_PROCEDURAL_BASELINE;
			return descendTo;
		}
		default:
			return 255;
	}
}

static UBYTE landSurfaceTileForColumn(LONG worldColumn, UBYTE terrainKind) {
	/* Sprint 14.95 Part 3/4: for the CPC-procedural land section, the tile
	 * chosen at generation time (generateCpcLandHeightTable()) is now stored
	 * directly and just looked up here - no more re-deriving it by comparing
	 * this column's height against its neighbours', which could (and did)
	 * tag two consecutive columns as a slope tile for one real height
	 * change. Other terrain kinds (coast rise/fall, town, descend-to-town)
	 * aren't generated by that table and keep the neighbour-comparison
	 * fallback below - their height formulas are simple deterministic
	 * functions with no per-column random-mode ambiguity to begin with, so
	 * there's no equivalent bug there. */
	if (terrainKind == HAR_TERRAIN_CPC_RANDOM_LAND) {
		const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
		LONG localColumn = segment ? worldColumn - segment->startColumn : worldColumn;
		if (localColumn < 0)
			localColumn = 0;
		if (localColumn >= CPC_LAND_PROCEDURAL_LENGTH)
			localColumn = CPC_LAND_PROCEDURAL_LENGTH - 1;
		return cpcLandProceduralSurface((UWORD)localColumn);
	}

	const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
	UBYTE currentY = terrainYForWorldColumn(worldColumn, segment, terrainKind);
	UBYTE previousY = currentY;
	UBYTE nextY = currentY;
	const LevelSegmentDef* previousSegment = levelSegmentForWorldColumn(worldColumn - 1);
	const LevelSegmentDef* nextSegment = levelSegmentForWorldColumn(worldColumn + 1);
	UBYTE previousKind = previousSegment ? previousSegment->terrainKind : terrainKindForStage(stageForWorldColumn(worldColumn - 1, previousSegment));
	UBYTE nextKind = nextSegment ? nextSegment->terrainKind : terrainKindForStage(stageForWorldColumn(worldColumn + 1, nextSegment));

#if HAR_DEBUG_FORCE_STAGE >= 0
	previousKind = terrainKindForStage((UBYTE)HAR_DEBUG_FORCE_STAGE);
	nextKind = terrainKindForStage((UBYTE)HAR_DEBUG_FORCE_STAGE);
#endif

	if (previousSegment || worldColumn > 0)
		previousY = terrainYForWorldColumn(worldColumn - 1, previousSegment, previousKind);
	nextY = terrainYForWorldColumn(worldColumn + 1, nextSegment, nextKind);

	UBYTE hillPhase = hillPhaseByCoverage[currentY & 3];

	if (terrainKind == HAR_TERRAIN_COAST_RISE)
		return 24 + hillPhase;
	if (terrainKind == HAR_TERRAIN_COAST_FALL)
		return 28 + hillPhase;
	if (previousY == 255 && currentY != 255)
		return 24 + hillPhase;
	if (nextY == 255 && currentY != 255)
		return 28 + hillPhase;
	if (currentY < previousY || nextY < currentY)
		return 24 + hillPhase;
	if (currentY > previousY || nextY > currentY)
		return 28 + hillPhase;
	if (terrainKind == HAR_TERRAIN_TOWN)
		return 1;
	return 32 + ((worldColumn + (worldColumn >> 2)) & 3);
}

static WORD landSurfaceYForWorldColumn(LONG worldColumn) {
	const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
	UBYTE stage = stageForWorldColumn(worldColumn, segment);
	UBYTE terrainKind = segment ? segment->terrainKind : terrainKindForStage(stage);
#if HAR_DEBUG_FORCE_STAGE >= 0
	terrainKind = terrainKindForStage(stage);
#endif
	UBYTE terrainY = terrainYForWorldColumn(worldColumn, segment, terrainKind);
	if (terrainY == 255 || terrainY >= GAME_SEA_TOP_TILE_Y)
		return -1;
	return (WORD)terrainY;
}

static void resetDestroyedTargets(void) {
	destroyedTargetCount = 0;
}

static UBYTE isTargetDestroyedAtColumn(LONG worldColumn) {
	if (worldColumn < 0)
		return 0;
	for (UBYTE index = 0; index < destroyedTargetCount; index++) {
		if (destroyedTargetColumns[index] == (UWORD)worldColumn)
			return 1;
	}
	return 0;
}

static void markTargetDestroyedAtColumn(LONG worldColumn) {
	if (worldColumn < 0 || isTargetDestroyedAtColumn(worldColumn))
		return;
	if (destroyedTargetCount >= GAME_DESTROYED_TARGET_MAX)
		return;
	destroyedTargetColumns[destroyedTargetCount++] = (UWORD)worldColumn;
}

static void resetDestroyedShipColumns(void) {
	destroyedShipCellCount = 0;
	shipWreckSmokeCount = 0;
}

static BYTE enemyShipGroupIndexForColumn(LONG worldColumn) {
	if (worldColumn < 0)
		return -1;
	for (UBYTE index = 0; index < GAME_ENEMY_SHIP_GROUP_COUNT; index++) {
		if ((UWORD)worldColumn >= enemyShipGroups[index].startColumn && (UWORD)worldColumn <= enemyShipGroups[index].endColumn)
			return (BYTE)index;
	}
	return -1;
}

/* Sprint 14.95 Part 6/7: CPC's bombhitenemyship replaces only the struck
 * tile with smoke - there's no whole-ship or whole-column removal (confirmed
 * by direct inspection of checkenemyhit/bombhitenemyship, see
 * AMIGA_PORT_PLAN.md). Tracks destroyed (column,row) cells individually
 * instead of blanket-destroying every tile in a hit column, so a ship with
 * two vertically-stacked tiles in the same column (e.g. the start group's
 * columns 51/52 in level_route.h) only loses the specific tile actually
 * hit. */
static UBYTE isShipCellDestroyed(LONG worldColumn, WORD tileY) {
	if (worldColumn < 0 || tileY < 0)
		return 0;
	for (UBYTE index = 0; index < destroyedShipCellCount; index++) {
		if (destroyedShipCellColumns[index] == (UWORD)worldColumn && destroyedShipCellRows[index] == (UBYTE)tileY)
			return 1;
	}
	return 0;
}

static UBYTE markShipCellDestroyed(LONG worldColumn, WORD tileY) {
	if (worldColumn < 0 || tileY < 0 || isShipCellDestroyed(worldColumn, tileY))
		return 0;
	if (destroyedShipCellCount >= GAME_DESTROYED_SHIP_CELL_MAX)
		return 0;
	destroyedShipCellColumns[destroyedShipCellCount] = (UWORD)worldColumn;
	destroyedShipCellRows[destroyedShipCellCount] = (UBYTE)tileY;
	destroyedShipCellCount++;
	return 1;
}

static UBYTE shipWreckSmokeTileAtColumnRow(LONG worldColumn, WORD tileY) {
	if (worldColumn < 0 || tileY < 0)
		return 0;
	for (UBYTE index = 0; index < shipWreckSmokeCount; index++) {
		if (shipWreckSmokeColumns[index] == (UWORD)worldColumn && shipWreckSmokeRows[index] == (UBYTE)tileY)
			return shipWreckSmokeTiles[index];
	}
	return 0;
}

static UBYTE markShipWreckSmokeAtColumnRow(LONG worldColumn, WORD tileY, UBYTE tile) {
	if (worldColumn < 0 || tileY < 0 || tileY >= GAME_OBJECT_MAP_HEIGHT_TILES)
		return 0;
	if (shipWreckSmokeTileAtColumnRow(worldColumn, tileY))
		return 0;
	if (shipWreckSmokeCount >= GAME_SHIP_WRECK_SMOKE_MAX)
		return 0;
	shipWreckSmokeColumns[shipWreckSmokeCount] = (UWORD)worldColumn;
	shipWreckSmokeRows[shipWreckSmokeCount] = (UBYTE)tileY;
	shipWreckSmokeTiles[shipWreckSmokeCount] = tile;
	shipWreckSmokeCount++;
	return 1;
}

/* Sprint 14.95 Part 7 correction: CPC's drawsmokesprite (asm:6259-6279) does
 * `dec h` where H is the Y coord (getskytilemapid's comment at asm:4812
 * confirms "H = Y COORD, L = X COORD") - so tile 51 goes ONE ROW ABOVE the
 * struck cell, in the SAME column. This port previously placed it one column
 * to the LEFT (worldColumn-1, same row), which detached the upper smoke from
 * the lower smoke and could overwrite an unrelated neighbour cell. Verified
 * against the CPC source directly. */
static void addCpcHitSmokeAtColumnRow(LONG worldColumn, WORD tileY) {
	ObjectCell aboveCell;
	markShipWreckSmokeAtColumnRow(worldColumn, tileY, GAME_SHIP_WRECK_SMOKE_TILE_B);
	if (tileY > 0 && objectCellForWorldColumnTile(worldColumn, tileY - 1, &aboveCell) && aboveCell.id == HAR_OBJ_SKY)
		markShipWreckSmokeAtColumnRow(worldColumn, tileY - 1, GAME_SHIP_WRECK_SMOKE_TILE_A);
}

/* CPC's bombhitenemyship (checkenemyhit) has no whole-ship health counter -
 * every successful hit just replaces the exact struck tile with smoke and
 * awards points. Kept CPC-style tile-based destruction rather than the
 * previously-considered unified ship-HP model (see AMIGA_PORT_PLAN.md). */
static UBYTE damageEnemyShipAtColumnRow(LONG worldColumn, WORD tileY) {
	UBYTE changed = markShipCellDestroyed(worldColumn, tileY);
	addCpcHitSmokeAtColumnRow(worldColumn, tileY);
	return changed;
}

static void dirtyRedrawEnemyShipGroup(UBYTE** worldBuffers, LONG worldColumn) {
	BYTE groupIndex = enemyShipGroupIndexForColumn(worldColumn);
	if (groupIndex < 0) {
		dirtyRedrawWorldColumn(worldBuffers, worldColumn);
		return;
	}
	for (UWORD column = enemyShipGroups[(UBYTE)groupIndex].startColumn; column <= enemyShipGroups[(UBYTE)groupIndex].endColumn; column++)
		dirtyRedrawWorldColumn(worldBuffers, column);
}

static void resetLandCraters(void) {
	landCraterCount = 0;
}

static UBYTE isLandCraterAtColumnRow(LONG worldColumn, WORD tileY) {
	if (worldColumn < 0 || tileY < 0)
		return 0;
	for (UBYTE index = 0; index < landCraterCount; index++) {
		if (landCraterColumns[index] == (UWORD)worldColumn && landCraterRows[index] == (UBYTE)tileY)
			return 1;
	}
	return 0;
}

static UBYTE markLandCraterAtColumnRow(LONG worldColumn, WORD tileY) {
	if (worldColumn < 0 || tileY < 0 || tileY >= GAME_SEA_TOP_TILE_Y)
		return 0;
	if (isLandCraterAtColumnRow(worldColumn, tileY))
		return 0;
	if (landCraterCount >= GAME_LAND_CRATER_MAX)
		return 0;
	landCraterColumns[landCraterCount] = (UWORD)worldColumn;
	landCraterRows[landCraterCount] = (UBYTE)tileY;
	landCraterCount++;
	return 1;
}

static UBYTE objectCellForWorldColumnTile(LONG worldColumn, WORD tileY, ObjectCell* outCell) {
	if (worldColumn < 0 || tileY < 0 || tileY >= GAME_OBJECT_MAP_HEIGHT_TILES)
		return 0;
	const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
	UBYTE stage = stageForWorldColumn(worldColumn, segment);
	UBYTE terrainKind = segment ? segment->terrainKind : terrainKindForStage(stage);
#if HAR_DEBUG_FORCE_STAGE >= 0
	terrainKind = terrainKindForStage(stage);
#endif
	UBYTE terrainY = terrainYForWorldColumn(worldColumn, segment, terrainKind);
	UBYTE cloudTile = tileY < GAME_SEA_TOP_TILE_Y ?
		cpcCloudTileAtColumnRow(worldColumn, tileY) : 0;

	outCell->id = tileY < GAME_SEA_TOP_TILE_Y ?
		(cloudTile ? HAR_OBJ_CLOUD : HAR_OBJ_SKY) : HAR_OBJ_SEA;
	outCell->tile = tileY < GAME_SEA_TOP_TILE_Y ?
		cloudTile : seaTileForColumn(worldColumn, (UWORD)tileY);
	outCell->flags = 0;
	outCell->hp = 0;

	if (terrainKind == HAR_TERRAIN_COAST_FALL) {
		LONG localColumn = segment ? worldColumn - segment->startColumn : worldColumn;
		if (localColumn >= 0 && localColumn < 3 && (tileY == 14 || tileY == 15)) {
			outCell->id = HAR_OBJ_LAND;
			outCell->tile = 1;
			return 1;
		}
	}

	if (terrainY != 255 && tileY >= terrainY && (terrainKind != HAR_TERRAIN_COAST_FALL || tileY < GAME_SEA_TOP_TILE_Y)) {
		outCell->id = HAR_OBJ_LAND;
		outCell->tile = isLandCraterAtColumnRow(worldColumn, tileY) ? GAME_LAND_CRATER_TILE : (tileY == terrainY ? landSurfaceTileForColumn(worldColumn, terrainKind) : 1);
		return 1;
	}

	UBYTE smokeTile = shipWreckSmokeTileAtColumnRow(worldColumn, tileY);
	if (smokeTile) {
		outCell->id = HAR_OBJ_SMOKE;
		outCell->tile = smokeTile;
		outCell->flags = 0;
		outCell->hp = 0;
		return 1;
	}

	for (UBYTE index = harLevelObjectFirstIndexForColumn(worldColumn); index != HAR_LEVEL_OBJECT_COLUMN_INDEX_NONE; index = harLevelObjectNext[index]) {
		const LevelObjectDef* object = &harLevelObjects[index];
		WORD row;

		if (object->id == HAR_OBJ_GROUND_TARGET && isTargetDestroyedAtColumn(worldColumn))
			continue;
		if (object->rowMode == HAR_ROW_TERRAIN_RELATIVE) {
			if (terrainY == 255)
				continue;
			row = (WORD)terrainY + object->row;
		} else {
			row = object->row;
		}
		if (object->id == HAR_OBJ_ENEMY_SHIP && isShipCellDestroyed(worldColumn, row))
			continue;
		if (row == tileY) {
			outCell->id = object->id;
			outCell->tile = object->tile;
			outCell->flags = object->flags;
			outCell->hp = object->hp;
			return 1;
		}
	}

	if (terrainKind == HAR_TERRAIN_CPC_RANDOM_LAND && terrainY != 255 && !isTargetDestroyedAtColumn(worldColumn)) {
		LONG localColumn = segment ? worldColumn - segment->startColumn : worldColumn;
		if (localColumn >= 0 && localColumn < CPC_LAND_PROCEDURAL_LENGTH) {
			UBYTE target = cpcLandProceduralTarget((UWORD)localColumn);
			if (target != CPC_LAND_TARGET_NONE && tileY == terrainY - 1) {
				static const UBYTE targetTiles[] = { 42, 43, 44, 45, 46 };
				outCell->id = HAR_OBJ_GROUND_TARGET;
				outCell->tile = targetTiles[target - 1];
				outCell->flags = 0;
				outCell->hp = 1;
				return 1;
			}
		}
	}

	/* Sprint 14.95 Part 2: runtime flak (spawned live at the screen's right
	 * edge by trySpawnFlak(), matching real CPC's launchflakattack) replaces
	 * the old precomputed per-column lookahead tables - applies uniformly to
	 * both land and town stages since spawn eligibility was already decided
	 * at spawn time, not lookup time. */
	UBYTE runtimeFlakTile = runtimeFlakTileAtColumnRow(worldColumn, tileY);
	if (runtimeFlakTile) {
		outCell->id = HAR_OBJ_FLAK;
		outCell->tile = runtimeFlakTile;
		outCell->flags = 0;
		outCell->hp = 0;
		return 1;
	}

	if (tileY == GAME_HORIZON_TILE_Y) {
		if (!harLevelObjectIndexReady)
			buildHarLevelObjectIndex();
		for (UBYTE wideIndex = 0; wideIndex < harWideObjectCount; wideIndex++) {
			const LevelObjectDef* object = &harLevelObjects[harWideObjectIndex[wideIndex]];
			if (object->id != HAR_OBJ_OWN_FRIGATE || !(object->flags & HAR_OBJECT_FLAG_NATIVE_CARRIER))
				continue;
			if (worldColumn < object->column || worldColumn >= object->column + 12)
				continue;
			outCell->id = HAR_OBJ_OWN_FRIGATE;
			outCell->tile = 0;
			outCell->flags = HAR_OBJECT_FLAG_NATIVE_DECK;
			outCell->hp = 0;
			return 1;
		}
	}

	return 1;
}

static UBYTE objectCellForWorldPoint(const GameState* game, WORD screenX, WORD screenY, ObjectCell* outCell, LONG* outWorldColumn, WORD* outTileY) {
	if (screenY < 0 || screenY >= HUD_TOP)
		return 0;

	LONG worldPixelX = (LONG)game->scrollX + screenX;
	LONG worldColumn = worldPixelX >> 3;
	WORD tileY = screenY >> 3;
	if (outWorldColumn)
		*outWorldColumn = worldColumn;
	if (outTileY)
		*outTileY = tileY;
	return objectCellForWorldColumnTile(worldColumn, tileY, outCell);
}

static UBYTE enemyShipCellNearWorldPoint(const GameState* game, WORD screenX, WORD screenY, WORD minColumnOffset, WORD maxColumnOffset, WORD minRowOffset, WORD maxRowOffset, ObjectCell* outCell, LONG* outWorldColumn, WORD* outTileY) {
	if (screenY < 0 || screenY >= HUD_TOP)
		return 0;

	LONG worldPixelX = (LONG)game->scrollX + screenX;
	LONG centerColumn = worldPixelX >> 3;
	WORD centerTileY = screenY >> 3;

	for (WORD rowOffset = minRowOffset; rowOffset <= maxRowOffset; rowOffset++) {
		for (WORD columnOffset = minColumnOffset; columnOffset <= maxColumnOffset; columnOffset++) {
			ObjectCell cell;
			LONG worldColumn = centerColumn + columnOffset;
			WORD tileY = centerTileY + rowOffset;
			if (!objectCellForWorldColumnTile(worldColumn, tileY, &cell))
				continue;
			if (cell.id != HAR_OBJ_ENEMY_SHIP)
				continue;
			if (outCell)
				*outCell = cell;
			if (outWorldColumn)
				*outWorldColumn = worldColumn;
			if (outTileY)
				*outTileY = tileY;
			return 1;
		}
	}

	return 0;
}

static UBYTE ownFrigateCellNearWorldPoint(const GameState* game, WORD screenX, WORD screenY, ObjectCell* outCell, LONG* outWorldColumn, WORD* outTileY) {
	if (screenY < 0 || screenY >= HUD_TOP)
		return 0;

	LONG worldPixelX = (LONG)game->scrollX + screenX;
	LONG centerColumn = worldPixelX >> 3;
	WORD centerTileY = screenY >> 3;

	for (UWORD index = 0; index < sizeof(harLevelObjects) / sizeof(harLevelObjects[0]); index++) {
		const LevelObjectDef* object = &harLevelObjects[index];
		WORD row;

		if (object->id != HAR_OBJ_OWN_FRIGATE)
			continue;
		if (object->rowMode != HAR_ROW_ABSOLUTE)
			continue;

		row = object->row;
		if (centerTileY < row - 1 || centerTileY > row + 1)
			continue;

		if (object->flags & HAR_OBJECT_FLAG_NATIVE_CARRIER) {
			if (centerColumn < object->column || centerColumn >= object->column + 12)
				continue;
		} else if (centerColumn != object->column) {
			continue;
		}

		if (outCell) {
			outCell->id = HAR_OBJ_OWN_FRIGATE;
			outCell->tile = object->tile;
			outCell->flags = object->flags;
			outCell->hp = object->hp;
		}
		if (outWorldColumn)
			*outWorldColumn = centerColumn;
		if (outTileY)
			*outTileY = centerTileY;
		return 1;
	}

	return 0;
}

static UBYTE playerOnOwnFrigateDeck(const GameState* game) {
	ObjectCell cell;
	LONG worldColumn;
	WORD tileY;
	WORD probeY = (WORD)(game->playerY + PLAYER_SPRITE_HEIGHT + 1);

	if (game->takeoffState != TAKEOFF_STATE_AIRBORNE || game->crashTimer || game->gameOver)
		return 0;

	if (ownFrigateCellNearWorldPoint(game, (WORD)(game->playerX + 4), probeY, &cell, &worldColumn, &tileY))
		return 1;
	if (ownFrigateCellNearWorldPoint(game, (WORD)(game->playerX + PLAYER_SPRITE_WIDTH - 4), probeY, &cell, &worldColumn, &tileY))
		return 1;
	return 0;
}

static UBYTE playerOnNativeCarrierDeckPixels(const GameState* game) {
	if (game->takeoffState != TAKEOFF_STATE_AIRBORNE || game->crashTimer || game->gameOver)
		return 0;

	WORD playerBottom = (WORD)(game->playerY + PLAYER_SPRITE_HEIGHT);
	/* CPC accepts only its exact landing row (#0d). The Amiga moves in 2px
	 * steps, so accept contact through one pixel into the deck, but never
	 * accept the previous step with visible air below the aircraft. The
	 * collision path snaps an accepted position back onto the exact row. */
	if (playerBottom < CARRIER_DECK_PIXEL_Y ||
		playerBottom > CARRIER_DECK_PIXEL_Y + 2)
		return 0;

	LONG playerLeftWorld = (LONG)game->scrollX + game->playerX + 2;
	LONG playerRightWorld = (LONG)game->scrollX + game->playerX + PLAYER_SPRITE_WIDTH - 2;

	/* Sprint 14.94 Part 3: same NATIVE_CARRIER match criteria as
	 * isWideLevelObject(), so this per-frame check (unlike the rendering
	 * hot path) can reuse harWideObjectIndex[] directly too. */
	if (!harLevelObjectIndexReady)
		buildHarLevelObjectIndex();
	for (UBYTE wideIndex = 0; wideIndex < harWideObjectCount; wideIndex++) {
		const LevelObjectDef* object = &harLevelObjects[harWideObjectIndex[wideIndex]];
		if (object->id != HAR_OBJ_OWN_FRIGATE || !(object->flags & HAR_OBJECT_FLAG_NATIVE_CARRIER))
			continue;
		LONG deckLeftWorld = (LONG)object->column * GAME_TILE_WIDTH;
		LONG deckRightWorld = deckLeftWorld + CARRIER_DECK_PIXEL_WIDTH;
		if (playerRightWorld >= deckLeftWorld && playerLeftWorld < deckRightWorld)
			return 1;
	}

	return 0;
}

static UBYTE playerProbeOnOwnFrigate(const GameState* game, WORD screenX, WORD screenY) {
	ObjectCell cell;
	LONG worldColumn;
	WORD tileY;
	if (!ownFrigateCellNearWorldPoint(game, screenX, screenY, &cell, &worldColumn, &tileY))
		return 0;
	if (tileY != GAME_HORIZON_TILE_Y)
		return 0;
	if (screenY < (WORD)(GAME_HORIZON_TILE_Y * GAME_TILE_HEIGHT - 1))
		return 0;
	(void)worldColumn;
	return 1;
}

static UBYTE replenishPlayerFromFrigate(GameState* game) {
	UBYTE changed = 0;
	UBYTE onNativeCarrier = playerOnNativeCarrierDeckPixels(game);
	UBYTE onMappedFrigate = playerOnOwnFrigateDeck(game);

	/* During the final hover/landing sequence, the tile-map probe is too
	 * coarse vertically and can rearm the player one movement step above the
	 * visible deck. Require the native carrier's pixel-precise contact test
	 * there. Keep the broader probe for ordinary friendly-frigate service
	 * during gameplay. */
	if (!onNativeCarrier &&
		(!onMappedFrigate || game->landingState == LANDING_STATE_HOVER))
		return 0;

	/* Only the pixel-precise native-carrier test may complete landing. The
	 * broader object-map deck probe remains useful for refuel/rearm, but its
	 * cell-sized vertical reach used to display LANDED before touchdown. */
	if (!game->missionComplete && onNativeCarrier &&
		game->landingState == LANDING_STATE_HOVER &&
		(!game->wingman.active ||
		 (game->wingman.mode == WINGMAN_LANDING_DECK &&
		  game->wingman.screenY ==
			(WORD)(CARRIER_DECK_PIXEL_Y - PLAYER_SPRITE_HEIGHT)))) {
		game->missionComplete = 1;
		game->missionCompleteTimer = LANDING_COMPLETE_HOLD_FRAMES;
		game->speedLevel = 0;
		/* CPC landinghoverloop awards &00c8 (200) after both aircraft have
		 * completed the landing sequence. */
		game->bonusScore += LANDING_SCORE_VALUE;
		game->score = game->bonusScore;
		/* Landing is a one-shot state transition, so the fanfare starts
		 * exactly once here rather than being retriggered while LANDED is
		 * displayed. Give its four MOD voices sole ownership of Paula. */
		stopAllSfx();
		startCarrierLandingMusic();
		changed = 1;
	}

	if (game->fuel != 999) {
		game->fuel = 999;
		changed = 1;
	}
	{
		UBYTE fullBombs, fullRockets;
		ammoForSkill(game->skillLevel, &fullBombs, &fullRockets);
		if (game->rockets != fullRockets) {
			game->rockets = fullRockets;
			changed = 1;
		}
		if (game->bombs != fullBombs) {
			game->bombs = fullBombs;
			changed = 1;
		}
	}
	if (game->armour != 100 || game->flakDamageCount != 0) {
		game->armour = 100;
		game->flakDamageCount = 0;
		changed = 1;
	}
	game->playerFrigateStatus = PLAYER_FRIGATE_STATUS_SERVICED;

	return changed;
}

/* The double-buffered page renderer that used to consume this ObjectMap
 * cache (buildObjectMap/renderObjectMapTiles/drawObjectMapNativeObjects/
 * drawObjectMapOverlayIfEnabled and their *Range siblings) was removed once
 * ring-buffer streaming (which queries objectCellForWorldColumnTile directly,
 * per column, with no cache) became the only scroller. See
 * AMIGA_PORT_PLAN.md Sprint 14.85 for the removal record. */

typedef struct RenderColumn {
	UBYTE tile[GAME_OBJECT_MAP_HEIGHT_TILES];
} RenderColumn;

/* Sprint 14.94 Part 1+2: objectCellForWorldColumnTile() resolves one row at a
 * time and gets called once per row by every caller below - up to
 * GAME_OBJECT_MAP_HEIGHT_TILES(25) times for the very same column, each call
 * redundantly re-deriving the column's level segment/stage/terrain and
 * re-scanning the entire harLevelObjects array from scratch. This builds an
 * entire column's worth of tiles in one pass instead, for callers that need
 * the whole column (or a cached partial range) rather than a single cell -
 * objectCellForWorldColumnTile() itself is untouched and still serves its
 * other callers (collision probes, objectCellForWorldPoint(), enemy-plane
 * flak-avoidance) exactly as before.
 *
 * Faithfully reproduces the original function's priority order - land >
 * ship-wreck smoke > harLevelObjects > procedural town block > procedural
 * land target > runtime flak > native-carrier-deck horizon row - via a
 * claimed[] array: once a row is finalized by a higher-priority rule,
 * lower-priority rules skip it, exactly matching the original's "first
 * match wins, in this order" per-row early-return behaviour, just computed
 * with one forward pass over harLevelObjects instead of a fresh 95-entry
 * scan per row. */
static void buildWorldTileColumn(LONG worldColumn, RenderColumn* outColumn) {
	const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
	UBYTE stage = stageForWorldColumn(worldColumn, segment);
	UBYTE terrainKind = segment ? segment->terrainKind : terrainKindForStage(stage);
#if HAR_DEBUG_FORCE_STAGE >= 0
	terrainKind = terrainKindForStage(stage);
#endif
	UBYTE terrainY = terrainYForWorldColumn(worldColumn, segment, terrainKind);
	UBYTE claimed[GAME_OBJECT_MAP_HEIGHT_TILES];
	memset(claimed, 0, sizeof(claimed));

	/* Priority 1: sky/sea baseline, then land - both already O(1) or
	 * small-bounded per row (seaTileForColumn, isLandCraterAtColumnRow), not
	 * the hotspot this change targets, but computed with the column's
	 * segment/terrainY resolved only once instead of once per row. */
	for (UWORD tileY = 0; tileY < GAME_OBJECT_MAP_HEIGHT_TILES; tileY++) {
		UBYTE cloudTile = tileY < GAME_SEA_TOP_TILE_Y ?
			cpcCloudTileAtColumnRow(worldColumn, (WORD)tileY) : 0;
		outColumn->tile[tileY] = tileY < GAME_SEA_TOP_TILE_Y ?
			cloudTile : seaTileForColumn(worldColumn, tileY);
		if (terrainKind == HAR_TERRAIN_COAST_FALL) {
			LONG localColumn = segment ? worldColumn - segment->startColumn : worldColumn;
			if (localColumn >= 0 && localColumn < 3 && (tileY == 14 || tileY == 15)) {
				outColumn->tile[tileY] = 1;
				claimed[tileY] = 1; /* base terrain: town may overwrite */
				continue;
			}
		}
		if (terrainY != 255 && tileY >= terrainY && (terrainKind != HAR_TERRAIN_COAST_FALL || tileY < GAME_SEA_TOP_TILE_Y)) {
			outColumn->tile[tileY] = isLandCraterAtColumnRow(worldColumn, tileY) ? GAME_LAND_CRATER_TILE : (tileY == terrainY ? landSurfaceTileForColumn(worldColumn, terrainKind) : 1);
			claimed[tileY] = 1; /* base terrain: town may overwrite */
		}
	}

	/* Priority 2: ship-wreck smoke - small bounded scans, only for
	 * non-land rows. */
	for (UWORD tileY = 0; tileY < GAME_OBJECT_MAP_HEIGHT_TILES; tileY++) {
		if (claimed[tileY])
			continue;
		UBYTE smokeTile = shipWreckSmokeTileAtColumnRow(worldColumn, tileY);
		if (smokeTile) {
			outColumn->tile[tileY] = smokeTile;
			claimed[tileY] = 2; /* protected from town overwrite */
		}
	}

	/* Priority 3: harLevelObjects - the actual hot path this change targets.
	 * Sprint 14.94 Part 3: walks only the (usually 0 or 1, rarely more)
	 * entries actually at this column via harLevelObjectColumnHead[]/
	 * harLevelObjectNext[], instead of scanning all 95 entries checking
	 * worldColumn==object->column on each. */
	for (UBYTE index = harLevelObjectFirstIndexForColumn(worldColumn); index != HAR_LEVEL_OBJECT_COLUMN_INDEX_NONE; index = harLevelObjectNext[index]) {
		const LevelObjectDef* object = &harLevelObjects[index];
		WORD row;

		if (object->id == HAR_OBJ_GROUND_TARGET && isTargetDestroyedAtColumn(worldColumn))
			continue;
		if (object->rowMode == HAR_ROW_TERRAIN_RELATIVE) {
			if (terrainY == 255)
				continue;
			row = (WORD)terrainY + object->row;
		} else {
			row = object->row;
		}
		if (row < 0 || row >= GAME_OBJECT_MAP_HEIGHT_TILES || claimed[row])
			continue;
		if (object->id == HAR_OBJ_ENEMY_SHIP && isShipCellDestroyed(worldColumn, row))
			continue;
		/* In the CPC+ path enemyshipsprite supplies the destructible
		 * object-map cells behind the promoted ASIC gunship. It is not a
		 * second visible black vessel. Keep those cells in
		 * objectCellForWorldColumnTile() for collision/damage, but leave
		 * the sky/sea baseline intact in the render column. */
		if (object->id == HAR_OBJ_ENEMY_SHIP)
			continue;
		outColumn->tile[row] = object->tile;
		claimed[row] = 2; /* explicit object: protected from town overwrite */
	}

	/* Priority 4: procedural town blocks (Sprint 14.95 Part 5, moved here from
	 * a separate post-column draw pass). drawDirectColumnRangeObjects() used
	 * to call drawProceduralTownBlockColumn() synchronously the instant a
	 * streamed column finished its row budget - fine for the old ~15 sparse
	 * hand-placed blocks, but the new continuous generator means almost every
	 * town column pays for up to HAR_CPC_TOWN_BLOCK_HEIGHT(5) extra tile
	 * draws in one lump outside serviceRingWorldStream()'s per-frame row
	 * budget, right when the town scrolls past - exactly the kind of
	 * per-column stutter that budget was built to avoid. Resolving it as
	 * ordinary tile data here instead lets it ride the same distributed
	 * budget as every other row, and the existing claimed[] priority order
	 * already gives ship-wreck/hit smoke (priority 2) precedence for free -
	 * no separate shipWreckSmokeTileAtColumnRow() check needed here like the
	 * old draw-pass version required. */
	if (terrainKind == HAR_TERRAIN_TOWN && terrainY != 255) {
		LONG localColumn = segment ? worldColumn - segment->startColumn : worldColumn;
		if (localColumn >= 0 && localColumn < CPC_TOWN_PROCEDURAL_LENGTH) {
			UBYTE blockId = cpcTownProceduralBlockId((UWORD)localColumn);
			if (blockId != CPC_TOWN_BLOCK_NONE) {
				UBYTE blockLocalColumn = cpcTownProceduralLocalColumn((UWORD)localColumn);
				short baseRow = (short)(terrainY - 3);
				const UBYTE* block = harCpcTownBlockTiles[blockId];
				for (UBYTE row = 0; row < HAR_CPC_TOWN_BLOCK_HEIGHT; row++) {
					UBYTE tileId = block[blockLocalColumn * HAR_CPC_TOWN_BLOCK_HEIGHT + row];
					if (!tileId || tileId == 1 || tileId >= GAME_TILE_COUNT)
						continue;
					short outY = (short)(baseRow + row);
					/* Buildings may replace base land (claim 1), as CPC's
					 * later sprite-block draw does, but not smoke or an
					 * explicit level object (claim 2). */
					if (outY < 0 || outY >= GAME_OBJECT_MAP_HEIGHT_TILES || claimed[outY] >= 2)
						continue;
					outColumn->tile[outY] = tileId;
					claimed[outY] = 1;
				}
			}
		}
	}

	/* Priority 5: procedural land target (CPC_RANDOM_LAND terrain). */
	if (terrainKind == HAR_TERRAIN_CPC_RANDOM_LAND && terrainY != 255 && !isTargetDestroyedAtColumn(worldColumn)) {
		LONG localColumn = segment ? worldColumn - segment->startColumn : worldColumn;
		if (localColumn >= 0 && localColumn < CPC_LAND_PROCEDURAL_LENGTH) {
			UBYTE target = cpcLandProceduralTarget((UWORD)localColumn);
			WORD targetRow = (WORD)(terrainY - 1);
			if (target != CPC_LAND_TARGET_NONE && targetRow >= 0 && targetRow < GAME_OBJECT_MAP_HEIGHT_TILES && !claimed[targetRow]) {
				static const UBYTE targetTiles[] = { 42, 43, 44, 45, 46 };
				outColumn->tile[targetRow] = targetTiles[target - 1];
				claimed[targetRow] = 1;
			}
		}
	}

	/* Priority 6: runtime flak (Sprint 14.95 Part 2) - spawned live at the
	 * screen's right edge by trySpawnFlak(), applies uniformly to land and
	 * town since eligibility was already decided at spawn time. Replaces the
	 * old precomputed per-column lookahead tables for both terrain kinds. */
	for (UWORD tileY = 0; tileY < GAME_OBJECT_MAP_HEIGHT_TILES; tileY++) {
		if (claimed[tileY])
			continue;
		UBYTE runtimeFlakTile = runtimeFlakTileAtColumnRow(worldColumn, tileY);
		if (runtimeFlakTile) {
			outColumn->tile[tileY] = runtimeFlakTile;
			claimed[tileY] = 1;
		}
	}

	/* Priority 7: native-carrier deck at the horizon row - only ever one
	 * specific row, kept as its own pass (a different match condition - a
	 * column range, not an exact column - so it doesn't fold into priority
	 * 3's loop) rather than folded in above. Sprint 14.94 Part 3: scans only
	 * the short harWideObjectIndex[] list (native-carrier/gunship/town-block
	 * entries, ~21 of 95) instead of the full array - the index is already
	 * guaranteed built by priority 3's harLevelObjectFirstIndexForColumn()
	 * call above, earlier in this same function. */
	if (!claimed[GAME_HORIZON_TILE_Y]) {
		for (UBYTE wideIndex = 0; wideIndex < harWideObjectCount; wideIndex++) {
			const LevelObjectDef* object = &harLevelObjects[harWideObjectIndex[wideIndex]];
			if (object->id != HAR_OBJ_OWN_FRIGATE || !(object->flags & HAR_OBJECT_FLAG_NATIVE_CARRIER))
				continue;
			if (worldColumn < object->column || worldColumn >= object->column + 12)
				continue;
			outColumn->tile[GAME_HORIZON_TILE_Y] = 0;
			claimed[GAME_HORIZON_TILE_Y] = 1;
			break;
		}
	}
}

static void drawWorldColumnRowsFromCache(UBYTE* bitmap, UWORD tileX, const RenderColumn* column, UWORD rowStart, UWORD rowCount) {
	UWORD rowEnd = (UWORD)(rowStart + rowCount);
	if (rowEnd > GAME_OBJECT_MAP_HEIGHT_TILES)
		rowEnd = GAME_OBJECT_MAP_HEIGHT_TILES;
	for (UWORD tileY = rowStart; tileY < rowEnd; tileY++)
		drawGameScrollTile(bitmap, (short)tileX, (short)tileY, column->tile[tileY]);
}

static void renderWorldColumnRowsDirect(UBYTE* bitmap, UWORD tileX, LONG worldColumn, UWORD rowStart, UWORD rowCount) {
	RenderColumn column;
	buildWorldTileColumn(worldColumn, &column);
	drawWorldColumnRowsFromCache(bitmap, tileX, &column, rowStart, rowCount);
}

static void renderWorldColumnDirect(UBYTE* bitmap, UWORD tileX, LONG worldColumn) {
	renderWorldColumnRowsDirect(bitmap, tileX, worldColumn, 0, GAME_OBJECT_MAP_HEIGHT_TILES);
}

static WORD levelObjectRowForColumnObject(const LevelObjectDef* object) {
	if (object->rowMode == HAR_ROW_TERRAIN_RELATIVE) {
		const LevelSegmentDef* segment = levelSegmentForWorldColumn(object->column);
		UBYTE stage = stageForWorldColumn(object->column, segment);
		UBYTE terrainKind = segment ? segment->terrainKind : terrainKindForStage(stage);
#if HAR_DEBUG_FORCE_STAGE >= 0
		terrainKind = terrainKindForStage(stage);
#endif
		UBYTE terrainY = terrainYForWorldColumn(object->column, segment, terrainKind);
		if (terrainY == 255)
			return -1;
		return (WORD)terrainY + object->row;
	}
	return object->row;
}

/* Sprint 14.94 Part 3: every branch in this loop only ever matches one of
 * the three "wide" object kinds (native-carrier frigate, gunship, town
 * block) - every other harLevelObjects entry falls through all three guards
 * as a no-op. Scans harWideObjectIndex[] (~21 entries) instead of the full
 * 95-entry array for exactly that reason. */
static void drawDirectColumnRangeObjects(UBYTE* bitmap, UWORD physicalTileX, LONG worldColumn) {
	if (!harLevelObjectIndexReady)
		buildHarLevelObjectIndex();
	for (UBYTE wideIndex = 0; wideIndex < harWideObjectCount; wideIndex++) {
		const LevelObjectDef* object = &harLevelObjects[harWideObjectIndex[wideIndex]];
		LONG objectColumn = object->column;
		WORD row;

		if (object->id == HAR_OBJ_OWN_FRIGATE && (object->flags & HAR_OBJECT_FLAG_NATIVE_CARRIER)) {
			if (worldColumn < objectColumn || worldColumn >= objectColumn + WORLD_RENDER_CARRIER_WIDTH_TILES)
				continue;
			/* Sprint 14.94 Part 6: passes the composite's own column index
			 * (0..HAR_CARRIER_TILES_WIDE-1) and the buffer column being drawn
			 * directly now, instead of the old "pixel x of the whole
			 * composite's origin + a start/end clip range" the per-pixel
			 * renderer needed - this function is always called for exactly
			 * one buffer column at a time, so there's no clipping left to do
			 * here. */
			drawPromotedCpcCarrierRangeAt(bitmap, physicalTileX, (UWORD)(worldColumn - objectColumn), (object->flags & HAR_OBJECT_FLAG_NATIVE_CARRIER_REVERSED) != 0);
			continue;
		}

		if (object->id == HAR_OBJ_GUNSHIP && (object->flags & HAR_OBJECT_FLAG_CPC_GUNSHIP)) {
			if (worldColumn < objectColumn || worldColumn >= objectColumn + WORLD_RENDER_GUNSHIP_WIDTH_TILES)
				continue;
			row = levelObjectRowForColumnObject(object);
			if (row < 0)
				continue;
			drawPromotedCpcGunshipRangeAt(bitmap, physicalTileX, (UWORD)(worldColumn - objectColumn), row);
			continue;
		}
	}
}

/* Sprint 14.95 Part 5: direct O(1) lookup against the procedural town-block
 * tables instead of the old harLevelObjects scan (there are no more
 * HAR_OBJ_TOWN_BLOCK entries to scan for). Resolves the exact tile at the
 * hit cell so callers can skip transparent gaps within the block's bounding
 * box (matches buildWorldTileColumn()'s own !tileId||tileId==1 skip for the
 * same block data) and skips cells already replaced with smoke so a
 * destroyed section can't be "hit" again. */
static UBYTE townBlockCellAtWorldColumnRow(LONG centerColumn,
	WORD centerTileY, ObjectCell* outCell) {
	const LevelSegmentDef* segment = levelSegmentForWorldColumn(centerColumn);
	if (!segment || segment->terrainKind != HAR_TERRAIN_TOWN)
		return 0;
	LONG localColumn = centerColumn - segment->startColumn;
	if (localColumn < 0 || localColumn >= CPC_TOWN_PROCEDURAL_LENGTH)
		return 0;

	UBYTE blockId = cpcTownProceduralBlockId((UWORD)localColumn);
	if (blockId == CPC_TOWN_BLOCK_NONE)
		return 0;
	UBYTE blockLocalColumn = cpcTownProceduralLocalColumn((UWORD)localColumn);

	UBYTE terrainY = terrainYForWorldColumn(centerColumn, segment, HAR_TERRAIN_TOWN);
	if (terrainY == 255)
		return 0;
	WORD baseRow = (WORD)(terrainY - 3);
	if (centerTileY < baseRow || centerTileY >= baseRow + HAR_CPC_TOWN_BLOCK_HEIGHT)
		return 0;

	UBYTE localRow = (UBYTE)(centerTileY - baseRow);
	UBYTE tileId = harCpcTownBlockTiles[blockId][blockLocalColumn * HAR_CPC_TOWN_BLOCK_HEIGHT + localRow];
	if (!tileId || tileId == 1 || tileId >= GAME_TILE_COUNT)
		return 0;
	if (shipWreckSmokeTileAtColumnRow(centerColumn, centerTileY))
		return 0;

	if (outCell) {
		outCell->id = HAR_OBJ_TOWN_BLOCK;
		outCell->tile = tileId;
		outCell->flags = HAR_OBJECT_FLAG_CPC_TOWN_BLOCK;
		outCell->hp = 0;
	}
	return 1;
}

static UBYTE townBlockCellNearWorldPoint(const GameState* game, WORD screenX, WORD screenY, ObjectCell* outCell, LONG* outWorldColumn, WORD* outTileY) {
	if (screenY < 0 || screenY >= HUD_TOP)
		return 0;

	LONG worldPixelX = (LONG)game->scrollX + screenX;
	LONG centerColumn = worldPixelX >> 3;
	WORD centerTileY = screenY >> 3;
	if (!townBlockCellAtWorldColumnRow(centerColumn, centerTileY, outCell))
		return 0;
	if (outWorldColumn)
		*outWorldColumn = centerColumn;
	if (outTileY)
		*outTileY = centerTileY;
	return 1;
}

static UWORD ringWorldTileXForColumn(LONG worldColumn) {
	/* Sprint 14.94 Part 4: same 32-bit-modulo-on-68000 cost as
	 * seaTileForColumn() above - worldColumn's practical range (level width
	 * 704 tiles, plus a small negative margin) fits safely in WORD, so
	 * narrowing here lets the compiler use hardware DIVS.W instead of a
	 * 32-bit library divide. Called once per column-render/stream-step. */
	WORD local = (WORD)worldColumn % (WORD)GAME_WORLD_SCROLL_PAGE_BYTES;
	if (local < 0)
		local += GAME_WORLD_SCROLL_PAGE_BYTES;
	return (UWORD)(GAME_WORLD_BUFFER_MARGIN_TILES + local);
}

/* Sprint 14.94 Part 1+2: builds the column once and draws it to both the
 * primary and (if within the ring seam's duplicate zone) wrap-duplicate X
 * position from that single result, instead of the previous
 * renderRingWorldColumnDirect() being called twice - each of which used to
 * redo the full per-row resolution from scratch. drawDirectColumnRangeObjects()
 * (the carrier/gunship/town-block overlay) is untouched here - still called
 * once per X position, still its own 95-entry scan; that's Part 3's target. */
static void renderRingWorldColumn(UBYTE* bitmap, LONG worldColumn) {
	UWORD tileX = ringWorldTileXForColumn(worldColumn);
	RenderColumn column;
	buildWorldTileColumn(worldColumn, &column);
	drawWorldColumnRowsFromCache(bitmap, tileX, &column, 0, GAME_OBJECT_MAP_HEIGHT_TILES);
	drawDirectColumnRangeObjects(bitmap, tileX, worldColumn);
	if (tileX < GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) {
		UWORD duplicateTileX = (UWORD)(tileX + GAME_WORLD_SCROLL_PAGE_BYTES);
		drawWorldColumnRowsFromCache(bitmap, duplicateTileX, &column, 0, GAME_OBJECT_MAP_HEIGHT_TILES);
		drawDirectColumnRangeObjects(bitmap, duplicateTileX, worldColumn);
	}
}

static UWORD scrollLeftWorldColumnForScroll(UWORD scrollX) {
	LONG pointerPixelX = scrollPointerPixelX(scrollX);
	if (pointerPixelX < 0)
		pointerPixelX = 0;
	return (UWORD)(pointerPixelX >> 3);
}

static void initRingWorldBuffer(UBYTE* bitmap, UWORD startColumn) {
	(void)startColumn;
	memset(bitmap, 0, GAME_WORLD_BITMAP_BYTES);
	ringWorldLastStreamedColumn = 0xffff;
	ringStreamColumn = -1;
	ringStreamRow = 0;
	for (LONG worldColumn = -(LONG)GAME_WORLD_BUFFER_MARGIN_TILES; worldColumn < (LONG)GAME_WORLD_SCROLL_PAGE_TILES; worldColumn++) {
		if (worldColumn < 0)
			continue;
		renderRingWorldColumn(bitmap, worldColumn);
		ringWorldLastStreamedColumn = (UWORD)worldColumn;
	}
}

/* Renders terrain a few rows at a time instead of a whole 25-row column at
 * once. A single column's full cost (terrain lookup + tile draw + object
 * overlay for every row) done synchronously in one frame was showing up as a
 * small, perfectly regular hitch every time a new column streamed in (i.e.
 * every 8 scrolled pixels).
 *
 * Spreading a column over a *fixed* number of frames isn't enough by itself:
 * gating a fixed budget on "is a new column due yet" produces bursts (spend
 * the budget for a couple of frames, then do nothing for several frames until
 * the next column is due), which reads as an uneven, pulsing scroll rather
 * than a single hitch. Instead the per-frame row budget tracks the *current*
 * scroll speed directly (rows needed = pixels/frame * map-height / tile
 * width, rounded up) - exactly enough rows to keep one column's worth of
 * progress flowing continuously alongside the real scroll rate, so there's
 * no gap where it does nothing and no burst where it rushes to catch up.
 * RING_WORLD_STREAM_MAX_AHEAD_TILES is only a safety backstop (comfortably
 * under one ring period, so the wraparound-safety margin is untouched) for
 * cases like a crash freeze where scrollX stops changing entirely.
 *
 * Sprint 14.94 Part 1+2: a column's tiles are now built exactly once, the
 * moment it starts streaming (ringStreamRow==0), into ringStreamTileColumn -
 * every subsequent frame's partial row range for that same column (and its
 * wrap duplicate, if any) draws from that cache instead of re-resolving via
 * renderWorldColumnRowsDirect()'s old per-row objectCellForWorldColumnTile()
 * calls. */
static RenderColumn ringStreamTileColumn;

static void serviceRingWorldStream(UBYTE* bitmap, const GameState* game) {
	if (useFixedTakeoffWorldWindow(game))
		return;

	UWORD leftColumn = scrollLeftWorldColumnForScroll(game->scrollX);
	UWORD maxAheadColumn = (UWORD)(leftColumn + RING_WORLD_STREAM_MAX_AHEAD_TILES);

	UBYTE scrollPixels = (game->missionComplete || game->landingState == LANDING_STATE_HOVER) ?
		0 : scrollPixelsForSpeedLevel(game->speedLevel);
	UWORD rowBudget = (UWORD)((scrollPixels * GAME_OBJECT_MAP_HEIGHT_TILES + (GAME_TILE_WIDTH - 1)) / GAME_TILE_WIDTH);
	while (rowBudget > 0) {
		if (ringStreamColumn < 0) {
			if (!(ringWorldLastStreamedColumn < maxAheadColumn))
				break;
			ringStreamColumn = (LONG)ringWorldLastStreamedColumn + 1;
			ringStreamRow = 0;
			buildWorldTileColumn(ringStreamColumn, &ringStreamTileColumn);
		}

		UWORD tileX = ringWorldTileXForColumn(ringStreamColumn);
		UBYTE hasDuplicate = tileX < GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES;
		UWORD duplicateTileX = (UWORD)(tileX + GAME_WORLD_SCROLL_PAGE_BYTES);

		UWORD rowsThisStep = rowBudget;
		if (ringStreamRow + rowsThisStep > GAME_OBJECT_MAP_HEIGHT_TILES)
			rowsThisStep = (UWORD)(GAME_OBJECT_MAP_HEIGHT_TILES - ringStreamRow);

		drawWorldColumnRowsFromCache(bitmap, tileX, &ringStreamTileColumn, ringStreamRow, rowsThisStep);
		if (hasDuplicate)
			drawWorldColumnRowsFromCache(bitmap, duplicateTileX, &ringStreamTileColumn, ringStreamRow, rowsThisStep);

		ringStreamRow = (UWORD)(ringStreamRow + rowsThisStep);
		rowBudget = (UWORD)(rowBudget - rowsThisStep);

		if (ringStreamRow >= GAME_OBJECT_MAP_HEIGHT_TILES) {
			drawDirectColumnRangeObjects(bitmap, tileX, ringStreamColumn);
			if (hasDuplicate)
				drawDirectColumnRangeObjects(bitmap, duplicateTileX, ringStreamColumn);
			ringWorldLastStreamedColumn = (UWORD)ringStreamColumn;
			ringStreamColumn = -1;
			if (telemetryEnabled && telemetryAvailable)
				telemetryWorldTileColumns++;
#if HAR_DEBUG_PERF_LOG
			perfWorldTileColumns++;
#endif
		}
	}
}

static void dirtyRedrawWorldColumn(UBYTE** worldBuffers, LONG worldColumn) {
	renderRingWorldColumn(worldBuffers[0], worldColumn);
}

static void dirtyRedrawNativeCarrierAt(UBYTE** worldBuffers, LONG carrierColumn) {
	if (!harLevelObjectIndexReady)
		buildHarLevelObjectIndex();
	for (UBYTE wideIndex = 0; wideIndex < harWideObjectCount; wideIndex++) {
		const LevelObjectDef* object =
			&harLevelObjects[harWideObjectIndex[wideIndex]];
		if (object->id != HAR_OBJ_OWN_FRIGATE ||
			!(object->flags & HAR_OBJECT_FLAG_NATIVE_CARRIER) ||
			object->column != carrierColumn)
			continue;
		for (LONG column = object->column;
			column < object->column + WORLD_RENDER_CARRIER_WIDTH_TILES;
			column++)
			dirtyRedrawWorldColumn(worldBuffers, column);
		return;
	}
}

/* Menu review: dirtyRedrawWorldColumn() rebuilds and redraws all
 * GAME_OBJECT_MAP_HEIGHT_TILES(25) rows of a column (plus the carrier/
 * gunship overlay pass) even when only a single tile actually changed -
 * flagged as a contributor to small stutters. For a plain single-tile
 * change (flak appearing, a town-block tile turning to smoke) this writes
 * just that one tile, at both the primary and wrap-duplicate ring-buffer
 * positions - same dual-write logic renderRingWorldColumn() uses, just
 * without rebuilding the other 24 rows or re-scanning harWideObjectIndex[]
 * for wide objects that were never at this cell to begin with. */
static void dirtyRedrawWorldTile(UBYTE** worldBuffers, LONG worldColumn, WORD tileY, UBYTE tile) {
	UWORD tileX = ringWorldTileXForColumn(worldColumn);
	drawGameScrollTile(worldBuffers[0], (short)tileX, (short)tileY, tile);
	if (tileX < GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) {
		UWORD duplicateTileX = (UWORD)(tileX + GAME_WORLD_SCROLL_PAGE_BYTES);
		drawGameScrollTile(worldBuffers[0], (short)duplicateTileX, (short)tileY, tile);
	}
}

/* Sprint 15.2/15.3, generalised in 15.6: generic masked-Bob-over-ring-buffer
 * compositor. Not tied to any one object by construction (draws whatever
 * masked tile it's given at whatever column/row it's given) - originally
 * built for the wingman, which has since moved to a real hardware sprite
 * (see updateWingmanSprite()'s own comment); today's callers are the
 * bomb/impact effect and the powerup pickup, both moved onto Bobs to free
 * channel 6 for Wingman. See AMIGA_PORT_PLAN.md's
 * Sprint 15 roadmap for why this needed to be a new subsystem in the first
 * place (all 8 hardware sprite channels were already committed, and
 * nothing in this codebase drew a moving masked object into the scrolling
 * world buffer before).
 *
 * Design note on why this is "redraw the old footprint from truth" rather
 * than the classic Amiga "save background pixels, blit, later restore those
 * exact saved pixels" Bob technique: research into this ring buffer found
 * that already-visible, already-drawn columns can be rewritten at any time,
 * off-schedule, by dirtyRedrawWorldColumn()/dirtyRedrawWorldTile() (weapon
 * impacts, flak spawns/clears, ship/town-block hits - see those functions'
 * own call sites). A cached "saved background" snapshot can go stale the
 * instant one of those fires under the Bob's footprint, and restoring it
 * would silently undo a real gameplay change (e.g. paint over a crater or a
 * newly-cleared flak tile). Re-deriving the erased cells from
 * buildWorldTileColumn() sidesteps that entirely - there is no snapshot to
 * go stale. Only the single occupied tileRow is rebuilt (not the full
 * renderRingWorldColumn() column-plus-overlay pass) since callers may need
 * to erase every frame (continuous movement), and a full 25-row-plus-
 * overlay rebuild at that rate would be far too expensive. Known
 * limitation: this skips drawDirectColumnRangeObjects()'s carrier/gunship/
 * town-block overlay pass, so erasing while a Bob crosses directly over
 * one of those would briefly show the plain terrain tile underneath
 * instead of the overlay - a narrow, cosmetic edge case, not fixed here. */
static void bobCompositorErase(UBYTE* bitmap, LONG worldColumnLeft, WORD tileRow, UBYTE columnCount) {
	for (UBYTE column = 0; column < columnCount; column++) {
		RenderColumn rebuilt;
		LONG worldColumn = worldColumnLeft + column;
		buildWorldTileColumn(worldColumn, &rebuilt);
		UWORD tileX = ringWorldTileXForColumn(worldColumn);
		drawGameScrollTile(bitmap, (short)tileX, (short)tileRow, rebuilt.tile[tileRow]);
		if (tileX < GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES)
			drawGameScrollTile(bitmap, (short)(tileX + GAME_WORLD_SCROLL_PAGE_BYTES),
				(short)tileRow, rebuilt.tile[tileRow]);
	}
}

static void bobCompositorDrawMasked(UBYTE* bitmap, LONG worldColumn, WORD tileRow, const UBYTE* tile) {
	UWORD tileX = ringWorldTileXForColumn(worldColumn);
	drawGameScrollTileMasked(bitmap, (short)tileX, (short)tileRow, tile);
	if (tileX < GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) {
		UWORD duplicateTileX = (UWORD)(tileX + GAME_WORLD_SCROLL_PAGE_BYTES);
		drawGameScrollTileMasked(bitmap, (short)duplicateTileX, (short)tileRow, tile);
	}
}

/* Sprint 15.6: bomb + impact rendering, as a single Bob "slot" - bombShot
 * and impact are two separate WeaponState objects (see startWorldImpact()),
 * but never both active at once (the bomb-hit handler clears bombShot the
 * same instant it starts an impact), so one shared footprint is enough,
 * exactly mirroring how they used to share hardware sprite channel 2. Both
 * are tile-row-locked (no pixel-shift) - short-lived, fast-moving objects
 * that don't need the wingman body's smoothing treatment. */
static UBYTE bombImpactBobFootprintValid = 0;
static LONG bombImpactBobFootprintWorldColumn = 0;
static WORD bombImpactBobFootprintRow = 0;

static void eraseBombImpactBobFootprint(UBYTE* bitmap) {
	if (!bombImpactBobFootprintValid)
		return;
	bobCompositorErase(bitmap, bombImpactBobFootprintWorldColumn, bombImpactBobFootprintRow, 1);
	bombImpactBobFootprintValid = 0;
}

static void drawBombImpactBobAt(UBYTE* bitmap, LONG worldColumn, WORD row, UBYTE kind) {
	buildBombImpactBobTileIfNeeded(kind);
	if (bombImpactBobFootprintValid &&
		(bombImpactBobFootprintWorldColumn != worldColumn || bombImpactBobFootprintRow != row))
		eraseBombImpactBobFootprint(bitmap);
	bobCompositorDrawMasked(bitmap, worldColumn, row, bombImpactBobTile);
	bombImpactBobFootprintWorldColumn = worldColumn;
	bombImpactBobFootprintRow = row;
	bombImpactBobFootprintValid = 1;
}

static void updateBombImpactBob(UBYTE* bitmap, const GameState* game) {
	if (game->impact.active) {
		WORD screenX = game->impact.worldAnchored ?
			(WORD)(game->impact.worldX - game->scrollX) : game->impact.x;
		if (screenX < -16 || screenX > SCREEN_WIDTH) {
			eraseBombImpactBobFootprint(bitmap);
		} else {
			LONG worldColumn = ((LONG)game->scrollX + screenX) >> 3;
			WORD row = (WORD)(game->impact.y / GAME_TILE_HEIGHT);
			drawBombImpactBobAt(bitmap, worldColumn, row,
				(game->impact.timer & 2) ? BOMB_IMPACT_BOB_KIND_IMPACT_LARGE : BOMB_IMPACT_BOB_KIND_IMPACT_SMALL);
		}
	} else {
		eraseBombImpactBobFootprint(bitmap);
	}
}

/* Restore only the bytes covered by the flying bomb in this specific world
 * buffer. The caller keeps this immediately adjacent to the late-frame
 * redraw; that is important while the world remains single-buffered. */
static void eraseBombPixelBobFootprint(UBYTE* bitmap, UBYTE bufferIndex,
	BombShotFootprint* footprints) {
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT)
		return;
	BombShotFootprint* footprint = &footprints[bufferIndex];
	if (!footprint->valid)
		return;

	for (UBYTE placement = 0; placement < footprint->placementCount; placement++) {
		UBYTE byteCount = footprint->byteCount[placement];
		for (UBYTE row = 0; row < BOMB_SHOT_PIXEL_BOB_HEIGHT; row++) {
			UBYTE* dest = bitmap +
				(footprint->y + row) * SCREEN_PLANES * GAME_WORLD_ROW_BYTES +
				footprint->byteX[placement];
			for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++) {
				for (UBYTE byte = 0; byte < byteCount; byte++)
					dest[plane * GAME_WORLD_ROW_BYTES + byte] =
						footprint->background[placement][row][plane][byte];
			}
		}
	}
	footprint->valid = 0;
	footprint->placementCount = 0;
}

static void resetBombShotPixelBobFootprints(void) {
	memset(bombShotFootprints, 0, sizeof(bombShotFootprints));
	memset(wingmanBombFootprints, 0, sizeof(wingmanBombFootprints));
}

static void eraseRocketPixelBobFootprint(UBYTE* bitmap, UBYTE bufferIndex,
	RocketShotFootprint* footprints) {
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT)
		return;
	RocketShotFootprint* footprint = &footprints[bufferIndex];
	if (!footprint->valid)
		return;
	for (UBYTE placement = 0; placement < footprint->placementCount; placement++) {
		for (UBYTE row = 0; row < ROCKET_PIXEL_BOB_HEIGHT; row++) {
			UBYTE* dest = bitmap +
				(footprint->y + row) * SCREEN_PLANES * GAME_WORLD_ROW_BYTES +
				footprint->byteX[placement];
			for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++)
				for (UBYTE byte = 0;
					byte < footprint->byteCount[placement]; byte++)
					dest[plane * GAME_WORLD_ROW_BYTES + byte] =
						footprint->background[placement][row][plane][byte];
		}
	}
	footprint->valid = 0;
	footprint->placementCount = 0;
}

static void resetRocketShotPixelBobFootprints(void) {
	memset(rocketShotFootprints, 0, sizeof(rocketShotFootprints));
	memset(wingmanRocketFootprints, 0, sizeof(wingmanRocketFootprints));
}

static void drawRocketShotPixelBobPlacement(UBYTE* bitmap,
	RocketShotFootprint* footprint, UBYTE placement, UWORD bufferPixelX,
	WORD screenY, UBYTE tileId) {
	UBYTE bitOffset = (UBYTE)(bufferPixelX & 7);
	UWORD byteX = bufferPixelX >> 3;
	UBYTE byteCount = bitOffset ? 2 : 1;
	if (byteX + byteCount > GAME_WORLD_ROW_BYTES)
		byteCount = (UBYTE)(GAME_WORLD_ROW_BYTES - byteX);
	if (!byteCount)
		return;
	footprint->byteX[placement] = byteX;
	footprint->byteCount[placement] = byteCount;

	for (UBYTE row = 0; row < ROCKET_PIXEL_BOB_HEIGHT; row++) {
		UWORD sourceMask = 0;
		UWORD sourcePlane[GAME_WORLD_DISPLAY_PLANES] = { 0, 0, 0, 0 };
		for (UBYTE col = 0; col < ROCKET_PIXEL_BOB_WIDTH; col++) {
			UBYTE color = gameTilePixelColor(tileId, col, row);
			if (color == GAME_COLOR_SKY)
				continue;
			UBYTE pixelColor = color == GAME_COLOR_BLACK ?
				GAME_COLOR_BLACK : GAME_COLOR_YELLOW;
			UWORD bit = (UWORD)(0x8000 >> col);
			sourceMask |= bit;
			for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++)
				if (pixelColor & (1 << plane))
					sourcePlane[plane] |= bit;
		}
		sourceMask >>= bitOffset;
		for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++)
			sourcePlane[plane] >>= bitOffset;
		UBYTE masks[2] = {
			(UBYTE)(sourceMask >> 8), (UBYTE)sourceMask
		};
		UBYTE* dest = bitmap +
			(screenY + row) * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + byteX;
		for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++) {
			UBYTE colors[2] = {
				(UBYTE)(sourcePlane[plane] >> 8),
				(UBYTE)sourcePlane[plane]
			};
			for (UBYTE byte = 0; byte < byteCount; byte++) {
				UBYTE* target = dest + plane * GAME_WORLD_ROW_BYTES + byte;
				footprint->background[placement][row][plane][byte] = *target;
				*target = (UBYTE)((*target & ~masks[byte]) |
					(colors[byte] & masks[byte]));
			}
		}
	}
}

static void drawRocketPixelBob(UBYTE* bitmap, UBYTE bufferIndex,
	const WeaponState* rocket, RocketShotFootprint* footprints,
	UBYTE crashActive) {
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT || !rocket->active ||
		crashActive)
		return;
	if (rocket->y < 0 ||
		rocket->y + ROCKET_PIXEL_BOB_HEIGHT > GAME_WORLD_HEIGHT ||
		rocket->x <= -ROCKET_PIXEL_BOB_WIDTH ||
		rocket->x >= SCREEN_WIDTH)
		return;

	const LONG pagePixels =
		(LONG)GAME_WORLD_SCROLL_PAGE_BYTES * GAME_TILE_WIDTH;
	LONG localPixelX = rocket->worldX % pagePixels;
	if (localPixelX < 0)
		localPixelX += pagePixels;
	UWORD primaryPixelX =
		(UWORD)(GAME_WORLD_BUFFER_MARGIN_PIXELS + localPixelX);
	RocketShotFootprint* footprint = &footprints[bufferIndex];
	footprint->valid = 1;
	footprint->placementCount = 1;
	footprint->y = rocket->y;
	UBYTE tileId = rocketTileForState(rocket);
	drawRocketShotPixelBobPlacement(bitmap, footprint, 0, primaryPixelX,
		rocket->y, tileId);
	if (primaryPixelX <
		(GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) *
			GAME_TILE_WIDTH) {
		footprint->placementCount = 2;
		drawRocketShotPixelBobPlacement(bitmap, footprint, 1,
			(UWORD)(primaryPixelX + pagePixels), rocket->y,
			tileId);
	}
}

/* CPU masked plot of one 4x3 placement. Converting the compact row shape
 * to a 16-bit mask handles
 * both aligned and byte-straddling X positions without per-pixel divisions. */
static void drawBombShotPixelBobPlacement(UBYTE* bitmap,
	BombShotFootprint* footprint, UBYTE placement, UWORD bufferPixelX,
	WORD screenY, UBYTE phase) {
	/* Exact trimmed silhouettes from CPC tiles 40/41:
	 * launched = three black pixels diagonally forward/down;
	 * descending = three black pixels vertically aligned.
	 * Keep this deliberately tiny and monochrome instead of inventing a
	 * coloured Amiga projectile that reads as an egg. */
	static const UBYTE rowShape[2][BOMB_SHOT_PIXEL_BOB_HEIGHT] = {
		{ 0x08, 0x02, 0x01 },
		{ 0x04, 0x04, 0x04 }
	};
	static const UBYTE rowColor[2][BOMB_SHOT_PIXEL_BOB_HEIGHT] = {
		{ GAME_COLOR_BLACK, GAME_COLOR_BLACK, GAME_COLOR_BLACK },
		{ GAME_COLOR_BLACK, GAME_COLOR_BLACK, GAME_COLOR_BLACK }
	};
	phase &= 1;
	UBYTE bitOffset = (UBYTE)(bufferPixelX & 7);
	UWORD byteX = (UWORD)(bufferPixelX >> 3);
	UBYTE byteCount = (UBYTE)(bitOffset > 8 - BOMB_SHOT_PIXEL_BOB_WIDTH ? 2 : 1);
	/* The seam duplicate may begin in the final byte of the allocation.
	 * Clip a straddling second byte rather than writing into the next
	 * scanline; the primary placement still contains the complete shape. */
	if (byteX + byteCount > GAME_WORLD_ROW_BYTES)
		byteCount = (UBYTE)(GAME_WORLD_ROW_BYTES - byteX);
	if (!byteCount)
		return;

	footprint->byteX[placement] = byteX;
	footprint->byteCount[placement] = byteCount;

	for (UBYTE row = 0; row < BOMB_SHOT_PIXEL_BOB_HEIGHT; row++) {
		UWORD mask16 = (UWORD)rowShape[phase][row] <<
			(16 - BOMB_SHOT_PIXEL_BOB_WIDTH - bitOffset);
		UBYTE masks[BOMB_SHOT_PIXEL_BOB_MAX_BYTES_PER_ROW] = {
			(UBYTE)(mask16 >> 8), (UBYTE)mask16
		};
		UBYTE* dest = bitmap +
			(screenY + row) * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + byteX;

		for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++) {
			UBYTE colorSet = (UBYTE)(rowColor[phase][row] & (1 << plane));
			for (UBYTE byte = 0; byte < byteCount; byte++) {
				UBYTE* target = dest + plane * GAME_WORLD_ROW_BYTES + byte;
				UBYTE old = *target;
				footprint->background[placement][row][plane][byte] = old;
				*target = (UBYTE)((old & ~masks[byte]) |
					(colorSet ? masks[byte] : 0));
			}
		}
	}
}

/* Convert the bomb's exact world pixel to the ring-buffer pixel that stores
 * it.  scrollX + bombShot.x is already the world coordinate: Copper pointer
 * alignment (scrollPointerPixelX) and BPLCON1 fine scroll affect presentation
 * only and must not be subtracted here. */
static void drawBombPixelBob(UBYTE* bitmap, UBYTE bufferIndex,
	const WeaponState* bomb, BombShotFootprint* footprints,
	UWORD scrollX) {
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT || !bomb->active)
		return;
	if (bomb->y < 0 ||
		bomb->y + BOMB_SHOT_PIXEL_BOB_HEIGHT > GAME_WORLD_HEIGHT ||
		bomb->x <= -BOMB_SHOT_PIXEL_BOB_WIDTH ||
		bomb->x >= SCREEN_WIDTH)
		return;

	const LONG pagePixels = (LONG)GAME_WORLD_SCROLL_PAGE_BYTES * GAME_TILE_WIDTH;
	LONG worldPixelX = bomb->worldAnchored ?
		bomb->worldX : (LONG)scrollX + bomb->x;
	LONG localPixelX = worldPixelX % pagePixels;
	if (localPixelX < 0)
		localPixelX += pagePixels;
	UWORD primaryPixelX = (UWORD)(GAME_WORLD_BUFFER_MARGIN_PIXELS + localPixelX);

	BombShotFootprint* footprint = &footprints[bufferIndex];
	UBYTE phase = bomb->timer < BOMB_FORWARD_MOMENTUM_FRAMES ? 0 : 1;
	footprint->valid = 1;
	footprint->placementCount = 1;
	footprint->y = bomb->y;
	drawBombShotPixelBobPlacement(bitmap, footprint, 0, primaryPixelX,
		bomb->y, phase);

	/* Mirror early-page pixels into the seam duplicate, matching
	 * renderRingWorldColumn()/bobCompositorDrawMasked(). */
	if (primaryPixelX <
		(GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) * GAME_TILE_WIDTH) {
		footprint->placementCount = 2;
		drawBombShotPixelBobPlacement(bitmap, footprint, 1,
			(UWORD)(primaryPixelX + pagePixels), bomb->y, phase);
	}
}

/* Powerup pickup rendering.  Its world X stays fixed, while Y is an exact
 * screen-pixel coordinate.  Earlier versions rounded Y to an 8-pixel tile
 * row even though updatePowerup() already moves at a smooth 2/3 pixel per
 * frame.  Draw the two masked bytes at the real Y and restore the one or two
 * underlying tile rows from world truth before moving it. */
static UBYTE powerupBobFootprintValid = 0;
static LONG powerupBobFootprintWorldColumnLeft = 0;
static WORD powerupBobFootprintY = 0;

static void erasePowerupBobFootprint(UBYTE* bitmap) {
	if (!powerupBobFootprintValid)
		return;
	WORD firstRow = powerupBobFootprintY >> 3;
	WORD lastRow = (powerupBobFootprintY + POWERUP_SPRITE_HEIGHT - 1) >> 3;
	for (UBYTE column = 0; column < 2; column++) {
		LONG worldColumn = powerupBobFootprintWorldColumnLeft + column;
		RenderColumn rebuilt;
		buildWorldTileColumn(worldColumn, &rebuilt);
		UWORD tileX = ringWorldTileXForColumn(worldColumn);
		for (WORD row = firstRow; row <= lastRow; row++) {
			if (row < 0 || row >= GAME_OBJECT_MAP_HEIGHT_TILES)
				continue;
			drawGameScrollTile(bitmap, (short)tileX, (short)row,
				rebuilt.tile[row]);
			if (tileX < GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES)
				drawGameScrollTile(bitmap,
					(short)(tileX + GAME_WORLD_SCROLL_PAGE_BYTES),
					(short)row, rebuilt.tile[row]);
		}
	}
	powerupBobFootprintValid = 0;
}

static void drawPowerupBobColumnAtPixelY(UBYTE* bitmap, UWORD tileX,
	WORD pixelY, const UBYTE* tile) {
	for (UBYTE row = 0; row < POWERUP_SPRITE_HEIGHT; row++) {
		WORD screenY = (WORD)(pixelY + row);
		if (screenY < 0 || screenY >= GAME_WORLD_HEIGHT)
			continue;
		const UBYTE* src = tile +
			row * (GAME_WORLD_DISPLAY_PLANES + 1);
		UBYTE mask = src[GAME_WORLD_DISPLAY_PLANES];
		if (!mask)
			continue;
		UBYTE* dest = bitmap +
			screenY * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + tileX;
		for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++)
			dest[plane * GAME_WORLD_ROW_BYTES] =
				(UBYTE)((dest[plane * GAME_WORLD_ROW_BYTES] & ~mask) |
					(src[plane] & mask));
	}
}

static void updatePowerupBob(UBYTE* bitmap, const GameState* game) {
	if (!game->powerup.active || game->gameOver) {
		erasePowerupBobFootprint(bitmap);
		return;
	}

	buildPowerupBobTileIfNeeded(game->powerup.type);
	LONG worldColumnLeft = game->powerup.worldX >> 3;
	WORD pixelY = game->powerup.y;

	if (powerupBobFootprintValid &&
		powerupBobFootprintWorldColumnLeft == worldColumnLeft &&
		powerupBobFootprintY == pixelY)
		return;
	if (powerupBobFootprintValid)
		erasePowerupBobFootprint(bitmap);

	for (UBYTE column = 0; column < 2; column++) {
		LONG worldColumn = worldColumnLeft + column;
		UWORD tileX = ringWorldTileXForColumn(worldColumn);
		const UBYTE* tile = column ? powerupBobTileRight :
			powerupBobTileLeft;
		drawPowerupBobColumnAtPixelY(bitmap, tileX, pixelY, tile);
		if (tileX < GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES)
			drawPowerupBobColumnAtPixelY(bitmap,
				(UWORD)(tileX + GAME_WORLD_SCROLL_PAGE_BYTES),
				pixelY, tile);
	}
	powerupBobFootprintWorldColumnLeft = worldColumnLeft;
	powerupBobFootprintY = pixelY;
	powerupBobFootprintValid = 1;
}

static UBYTE wingmanFormationRowIsSafe(LONG worldColumnLeft, WORD tileRow);

/* CPC uses wingmanbelowplayer as persistent formation state, but changes it
 * in both directions: wingmanavoidsky selects below, while
 * forcewingmanavoidsea selects above.  Keep the current side while it is
 * viable (avoids rapid toggling), and cross the player only when that slot
 * becomes unsafe and the opposite slot is clear. */
static WORD updateWingmanFormationTargetRow(GameState* game,
	LONG worldColumnLeft) {
	WORD playerRow = (WORD)(game->playerY / GAME_TILE_HEIGHT);
	WORD aboveRow = (WORD)(playerRow - WINGMAN_FORMATION_ROWS_OFFSET);
	WORD belowRow = (WORD)(playerRow + WINGMAN_FORMATION_ROWS_OFFSET);
	UBYTE aboveSafe = wingmanFormationRowIsSafe(worldColumnLeft, aboveRow);
	UBYTE belowSafe = wingmanFormationRowIsSafe(worldColumnLeft, belowRow);

	if (!game->wingman.formationBelow) {
		if (!aboveSafe && belowSafe)
			game->wingman.formationBelow = 1;
	} else if (!belowSafe && aboveSafe) {
		game->wingman.formationBelow = 0;
	}

	WORD targetRow = game->wingman.formationBelow
		? belowRow : aboveRow;
	if (targetRow < 0)
		targetRow = 0;
	if (targetRow > WINGMAN_MAX_ROW)
		targetRow = WINGMAN_MAX_ROW;
	return targetRow;
}

static UBYTE wingmanCellIsPassable(LONG worldColumn, WORD tileRow) {
	ObjectCell cell;
	/* Procedural town blocks span several columns but are not represented
	 * by objectCellForWorldColumnTile() outside an anchor column. Check the
	 * actual promoted town tile first so the radar sees the whole facade. */
	if (townBlockCellAtWorldColumnRow(worldColumn, tileRow, &cell))
		return 0;
	if (!objectCellForWorldColumnTile(worldColumn, tileRow, &cell))
		return 0;
	return cell.id == HAR_OBJ_SKY || cell.id == HAR_OBJ_CLOUD || cell.id == HAR_OBJ_FLAK;
}

/* CPC checkwingmanradar accepts only sky/cloud/flak and looks several cells
 * ahead. Use the same rule for the full 16px footprint plus four forward
 * cells so a rising hill is avoided before either half enters it. */
static UBYTE wingmanFormationRowIsSafe(LONG worldColumnLeft, WORD tileRow) {
	if (tileRow < 1 || tileRow >= GAME_OBJECT_MAP_HEIGHT_TILES)
		return 0;
	for (UBYTE column = 0; column < 6; column++) {
		if (!wingmanCellIsPassable(worldColumnLeft + column, tileRow))
			return 0;
	}
	return 1;
}

static WORD wingmanSafeTargetRow(LONG worldColumnLeft, WORD requestedRow) {
	WORD row = requestedRow;
	while (row > 1 && !wingmanFormationRowIsSafe(worldColumnLeft, row))
		row--;
	return wingmanFormationRowIsSafe(worldColumnLeft, row) ? row : 1;
}

/* Sprint 15.6: the wingman's own formation row-seeking, extracted from the
 * old combined logic+rendering updateWingmanBob() (removed - see
 * updateWingmanSprite()'s own comment for why the wingman moved off Bob
 * rendering entirely). Pure state update now, no bitmap - the equivalent
 * INTERCEPT_APPROACH-mode row-seeking already lived in its own place
 * (updateWingmanIntercept()); this is that same logic's FORMATION-mode
 * counterpart, called from the same per-frame spot. */
static void updateWingmanFormationRow(GameState* game) {
	WingmanState* wingman = &game->wingman;
	if (wingman->mode != WINGMAN_FORMATION)
		return;

	WORD screenOffsetPixels = (WORD)(game->playerX -
		WINGMAN_FORMATION_COLUMNS_BEHIND * GAME_TILE_WIDTH);
	LONG worldColumnLeft = ((LONG)game->scrollX + screenOffsetPixels) >> 3;

	WORD targetRow = wingmanSafeTargetRow(worldColumnLeft,
		updateWingmanFormationTargetRow(game, worldColumnLeft));
	if (wingman->row != targetRow) {
		wingman->moveTimer++;
		if (wingman->moveTimer >= WINGMAN_MOVE_FRAME_INTERVAL) {
			wingman->moveTimer = 0;
			WORD candidate = (WORD)(wingman->row + ((wingman->row < targetRow) ? 1 : -1));
			wingman->row = wingmanFormationRowIsSafe(worldColumnLeft, candidate) ?
				candidate : wingmanSafeTargetRow(worldColumnLeft, candidate);
		}
	} else {
		wingman->moveTimer = 0;
	}
}

/* Sprint 15.17: CPC AI and terrain radar deliberately remain tile based,
 * but an Amiga hardware sprite does not need to inherit the CPC character
 * grid's visible 8-pixel jumps. Follow the accepted safe row at the same
 * 2px/frame vertical rate as the player. WINGMAN_MOVE_FRAME_INTERVAL is
 * four frames, so this reaches each new 8px row exactly as the logical AI
 * is ready to request another one. */
static void updateWingmanVisualY(GameState* game) {
	WingmanState* wingman = &game->wingman;
	if (!wingman->active)
		return;
	if (wingman->mode == WINGMAN_TAKEOFF ||
		wingman->mode == WINGMAN_LANDING_APPROACH ||
		wingman->mode == WINGMAN_LANDING_DECK)
		return;

	WORD targetY = (WORD)(wingman->row * GAME_TILE_HEIGHT);
	if (wingman->screenY < targetY) {
		WORD stepped = (WORD)(wingman->screenY + WINGMAN_VISUAL_MOVE_PIXELS);
		wingman->screenY = stepped > targetY ? targetY : stepped;
	} else if (wingman->screenY > targetY) {
		WORD stepped = (WORD)(wingman->screenY - WINGMAN_VISUAL_MOVE_PIXELS);
		wingman->screenY = stepped < targetY ? targetY : stepped;
	}
}

/* CPC wingmanstilloncarrier changes wingmantakeoff from 0 to 1 at the
 * forward-deck position, after which normal movement approaches the player.
 * Preserve that visible journey: climb clear of the carrier first, then
 * converge on the live formation slot without teleporting. */
static void updateWingmanTakeoff(GameState* game) {
	WingmanState* wingman = &game->wingman;
	if (!wingman->active || wingman->mode != WINGMAN_TAKEOFF)
		return;

	if (wingman->screenY > TAKEOFF_CLEAR_Y) {
		wingman->screenY -= WINGMAN_TAKEOFF_MOVE_PIXELS;
		if (wingman->screenY < TAKEOFF_CLEAR_Y)
			wingman->screenY = TAKEOFF_CLEAR_Y;
		return;
	}

	WORD targetX = (WORD)(game->playerX -
		WINGMAN_FORMATION_COLUMNS_BEHIND * GAME_TILE_WIDTH);
	LONG targetWorldColumn = ((LONG)game->scrollX + targetX) >> 3;
	WORD targetRow = wingmanSafeTargetRow(targetWorldColumn,
		updateWingmanFormationTargetRow(game, targetWorldColumn));
	WORD targetY = (WORD)(targetRow * GAME_TILE_HEIGHT);

	if (wingman->interceptScreenX < targetX) {
		wingman->interceptScreenX += WINGMAN_TAKEOFF_MOVE_PIXELS;
		if (wingman->interceptScreenX > targetX)
			wingman->interceptScreenX = targetX;
	} else if (wingman->interceptScreenX > targetX) {
		wingman->interceptScreenX -= WINGMAN_TAKEOFF_MOVE_PIXELS;
		if (wingman->interceptScreenX < targetX)
			wingman->interceptScreenX = targetX;
	}
	if (wingman->screenY < targetY) {
		wingman->screenY += WINGMAN_TAKEOFF_MOVE_PIXELS;
		if (wingman->screenY > targetY)
			wingman->screenY = targetY;
	} else if (wingman->screenY > targetY) {
		wingman->screenY -= WINGMAN_TAKEOFF_MOVE_PIXELS;
		if (wingman->screenY < targetY)
			wingman->screenY = targetY;
	}

	if (wingman->interceptScreenX == targetX &&
		wingman->screenY == targetY) {
		wingman->row = targetRow;
		wingman->mode = WINGMAN_FORMATION;
		wingman->moveTimer = 0;
	}
}

static WORD wingmanScreenX(const GameState* game) {
	const WingmanState* wingman = &game->wingman;
	return (wingman->mode == WINGMAN_TAKEOFF ||
		wingman->mode == WINGMAN_INTERCEPT_APPROACH ||
		wingman->mode == WINGMAN_BOMB_APPROACH ||
		wingman->mode == WINGMAN_LANDING_APPROACH ||
		wingman->mode == WINGMAN_LANDING_DECK)
		? wingman->interceptScreenX
		: (WORD)(game->playerX -
			WINGMAN_FORMATION_COLUMNS_BEHIND * GAME_TILE_WIDTH);
}

/* Wingman uses channel 6. Channel 7 is independent and is used by the
 * player's ejector-seat/parachute sequence. */
static void updateWingmanSprite(UWORD* sprite, UWORD* unusedSprite7, const GameState* game) {
	(void)unusedSprite7;
	const WingmanState* wingman = &game->wingman;
	if (!wingman->active ||
		(wingman->mode != WINGMAN_TAKEOFF &&
		 wingman->mode != WINGMAN_FORMATION &&
		 wingman->mode != WINGMAN_INTERCEPT_APPROACH &&
		 wingman->mode != WINGMAN_BOMB_APPROACH &&
		 wingman->mode != WINGMAN_LANDING_APPROACH &&
		 wingman->mode != WINGMAN_LANDING_DECK)) {
		hideHardwareSprite(sprite);
		return;
	}

	WORD screenX = wingmanScreenX(game);
	WORD screenY = wingman->screenY;
	buildSpriteFromCpcPlusHalves(sprite, PLAYER_SPRITE_HEIGHT, screenX, screenY,
		harCpcWingmanFlyingLeftPixels, harCpcWingmanFlyingRightPixels,
		cpcPlusPenToWingmanHardwareColor);
}

/* Menu review: addCpcHitSmokeAtColumnRow() marks smoke at the exact hit
 * cell always, plus the cell ONE ROW ABOVE only if that one turned out to
 * be empty sky - callers redrawing "the hit column" via
 * dirtyRedrawWorldColumn() twice (once per row) to cover both cases were
 * both doing far more work than needed AND redrawing the left column
 * unconditionally even on the common case where nothing there actually
 * changed. Looking the tile back up here and only drawing if something is
 * actually marked there handles both cells correctly with a single shared
 * call at each site. */
static void dirtyRedrawWorldTileIfSmoke(UBYTE** worldBuffers, LONG worldColumn, WORD tileY) {
	UBYTE smokeTile = shipWreckSmokeTileAtColumnRow(worldColumn, tileY);
	if (smokeTile)
		dirtyRedrawWorldTile(worldBuffers, worldColumn, tileY, smokeTile);
}

static LONG scrollPointerPixelX(UWORD scrollX) {
	UWORD fine = scrollX & 15;
	if (fine == 0)
		return (LONG)scrollX - 16;
	return (LONG)(scrollX - fine);
}

static UWORD scrollDelayForBplcon1(UWORD scrollX) {
	UWORD fine = scrollX & 15;
	return fine == 0 ? 0 : 16 - fine;
}

static USHORT scrollAbsoluteByteOffset(UWORD scrollX) {
	LONG pointerPixelX = scrollPointerPixelX(scrollX) + GAME_WORLD_BUFFER_MARGIN_PIXELS;
	if (pointerPixelX < 0)
		pointerPixelX = 0;
	return (USHORT)(pointerPixelX >> 3);
}

static USHORT scrollLocalByteOffset(UWORD scrollX) {
	USHORT absoluteByteOffset = scrollAbsoluteByteOffset(scrollX);
	USHORT pageOffset;

	if (absoluteByteOffset <= GAME_WORLD_BUFFER_MARGIN_TILES)
		return absoluteByteOffset;

	/* Sprint 14.94 Part 4: despite absoluteByteOffset already being USHORT,
	 * C's usual arithmetic conversions promote the subtraction to (32-bit)
	 * int before the modulo, so this silently hit the same 68000 32-bit
	 * library-divide cost as the other two sites unless narrowed back down
	 * explicitly - called once per frame (feeds the copper bitplane
	 * pointer), same fix as seaTileForColumn()/ringWorldTileXForColumn(). */
	pageOffset = (USHORT)((UWORD)(absoluteByteOffset - GAME_WORLD_BUFFER_MARGIN_TILES) % (UWORD)GAME_WORLD_SCROLL_PAGE_BYTES);
	return (USHORT)(GAME_WORLD_BUFFER_MARGIN_TILES + pageOffset);
}

static UBYTE useFixedTakeoffWorldWindow(const GameState* game) {
	return game->takeoffState == TAKEOFF_STATE_ROLLING_IN || game->takeoffState == TAKEOFF_STATE_READY;
}

static USHORT displayByteOffsetForGameState(const GameState* game) {
	if (useFixedTakeoffWorldWindow(game))
		return scrollAbsoluteByteOffset(game->scrollX);
	return scrollLocalByteOffset(game->scrollX);
}

static void updateGameScrollCopper(const UBYTE* worldBuffer, const GameState* game) {
	setCopperPlanePointers(worldBuffer, GAME_WORLD_ROW_BYTES, displayByteOffsetForGameState(game));
	setCopperFineScroll(horizontalScrollDelayToBplcon1(scrollDelayForBplcon1(game->scrollX)));
}

static WORD clampSpriteX(WORD x) {
	if (x < 0)
		return 0;
	if (x > SCREEN_WIDTH - 16)
		return SCREEN_WIDTH - 16;
	return x;
}

static WORD clampSpriteY(WORD y) {
	if (y < 0)
		return 0;
	if (y > HUD_TOP - WEAPON_SPRITE_HEIGHT)
		return HUD_TOP - WEAPON_SPRITE_HEIGHT;
	return y;
}

static void startImpact(GameState* game, WORD x, WORD y) {
	game->impact.active = 1;
	game->impact.timer = IMPACT_FRAMES;
	game->impact.worldAnchored = 0;
	game->impact.x = clampSpriteX(x);
	game->impact.y = clampSpriteY(y);
	game->impact.worldX = (LONG)game->scrollX + game->impact.x;
	game->impact.dx = 0;
	game->impact.dy = 0;
	playSfxAt(SFX_IMPACT, game->impact.x);
}

static void startImpactQuiet(GameState* game, WORD x, WORD y) {
	game->impact.active = 1;
	game->impact.timer = IMPACT_FRAMES;
	game->impact.worldAnchored = 0;
	game->impact.x = clampSpriteX(x);
	game->impact.y = clampSpriteY(y);
	game->impact.worldX = (LONG)game->scrollX + game->impact.x;
	game->impact.dx = 0;
	game->impact.dy = 0;
}

static void startWorldImpact(GameState* game, WORD x, WORD y) {
	startImpact(game, x, y);
	game->impact.worldAnchored = 1;
	game->impact.worldX = (LONG)game->scrollX + x;
}

static void startWorldImpactQuiet(GameState* game, WORD x, WORD y) {
	startImpactQuiet(game, x, y);
	game->impact.worldAnchored = 1;
	game->impact.worldX = (LONG)game->scrollX + x;
}

static UBYTE objectUsesGroundTargetHitSfx(UBYTE objectId) {
	return objectId == HAR_OBJ_GROUND_TARGET ||
		objectId == HAR_OBJ_ENEMY_SHIP ||
		objectId == HAR_OBJ_LAND ||
		objectId == HAR_OBJ_TOWN_BLOCK;
}

static void startGroundTargetHitImpact(GameState* game, WORD x, WORD y,
	LONG worldColumn, WORD tileY) {
	startWorldImpactQuiet(game, x, y);
	playGroundTargetHitSfx((UWORD)(worldColumn ^
		((LONG)tileY << 8) ^ frameCounter), x);
}

static void destroyWingman(GameState* game) {
	WingmanState* wingman = &game->wingman;
	if (!wingman->active)
		return;
	WORD impactX = wingmanScreenX(game);
	WORD impactY = wingman->screenY;
	wingman->active = 0;
	wingman->destroyed = 1;
	wingman->mode = WINGMAN_DESTROYED;
	wingman->rocket.active = 0;
	wingman->bomb.active = 0;
	wingman->returningToFormation = 0;
	if (game->enemyMissileTarget == ENEMY_TARGET_WINGMAN)
		game->enemyMissileTarget = ENEMY_TARGET_PLAYER;
	startImpact(game, impactX, impactY);
}

static UBYTE targetLockIsVisibleAhead(const GameState* game) {
	WORD screenX;
	if (!game->targetLock.active)
		return 0;
	screenX = (WORD)(game->targetLock.worldX - game->scrollX);
	return screenX >= game->playerX + PLAYER_SPRITE_WIDTH && screenX < SCREEN_WIDTH;
}

static UBYTE launchRocket(GameState* game, UBYTE requestMaverick) {
	if (game->rocketShot.active || game->rockets == 0 || playerOnOwnFrigateDeck(game))
		return 0;

	if (!debugInfiniteRockets)
		game->rockets--;
	game->rocketShot.active = 1;
	game->rocketShot.timer = 0;
	game->rocketShot.x = (WORD)(game->playerX + PLAYER_SPRITE_WIDTH - 2);
	game->rocketShot.y = (WORD)(game->playerY + 2);
	game->rocketShot.worldX = (LONG)game->scrollX + game->rocketShot.x;
	game->rocketShot.dx = ROCKET_SPEED_PIXELS;
	game->rocketShot.dy = 0;
	game->rocketShot.direction = MAVERICK_DIRECTION_RIGHT;
	game->rocketShot.guidanceDistance = 0;
	if (requestMaverick && targetLockIsVisibleAhead(game)) {
		game->rocketShot.type = ROCKET_SHOT_MAVERICK_LAUNCH;
		game->rocketShot.targetWorldX = game->targetLock.worldX + GAME_TILE_WIDTH / 2;
		game->rocketShot.targetY = (WORD)(game->targetLock.y + GAME_TILE_HEIGHT / 2);
	} else {
		/* CPC falls back to an ordinary rocket when Left+Fire has no lock. */
		game->rocketShot.type = ROCKET_SHOT_STANDARD;
		game->rocketShot.targetWorldX = 0;
		game->rocketShot.targetY = 0;
	}
	playSfxAt(SFX_FIRE, game->rocketShot.x);
	return 1;
}

static UBYTE directionToMaverickTarget(const WeaponState* rocket) {
	LONG dx = rocket->targetWorldX - (rocket->worldX + 8);
	WORD dy = (WORD)(rocket->targetY - (rocket->y + 4));
	if (dx == 0 && dy == 0)
		return MAVERICK_DIRECTION_NONE;
	if (dy < 0)
		return dx < 0 ? MAVERICK_DIRECTION_UP_LEFT :
			(dx > 0 ? MAVERICK_DIRECTION_UP_RIGHT : MAVERICK_DIRECTION_UP);
	if (dy > 0)
		return dx < 0 ? MAVERICK_DIRECTION_DOWN_LEFT :
			(dx > 0 ? MAVERICK_DIRECTION_DOWN_RIGHT : MAVERICK_DIRECTION_DOWN);
	return dx < 0 ? MAVERICK_DIRECTION_LEFT : MAVERICK_DIRECTION_RIGHT;
}

static void moveGuidedMaverick(WeaponState* rocket, UBYTE lockStillActive) {
	if (lockStillActive) {
		UBYTE newDirection = directionToMaverickTarget(rocket);
		if (newDirection != MAVERICK_DIRECTION_NONE)
			rocket->direction = newDirection;
	}

	switch (rocket->direction) {
		case MAVERICK_DIRECTION_UP:
			rocket->y -= MAVERICK_GUIDED_SPEED_PIXELS;
			break;
		case MAVERICK_DIRECTION_UP_RIGHT:
			rocket->worldX += MAVERICK_GUIDED_SPEED_PIXELS;
			rocket->y -= MAVERICK_GUIDED_SPEED_PIXELS;
			break;
		case MAVERICK_DIRECTION_RIGHT:
			rocket->worldX += MAVERICK_GUIDED_SPEED_PIXELS;
			break;
		case MAVERICK_DIRECTION_DOWN_RIGHT:
			rocket->worldX += MAVERICK_GUIDED_SPEED_PIXELS;
			rocket->y += MAVERICK_GUIDED_SPEED_PIXELS;
			break;
		case MAVERICK_DIRECTION_DOWN:
			rocket->y += MAVERICK_GUIDED_SPEED_PIXELS;
			break;
		case MAVERICK_DIRECTION_DOWN_LEFT:
			rocket->worldX -= MAVERICK_GUIDED_SPEED_PIXELS;
			rocket->y += MAVERICK_GUIDED_SPEED_PIXELS;
			break;
		case MAVERICK_DIRECTION_LEFT:
			rocket->worldX -= MAVERICK_GUIDED_SPEED_PIXELS;
			break;
		case MAVERICK_DIRECTION_UP_LEFT:
			rocket->worldX -= MAVERICK_GUIDED_SPEED_PIXELS;
			rocket->y -= MAVERICK_GUIDED_SPEED_PIXELS;
			break;
	}
}

static UBYTE launchBomb(GameState* game) {
	if (game->bombLaunchCooldown > 0 || game->bombShot.active || game->bombs == 0 || playerOnOwnFrigateDeck(game))
		return 0;

	if (!debugInfiniteBombs)
		game->bombs--;
	game->bombLaunchCooldown = BOMB_LAUNCH_COOLDOWN_FRAMES;
	game->bombShot.active = 1;
	game->bombShot.timer = 0;
	game->bombShot.x = (WORD)(game->playerX + 6);
	game->bombShot.y = (WORD)(game->playerY + PLAYER_SPRITE_HEIGHT - 1);
	game->bombShot.worldX = (LONG)game->scrollX + game->bombShot.x;
	game->bombShot.worldAnchored = 0;
	game->bombShot.dx = BOMB_SPEED_X_PIXELS;
	game->bombShot.dy = BOMB_SPEED_Y_PIXELS;
	playSfxAt(SFX_BOMB, game->bombShot.x);
	return 1;
}

static UBYTE updateWeapons(GameState* game, UBYTE scrollPixels, UBYTE** worldBuffers) {
	UBYTE changed = 0;

	if (game->rocketShot.active) {
		if (game->rocketShot.type == ROCKET_SHOT_STANDARD) {
			game->rocketShot.worldX += game->rocketShot.dx;
			/* Real CPC (lockinmissileheighttoplayer, :6994-7003): the
			 * player's standard fired rocket isn't ballistic - every frame
			 * it's in flight, its height is overwritten to match the
			 * player's CURRENT position, so steering up/down after firing
			 * visibly curves the rocket's flight path. */
			game->rocketShot.y = (WORD)(game->playerY + 2);
		} else if (game->rocketShot.type == ROCKET_SHOT_MAVERICK_LAUNCH) {
			game->rocketShot.worldX += game->rocketShot.dx;
			game->rocketShot.guidanceDistance += (UWORD)game->rocketShot.dx;
			if (game->rocketShot.guidanceDistance >= MAVERICK_GUIDANCE_DELAY_PIXELS)
				game->rocketShot.type = ROCKET_SHOT_MAVERICK_GUIDED;
		} else {
			UBYTE lockStillActive = game->targetLock.active &&
				game->targetLock.worldX + GAME_TILE_WIDTH / 2 == game->rocketShot.targetWorldX;
			moveGuidedMaverick(&game->rocketShot, lockStillActive);
		}
		game->rocketShot.x = (WORD)(game->rocketShot.worldX - game->scrollX);
		if (game->rocketShot.timer < 255)
			game->rocketShot.timer++;
		ObjectCell rocketCell;
		LONG rocketWorldColumn = -1;
		WORD rocketTileY = -1;
		WORD rocketProbeX = (WORD)(game->rocketShot.x +
			((game->rocketShot.type == ROCKET_SHOT_MAVERICK_GUIDED &&
				(game->rocketShot.direction == MAVERICK_DIRECTION_LEFT ||
				 game->rocketShot.direction == MAVERICK_DIRECTION_UP_LEFT ||
				 game->rocketShot.direction == MAVERICK_DIRECTION_DOWN_LEFT)) ? 2 : 12));
		WORD rocketProbeY = (WORD)(game->rocketShot.y + 4);
		UBYTE rocketHitObject = objectCellForWorldPoint(game, rocketProbeX, rocketProbeY, &rocketCell, &rocketWorldColumn, &rocketTileY) &&
			(rocketCell.id == HAR_OBJ_LAND || rocketCell.id == HAR_OBJ_GROUND_TARGET || rocketCell.id == HAR_OBJ_ENEMY_SHIP || rocketCell.id == HAR_OBJ_FLAK || rocketCell.id == HAR_OBJ_SMOKE || rocketCell.id == HAR_OBJ_OWN_FRIGATE);
		if (!rocketHitObject)
			rocketHitObject = enemyShipCellNearWorldPoint(game, rocketProbeX, rocketProbeY, -1, 1, -1, 1, &rocketCell, &rocketWorldColumn, &rocketTileY);
		if (!rocketHitObject)
			rocketHitObject = ownFrigateCellNearWorldPoint(game, rocketProbeX, rocketProbeY, &rocketCell, &rocketWorldColumn, &rocketTileY);
		if (!rocketHitObject)
			rocketHitObject = townBlockCellNearWorldPoint(game, rocketProbeX, rocketProbeY, &rocketCell, &rocketWorldColumn, &rocketTileY);
		if (!rocketHitObject &&
			game->rocketShot.type == ROCKET_SHOT_MAVERICK_GUIDED) {
			/* CPC guidance advances in character-sized logical coordinates
			 * and therefore cannot skip its target.  The Amiga port moves
			 * four pixels per axis and can alternate forever on opposite
			 * sides of the target centre.  A one-step proximity fuse maps
			 * that crossed centre back to the locked object cell. */
			LONG targetDx = game->rocketShot.targetWorldX -
				(game->rocketShot.worldX + 8);
			WORD targetDy = (WORD)(game->rocketShot.targetY -
				(game->rocketShot.y + 4));
			if (targetDx >= -MAVERICK_GUIDED_SPEED_PIXELS &&
				targetDx <= MAVERICK_GUIDED_SPEED_PIXELS &&
				targetDy >= -MAVERICK_GUIDED_SPEED_PIXELS &&
				targetDy <= MAVERICK_GUIDED_SPEED_PIXELS) {
				rocketWorldColumn = game->rocketShot.targetWorldX >>
					3;
				rocketTileY = game->rocketShot.targetY >> 3;
				rocketHitObject = objectCellForWorldColumnTile(
					rocketWorldColumn, rocketTileY, &rocketCell) &&
					(rocketCell.id == HAR_OBJ_GROUND_TARGET ||
					 rocketCell.id == HAR_OBJ_ENEMY_SHIP ||
					 rocketCell.id == HAR_OBJ_FLAK ||
					 rocketCell.id == HAR_OBJ_SMOKE ||
					 rocketCell.id == HAR_OBJ_OWN_FRIGATE ||
					 rocketCell.id == HAR_OBJ_TOWN_BLOCK ||
					 rocketCell.id == HAR_OBJ_LAND);
			}
		}
		if (rocketHitObject) {
			if (rocketCell.id == HAR_OBJ_GROUND_TARGET) {
				game->bonusScore += GROUND_TARGET_SCORE_VALUE;
				game->hitsCount++;
				updateHudValues(game);
			}
			if (rocketCell.id == HAR_OBJ_GROUND_TARGET) {
				/* Part 7 remainder: CPC's drawsmokesprite replaces the struck
				 * target tile itself with smoke (52, plus 51 above if that
				 * cell is sky) - it does not touch the ground surface at all,
				 * so no crater. rocketTileY here already IS the target's own
				 * row (objectCellForWorldColumnTile only matches
				 * HAR_OBJ_GROUND_TARGET at tileY==terrainY-1). */
				markTargetDestroyedAtColumn(rocketWorldColumn);
				addCpcHitSmokeAtColumnRow(rocketWorldColumn, rocketTileY);
				dirtyRedrawWorldColumn(worldBuffers, rocketWorldColumn);
			}
			if (rocketCell.id == HAR_OBJ_ENEMY_SHIP) {
				UBYTE shipChanged = damageEnemyShipAtColumnRow(rocketWorldColumn, rocketTileY);
				game->bonusScore += ENEMY_SHIP_SCORE_VALUE;
				game->hitsCount++;
				updateHudValues(game);
				dirtyRedrawWorldColumn(worldBuffers, rocketWorldColumn);
				if (shipChanged)
					dirtyRedrawWorldColumn(worldBuffers, rocketWorldColumn - 1);
			}
			if (rocketCell.id == HAR_OBJ_OWN_FRIGATE) {
				game->playerFrigateStatus = PLAYER_FRIGATE_STATUS_HIT;
				addCpcHitSmokeAtColumnRow(rocketWorldColumn, rocketTileY);
				dirtyRedrawWorldColumn(worldBuffers, rocketWorldColumn);
				dirtyRedrawWorldColumn(worldBuffers, rocketWorldColumn - 1);
			}
			if (rocketCell.id == HAR_OBJ_LAND) {
				if (markLandCraterAtColumnRow(rocketWorldColumn, rocketTileY))
					dirtyRedrawWorldColumn(worldBuffers, rocketWorldColumn);
			}
			if (rocketCell.id == HAR_OBJ_TOWN_BLOCK) {
				/* Part 7: CPC destroys buildings tile-by-tile (checkenemyhit's
				 * shared destructible path), not whole-building - only the
				 * exact hit cell turns to smoke, matching the same
				 * addCpcHitSmokeAtColumnRow() two-tile pattern already used
				 * for the own frigate above. Menu review: single-tile redraw
				 * instead of the whole column. */
				game->bonusScore += TOWN_BLOCK_SCORE_VALUE;
				game->hitsCount++;
				updateHudValues(game);
				addCpcHitSmokeAtColumnRow(rocketWorldColumn, rocketTileY);
				dirtyRedrawWorldTileIfSmoke(worldBuffers, rocketWorldColumn, rocketTileY);
				dirtyRedrawWorldTileIfSmoke(worldBuffers, rocketWorldColumn, rocketTileY - 1);
			}
			if (rocketCell.id == HAR_OBJ_FLAK || rocketCell.id == HAR_OBJ_SMOKE) {
				/* Sprint 14.95 Part 2: real CPC shares one object ID between
				 * flak and smoke - checkenemyhit's dec-a/cp-9 bombhitsealand
				 * path absorbs the weapon into either with no visible/
				 * audible effect at all: no explosion, no further smoke, no
				 * score. Neither is disturbed and both keep existing - skip
				 * startWorldImpact() entirely, unlike every other hit type
				 * below. */
				game->rocketShot.active = 0;
			} else {
				if (objectUsesGroundTargetHitSfx(rocketCell.id))
					startGroundTargetHitImpact(game, game->rocketShot.x,
						game->rocketShot.y, rocketWorldColumn, rocketTileY);
				else
					startWorldImpact(game, game->rocketShot.x,
						game->rocketShot.y);
				game->rocketShot.active = 0;
			}
			game->targetLock.active = 0;
		} else if (game->rocketShot.x >= SCREEN_WIDTH - 18) {
			startImpact(game, (WORD)(SCREEN_WIDTH - 28), game->rocketShot.y);
			game->rocketShot.active = 0;
		} else if (game->rocketShot.x < -16) {
			game->rocketShot.active = 0;
		}
		changed = 1;
	}

	if (game->bombShot.active) {
		/* The launch tile advances briefly down/forward.  Once momentum is
		 * exhausted the bomb becomes a world-space object: it continues
		 * falling, while horizontal screen motion comes only from scrolling
		 * the world beneath the Harrier. */
		if (game->bombShot.timer < BOMB_FORWARD_MOMENTUM_FRAMES) {
			game->bombShot.x += game->bombShot.dx;
			game->bombShot.y += game->bombShot.dy;
			game->bombShot.worldX = (LONG)game->scrollX +
				game->bombShot.x;
		} else {
			if (!game->bombShot.worldAnchored) {
				game->bombShot.worldX = (LONG)game->scrollX +
					game->bombShot.x;
				game->bombShot.worldAnchored = 1;
			}
			game->bombShot.x = (WORD)(game->bombShot.worldX -
				game->scrollX);
			game->bombShot.y += game->bombShot.dy;
		}
		if ((game->bombShot.timer % BOMB_EXTRA_FALL_INTERVAL) ==
			(BOMB_EXTRA_FALL_INTERVAL - 1))
			game->bombShot.y += game->bombShot.dy;
		if (game->bombShot.timer < 255)
			game->bombShot.timer++;
		ObjectCell bombCell;
		LONG bombWorldColumn = -1;
		WORD bombTileY = -1;
		UBYTE bombHitObject = objectCellForWorldPoint(game, (WORD)(game->bombShot.x + 3), (WORD)(game->bombShot.y + BOMB_SHOT_PIXEL_BOB_HEIGHT), &bombCell, &bombWorldColumn, &bombTileY) &&
			(bombCell.id == HAR_OBJ_LAND || bombCell.id == HAR_OBJ_GROUND_TARGET || bombCell.id == HAR_OBJ_ENEMY_SHIP || bombCell.id == HAR_OBJ_FLAK || bombCell.id == HAR_OBJ_SMOKE || bombCell.id == HAR_OBJ_OWN_FRIGATE);
		if (!bombHitObject)
			bombHitObject = objectCellForWorldPoint(game, (WORD)(game->bombShot.x + 1), (WORD)(game->bombShot.y + BOMB_SHOT_PIXEL_BOB_HEIGHT), &bombCell, &bombWorldColumn, &bombTileY) &&
				(bombCell.id == HAR_OBJ_LAND || bombCell.id == HAR_OBJ_GROUND_TARGET);
		if (!bombHitObject)
			bombHitObject = objectCellForWorldPoint(game, (WORD)(game->bombShot.x + 5), (WORD)(game->bombShot.y + BOMB_SHOT_PIXEL_BOB_HEIGHT), &bombCell, &bombWorldColumn, &bombTileY) &&
				(bombCell.id == HAR_OBJ_LAND || bombCell.id == HAR_OBJ_GROUND_TARGET);
		if (!bombHitObject)
			bombHitObject = objectCellForWorldPoint(game, (WORD)(game->bombShot.x + 3), (WORD)(game->bombShot.y + 1), &bombCell, &bombWorldColumn, &bombTileY) &&
				(bombCell.id == HAR_OBJ_GROUND_TARGET);
		if (!bombHitObject)
			bombHitObject = ownFrigateCellNearWorldPoint(game, (WORD)(game->bombShot.x + 3), (WORD)(game->bombShot.y + BOMB_SHOT_PIXEL_BOB_HEIGHT), &bombCell, &bombWorldColumn, &bombTileY);
		if (!bombHitObject)
			bombHitObject = townBlockCellNearWorldPoint(game, (WORD)(game->bombShot.x + 3), (WORD)(game->bombShot.y + BOMB_SHOT_PIXEL_BOB_HEIGHT), &bombCell, &bombWorldColumn, &bombTileY);
		if (bombHitObject) {
			if (bombCell.id == HAR_OBJ_GROUND_TARGET) {
				game->bonusScore += GROUND_TARGET_SCORE_VALUE;
				game->hitsCount++;
				updateHudValues(game);
			}
			if (bombCell.id == HAR_OBJ_GROUND_TARGET) {
				/* See the matching rocket branch above. */
				markTargetDestroyedAtColumn(bombWorldColumn);
				if (game->targetLock.active &&
					game->targetLock.worldX / GAME_TILE_WIDTH == bombWorldColumn)
					game->targetLock.active = 0;
				addCpcHitSmokeAtColumnRow(bombWorldColumn, bombTileY);
				dirtyRedrawWorldColumn(worldBuffers, bombWorldColumn);
			}
			if (bombCell.id == HAR_OBJ_ENEMY_SHIP) {
				UBYTE shipChanged = damageEnemyShipAtColumnRow(bombWorldColumn, bombTileY);
				game->bonusScore += ENEMY_SHIP_SCORE_VALUE;
				game->hitsCount++;
				updateHudValues(game);
				dirtyRedrawWorldColumn(worldBuffers, bombWorldColumn);
				if (shipChanged)
					dirtyRedrawWorldColumn(worldBuffers, bombWorldColumn - 1);
			}
			if (bombCell.id == HAR_OBJ_OWN_FRIGATE) {
				game->playerFrigateStatus = PLAYER_FRIGATE_STATUS_HIT;
				addCpcHitSmokeAtColumnRow(bombWorldColumn, bombTileY);
				dirtyRedrawWorldColumn(worldBuffers, bombWorldColumn);
				dirtyRedrawWorldColumn(worldBuffers, bombWorldColumn - 1);
			}
			if (bombCell.id == HAR_OBJ_LAND) {
				if (markLandCraterAtColumnRow(bombWorldColumn, bombTileY))
					dirtyRedrawWorldColumn(worldBuffers, bombWorldColumn);
			}
			if (bombCell.id == HAR_OBJ_TOWN_BLOCK) {
				/* CPC checkenemyhit -> drawsmokesprite: exact struck
				 * building cell becomes smoke 52, plus smoke 51 one row
				 * above only when that cell is sky. Rebuild the complete
				 * column so the wide-building overlay also re-evaluates its
				 * per-cell smoke skip. */
				game->bonusScore += TOWN_BLOCK_SCORE_VALUE;
				game->hitsCount++;
				updateHudValues(game);
				addCpcHitSmokeAtColumnRow(bombWorldColumn, bombTileY);
				dirtyRedrawWorldColumn(worldBuffers, bombWorldColumn);
			}
			if (bombCell.id == HAR_OBJ_FLAK || bombCell.id == HAR_OBJ_SMOKE) {
				/* Sprint 14.95 Part 2: see the matching rocket branch above -
				 * flak/smoke absorb the weapon with no visible/audible
				 * effect. */
				game->bombShot.active = 0;
			} else {
				if (objectUsesGroundTargetHitSfx(bombCell.id))
					startGroundTargetHitImpact(game, game->bombShot.x,
						game->bombShot.y, bombWorldColumn, bombTileY);
				else if (game->bombShot.timer <= BOMB_IMPACT_SFX_GRACE_FRAMES)
					startWorldImpactQuiet(game, game->bombShot.x, game->bombShot.y);
				else
					startWorldImpact(game, game->bombShot.x, game->bombShot.y);
				game->bombShot.active = 0;
			}
		} else if (game->bombShot.y >= SEA_SURFACE_Y) {
			game->bombShot.active = 0;
		} else if (game->bombShot.x < -16) {
			game->bombShot.active = 0;
		}
		changed = 1;
	}

	if (game->impact.active) {
		if (game->impact.timer > 0)
			game->impact.timer--;
		else
			game->impact.active = 0;
		changed = 1;
	}

	return changed;
}

/* Channels 3 and 5 are temporarily borrowed for crash debris. Channel 3 is
 * otherwise the enemy plane's attached half; channel 5 is hidden in normal
 * play because player and Wingman weapons are rendered as playfield Bobs. */
static void updateCrashPartSprites(UWORD* crashPart1Sprite, UWORD* enemyAttachSprite, const GameState* game) {
	if (game->crashTimer) {
		if (game->crashPart[1].active)
			buildPlayerCrashPartSprite(crashPart1Sprite, game->crashPart[1].x, game->crashPart[1].y, 1);
		else
			hideHardwareSprite(crashPart1Sprite);

		if (game->crashPart[2].active)
			buildPlayerCrashPartSprite(enemyAttachSprite, game->crashPart[2].x, game->crashPart[2].y, 2);
		else
			hideHardwareSprite(enemyAttachSprite);
		return;
	}

	/* Player rocket is a pixel-precise playfield Bob from Sprint 15.12.
	 * Channel 5 remains hidden during normal play and is only borrowed by
	 * crash debris above, until a later sprite allocation uses it. */
	hideHardwareSprite(crashPart1Sprite);
}

static UBYTE rectsOverlap(WORD ax, WORD ay, WORD aw, WORD ah, WORD bx, WORD by, WORD bw, WORD bh) {
	return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static UBYTE runtimeFlakTileAtColumnRow(LONG worldColumn, WORD tileY) {
	if (worldColumn < 0 || tileY < 0)
		return 0;
	for (UBYTE index = 0; index < runtimeFlakCount; index++) {
		if (runtimeFlakColumns[index] == (UWORD)worldColumn && runtimeFlakRows[index] == (UBYTE)tileY)
			return runtimeFlakTiles[index];
	}
	return 0;
}

/* Removes any flak entry the player can never see again (behind the current
 * scroll position by more than roughly two screens). Flak now spawns
 * continuously as new columns scroll in and, matching real CPC, is never
 * removed by the player (see the updateWeapons() flak branch) - without this
 * the runtime list would otherwise grow for the entire length of a flight. */
static void pruneRuntimeFlakBehindColumn(LONG cutoffColumn) {
	UBYTE index = 0;
	while (index < runtimeFlakCount) {
		if ((LONG)runtimeFlakColumns[index] < cutoffColumn) {
			runtimeFlakCount--;
			runtimeFlakColumns[index] = runtimeFlakColumns[runtimeFlakCount];
			runtimeFlakRows[index] = runtimeFlakRows[runtimeFlakCount];
			runtimeFlakTiles[index] = runtimeFlakTiles[runtimeFlakCount];
		} else {
			index++;
		}
	}
}

static UBYTE removeRuntimeFlakAt(LONG worldColumn, WORD tileY) {
	if (worldColumn < 0 || tileY < 0)
		return 0;
	for (UBYTE index = 0; index < runtimeFlakCount; index++) {
		if (runtimeFlakColumns[index] != (UWORD)worldColumn ||
			runtimeFlakRows[index] != (UBYTE)tileY)
			continue;
		/* Keep the compact runtime list by moving its final entry into the
		 * consumed slot, matching pruneRuntimeFlakBehindColumn(). */
		runtimeFlakCount--;
		runtimeFlakColumns[index] = runtimeFlakColumns[runtimeFlakCount];
		runtimeFlakRows[index] = runtimeFlakRows[runtimeFlakCount];
		runtimeFlakTiles[index] = runtimeFlakTiles[runtimeFlakCount];
		return 1;
	}
	return 0;
}

static UBYTE addRuntimeFlak(LONG worldColumn, WORD tileY, UBYTE tile) {
	if (worldColumn < 0 || tileY < 0 || tileY >= GAME_OBJECT_MAP_HEIGHT_TILES)
		return 0;
	if (runtimeFlakTileAtColumnRow(worldColumn, tileY))
		return 0;
	if (runtimeFlakCount >= GAME_RUNTIME_FLAK_MAX)
		return 0;
	runtimeFlakColumns[runtimeFlakCount] = (UWORD)worldColumn;
	runtimeFlakRows[runtimeFlakCount] = (UBYTE)tileY;
	runtimeFlakTiles[runtimeFlakCount] = tile;
	runtimeFlakCount++;
	return 1;
}

static UWORD runtimeFlakLastColumn = 0xffff;

/* Menu review: real CPC gates flak on a per-target countdown (l8864), not a
 * flat per-column probability - a flat roll can't reproduce CPC's bursty
 * "several in a row, then a pause" groupings. Exact CPC threshold values
 * weren't available to reconstruct precisely (only the qualitative
 * structure - terrain height, land-vs-town phase, a countdown, running RNG
 * bits, fixed insertion column - all of which this already uses except the
 * countdown itself), so this rebuilds the STRUCTURE rather than a verified
 * formula: a countdown reseeded from the running per-column RNG state each
 * time it's spent, biased shorter in town to match the review's "flak
 * clearly denser in town" observation. Flagged as an approximation, same as
 * the row-offset range and per-column probabilities it replaces. -1 means
 * "not yet seeded" (fresh session). */
/* Sprint 14.97 PRI 2: CPC's launchflakattack state machine (asm:6033-6095),
 * translated directly. CPC maintains three state variables that evolve
 * column-by-column:
 *   l884b: target type from last insertenemylandtile ((R>>3)&3), or 4 after
 *          a reset. Bit 1 of (l884b+1) triggers a reset when l884b is 1 or 2.
 *   l884c: flak threshold. Set to (terrainHeight-3) on reset, or 10 in town.
 *   l8864: 4-stage countdown counter. Starts at 4 on reset, decrements each
 *          land column. When 0, only town can spawn flak.
 * Flak spawns when h <= threshold, where h = (l8859 >> 4) & 0x0F (upper
 * nibble of genrandomhl's high byte, range 0-15) — an ABSOLUTE row, not an
 * offset above terrain. The cell at (column, h) must be sky. */
static UBYTE cpcFlakL884b = 0;
static UBYTE cpcFlakL884c = 0;
static UBYTE cpcFlakL8864 = 0;

/* Returns the raw l884b value (0-3) if a target was placed at this column,
 * or 0xFF if no target was placed (l884b unchanged from previous column).
 * TANK_REAR doesn't update l884b (it's the second half of a tank, not a
 * fresh insertenemylandtile call). */
static UBYTE cpcFlakL884bForWorldColumn(LONG worldColumn) {
	const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
	if (!segment || segment->terrainKind != HAR_TERRAIN_CPC_RANDOM_LAND)
		return 0xFF;
	LONG localColumn = worldColumn - segment->startColumn;
	if (localColumn < 0 || localColumn >= CPC_LAND_PROCEDURAL_LENGTH)
		return 0xFF;
	UBYTE target = cpcLandProceduralTarget((UWORD)localColumn);
	if (target == CPC_LAND_TARGET_NONE || target == CPC_LAND_TARGET_TANK_REAR)
		return 0xFF;
	return (UBYTE)(target - CPC_LAND_TARGET_RADAR);
}

static void resetRuntimeFlak(void) {
	runtimeFlakCount = 0;
	runtimeFlakLastColumn = 0xffff;
	cpcFlakL884b = 0;
	cpcFlakL884c = 0;
	cpcFlakL8864 = 0;
}

static void resetPowerup(GameState* game) {
	memset(&game->powerup, 0, sizeof(PowerupState));
	game->powerup.lastSpawnCheckColumn = 0xffff;
}

/* Real CPC launchflakattack (:6055-6120) runs continuously from
 * checkenemyattacks. This reconstruction preserves l884b/l884c/l8864,
 * derives the absolute row from l8859's upper nibble, requires a sky cell,
 * and uses R bit 0 for the sprite variant. R is currently the column-start
 * approximation documented by cpcRStateForWorldColumn(); it must later be
 * advanced to the assembly's flak decision point by an M1-derived offset.
 *
 * Correctness fix: the ring buffer streams up to RING_WORLD_STREAM_MAX_AHEAD_
 * TILES(64) columns ahead of the visible screen edge, so the column at
 * checkColumn (the rightmost VISIBLE column, not the streaming frontier) was
 * already fully rendered into the bitplane buffer up to 64 columns/many
 * frames earlier. Just appending to runtimeFlakColumns[] here updates the
 * data objectCellForWorldColumnTile()/buildWorldTileColumn() consult, but
 * the already-painted pixels for that column keep showing plain sky - the
 * flak becomes real for collision purposes while staying invisible on
 * screen, only revealed incidentally if something else later triggers a
 * redraw of that same column. Takes worldBuffers now specifically to force
 * that redraw itself, immediately, via the same dirtyRedrawWorldColumn()
 * every other "something changed here" case in this file already uses. */
static void trySpawnFlak(GameState* game, UBYTE** worldBuffers) {
	UWORD checkColumn = (UWORD)((game->scrollX >> 3) + GAME_MAP_WIDTH);
	if (checkColumn == runtimeFlakLastColumn)
		return;
	runtimeFlakLastColumn = checkColumn;
	pruneRuntimeFlakBehindColumn((LONG)(game->scrollX >> 3) - GAME_MAP_WIDTH * 2);

	if (game->gameOver || game->crashTimer)
		return;

	const LevelSegmentDef* segment = levelSegmentForWorldColumn((LONG)checkColumn);
	UBYTE stage = stageForWorldColumn((LONG)checkColumn, segment);
	UBYTE terrainKind = segment ? segment->terrainKind : terrainKindForStage(stage);
	UBYTE isLand = terrainKind == HAR_TERRAIN_CPC_RANDOM_LAND;
	UBYTE isTown = terrainKind == HAR_TERRAIN_TOWN;
	if (!isLand && !isTown)
		return;

	/* Update l884b from this column's target placement (insertenemylandtile
	 * only runs in land, not town). 0xFF means no target this column -
	 * l884b keeps its previous value. */
	UBYTE newL884b = cpcFlakL884bForWorldColumn((LONG)checkColumn);
	if (newL884b != 0xFF)
		cpcFlakL884b = newL884b;


	/* Reset block (asm:6038-6050): fires when bit 1 of (l884b+1) is set,
	 * i.e. l884b is 1 or 2 (launcher or gun). Sets l8864=4, l884b=4 (so it
	 * won't reset again until next target), l884c=terrainHeight-3. */
	if (cpcFlakL884b != 4) {
		UBYTE testVal = (UBYTE)(cpcFlakL884b + 1);
		if (testVal & 2) {
			cpcFlakL8864 = 4;
			cpcFlakL884b = 4;
			UBYTE terrainY = terrainYForWorldColumn((LONG)checkColumn, segment, terrainKind);
			cpcFlakL884c = (UBYTE)(terrainY >= 3 ? terrainY - 3 : 0);
		}
	}

	/* Stage dispatch (asm:6052-6064). */
	UBYTE threshold;
	if (cpcFlakL8864 != 0) {
		if (isLand) {
			cpcFlakL8864--;
			threshold = cpcFlakL884c;
		} else if (isTown) {
			threshold = 10;
		} else {
			return;
		}
	} else {
		/* l8864 == 0: only town can spawn flak */
		if (isTown) {
			threshold = 10;
		} else {
			return;
		}
	}

	/* h = upper nibble of l8859 (genrandomhl high byte), range 0-15.
	 * This is an ABSOLUTE tile row, not an offset above terrain.
	 * asm:6066-6070: ld a,(l8859); rlca*4; and #0f */
	UWORD rng = cpcRandomStateForWorldColumn((LONG)checkColumn);
	UBYTE l8859 = (UBYTE)(rng >> 8);
	UBYTE h = (UBYTE)((l8859 >> 4) & 0x0f);

	/* Spawn check: if threshold < h, no flak (asm:6078-6079: cp h; ret c). */
	if (h > threshold)
		return;

	/* Cell at (column, h) must be sky (asm:6071-6076: getskytilemapid; dec a;
	 * ret nz — only sky (ID 1) passes). */
	WORD flakRow = (WORD)h;
	if (flakRow >= GAME_OBJECT_MAP_HEIGHT_TILES)
		return;
	ObjectCell existingCell;
	if (!objectCellForWorldColumnTile((LONG)checkColumn, flakRow, &existingCell) || existingCell.id != HAR_OBJ_SKY)
		return;

	/* Flak sprite: 57 or 58 based on R bit 0 (asm:6082-6087). Sprint 14.102:
	 * launchflakattack got its own LOGGEN instrumentation (R at function
	 * entry vs. R at this exact ld a,r). Measured tightly across 30 real
	 * flak spawns in one session: 29/30 exactly 55 M1 fetches (1 outlier at
	 * 91, almost certainly an interrupt during capture). Real CPC reads R
	 * here only after the gate/threshold checks above have already run, not
	 * at this column's generation time - applying that measured +55 offset
	 * before taking bit 0 is a real correction, not just documentation:
	 * since 55 is odd, it actually flips which tile parity comes out
	 * compared to using the raw column-start R directly. This is still an
	 * approximation (it assumes launchflakattack's own entry R lines up
	 * with this column's start R, which real CPC doesn't guarantee - they're
	 * called from different points in the per-frame sequence) but it's a
	 * measurement-informed correction rather than an unexamined reuse. Only
	 * affects which of the two near-identical flak tiles gets drawn - not
	 * the spawn threshold, row, or sky-cell gating above. */
	UBYTE rState = cpcRStateForWorldColumn((LONG)checkColumn);
	UBYTE tile = (UBYTE)(((rState + 55) & 1) ? 58 : 57);
	if (addRuntimeFlak((LONG)checkColumn, flakRow, tile)) {
		dirtyRedrawWorldTile(worldBuffers, (LONG)checkColumn, flakRow, tile);
		playFlakGunSfx((UWORD)(checkColumn ^ ((UWORD)flakRow << 8) ^ rState),
			SCREEN_WIDTH - 1, (WORD)(flakRow * GAME_TILE_HEIGHT),
			game->playerY);
	}
}

static UWORD targetLockLastColumn = 0xffff;

static void resetTargetLock(void) {
	targetLockLastColumn = 0xffff;
}

/* CPC scrolls the single lock left and removes it behind the Harrier. New
 * targets cannot replace it while any player rocket is in flight. */
static void updateTargetLock(GameState* game) {
	if (game->targetLock.active &&
		game->targetLock.worldX - game->scrollX <= 5 * GAME_TILE_WIDTH)
		game->targetLock.active = 0;

	if (game->rocketShot.active)
		return;

	UWORD checkColumn = (UWORD)((game->scrollX >> 3) + GAME_MAP_WIDTH);
	if (checkColumn == targetLockLastColumn)
		return;
	targetLockLastColumn = checkColumn;

	const LevelSegmentDef* segment = levelSegmentForWorldColumn((LONG)checkColumn);
	UBYTE stage = stageForWorldColumn((LONG)checkColumn, segment);
	UBYTE terrainKind = segment ? segment->terrainKind : terrainKindForStage(stage);
	if (terrainKind != HAR_TERRAIN_CPC_RANDOM_LAND)
		return;

	LONG localColumn = segment ? (LONG)checkColumn - segment->startColumn : (LONG)checkColumn;
	if (localColumn < 0 || localColumn >= CPC_LAND_PROCEDURAL_LENGTH)
		return;

	UBYTE target = cpcLandProceduralTarget((UWORD)localColumn);
	if (target == CPC_LAND_TARGET_NONE)
		return;

	UBYTE terrainY = terrainYForWorldColumn((LONG)checkColumn, segment, terrainKind);
	if (terrainY == 255)
		return;

	game->targetLock.active = 1;
	game->targetLock.worldX = (LONG)checkColumn * GAME_TILE_WIDTH;
	game->targetLock.y = (WORD)((terrainY - 1) * GAME_TILE_HEIGHT);
	game->targetLock.targetType = target;
}

static UBYTE lerpFadeNibble(UBYTE from, UBYTE to, UBYTE step) {
	return (UBYTE)(from + (((WORD)to - (WORD)from) * step) / CITY_FADE_STEP_COUNT);
}

static UWORD lerpFadeRgb(UWORD dayRgb, UWORD duskRgb, UBYTE step) {
	UBYTE dayR = (UBYTE)((dayRgb >> 8) & 0xf), dayG = (UBYTE)((dayRgb >> 4) & 0xf), dayB = (UBYTE)(dayRgb & 0xf);
	UBYTE duskR = (UBYTE)((duskRgb >> 8) & 0xf), duskG = (UBYTE)((duskRgb >> 4) & 0xf), duskB = (UBYTE)(duskRgb & 0xf);
	UBYTE r = lerpFadeNibble(dayR, duskR, step);
	UBYTE g = lerpFadeNibble(dayG, duskG, step);
	UBYTE b = lerpFadeNibble(dayB, duskB, step);
	return (UWORD)((r << 8) | (g << 4) | b);
}

static void applyCityFadeStep(UBYTE step) {
	currentSkyTopRgb = lerpFadeRgb(GAME_SKY_TOP_RGB, GAME_SKY_TOP_DUSK_RGB, step);
	currentSkyMidRgb = lerpFadeRgb(GAME_SKY_MID_RGB, GAME_SKY_MID_DUSK_RGB, step);
	currentSkyLowRgb = lerpFadeRgb(GAME_SKY_LOW_RGB, GAME_SKY_LOW_DUSK_RGB, step);
	currentCloudTopRgb = lerpFadeRgb(GAME_SKY_TOP_CLOUD_RGB, GAME_SKY_TOP_CLOUD_DUSK_RGB, step);
	currentSeaLowRgb = lerpFadeRgb(GAME_SKY_LOW_SEA_RGB, GAME_SKY_LOW_SEA_DUSK_RGB, step);
	currentPanelSeaRgb = lerpFadeRgb(GAME_HUD_PANEL_SEA_RGB, GAME_HUD_PANEL_SEA_DUSK_RGB, step);

	if (activeCopperSkyTopColor)
		*activeCopperSkyTopColor = currentSkyTopRgb;
	if (activeCopperSkyMidColor)
		*activeCopperSkyMidColor = currentSkyMidRgb;
	if (activeCopperSkyLowColor)
		*activeCopperSkyLowColor = currentSkyLowRgb;
	if (activeCopperCloudTopColor)
		*activeCopperCloudTopColor = currentCloudTopRgb;
	if (activeCopperSeaLowColor)
		*activeCopperSeaLowColor = currentSeaLowRgb;
	if (activeCopperPanelSeaColor)
		*activeCopperPanelSeaColor = currentPanelSeaRgb;
}

/* Reset for a fresh session - the current*Rgb globals persist across
 * sessions (same reason the copper list itself is rebuilt fresh each
 * session, see buildGameHudCopper()'s call site), so a session starting
 * right after a previous flight ended mid-town would otherwise bake stale
 * dusk colours into the new copper list before the player has gone
 * anywhere. */
static void resetCityFade(GameState* game) {
	game->cityFadeStep = 0;
	game->cityFadeTimer = CITY_FADE_STEP_FRAMES;
	currentSkyTopRgb = GAME_SKY_TOP_RGB;
	currentSkyMidRgb = GAME_SKY_MID_RGB;
	currentSkyLowRgb = GAME_SKY_LOW_RGB;
	currentCloudTopRgb = GAME_SKY_TOP_CLOUD_RGB;
	currentSeaLowRgb = GAME_SKY_LOW_SEA_RGB;
	currentPanelSeaRgb = GAME_HUD_PANEL_SEA_RGB;
}

/* Sprint 14.95 Part 5: the "red flash" originally reported turned out to be
 * this missing feature, not a bug - see the CITY_FADE_STEP_COUNT comment.
 * Driven by the terrain kind at the rightmost VISIBLE column (same
 * checkColumn convention as trySpawnFlak()), not the player's own/left-edge
 * position - CPC's startpalettefade fires at world-generation time, i.e. the
 * instant town terrain first scrolls onto the right edge, not ~a screen's
 * width later once it reaches the player. Triggering off the left edge was
 * tried first and felt late for exactly that reason (a full GAME_MAP_WIDTH
 * of town already visible before the fade even started).
 *
 * CPC does not restore day colours when the town geometry ends:
 * beginlandingapproach enters landinghoverloop with the current red/dusk
 * palette still active.  Day is restored only when the successful landing
 * starts the next newlevelloop.  Therefore the first non-zero fade step
 * latches the dusk target for the rest of this GameState; resetCityFade()
 * releases it when startGameSession() creates the next level. */
static void updateCityFade(GameState* game) {
	LONG worldColumn = (LONG)((game->scrollX >> 3) + GAME_MAP_WIDTH);
	const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
	UBYTE stage = stageForWorldColumn(worldColumn, segment);
	UBYTE terrainKind = segment ? segment->terrainKind : terrainKindForStage(stage);
	UBYTE targetStep =
		(terrainKind == HAR_TERRAIN_TOWN || game->cityFadeStep != 0) ?
			CITY_FADE_STEP_COUNT : 0;

	if (game->cityFadeStep == targetStep) {
		game->cityFadeTimer = CITY_FADE_STEP_FRAMES;
		return;
	}
	if (game->cityFadeTimer > 0) {
		game->cityFadeTimer--;
		return;
	}
	game->cityFadeStep = (UBYTE)(game->cityFadeStep + (targetStep > game->cityFadeStep ? 1 : -1));
	game->cityFadeTimer = CITY_FADE_STEP_FRAMES;
	applyCityFadeStep(game->cityFadeStep);
}

/* Sprint 14.96: descending powerups. CPC's launchenemyplane routine runs
 * a spawn roll on every right-edge column while in level phase 2..7 (after
 * the opening sea, through land and town, before the pier/harbor) - on a
 * 1/16 R-register pass it then either spawns an enemy plane, or spawns a
 * powerup selected from a deterministic 1..6 sequence (1=health, 2=rockets,
 * 3=bombs, 4=rockets, 5=bombs, 6=skip). Mirrors that here, using
 * frameCounter's low bits in place of Z80's R (CPC's R is itself just
 * instruction-timing dependent, not a true RNG, so a frame-count mask is a
 * fair Amiga equivalent that doesn't lock spawning to the renderer's
 * deterministic per-column RNG the way flak/terrain/cloud generation are).
 * Player must also be flying high enough: CPC requires
 * playerTileY < (11 - difficulty), i.e. above the terrain-difficulty floor
 * - same floor as cpcLandMinimumRow(). No active enemy missile may be in
 * flight (CPC reuses that missile's channel/slot for the spawn test). */
static UBYTE stageAllowsPowerup(const GameState* game) {
	LONG worldColumn = (LONG)((game->scrollX >> 3) + GAME_MAP_WIDTH);
	const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
	UBYTE stage = stageForWorldColumn(worldColumn, segment);
	/* CPC: cp 2 / ret c ; cp 8 / ret nc  ->  stages 2..7 inclusive. */
	return stage >= HAR_STAGE_ENEMY_SHIP_FIRED_MISSILE && stage < HAR_STAGE_START_PIER;
}

static UBYTE playerHighEnoughForPowerup(const GameState* game) {
	/* CPC: ld a,(leveldifficulty); ld c,a; ld a,11; sub c; ld c,a; ld a,h;
	 * cp c; ret nc. H is the player's tile Y; lower Y = higher on screen.
	 * Skill 1 -> floor 10, skill 5 -> floor 6. */
	UBYTE floor = (UBYTE)(POWERUP_ALTITUDE_FLOOR_BASE - game->skillLevel);
	return (UBYTE)(game->playerY >> 3) < floor;
}

static void spawnPowerup(GameState* game, UBYTE type, UBYTE startRow) {
	PowerupState* p = &game->powerup;
	p->active = 1;
	p->type = type;
	p->worldX = (LONG)game->scrollX + (POWERUP_SPAWN_COLUMN << 3);
	p->y = (WORD)(startRow << 3);
	p->fallCounter = 0;
}

/* CPC's spawnid sequence (post-R-roll, post-wingman-check):
 * 1=health, 2=rockets, 3=bombs, 4=rockets, 5=bombs, 6=skip (no powerup,
 * enemy plane spawns instead). Wrapped at 6. */
static UBYTE nextPowerupTypeForSpawnId(UBYTE spawnId) {
	switch (spawnId) {
		case 1: return POWERUP_HEALTH;
		case 2: return POWERUP_ROCKETS;
		case 3: return POWERUP_BOMBS;
		case 4: return POWERUP_ROCKETS;
		case 5: return POWERUP_BOMBS;
		default: return POWERUP_NONE;
	}
}

static void trySpawnPowerup(GameState* game) {
	PowerupState* p = &game->powerup;
	/* CPC calls launchenemyplane once from the newly generated right-edge
	 * world column, not once per video frame. Consume each column exactly
	 * once even if an active powerup or another gate rejects the attempt. */
	UWORD checkColumn = (UWORD)((game->scrollX >> 3) + POWERUP_SPAWN_COLUMN);
	if (checkColumn == p->lastSpawnCheckColumn)
		return;
	p->lastSpawnCheckColumn = checkColumn;

	if (p->active)
		return;
	if (game->gameOver || game->crashTimer || game->respawnSafeTimer > 0)
		return;
	if (game->enemyMissile.active)
		return;
	if (!stageAllowsPowerup(game))
		return;
	if (!playerHighEnoughForPowerup(game))
		return;

	/* CPC: ld a,r; and #0f; ret nz. Use the modeled R value belonging to
	 * this generated column rather than a frame counter. */
	UBYTE rState = cpcRStateForWorldColumn(checkColumn);
	if ((rState & POWERUP_SPAWN_ROLL_MASK) != 0)
		return;

	/* CPC's wingman-resurrection branch: if wingman was destroyed (status
	 * 254), every qualifying spawn becomes a wingman powerup instead of
	 * consulting the normal health/rockets/bombs rotation below - it does
	 * not consume a spawnId slot, so the rotation resumes exactly where it
	 * left off once the wingman is back. */
	if (game->wingman.destroyed) {
		LONG spawnWorldColumn = checkColumn;
		UBYTE startRow = (UBYTE)(cpcRandomStateForWorldColumn(spawnWorldColumn) & POWERUP_SPAWN_MAX_ROW);
		spawnPowerup(game, POWERUP_WINGMAN, startRow);
		return;
	}

	/* CPC reads R again after roughly thirteen intervening opcode fetches.
	 * R's low seven bits advance linearly, so model that second read instead
	 * of inventing a separate frame-based random value. */
	UBYTE powerupRoll = (UBYTE)((rState + 13) & CPC_R_MASK);
	if (powerupRoll < 100)
		return;

	game->powerup.spawnId = (UBYTE)(game->powerup.spawnId + 1);
	if (game->powerup.spawnId > 6)
		game->powerup.spawnId = 1;

	UBYTE type = nextPowerupTypeForSpawnId(game->powerup.spawnId);
	if (type == POWERUP_NONE)
		return;  /* CPC's 6th slot: enemy plane spawns instead - leave to updateEnemyPlane */

	/* CPC uses currtime&3 after the right-edge column's RNG update. */
	LONG spawnWorldColumn = checkColumn;
	UBYTE startRow = (UBYTE)(cpcRandomStateForWorldColumn(spawnWorldColumn) & POWERUP_SPAWN_MAX_ROW);
	spawnPowerup(game, type, startRow);
}

static void destroyPowerup(GameState* game, UBYTE withExplosion) {
	(void)withExplosion;
	game->powerup.active = 0;
}

static void activatePowerup(GameState* game, UBYTE type) {
	UBYTE collected = 1;
	switch (type) {
		case POWERUP_HEALTH:
			/* CPC: xor a; ld (flakdamagecount),a; call displayhealth -
			 * clears all accumulated flak damage, restores full armour. */
			game->flakDamageCount = 0;
			game->armour = 100;
			break;
		case POWERUP_ROCKETS:
			{
				UBYTE fullBombs;
				ammoForSkill(game->skillLevel, &fullBombs,
					&game->rockets);
			}
			break;
		case POWERUP_BOMBS:
			{
				UBYTE fullRockets;
				ammoForSkill(game->skillLevel, &game->bombs,
					&fullRockets);
			}
			break;
		case POWERUP_WINGMAN:
			if (game->wingman.destroyed) {
				/* Revive at the pickup point and reuse WINGMAN_TAKEOFF's
				 * existing smooth converge-into-formation logic (its own
				 * "climb clear of the carrier" phase is a no-op here since
				 * a mid-air pickup already starts above TAKEOFF_CLEAR_Y). */
				WingmanState* wingman = &game->wingman;
				WORD pickupScreenX = (WORD)(game->powerup.worldX - (LONG)game->scrollX);
				wingman->active = 1;
				wingman->destroyed = 0;
				wingman->mode = WINGMAN_TAKEOFF;
				wingman->interceptScreenX = pickupScreenX;
				wingman->screenY = game->powerup.y;
				wingman->row = (WORD)(game->powerup.y / GAME_TILE_HEIGHT);
				wingman->moveTimer = 0;
				wingman->returningToFormation = 0;
				wingman->rocket.active = 0;
				wingman->bomb.active = 0;
			} else {
				/* CPC's own "or just give full health" fallback for the
				 * (normally unreachable, since trySpawnPowerup() only ever
				 * emits this type while a wingman is actually destroyed)
				 * case of collecting this type with nothing to revive. */
				game->flakDamageCount = 0;
				game->armour = 100;
			}
			break;
		default:
			collected = 0;
			break;
	}
	if (collected)
		playSfxAt(SFX_PICKUP_POWERUP, game->playerX);
	updateHudValues(game);
}

/* CPC checks the cell directly under the powerup before each fall step:
 * sky/cloud/flak = keep falling, player = collect, anything else (terrain,
 * building, ship, own frigate) = destroy. Collection also works the other
 * way - the player's own object-collision pass picks up the powerup when
 * flying into it - which is why updateGameCollisions() also has a powerup
 * branch. */
static UBYTE powerupHitsSolidWorld(const GameState* game, const PowerupState* p) {
	WORD probeX = (WORD)((p->worldX - (LONG)game->scrollX) + (POWERUP_COLLISION_WIDTH / 2));
	WORD probeY = (WORD)(p->y + POWERUP_SPRITE_HEIGHT);
	ObjectCell cell;
	LONG worldColumn;
	WORD tileY;
	if (!objectCellForWorldPoint(game, probeX, probeY, &cell, &worldColumn, &tileY))
		return 0;
	(void)worldColumn;
	(void)tileY;
	if (cell.id == HAR_OBJ_SKY || cell.id == HAR_OBJ_CLOUD)
		return 0;
	if (cell.id == HAR_OBJ_FLAK)
		return 0;
	/* Terrain, town block, ground target, enemy ship, smoke = solid. */
	return 1;
}

static UBYTE powerupHitsPlayer(const GameState* game, const PowerupState* p) {
	WORD screenX = (WORD)(p->worldX - (LONG)game->scrollX);
	return rectsOverlap(screenX, p->y, POWERUP_COLLISION_WIDTH, POWERUP_SPRITE_HEIGHT,
		game->playerX, game->playerY, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT);
}

static UBYTE updatePowerup(GameState* game) {
	PowerupState* p = &game->powerup;
	if (!p->active)
		return 0;

	WORD screenX = (WORD)(p->worldX - (LONG)game->scrollX);

	if (screenX < POWERUP_DESPAWN_LEFT_X) {
		destroyPowerup(game, 0);
		return 1;
	}

	/* Half the observed CPC-style port speed, distributed smoothly:
	 * 2/3 px per frame = 8 px over 12 frames. */
	p->fallCounter = (UBYTE)(p->fallCounter + POWERUP_FALL_PHASE_ADD);
	if (p->fallCounter >= POWERUP_FALL_PHASE_PIXEL) {
		p->fallCounter = (UBYTE)(p->fallCounter - POWERUP_FALL_PHASE_PIXEL);
		p->y++;
	}

	if (powerupHitsPlayer(game, p)) {
		activatePowerup(game, p->type);
		destroyPowerup(game, 0);
		return 1;
	}

	if (powerupHitsSolidWorld(game, p)) {
		destroyPowerup(game, 1);
		return 1;
	}

	return 1;
}

static UBYTE playerObjectMapCollision(const GameState* game, LONG* hitWorldColumn, WORD* hitTileY) {
	*hitWorldColumn = -1;
	*hitTileY = -1;

	if (game->respawnSafeTimer > 0 || game->crashTimer || game->gameOver)
		return PLAYER_OBJECT_COLLISION_SAFE;

	/* The scripted lift starts on the deck and moves only vertically. It is
	 * the one intentional carrier overlap; normal collision rules take over
	 * as soon as TAKEOFF_CLEAR_Y is reached. */
	if (game->takeoffState == TAKEOFF_STATE_LIFTING)
		return PLAYER_OBJECT_COLLISION_SAFE;

	/* Only the mission-end carrier in CPC-style hover mode is a valid
	 * landing/refuel surface.
	 * Re-contact with the start carrier after liftoff is fatal. */
	if ((game->landingState == LANDING_STATE_HOVER || game->missionComplete) &&
		playerOnNativeCarrierDeckPixels(game))
		return PLAYER_OBJECT_COLLISION_SAFE;

	/* Test every object-map cell overlapped by the visible 16x8 aircraft
	 * body (with a small transparent-edge inset). The old three-point probe
	 * sampled only the centre/right side, allowing the left wing and lower
	 * leading edge to pass through hills and ground targets. At most three
	 * columns by two rows are visited, so this remains a tiny fixed cost. */
	LONG leftWorldPixel = (LONG)game->scrollX + game->playerX + 2;
	LONG rightWorldPixel =
		(LONG)game->scrollX + game->playerX + PLAYER_SPRITE_WIDTH - 3;
	WORD topTileY = (WORD)((game->playerY + 1) >> 3);
	WORD bottomTileY =
		(WORD)((game->playerY + PLAYER_SPRITE_HEIGHT - 1) >> 3);
	LONG leftWorldColumn = leftWorldPixel >> 3;
	LONG rightWorldColumn = rightWorldPixel >> 3;
	UBYTE flakContact = 0;

	for (WORD tileY = topTileY; tileY <= bottomTileY; tileY++) {
		for (LONG worldColumn = leftWorldColumn;
			worldColumn <= rightWorldColumn; worldColumn++) {
			ObjectCell cell;
			if (!objectCellForWorldColumnTile(worldColumn, tileY, &cell))
				continue;
			/* CPC clouds use object ID 0 so they can occupy a visual sky
			 * cell, but neither clouds nor empty sky are solid. */
			if (cell.id == HAR_OBJ_SKY || cell.id == HAR_OBJ_CLOUD)
				continue;
			if (cell.id == HAR_OBJ_FLAK) {
				if (!flakContact) {
					*hitWorldColumn = worldColumn;
					*hitTileY = tileY;
					flakContact = 1;
				}
				continue;
			}
			/* Fatal geometry wins over a simultaneous flak overlap. */
			*hitWorldColumn = worldColumn;
			*hitTileY = tileY;
			return PLAYER_OBJECT_COLLISION_FATAL;
		}
	}
	if (flakContact)
		return PLAYER_OBJECT_COLLISION_FLAK;

	return PLAYER_OBJECT_COLLISION_SAFE;
}

static void maybeStartWingmanIntercept(GameState* game); /* Sprint 15.5, defined below */

/* Sprint 15.27: the fixed 5-lane rotation below has no idea where the
 * procedurally generated terrain actually is, so a tall hill/town facade
 * at the exact spawn column could put a lane's tile row underground - the
 * plane would spawn embedded in solid terrain. Reuse the wingman's own
 * sky/cloud/flak passability check (it's generic terrain logic despite the
 * name) across the enemy sprite's full 2-tile width, and fall back through
 * the other lanes in rotation order before giving up and using the
 * originally requested one anyway - same "better an occasional awkward
 * spawn than none at all" philosophy as wingmanSafeTargetRow(). */
static WORD enemyPlaneSafeSpawnY(LONG spawnWorldColumn, UBYTE preferredLane) {
	static const WORD lanes[] = { 40, 56, 72, 88, 104 };
	const UBYTE laneCount = (UBYTE)(sizeof(lanes) / sizeof(lanes[0]));
	for (UBYTE offset = 0; offset < laneCount; offset++) {
		UBYTE lane = (UBYTE)((preferredLane + offset) % laneCount);
		WORD tileRow = (WORD)(lanes[lane] / GAME_TILE_HEIGHT);
		if (wingmanCellIsPassable(spawnWorldColumn, tileRow) &&
			wingmanCellIsPassable(spawnWorldColumn + 1, tileRow))
			return lanes[lane];
	}
	return lanes[preferredLane];
}

static void spawnEnemyPlane(GameState* game) {
	static const WORD lanes[] = { 40, 56, 72, 88, 104 };
	UBYTE lane = game->enemySpawnIndex % (sizeof(lanes) / sizeof(lanes[0]));
	LONG spawnWorldColumn =
		((LONG)game->scrollX + SCREEN_WIDTH - ENEMY_SPRITE_WIDTH) >> 3;

	game->enemySpawnIndex++;
	game->enemyPlane.active = 1;
	game->enemyPlane.timer = 0;
	game->enemyPlane.x = SCREEN_WIDTH - ENEMY_SPRITE_WIDTH - 4;
	game->enemyPlane.y = enemyPlaneSafeSpawnY(spawnWorldColumn, lane);
	game->enemyPlane.dx = -ENEMY_SPEED_PIXELS;
	game->enemyPlane.dy = 0;
	/* CPC chooses once when the plane approaches and keeps that identity
	 * until its missile is gone. Only an airborne Wingman is eligible. */
	game->enemyMissileTarget = ENEMY_TARGET_PLAYER;
	if (game->wingman.active &&
		game->wingman.mode != WINGMAN_LANDING_APPROACH &&
		game->wingman.mode != WINGMAN_LANDING_DECK &&
		(cpcRStateForWorldColumn(spawnWorldColumn) & 1))
		game->enemyMissileTarget = ENEMY_TARGET_WINGMAN;

	maybeStartWingmanIntercept(game);
}

static UBYTE updateEnemyPlane(GameState* game, UBYTE scrollPixels) {
	if (!game->enemyPlane.active) {
		UWORD rightEdgeColumn = (UWORD)((game->scrollX >> 3) + GAME_MAP_WIDTH);
		if (game->enemyTriggerIndex < sizeof(harEnemyPlaneTriggers) / sizeof(harEnemyPlaneTriggers[0]) &&
			rightEdgeColumn >= harEnemyPlaneTriggers[game->enemyTriggerIndex]) {
			spawnEnemyPlane(game);
			game->enemyTriggerIndex++;
			return 1;
		}
		return 0;
	}

	/* Real CPC enemy plane actively converges its altitude toward the target's
	 * every tick (enemyplaneexitscreen, HarrierAttackSourceNew2...asm:6575-6608),
	 * stepping 1 row/frame - it never just flies level. */
	WORD enemyTargetY = (game->enemyMissileTarget == ENEMY_TARGET_WINGMAN &&
		game->wingman.active) ? game->wingman.screenY : game->playerY;
	if (game->enemyPlane.y < enemyTargetY)
		game->enemyPlane.dy = 1;
	else if (game->enemyPlane.y > enemyTargetY)
		game->enemyPlane.dy = -1;
	else
		game->enemyPlane.dy = 0;

	/* Real CPC also respects flak obstructions while steering (same routine,
	 * :6585-6600) - it won't climb/dive into a row a flak hazard occupies.
	 * Check the object-map cell the plane is about to step into and cancel
	 * the move for this frame if it's flak; re-evaluated every frame, so it
	 * naturally resumes once the flak scrolls fully past (flak itself is
	 * player-indestructible - see trySpawnFlak() - so scrolling past is the
	 * only way this ever clears). */
	if (game->enemyPlane.dy != 0) {
		LONG enemyWorldColumn = ((LONG)game->scrollX + game->enemyPlane.x) >> 3;
		WORD candidateTileY = (WORD)((game->enemyPlane.y + game->enemyPlane.dy) >> 3);
		ObjectCell aheadCell;
		if (objectCellForWorldColumnTile(enemyWorldColumn, candidateTileY, &aheadCell) && aheadCell.id == HAR_OBJ_FLAK)
			game->enemyPlane.dy = 0;
	}

	game->enemyPlane.x += (WORD)(game->enemyPlane.dx - scrollPixels);
	game->enemyPlane.y += game->enemyPlane.dy;
	if (game->enemyPlane.y < 0)
		game->enemyPlane.y = 0;
	else if (game->enemyPlane.y > HUD_TOP - ENEMY_SPRITE_HEIGHT)
		game->enemyPlane.y = HUD_TOP - ENEMY_SPRITE_HEIGHT;
	game->enemyPlane.timer++;
	if (game->enemyPlane.x < -ENEMY_SPRITE_WIDTH) {
		game->enemyPlane.active = 0;
	}
	return 1;
}

static void launchEnemyMissile(GameState* game) {
	if (!game->enemyPlane.active || game->enemyMissile.active)
		return;

	game->enemyMissile.active = 1;
	game->enemyMissileFromShip = 0;
	if (game->enemyMissileTarget != ENEMY_TARGET_WINGMAN ||
		!game->wingman.active)
		game->enemyMissileTarget = ENEMY_TARGET_PLAYER;
	game->enemyMissile.timer = 0;
	game->enemyMissile.x = (WORD)(game->enemyPlane.x - 8);
	game->enemyMissile.y = (WORD)(game->enemyPlane.y + 3);
	game->enemyMissile.dx = -ENEMY_MISSILE_SPEED_X_PIXELS;
	game->enemyMissile.dy = 0;
}

static void launchEnemyShipMissile(GameState* game, UWORD triggerColumn) {
	if (game->enemyMissile.active)
		return;

	game->enemyMissile.active = 1;
	game->enemyMissileFromShip = 1;
	game->enemyMissileTarget = ENEMY_TARGET_PLAYER;
	game->enemyMissile.timer = 0;
	/* Exact CPC launch point from marklastenemyshipfiredmissile:
	 * HL=&0d24 means screen row 13, column 36 -> pixel (288,104).
	 * The event occurs immediately after the fourth streamed ship column;
	 * it is deliberately screen-relative, not recomputed from a potentially
	 * overshot fine-scroll position. */
	(void)triggerColumn;
	game->enemyMissile.x = ENEMY_SHIP_MISSILE_START_X;
	game->enemyMissile.y = ENEMY_SHIP_MISSILE_START_Y;
	game->enemyMissile.dx = -ENEMY_MISSILE_SPEED_X_PIXELS;
	game->enemyMissile.dy = -1;
	playSfxAt(SFX_FIRE, game->enemyMissile.x);
}

static UBYTE updateEnemyShipMissileTrigger(GameState* game) {
	UWORD rightEdgeColumn;
	UBYTE changed = 0;
	const UBYTE triggerCount =
		(UBYTE)(sizeof(harEnemyShipMissileTriggers) /
			sizeof(harEnemyShipMissileTriggers[0]));

	rightEdgeColumn = (UWORD)((game->scrollX >> 3) + GAME_MAP_WIDTH);
	/* CPC advances gamelevelprogress from 9 to 10 at this boundary whether
	 * or not enemymissilestatus is already occupied. It returns without
	 * launching in the occupied case; it never queues the ship shot for a
	 * later frame. Consume every crossed one-shot trigger first, then launch
	 * only if the shared missile slot is free at that exact boundary. */
	while (game->enemyShipMissileTriggerIndex < triggerCount &&
		rightEdgeColumn >=
			harEnemyShipMissileTriggers[game->enemyShipMissileTriggerIndex]) {
		UWORD triggerColumn =
			harEnemyShipMissileTriggers[game->enemyShipMissileTriggerIndex];
		game->enemyShipMissileTriggerIndex++;
		if (!game->enemyMissile.active) {
			launchEnemyShipMissile(game, triggerColumn);
			changed = 1;
		}
	}

	return changed;
}

/* Menu review: no sourced CPC formula for enemy-timing difficulty scaling
 * was found (unlike the terrain-height and flak-damage formulas above,
 * which came directly from the review's own disassembly citations) - this
 * pair is this port's own directional approximation: enemies respawn
 * sooner and fire sooner if not yet in range, roughly halving both by
 * skill 5. Flagged as an approximation, not a verified CPC match. */
static UBYTE enemyRespawnFramesForSkill(UBYTE skillLevel) {
	WORD frames = ENEMY_RESPAWN_FRAMES - (WORD)(skillLevel - 1) * 8;
	return frames < 32 ? 32 : (UBYTE)frames;
}

static UBYTE enemyMissileFireFallbackFrameForSkill(UBYTE skillLevel) {
	WORD frame = ENEMY_MISSILE_FIRE_FALLBACK_FRAME - (WORD)(skillLevel - 1) * 6;
	return frame < 12 ? 12 : (UBYTE)frame;
}

/* Sprint 15.5: CPU wingman interception AI (real CPC
 * wingmantrackenemyplanefirstpass/wingmantrackenemyplane2ndwaypoint,
 * asm:2491-2512/2455-2487 - see the WingmanState comment on why the two CPC
 * states are merged into one continuous chase here). Called once per frame
 * for the whole chase; only acts while wingman->mode is actually
 * WINGMAN_INTERCEPT_APPROACH - the mode itself is set by
 * maybeStartWingmanIntercept() (called from spawnEnemyPlane()) and cleared
 * back to WINGMAN_FORMATION either here (enemy despawned, or missile fired)
 * or never entered at all (the 50/50 miss roll). */
/* Sprint 15.5 follow-up: the "flying back to formation" half of
 * updateWingmanIntercept() - see WingmanState.returningToFormation's own
 * comment for why this exists instead of an instant mode switch. Reuses
 * the exact same formation-target math updateWingmanBob()'s FORMATION
 * branch uses, so the moment this hands off to WINGMAN_FORMATION the
 * derived position is guaranteed to already match where this last drew it -
 * no seam. */
static void updateWingmanReturnToFormation(GameState* game) {
	WingmanState* wingman = &game->wingman;
	WORD targetScreenX = (WORD)(game->playerX - WINGMAN_FORMATION_COLUMNS_BEHIND * GAME_TILE_WIDTH);

	/* Clamp to the target instead of always stepping by a fixed amount -
	 * without this, a gap that isn't an exact multiple of
	 * WINGMAN_INTERCEPT_MOVE_PIXELS would overshoot back and forth across
	 * the target forever (X < target steps up past it, then > target steps
	 * back down past it, repeating), so interceptScreenX/targetScreenX
	 * would never become exactly equal and this would never hand off to
	 * WINGMAN_FORMATION - stuck oscillating instead. */
	if (wingman->interceptScreenX < targetScreenX) {
		WORD stepped = (WORD)(wingman->interceptScreenX + WINGMAN_INTERCEPT_MOVE_PIXELS);
		wingman->interceptScreenX = (stepped > targetScreenX) ? targetScreenX : stepped;
	} else if (wingman->interceptScreenX > targetScreenX) {
		WORD stepped = (WORD)(wingman->interceptScreenX - WINGMAN_INTERCEPT_MOVE_PIXELS);
		wingman->interceptScreenX = (stepped < targetScreenX) ? targetScreenX : stepped;
	}

	LONG worldColumnLeft = ((LONG)game->scrollX + wingman->interceptScreenX) >> 3;
	WORD targetRow = wingmanSafeTargetRow(worldColumnLeft,
		updateWingmanFormationTargetRow(game, worldColumnLeft));

	wingman->moveTimer++;
	if (wingman->moveTimer >= WINGMAN_MOVE_FRAME_INTERVAL) {
		wingman->moveTimer = 0;
		if (wingman->row != targetRow) {
			WORD candidate = (WORD)(wingman->row + ((wingman->row < targetRow) ? 1 : -1));
			wingman->row = wingmanFormationRowIsSafe(worldColumnLeft, candidate) ?
				candidate : wingmanSafeTargetRow(worldColumnLeft, candidate);
		}
	}

	if (wingman->interceptScreenX == targetScreenX && wingman->row == targetRow) {
		wingman->mode = WINGMAN_FORMATION;
		wingman->returningToFormation = 0;
	}
}

static void updateWingmanIntercept(GameState* game) {
	WingmanState* wingman = &game->wingman;
	if (wingman->mode != WINGMAN_INTERCEPT_APPROACH)
		return;

	if (wingman->returningToFormation) {
		updateWingmanReturnToFormation(game);
		return;
	}

	if (!game->enemyPlane.active) {
		wingman->returningToFormation = 1;
		wingman->moveTimer = 0;
		return;
	}

	WORD leadX = (WORD)(game->enemyPlane.x - WINGMAN_INTERCEPT_LEAD_PIXELS);

	/* Closes in every frame, not throttled like formation-keeping - a real
	 * intercept is urgent, and the first version of this (throttled to
	 * WINGMAN_MOVE_FRAME_INTERVAL like formation movement, ~1px/frame
	 * average) was measured via a temporary per-frame log to close barely
	 * half the needed gap before the enemy plane exited the screen, so the
	 * wingman peeled off but essentially never actually got a shot. Real
	 * CPC's findwaypoint() moves toward its target every single tick it's
	 * in this state (no throttle) - this restores that urgency, tuned so
	 * the combined closing speed reliably beats the enemy's own approach
	 * speed (up to ENEMY_SPEED_PIXELS + max scroll) before it despawns. */
	if (wingman->interceptScreenX < leadX) {
		WORD stepped = (WORD)(wingman->interceptScreenX + WINGMAN_INTERCEPT_MOVE_PIXELS);
		wingman->interceptScreenX = (stepped > leadX) ? leadX : stepped;
	} else if (wingman->interceptScreenX > leadX) {
		WORD stepped = (WORD)(wingman->interceptScreenX - WINGMAN_INTERCEPT_MOVE_PIXELS);
		wingman->interceptScreenX = (stepped < leadX) ? leadX : stepped;
	}

	wingman->moveTimer++;
	if (wingman->moveTimer >= WINGMAN_MOVE_FRAME_INTERVAL) {
		wingman->moveTimer = 0;
		WORD targetRow = (WORD)(game->enemyPlane.y / GAME_TILE_HEIGHT);
		if (wingman->row != targetRow) {
			LONG worldColumnLeft = ((LONG)game->scrollX + wingman->interceptScreenX) >> 3;
			WORD candidate = (WORD)(wingman->row + ((wingman->row < targetRow) ? 1 : -1));
			if (wingmanFormationRowIsSafe(worldColumnLeft, candidate))
				wingman->row = candidate;
		}
	}

	/* Fire condition matches asm:2467-2474 (same altitude, closing gap under
	 * 10px) - see WINGMAN_INTERCEPT_ROW_TOLERANCE's own comment for why an
	 * exact height match doesn't translate directly to this port. */
	/* Sprint 15.17.1: after adding pixel-smooth visual interpolation, row
	 * can be one logical tile ahead of the hardware sprite while climbing.
	 * Fire from the position the player actually sees, not from that future
	 * tile row, otherwise the missile can be released before Wingman has
	 * physically reached the enemy's altitude. */
	WORD altitudeGap = (WORD)(wingman->screenY - game->enemyPlane.y);
	if (altitudeGap < 0)
		altitudeGap = (WORD)-altitudeGap;
	WORD closingGap = (WORD)(game->enemyPlane.x - wingman->interceptScreenX);
	if (closingGap < 0)
		closingGap = (WORD)-closingGap;
	if (!wingman->rocket.active && altitudeGap <= WINGMAN_INTERCEPT_ROW_TOLERANCE &&
		closingGap <= WINGMAN_INTERCEPT_FIRE_RANGE_PIXELS) {
		memset(&wingman->rocket, 0, sizeof(wingman->rocket));
		wingman->rocket.active = 1;
		wingman->rocket.x = wingman->interceptScreenX;
		wingman->rocket.y = wingman->screenY;
		wingman->rocket.dx = ROCKET_SPEED_PIXELS;
		wingman->returningToFormation = 1;
		wingman->moveTimer = 0;
	}
}

/* Sprint 15.5: moves the wingman's own missile and checks it against the
 * enemy plane, mirroring updateGameCollisions()'s existing rocketShot-vs-
 * enemyPlane check (asm's firewingmanmissile just reuses the shared
 * missile-collision code via the IY block-swap trick; this port gives the
 * wingman its own WeaponState instead - see the Sprint 15 roadmap on why -
 * so the hit-test is duplicated here rather than shared). */
static void updateWingmanRocket(GameState* game, UBYTE* hudDirty, UBYTE* enemySpriteDirty) {
	WeaponState* rocket = &game->wingman.rocket;
	if (!rocket->active)
		return;

	rocket->x = (WORD)(rocket->x + rocket->dx);
	if (rocket->x > SCREEN_WIDTH) {
		rocket->active = 0;
		return;
	}

	if (game->enemyPlane.active &&
		rectsOverlap(rocket->x, rocket->y, 16, WEAPON_SPRITE_HEIGHT,
			game->enemyPlane.x, game->enemyPlane.y, ENEMY_SPRITE_WIDTH, ENEMY_SPRITE_HEIGHT)) {
		rocket->active = 0;
		game->enemyPlane.active = 0;
		game->enemyRespawnTimer = enemyRespawnFramesForSkill(game->skillLevel);
		game->bonusScore += ENEMY_SCORE_VALUE;
		game->hitsCount++;
		updateHudValues(game);
		startImpact(game, game->enemyPlane.x, game->enemyPlane.y);
		*hudDirty = 1;
		*enemySpriteDirty = 1;
	}
}

/* Real CPC rolls this when a plane spawns, but forces interception when that
 * plane selected the Wingman as its missile target. */
static void maybeStartWingmanIntercept(GameState* game) {
	WingmanState* wingman = &game->wingman;
	if (!wingman->active || wingman->mode != WINGMAN_FORMATION)
		return;

	LONG rollWorldColumn = ((LONG)game->scrollX + game->playerX) >> 3;
	if (game->enemyMissileTarget != ENEMY_TARGET_WINGMAN &&
		!(cpcRStateForWorldColumn(rollWorldColumn) &
			WINGMAN_INTERCEPT_CHANCE_MASK))
		return;

	wingman->mode = WINGMAN_INTERCEPT_APPROACH;
	wingman->interceptScreenX = (WORD)(game->playerX - WINGMAN_FORMATION_COLUMNS_BEHIND * GAME_TILE_WIDTH);
	wingman->moveTimer = 0;
}

/* CPC checkwingmandobombingrun consumes each shared ground lock once,
 * selects roughly one in four with R&3, then flies above/ahead of the
 * scrolling target before releasing its infinite-supply bomb. */
static void maybeStartWingmanBombingRun(GameState* game) {
	WingmanState* wingman = &game->wingman;
	if (!wingman->active || wingman->mode != WINGMAN_FORMATION ||
		game->wingmanControl != WINGMAN_CONTROL_CPU ||
		!game->targetLock.active)
		return;

	LONG targetColumn = game->targetLock.worldX >> 3;
	if (targetColumn == wingman->lastBombTargetColumn)
		return;
	wingman->lastBombTargetColumn = targetColumn;
	if (cpcRStateForWorldColumn(targetColumn) & WINGMAN_BOMB_CHANCE_MASK)
		return;

	wingman->bombTargetWorldX =
		game->targetLock.worldX + GAME_TILE_WIDTH / 2;
	wingman->bombTargetY =
		(WORD)(game->targetLock.y + GAME_TILE_HEIGHT / 2);
	wingman->interceptScreenX = (WORD)(game->playerX -
		WINGMAN_FORMATION_COLUMNS_BEHIND * GAME_TILE_WIDTH);
	wingman->mode = WINGMAN_BOMB_APPROACH;
	wingman->moveTimer = 0;
}

static void updateWingmanBombingRun(GameState* game, UBYTE** worldBuffers,
	UBYTE* hudDirty) {
	WingmanState* wingman = &game->wingman;
	if (wingman->mode == WINGMAN_BOMB_APPROACH) {
		WORD targetX = (WORD)(wingman->bombTargetWorldX - game->scrollX - 4);
		WORD targetRow = (WORD)((wingman->bombTargetY >> 3) -
			WINGMAN_BOMB_HEIGHT_TILES);
		if (targetRow < 1)
			targetRow = 1;

		WORD proposedX = wingman->interceptScreenX;
		if (wingman->interceptScreenX < targetX) {
			WORD stepped = (WORD)(wingman->interceptScreenX +
				WINGMAN_INTERCEPT_MOVE_PIXELS);
			proposedX = stepped > targetX ? targetX : stepped;
		} else if (wingman->interceptScreenX > targetX) {
			WORD stepped = (WORD)(wingman->interceptScreenX -
				WINGMAN_INTERCEPT_MOVE_PIXELS);
			proposedX = stepped < targetX ? targetX : stepped;
		}
		LONG proposedWorldColumn = ((LONG)game->scrollX + proposedX) >> 3;
		targetRow = wingmanSafeTargetRow(proposedWorldColumn, targetRow);

		/* CPC checkwingmanradar validates the requested direction before
		 * moving. If the next horizontal step intersects terrain/building,
		 * hold X and climb toward the nearest safe row first. */
		if (wingmanFormationRowIsSafe(proposedWorldColumn, wingman->row))
			wingman->interceptScreenX = proposedX;

		wingman->moveTimer++;
		if (wingman->moveTimer >= WINGMAN_MOVE_FRAME_INTERVAL) {
			wingman->moveTimer = 0;
			if (wingman->row != targetRow)
				wingman->row += wingman->row < targetRow ? 1 : -1;
		}

		if (wingman->interceptScreenX == targetX &&
			wingman->row == targetRow && !wingman->bomb.active) {
			memset(&wingman->bomb, 0, sizeof(wingman->bomb));
			wingman->bomb.active = 1;
			wingman->bomb.x = (WORD)(wingman->interceptScreenX + 6);
			wingman->bomb.y = (WORD)(wingman->screenY +
				PLAYER_SPRITE_HEIGHT - 1);
			wingman->bomb.worldX =
				(LONG)game->scrollX + wingman->bomb.x;
			wingman->bomb.worldAnchored = 1;
			wingman->bomb.dy = BOMB_SPEED_Y_PIXELS;
			wingman->bomb.timer = BOMB_FORWARD_MOMENTUM_FRAMES;
			wingman->mode = WINGMAN_INTERCEPT_APPROACH;
			wingman->returningToFormation = 1;
			wingman->moveTimer = 0;
			playSfxAt(SFX_BOMB, wingmanScreenX(game));
		}
	}

	if (!wingman->bomb.active)
		return;
	wingman->bomb.y += wingman->bomb.dy;
	if ((wingman->bomb.timer % BOMB_EXTRA_FALL_INTERVAL) ==
		(BOMB_EXTRA_FALL_INTERVAL - 1))
		wingman->bomb.y += wingman->bomb.dy;
	wingman->bomb.x = (WORD)(wingman->bomb.worldX - game->scrollX);
	if (wingman->bomb.timer < 255)
		wingman->bomb.timer++;

	if (wingman->bomb.y + BOMB_SHOT_PIXEL_BOB_HEIGHT >=
		wingman->bombTargetY) {
		LONG targetColumn = wingman->bombTargetWorldX >> 3;
		WORD targetRow = wingman->bombTargetY >> 3;
		markTargetDestroyedAtColumn(targetColumn);
		addCpcHitSmokeAtColumnRow(targetColumn, targetRow);
		dirtyRedrawWorldColumn(worldBuffers, targetColumn);
		game->targetLock.active = 0;
		game->bonusScore += GROUND_TARGET_SCORE_VALUE;
		game->hitsCount++;
		updateHudValues(game);
		*hudDirty = 1;
		startGroundTargetHitImpact(game, wingman->bomb.x,
			wingman->bomb.y, targetColumn, targetRow);
		wingman->bomb.active = 0;
	} else if (wingman->bomb.x < -BOMB_SHOT_PIXEL_BOB_WIDTH ||
		wingman->bomb.y >= HUD_TOP) {
		wingman->bomb.active = 0;
	}
}

static WORD wingmanLandingDeckLeftX(const GameState* game) {
	if (!harLevelObjectIndexReady)
		buildHarLevelObjectIndex();
	for (UBYTE wideIndex = 0; wideIndex < harWideObjectCount; wideIndex++) {
		const LevelObjectDef* object =
			&harLevelObjects[harWideObjectIndex[wideIndex]];
		if (object->id == HAR_OBJ_OWN_FRIGATE &&
			(object->flags & HAR_OBJECT_FLAG_NATIVE_CARRIER)) {
			LONG screenX = (LONG)object->column * GAME_TILE_WIDTH -
				(LONG)game->scrollX;
			if (screenX > -CARRIER_DECK_PIXEL_WIDTH &&
				screenX < SCREEN_WIDTH)
				return (WORD)screenX;
		}
	}
	return (WORD)(SCREEN_WIDTH / 2 - CARRIER_DECK_PIXEL_WIDTH / 2);
}

/* Sprint 15.18: CPU equivalent of CPC's two landing waypoints. First move
 * above an unused part of the carrier deck, then descend vertically. The
 * hardware sprite remains pixel precise; no tile-row movement is involved
 * during landing. */
static void updateWingmanLanding(GameState* game) {
	WingmanState* wingman = &game->wingman;
	if (!wingman->active ||
		(wingman->mode != WINGMAN_LANDING_APPROACH &&
		 wingman->mode != WINGMAN_LANDING_DECK))
		return;

	if (!wingman->landingTargetSet) {
		WORD deckLeft = wingmanLandingDeckLeftX(game);
		WORD leftSlot = (WORD)(deckLeft + 8);
		WORD rightSlot = (WORD)(deckLeft +
			CARRIER_DECK_PIXEL_WIDTH - PLAYER_SPRITE_WIDTH - 8);
		WORD leftGap = (WORD)(game->playerX - leftSlot);
		WORD rightGap = (WORD)(game->playerX - rightSlot);
		if (leftGap < 0)
			leftGap = (WORD)-leftGap;
		if (rightGap < 0)
			rightGap = (WORD)-rightGap;
		wingman->landingTargetX = leftGap >= rightGap ? leftSlot : rightSlot;
		wingman->landingTargetSet = 1;
	}

	if (wingman->mode == WINGMAN_LANDING_APPROACH) {
		if (wingman->interceptScreenX < wingman->landingTargetX) {
			WORD next = (WORD)(wingman->interceptScreenX +
				WINGMAN_LANDING_MOVE_PIXELS);
			wingman->interceptScreenX =
				next > wingman->landingTargetX ? wingman->landingTargetX : next;
		} else if (wingman->interceptScreenX > wingman->landingTargetX) {
			WORD next = (WORD)(wingman->interceptScreenX -
				WINGMAN_LANDING_MOVE_PIXELS);
			wingman->interceptScreenX =
				next < wingman->landingTargetX ? wingman->landingTargetX : next;
		}
		if (wingman->screenY < WINGMAN_LANDING_WAYPOINT_Y) {
			WORD next = (WORD)(wingman->screenY + WINGMAN_LANDING_MOVE_PIXELS);
			wingman->screenY =
				next > WINGMAN_LANDING_WAYPOINT_Y ? WINGMAN_LANDING_WAYPOINT_Y : next;
		} else if (wingman->screenY > WINGMAN_LANDING_WAYPOINT_Y) {
			WORD next = (WORD)(wingman->screenY - WINGMAN_LANDING_MOVE_PIXELS);
			wingman->screenY =
				next < WINGMAN_LANDING_WAYPOINT_Y ? WINGMAN_LANDING_WAYPOINT_Y : next;
		}
		if (wingman->interceptScreenX == wingman->landingTargetX &&
			wingman->screenY == WINGMAN_LANDING_WAYPOINT_Y)
			wingman->mode = WINGMAN_LANDING_DECK;
		return;
	}

	{
		WORD deckY = (WORD)(CARRIER_DECK_PIXEL_Y - PLAYER_SPRITE_HEIGHT);
		if (wingman->screenY < deckY) {
			WORD next = (WORD)(wingman->screenY +
				WINGMAN_LANDING_DESCEND_PIXELS);
			wingman->screenY = next > deckY ? deckY : next;
		}
	}
}

static UBYTE updateEnemyMissile(GameState* game, UBYTE scrollPixels) {
	UBYTE changed = 0;

	if (updateEnemyShipMissileTrigger(game))
		changed = 1;

	/* Real CPC fires based on horizontal proximity to the target
	 * (|enemy.x - target.x| < 10, :6556-6561), not a screen-position/timer
	 * threshold. The fallback frame count stays as a safety net so a plane
	 * that never gets in range still fires before leaving the screen. */
	if (game->enemyPlane.active && !game->enemyMissile.active) {
		WORD targetX = game->playerX;
		if (game->enemyMissileTarget == ENEMY_TARGET_WINGMAN &&
			game->wingman.active)
			targetX = wingmanScreenX(game);
		WORD planeToTargetX = (WORD)(game->enemyPlane.x - targetX);
		if (planeToTargetX < 0)
			planeToTargetX = (WORD)-planeToTargetX;
		if (planeToTargetX < ENEMY_MISSILE_FIRE_RANGE_PIXELS ||
				game->enemyPlane.timer >= enemyMissileFireFallbackFrameForSkill(game->skillLevel)) {
			launchEnemyMissile(game);
			changed = 1;
		}
	}

	if (game->enemyMissile.active) {
		/* Real CPC missiles are continuously heat-seeking for their whole
		 * flight (heatseekposition, :6124-6165) - re-reading the target's
		 * height every frame, not just the ship-fired missile within a
		 * distance cutoff. Apply the same tracking to both sources. */
		WORD targetY = (WORD)(game->playerY + 2);
		if (game->enemyMissileTarget == ENEMY_TARGET_WINGMAN &&
			game->wingman.active)
			targetY = (WORD)(game->wingman.screenY + 2);
		else
			game->enemyMissileTarget = ENEMY_TARGET_PLAYER;
		if (game->enemyMissile.y < targetY)
			game->enemyMissile.dy = 1;
		else if (game->enemyMissile.y > targetY)
			game->enemyMissile.dy = -1;
		else
			game->enemyMissile.dy = 0;
		game->enemyMissile.x += (WORD)(game->enemyMissile.dx - scrollPixels);
		game->enemyMissile.y += game->enemyMissile.dy;
		game->enemyMissile.timer++;
		if (game->enemyMissile.x < -ENEMY_MISSILE_SPRITE_WIDTH ||
			game->enemyMissile.y < 0 ||
			game->enemyMissile.y > HUD_TOP - ENEMY_MISSILE_SPRITE_HEIGHT) {
			game->enemyMissile.active = 0;
			game->enemyMissileFromShip = 0;
			game->enemyMissileTarget = ENEMY_TARGET_NONE;
		}
		/* Sprint 14.96: enemy heatseeker can also destroy a powerup (CPC's
		 * heatseekposition treats the powerup as a valid target). */
		if (game->enemyMissile.active && game->powerup.active) {
			WORD powerupScreenX = (WORD)(game->powerup.worldX - (LONG)game->scrollX);
			if (rectsOverlap(game->enemyMissile.x, game->enemyMissile.y, ENEMY_MISSILE_SPRITE_WIDTH, ENEMY_MISSILE_SPRITE_HEIGHT,
					powerupScreenX, game->powerup.y, POWERUP_COLLISION_WIDTH, POWERUP_SPRITE_HEIGHT)) {
				game->enemyMissile.active = 0;
				game->enemyMissileFromShip = 0;
				game->enemyMissileTarget = ENEMY_TARGET_NONE;
				destroyPowerup(game, 1);
			}
		}
		changed = 1;
	}

	return changed;
}

static void triggerGameOver(GameState* game) {
	if (game->gameOver)
		return;

	game->lives = 0;
	game->gameOver = 1;
	game->crashTimer = 0;
	game->rocketShot.active = 0;
	game->bombShot.active = 0;
	game->enemyPlane.active = 0;
	game->enemyMissile.active = 0;
	game->enemyMissileFromShip = 0;
	game->enemyRespawnTimer = 0;
	game->powerup.active = 0;
	stopAllSfx();
	startGameOverMusic();
}

static void losePlayerLife(GameState* game);

static void respawnPlayer(GameState* game) {
	game->playerX = PLAYER_START_X;
	game->playerY = PLAYER_START_Y;
	game->armour = 100;
	game->flakDamageCount = 0;
	game->playerFrigateStatus = PLAYER_FRIGATE_STATUS_CLEAR;
	game->takeoffState = TAKEOFF_STATE_AIRBORNE;
	game->respawnSafeTimer = PLAYER_RESPAWN_SAFE_FRAMES;
	game->crashTimer = 0;
	game->ejectState = 0;
	game->ejectTimer = 0;
	memset(game->crashPart, 0, sizeof(game->crashPart));

	game->rocketShot.active = 0;
	game->bombShot.active = 0;
	game->enemyPlane.active = 0;
	game->enemyMissile.active = 0;
	game->enemyMissileFromShip = 0;
	game->enemyRespawnTimer = 0;
	game->powerup.active = 0;
}

static void startPlayerEject(GameState* game) {
	if (game->gameOver || game->crashTimer || game->ejectState ||
		game->respawnSafeTimer > 0 ||
		game->takeoffState != TAKEOFF_STATE_AIRBORNE)
		return;

	game->ejectState = 1;
	game->ejectTimer = 0;
	game->ejectX = game->playerX;
	game->ejectY = (WORD)(game->playerY - 4);
	game->rocketShot.active = 0;
	game->bombShot.active = 0;
	game->enemyMissile.active = 0;
	stopSfxChannel(ENGINE_CHANNEL);
	engineActive = 0;
	playSfxAt(SFX_EJECT, game->playerX);
}

static UBYTE updatePlayerEject(GameState* game) {
	if (!game->ejectState)
		return 0;

	game->ejectTimer++;
	if (game->ejectState == 1) {
		game->ejectY--;
		if (game->ejectTimer >= 12) {
			game->ejectState = 2;
			game->ejectTimer = 0;
		}
		return 1;
	}

	/* CPC descends one character step per update. The Amiga port keeps the
	 * same state transition but moves one pixel every other frame. */
	if ((game->ejectTimer & 1) == 0)
		game->ejectY++;

	LONG worldColumn = ((LONG)game->scrollX + game->ejectX) /
		GAME_TILE_WIDTH;
	WORD surfaceY = landSurfaceYForWorldColumn(worldColumn);
	if (game->ejectY + HAR_CPC_PARACHUTE_HEIGHT >= surfaceY ||
		game->ejectY >= HUD_TOP - HAR_CPC_PARACHUTE_HEIGHT) {
		game->ejectState = 0;
		losePlayerLife(game);
	}
	return 1;
}

static void losePlayerLife(GameState* game) {
	if (game->gameOver)
		return;

	if (debugInfiniteLives) {
		respawnPlayer(game);
		return;
	}

	if (game->lives > 1) {
		game->lives--;
		respawnPlayer(game);
	} else {
		game->armour = 0;
		triggerGameOver(game);
	}
}

/* Menu review: Skill level previously only affected terrain height
 * (cpcLandMinimumRow()). Real CPC's totalflakdamagecount is computed as
 * ~25 - 2*difficulty (Amiga-Improvement-Plan-23.04.2026.md Part 2) - skill 1
 * tolerates ~23 flak hits, skill 5 only ~15, before flak alone is fatal.
 * Previously a fixed 100-hit budget regardless of skill (a placeholder
 * noted in Sprint 14.95 as deferred until difficulty scaling existed). */
static UBYTE flakDamageThresholdForSkill(UBYTE skillLevel) {
	WORD threshold = 25 - 2 * (WORD)skillLevel;
	return threshold < 5 ? 5 : (UBYTE)threshold;
}

/* CPC's numberofbombs/numberofrockets are decrement counters for each of
 * the 15 inventory/gauge units, not literal shot counts. Skill 1 therefore
 * starts with 4*15 bombs and 2*15 rockets, both with a full gauge. */
static void ammoForSkill(UBYTE skillLevel, UBYTE* bombs, UBYTE* rockets) {
	UBYTE bombsPerGaugeUnit = (UBYTE)(skillLevel + 3);
	UBYTE rocketsPerGaugeUnit = (UBYTE)(bombsPerGaugeUnit / 2);
	*bombs = (UBYTE)(bombsPerGaugeUnit * 15);
	*rockets = (UBYTE)(rocketsPerGaugeUnit * 15);
}

static void applyPlayerFlakDamage(GameState* game) {
	if (game->gameOver || game->respawnSafeTimer > 0 || game->crashTimer)
		return;

	UBYTE threshold = flakDamageThresholdForSkill(game->skillLevel);
	if (game->flakDamageCount < threshold)
		game->flakDamageCount++;
	game->armour = game->flakDamageCount < threshold ? (UWORD)(100 - (100 * (ULONG)game->flakDamageCount / threshold)) : 0;
	playSfxAt(SFX_HIT, game->playerX);
	if (game->armour == 0)
		startPlayerCrash(game, game->playerX, game->playerY);
}

static void startPlayerCrash(GameState* game, WORD x, WORD y) {
	if (game->gameOver || game->crashTimer || game->respawnSafeTimer > 0)
		return;

	game->armour = 0;
	game->crashTimer = PLAYER_CRASH_FRAMES;
	game->rocketShot.active = 0;
	game->bombShot.active = 0;
	game->impact.active = 0;
	game->enemyPlane.active = 0;
	game->enemyMissile.active = 0;
	game->enemyMissileFromShip = 0;
	game->powerup.active = 0;

	/* CPC planebrokepart{1,2,3}data stores (dy,dx) for three altitude
	 * bands. Every fragment keeps moving forward; none is thrown backwards
	 * in a radial explosion. Translate those tile-step directions to smooth
	 * pixel steps while retaining the same high/mid/low variants. */
	static const BYTE highDx[PLAYER_CRASH_PART_COUNT] = { 2, 2, 1 };
	static const BYTE highDy[PLAYER_CRASH_PART_COUNT] = { 0, 1, 2 };
	static const BYTE midDx[PLAYER_CRASH_PART_COUNT] = { 2, 1, 1 };
	static const BYTE midDy[PLAYER_CRASH_PART_COUNT] = { 1, 0, -1 };
	static const BYTE lowDx[PLAYER_CRASH_PART_COUNT] = { 1, 2, 2 };
	static const BYTE lowDy[PLAYER_CRASH_PART_COUNT] = { -1, 0, -1 };
	const BYTE* crashDx = y < 5 * GAME_TILE_HEIGHT ? highDx :
		(y < 10 * GAME_TILE_HEIGHT ? midDx : lowDx);
	const BYTE* crashDy = y < 5 * GAME_TILE_HEIGHT ? highDy :
		(y < 10 * GAME_TILE_HEIGHT ? midDy : lowDy);

	for (UBYTE part = 0; part < PLAYER_CRASH_PART_COUNT; part++) {
		game->crashPart[part].active = 1;
		game->crashPart[part].x = x;
		game->crashPart[part].y = y;
		game->crashPart[part].dx = crashDx[part];
		game->crashPart[part].dy = crashDy[part];
		game->crashPart[part].timer = 0;
	}

	stopSfxChannel(ENGINE_CHANNEL);
	engineActive = 0;
	playSfxAt(SFX_HIT, game->playerX);
}

static UBYTE updatePlayerCrash(GameState* game) {
	UBYTE changed = 0;

	if (!game->crashTimer)
		return 0;

	for (UBYTE part = 0; part < PLAYER_CRASH_PART_COUNT; part++) {
		WeaponState* crashPart = &game->crashPart[part];
		if (!crashPart->active)
			continue;
		crashPart->x += crashPart->dx;
		crashPart->y += crashPart->dy;
		crashPart->timer++;
		if (crashPart->x < -16 || crashPart->x > SCREEN_WIDTH || crashPart->y < 0 || crashPart->y > HUD_TOP - 8)
			crashPart->active = 0;
		changed = 1;
	}

	game->crashTimer--;
	if (!game->crashTimer) {
		if (debugInfiniteLives) {
			respawnPlayer(game);
		} else if (game->lives > 1) {
			game->lives--;
			respawnPlayer(game);
		} else {
			triggerGameOver(game);
		}
		changed = 1;
	}

	return changed;
}

static UBYTE updateGameCollisions(GameState* game, UBYTE** worldBuffers,
	UBYTE* hudChanged, UBYTE* weaponChanged, UBYTE* enemyMissileChanged,
	UBYTE* wingmanChanged) {
	UBYTE enemyChanged = 0;
	LONG collisionWorldColumn;
	WORD collisionTileY;
	UBYTE objectCollision = playerObjectMapCollision(game, &collisionWorldColumn, &collisionTileY);

	if (replenishPlayerFromFrigate(game))
		*hudChanged = 1;
	if (game->missionComplete && playerOnNativeCarrierDeckPixels(game) && game->speedLevel <= 3) {
		WORD deckPlayerY = (WORD)(CARRIER_DECK_PIXEL_Y - PLAYER_SPRITE_HEIGHT);
		if (game->playerY != deckPlayerY) {
			game->playerY = deckPlayerY;
			*weaponChanged = 1;
		}
	}

	if (objectCollision == PLAYER_OBJECT_COLLISION_FATAL) {
		telemetryLogRenderEvent(6, objectCollision, (UWORD)((game->scrollX + game->playerX) >> 3), game->scrollX, (UWORD)game->playerY);
		startPlayerCrash(game, game->playerX, game->playerY);
		*hudChanged = 1;
		*weaponChanged = 1;
		return enemyChanged;
	}
	if (objectCollision == PLAYER_OBJECT_COLLISION_FLAK) {
		telemetryLogRenderEvent(7, objectCollision, (UWORD)((game->scrollX + game->playerX) >> 3), game->scrollX, (UWORD)game->playerY);
		applyPlayerFlakDamage(game);
		/* CPC's non-fatal collision path draws the player into the flak's
		 * object-map cell, then clears that old player cell back to sky on
		 * the next move. Hardware sprites do not mutate Amiga world data,
		 * so consume the runtime flak explicitly and reconstruct whatever
		 * belongs underneath it (plain sky or a cloud tile). */
		if (removeRuntimeFlakAt(collisionWorldColumn, collisionTileY))
			dirtyRedrawWorldColumn(worldBuffers, collisionWorldColumn);
		*hudChanged = 1;
		if (game->crashTimer) {
			*weaponChanged = 1;
			return enemyChanged;
		}
	}

	/* Real CPC: shooting down an enemy missile both destroys it and awards
	 * score (playermissilehitenemymissile, :8235-8244 - clears
	 * enemymissilestatus and calls explosionnoise with A=1, i.e. 10 points).
	 * Previously only the player's own rocket/bomb was consumed here - the
	 * enemy missile kept flying and no score was awarded. */
	if (game->enemyMissile.active && game->rocketShot.active &&
		rectsOverlap(game->rocketShot.x, game->rocketShot.y, 16, WEAPON_SPRITE_HEIGHT,
			game->enemyMissile.x, game->enemyMissile.y, ENEMY_MISSILE_SPRITE_WIDTH, ENEMY_MISSILE_SPRITE_HEIGHT)) {
		game->rocketShot.active = 0;
		game->enemyMissile.active = 0;
		game->enemyMissileFromShip = 0;
		game->bonusScore += ENEMY_MISSILE_SCORE_VALUE;
		game->hitsCount++;
		updateHudValues(game);
		startImpact(game, game->enemyMissile.x, game->enemyMissile.y);
		*hudChanged = 1;
		*weaponChanged = 1;
		*enemyMissileChanged = 1;
	}

	if (game->enemyMissile.active && game->bombShot.active &&
		rectsOverlap(game->bombShot.x, game->bombShot.y, 16, WEAPON_SPRITE_HEIGHT,
			game->enemyMissile.x, game->enemyMissile.y, ENEMY_MISSILE_SPRITE_WIDTH, ENEMY_MISSILE_SPRITE_HEIGHT)) {
		game->bombShot.active = 0;
		game->enemyMissile.active = 0;
		game->enemyMissileFromShip = 0;
		game->bonusScore += ENEMY_MISSILE_SCORE_VALUE;
		game->hitsCount++;
		updateHudValues(game);
		startImpact(game, game->enemyMissile.x, game->enemyMissile.y);
		*hudChanged = 1;
		*weaponChanged = 1;
		*enemyMissileChanged = 1;
	}

	if (game->enemyPlane.active && game->rocketShot.active &&
		rectsOverlap(game->rocketShot.x, game->rocketShot.y, 16, WEAPON_SPRITE_HEIGHT,
			game->enemyPlane.x, game->enemyPlane.y, ENEMY_SPRITE_WIDTH, ENEMY_SPRITE_HEIGHT)) {
		game->rocketShot.active = 0;
		game->enemyPlane.active = 0;
		game->enemyRespawnTimer = enemyRespawnFramesForSkill(game->skillLevel);
		game->bonusScore += ENEMY_SCORE_VALUE;
		game->hitsCount++;
		updateHudValues(game);
		startImpact(game, game->enemyPlane.x, game->enemyPlane.y);
		*hudChanged = 1;
		*weaponChanged = 1;
		enemyChanged = 1;
	}

	if (game->enemyPlane.active && game->bombShot.active &&
		rectsOverlap(game->bombShot.x, game->bombShot.y, 16, WEAPON_SPRITE_HEIGHT,
			game->enemyPlane.x, game->enemyPlane.y, ENEMY_SPRITE_WIDTH, ENEMY_SPRITE_HEIGHT)) {
		game->bombShot.active = 0;
		game->enemyPlane.active = 0;
		game->enemyRespawnTimer = enemyRespawnFramesForSkill(game->skillLevel);
		game->bonusScore += ENEMY_SCORE_VALUE;
		game->hitsCount++;
		updateHudValues(game);
		startImpact(game, game->enemyPlane.x, game->enemyPlane.y);
		*hudChanged = 1;
		*weaponChanged = 1;
		enemyChanged = 1;
	}

	if (game->wingman.active) {
		WORD wingmanX = wingmanScreenX(game);
		WORD wingmanY = game->wingman.screenY;

		/* CPC object id 20: either friendly weapon destroys the Wingman. */
		if (game->rocketShot.active &&
			rectsOverlap(game->rocketShot.x, game->rocketShot.y,
				16, WEAPON_SPRITE_HEIGHT, wingmanX, wingmanY,
				PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT)) {
			game->rocketShot.active = 0;
			destroyWingman(game);
			*weaponChanged = 1;
			*wingmanChanged = 1;
		} else if (game->bombShot.active &&
			rectsOverlap(game->bombShot.x, game->bombShot.y,
				16, WEAPON_SPRITE_HEIGHT, wingmanX, wingmanY,
				PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT)) {
			game->bombShot.active = 0;
			destroyWingman(game);
			*weaponChanged = 1;
			*wingmanChanged = 1;
		}

		/* CPC enemy-plane contact destroys both aircraft. */
		if (game->wingman.active && game->enemyPlane.active &&
			rectsOverlap(wingmanX, wingmanY, PLAYER_SPRITE_WIDTH,
				PLAYER_SPRITE_HEIGHT, game->enemyPlane.x,
				game->enemyPlane.y, ENEMY_SPRITE_WIDTH,
				ENEMY_SPRITE_HEIGHT)) {
			game->enemyPlane.active = 0;
			game->enemyRespawnTimer =
				enemyRespawnFramesForSkill(game->skillLevel);
			destroyWingman(game);
			enemyChanged = 1;
			*wingmanChanged = 1;
		}

		/* The heat-seeker also kills the Wingman on physical contact,
		 * whether or not he was its originally selected target. */
		if (game->wingman.active && game->enemyMissile.active &&
			rectsOverlap(wingmanX, wingmanY, PLAYER_SPRITE_WIDTH,
				PLAYER_SPRITE_HEIGHT, game->enemyMissile.x,
				game->enemyMissile.y, ENEMY_MISSILE_SPRITE_WIDTH,
				ENEMY_MISSILE_SPRITE_HEIGHT)) {
			game->enemyMissile.active = 0;
			game->enemyMissileFromShip = 0;
			game->enemyMissileTarget = ENEMY_TARGET_NONE;
			destroyWingman(game);
			*enemyMissileChanged = 1;
			*wingmanChanged = 1;
		}
	}

	if (game->enemyPlane.active && game->respawnSafeTimer == 0 &&
		rectsOverlap(game->playerX, game->playerY, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT,
			game->enemyPlane.x, game->enemyPlane.y, ENEMY_SPRITE_WIDTH, ENEMY_SPRITE_HEIGHT)) {
		WORD impactX = game->playerX;
		WORD impactY = game->playerY;
		game->enemyPlane.active = 0;
		game->enemyRespawnTimer = enemyRespawnFramesForSkill(game->skillLevel);
		/* Real CPC: every collision object except flak is instant death
		 * (checkplayeragainstobjectmap/planehitbyobject, :7525-7544/:8127-8132)
		 * - no graduated damage points for enemy-plane contact. */
		startPlayerCrash(game, impactX, impactY);
		*hudChanged = 1;
		*weaponChanged = 1;
		enemyChanged = 1;
	}

	if (game->enemyMissile.active && game->respawnSafeTimer == 0 &&
		rectsOverlap(game->playerX, game->playerY, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT,
			game->enemyMissile.x, game->enemyMissile.y, ENEMY_MISSILE_SPRITE_WIDTH, ENEMY_MISSILE_SPRITE_HEIGHT)) {
		WORD impactX = game->playerX;
		WORD impactY = game->playerY;
		game->enemyMissile.active = 0;
		game->enemyMissileFromShip = 0;
		/* Same as enemy-plane contact above - instant death, matching CPC,
		 * regardless of which source fired the missile. */
		startPlayerCrash(game, impactX, impactY);
		*hudChanged = 1;
		*weaponChanged = 1;
		*enemyMissileChanged = 1;
	}

	/* Sprint 14.96: player weapons can destroy an active powerup (CPC's
	 * dododestroywingmanpowerup - explosionnoise + destroy). The weapon
	 * vanishes with an explosion, no score is awarded. Enemy missile vs
	 * powerup is handled in the missile's own update path. */
	if (game->powerup.active) {
		WORD powerupScreenX = (WORD)(game->powerup.worldX - (LONG)game->scrollX);
		if (game->rocketShot.active &&
			rectsOverlap(game->rocketShot.x, game->rocketShot.y, 16, WEAPON_SPRITE_HEIGHT,
				powerupScreenX, game->powerup.y, POWERUP_COLLISION_WIDTH, POWERUP_SPRITE_HEIGHT)) {
			game->rocketShot.active = 0;
			destroyPowerup(game, 1);
			startWorldImpact(game, game->rocketShot.x, game->rocketShot.y);
			*weaponChanged = 1;
		}
		if (game->powerup.active && game->bombShot.active &&
			rectsOverlap(game->bombShot.x, game->bombShot.y, 16, WEAPON_SPRITE_HEIGHT,
				powerupScreenX, game->powerup.y, POWERUP_COLLISION_WIDTH, POWERUP_SPRITE_HEIGHT)) {
			game->bombShot.active = 0;
			destroyPowerup(game, 1);
			startWorldImpact(game, game->bombShot.x, game->bombShot.y);
			*weaponChanged = 1;
		}
	}

	return enemyChanged;
}

static void updateEnemySprite(UWORD* enemySprite, UWORD* enemyAttachSprite,
	const GameState* game) {
	/* Channel 3 is temporarily borrowed by crash part 2.  Hide the enemy's
	 * even half during a crash, but leave the odd buffer alone so
	 * updateCrashPartSprites() remains its sole owner until the crash ends. */
	if (game->crashTimer) {
		hideHardwareSprite(enemySprite);
		return;
	}
	if (game->enemyPlane.active && game->enemyPlane.x >= 0 &&
		game->enemyPlane.x <= SCREEN_WIDTH - ENEMY_SPRITE_WIDTH)
		buildEnemyPlaneSprite(enemySprite, enemyAttachSprite,
			game->enemyPlane.x, game->enemyPlane.y);
	else {
		hideHardwareSprite(enemySprite);
		hideHardwareSprite(enemyAttachSprite);
	}
}

static void updateEnemyMissileSprite(UWORD* enemyMissileSprite, const GameState* game) {
	if (game->enemyMissile.active && game->enemyMissile.x >= 0 && game->enemyMissile.x <= SCREEN_WIDTH - 16)
		buildEnemyMissileSprite(enemyMissileSprite, game->enemyMissile.x, game->enemyMissile.y);
	else
		hideHardwareSprite(enemyMissileSprite);
}

static void drawStaticGameScene(UBYTE* bitmap) {
	drawGameTileMap(bitmap, gameSceneMap);
}

static void startGameSession(GameState* game,
	USHORT* copper,
	UBYTE** worldBuffers,
	UBYTE* activeWorldBuffer,
	UBYTE* hudBuffer,
	UWORD* playerSprite,
	UWORD* playerAttachSprite,
	UWORD* crashPart1Sprite,
	UWORD* enemyAttachSprite,
	UWORD* enemySprite,
	UWORD* enemyMissileSprite,
	UWORD* wingmanSprite,
	UWORD* unusedSprite7,
	UBYTE* pendingGameScrollCopperUpdate,
	UBYTE* pendingPlayerSpriteUpdate,
	UBYTE* pendingCrashSpriteUpdate,
	UBYTE* pendingEnemySpriteUpdate,
	UBYTE* pendingEnemyMissileSpriteUpdate,
	UBYTE* pendingWingmanSpriteUpdate,
	UBYTE* hudDirty,
	ULONG highScore,
	UBYTE skillLevel,
	UBYTE livesSetting,
	UBYTE wingmanControl) {
	stopAllSfx();
	/* Must be set before initGameState() below, not after: initGameState()
	 * calls resetCpcRandomSequence(), which now generates the land height/
	 * target table immediately (it reads cpcLandSkillLevel for the climb
	 * ceiling) rather than lazily on first render - setting this afterward
	 * would build the table for the previous session's skill level instead
	 * of the one the player just picked at the menu. */
	cpcLandSkillLevel = skillLevel;
	initGameState(game);
	game->takeoffState = TAKEOFF_STATE_ROLLING_IN;
	game->scrollX = TAKEOFF_SCROLL_START_PIXELS;
	setTakeoffDeckPosition(game);
	game->skillLevel = skillLevel;
	/* Menu review fix: must be set before drawHudBuffer() below, same reason
	 * skillLevel is set here rather than by the caller afterward - the menu's
	 * Lives toggle previously only took effect via `game.lives = livesSetting`
	 * AFTER this function returned, so a "Lives: 1" session's first HUD draw
	 * still baked in initGameState()'s PLAYER_START_LIVES(3) default. */
	game->lives = livesSetting;
	/* Same reasoning as lives above - initGameState() set a flat 12/6
	 * regardless of skill; ammoForSkill() gives the real CPC's
	 * skill-scaled starting ammo instead. */
	ammoForSkill(skillLevel, &game->bombs, &game->rockets);
	game->wingmanControl = wingmanControl;
	carrierParkedWingmanVisible =
		(wingmanControl == WINGMAN_CONTROL_CPU);
	*activeWorldBuffer = 0;
	resetBombShotPixelBobFootprints();
	resetRocketShotPixelBobFootprints();
	initRingWorldBuffer(worldBuffers[0], 0);

	*pendingGameScrollCopperUpdate = 0;
	*pendingPlayerSpriteUpdate = 0;
	*pendingCrashSpriteUpdate = 0;
	*pendingEnemySpriteUpdate = 0;
	*pendingEnemyMissileSpriteUpdate = 0;
	*pendingWingmanSpriteUpdate = 0;
	*hudDirty = 0;

	drawHudBuffer(hudBuffer, game, highScore, 0);
#if HAR_DEBUG_PERF_LOG
	perfHudGuardArm(hudBuffer, worldBuffers[0] + GAME_WORLD_BITMAP_BYTES);
#endif
	updatePlayerSprite(playerSprite, playerAttachSprite, game);
	updateCrashPartSprites(crashPart1Sprite, enemyAttachSprite, game);
	updateEnemySprite(enemySprite, enemyAttachSprite, game);
	updateEnemyMissileSprite(enemyMissileSprite, game);
	updateWingmanSprite(wingmanSprite, unusedSprite7, game);
	hideHardwareSprite(unusedSprite7);
	buildGameHudCopper(copper, worldBuffers[*activeWorldBuffer], hudBuffer, (const UWORD*)gamePalette,
		scrollDelayForBplcon1(game->scrollX), displayByteOffsetForGameState(game),
		playerSprite, playerAttachSprite, crashPart1Sprite, enemyAttachSprite, enemySprite, enemyMissileSprite, wingmanSprite, unusedSprite7);
	custom->copjmp1 = 0x7fff;
}

int main(void) {
	SysBase = *((struct ExecBase**)4UL);
	custom = (struct Custom*)0xdff000;
	ciaa = (volatile struct CIA*)0xbfe001;

	GfxBase = (struct GfxBase*)OpenLibrary((CONST_STRPTR)"graphics.library", 0);
	if (!GfxBase)
		Exit(0);

	DOSBase = (struct DosLibrary*)OpenLibrary((CONST_STRPTR)"dos.library", 0);
	if (!DOSBase)
		Exit(0);

	loadHighScoreTable();

	KPrintF("Harrier Attack Reloaded Amiga " HAR_BUILD_LABEL "\n");
	Write(Output(), (APTR)"Harrier Amiga flow sprint\n", 26);
#if HAR_DEBUG_PERF_LOG
	perfLogOpen();
#endif

	USHORT* copper = (USHORT*)AllocMem(COPPER_BYTES, MEMF_CHIP | MEMF_CLEAR);
	UBYTE* screenBuffer = (UBYTE*)AllocMem(SCREEN_BITMAP_BYTES, MEMF_CHIP | MEMF_CLEAR);
	menuTickerBitmap = (UBYTE*)AllocMem(MENU_TICKER_BITMAP_BYTES,
		MEMF_CHIP | MEMF_CLEAR);
	UBYTE* worldBuffers[GAME_WORLD_BUFFER_COUNT];
	worldBuffers[0] = (UBYTE*)AllocMem(GAME_WORLD_BITMAP_BYTES, MEMF_CHIP | MEMF_CLEAR);
	UBYTE* hudBuffer = (UBYTE*)AllocMem(HUD_BITMAP_BYTES, MEMF_CHIP | MEMF_CLEAR);
	UWORD* nullSprite = (UWORD*)AllocMem(2 * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* playerSprite = (UWORD*)AllocMem(PLAYER_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* playerAttachSprite = (UWORD*)AllocMem(PLAYER_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* crashPart1Sprite = (UWORD*)AllocMem(AUXILIARY_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* enemyAttachSprite = (UWORD*)AllocMem(AUXILIARY_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* enemySprite = (UWORD*)AllocMem(ENEMY_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* enemyMissileSprite = (UWORD*)AllocMem(ENEMY_MISSILE_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* wingmanSprite = (UWORD*)AllocMem(PLAYER_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* unusedSprite7 = (UWORD*)AllocMem(PLAYER_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	engineBuffer = (UBYTE*)AllocMem(ENGINE_BUFFER_BYTES, MEMF_CHIP | MEMF_CLEAR);
	carrierIdleDecodeBuffer = (UBYTE*)AllocMem(
		CARRIER_IDLE_DECODE_BUFFER_BYTES, MEMF_CHIP | MEMF_CLEAR);
	telemetrySamples = (TelemetrySample*)AllocMem(sizeof(TelemetrySample) * TELEMETRY_SAMPLE_COUNT, MEMF_FAST | MEMF_CLEAR);
	if (!telemetrySamples && AvailMem(MEMF_PUBLIC) > sizeof(TelemetrySample) * TELEMETRY_SAMPLE_COUNT + 4096)
		telemetrySamples = (TelemetrySample*)AllocMem(sizeof(TelemetrySample) * TELEMETRY_SAMPLE_COUNT, MEMF_PUBLIC | MEMF_CLEAR);
	telemetryAvailable = telemetrySamples ? 1 : 0;
	telemetryEnabled = 0;
	if (!copper || !screenBuffer || !menuTickerBitmap || !worldBuffers[0] || !hudBuffer || !nullSprite || !playerSprite || !playerAttachSprite || !crashPart1Sprite || !enemyAttachSprite || !enemySprite || !enemyMissileSprite || !wingmanSprite || !unusedSprite7 || !engineBuffer || !carrierIdleDecodeBuffer) {
		if (telemetrySamples)
			FreeMem(telemetrySamples, sizeof(TelemetrySample) * TELEMETRY_SAMPLE_COUNT);
		if (copper)
			FreeMem(copper, COPPER_BYTES);
		if (screenBuffer)
			FreeMem(screenBuffer, SCREEN_BITMAP_BYTES);
		if (menuTickerBitmap)
			FreeMem(menuTickerBitmap, MENU_TICKER_BITMAP_BYTES);
		if (worldBuffers[0])
			FreeMem(worldBuffers[0], GAME_WORLD_BITMAP_BYTES);
		if (hudBuffer)
			FreeMem(hudBuffer, HUD_BITMAP_BYTES);
		if (nullSprite)
			FreeMem(nullSprite, 2 * sizeof(UWORD));
		if (playerSprite)
			FreeMem(playerSprite, PLAYER_SPRITE_WORDS * sizeof(UWORD));
		if (playerAttachSprite)
			FreeMem(playerAttachSprite, PLAYER_SPRITE_WORDS * sizeof(UWORD));
		if (crashPart1Sprite)
			FreeMem(crashPart1Sprite, AUXILIARY_SPRITE_WORDS * sizeof(UWORD));
		if (enemyAttachSprite)
			FreeMem(enemyAttachSprite, AUXILIARY_SPRITE_WORDS * sizeof(UWORD));
		if (enemySprite)
			FreeMem(enemySprite, ENEMY_SPRITE_WORDS * sizeof(UWORD));
		if (enemyMissileSprite)
			FreeMem(enemyMissileSprite, ENEMY_MISSILE_SPRITE_WORDS * sizeof(UWORD));
		if (wingmanSprite)
			FreeMem(wingmanSprite, PLAYER_SPRITE_WORDS * sizeof(UWORD));
		if (unusedSprite7)
			FreeMem(unusedSprite7, PLAYER_SPRITE_WORDS * sizeof(UWORD));
		if (engineBuffer)
			FreeMem(engineBuffer, ENGINE_BUFFER_BYTES);
		engineBuffer = 0;
		if (carrierIdleDecodeBuffer)
			FreeMem(carrierIdleDecodeBuffer,
				CARRIER_IDLE_DECODE_BUFFER_BYTES);
		carrierIdleDecodeBuffer = 0;
		CloseLibrary((struct Library*)DOSBase);
		CloseLibrary((struct Library*)GfxBase);
		Exit(0);
	}

	/* loadingScreen is #embed'd straight from assets/loading_screen.bpl, so
	 * its real size is whatever that generated file's byte count is (still
	 * sized for the original 200-line screen) - NOT SCREEN_BITMAP_BYTES,
	 * which now reflects the taller 256-line runtime buffer (Sprint
	 * 14.91.2's HUD resize). Copying SCREEN_BITMAP_BYTES here over-read past
	 * the embedded array's actual bounds. sizeof(loadingScreen) always
	 * matches the asset regardless of screen size changes.
	 *
	 * Copying it to row 0 left it top-aligned in the taller buffer, with the
	 * extra rows (blank/black from the buffer's MEMF_CLEAR allocation)
	 * showing only below it instead of split evenly - reported as "the
	 * startup picture is not centered". Centre it vertically instead. */
	{
		const ULONG loadingScreenRowBytes = (ULONG)SCREEN_PLANES * SCREEN_ROW_BYTES;
		const ULONG loadingScreenHeight = sizeof(loadingScreen) / loadingScreenRowBytes;
		const ULONG loadingScreenTopRow = (SCREEN_HEIGHT > loadingScreenHeight) ? (SCREEN_HEIGHT - loadingScreenHeight) / 2 : 0;
		memcpy(screenBuffer + loadingScreenTopRow * loadingScreenRowBytes, loadingScreen, sizeof(loadingScreen));
	}
	initRingWorldBuffer(worldBuffers[0], 0);
	buildPlayerSprite(playerSprite, playerAttachSprite, PLAYER_START_X, PLAYER_START_Y);
	hideHardwareSprite(crashPart1Sprite);
	hideHardwareSprite(enemyAttachSprite);
	hideHardwareSprite(enemySprite);
	hideHardwareSprite(enemyMissileSprite);

	TakeSystem();
	initSfx();

	buildDisplayCopper(copper, screenBuffer, (const UWORD*)loadingPalette, nullSprite);

	if (HAR_DEBUG_REGISTER_RESOURCES) {
		debug_register_bitmap(screenBuffer, "screen_buffer.bpl", SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_PLANES, debug_resource_bitmap_interleaved);
		debug_register_bitmap(worldBuffers[0], "world_buffer_0.bpl", GAME_WORLD_BUFFER_WIDTH, GAME_WORLD_HEIGHT, SCREEN_PLANES, debug_resource_bitmap_interleaved);
		debug_register_bitmap(hudBuffer, "hud_buffer.bpl", SCREEN_WIDTH, HUD_HEIGHT, SCREEN_PLANES, debug_resource_bitmap_interleaved);
		debug_register_palette(loadingPalette, "loading_screen.pal", 32, 0);
		debug_register_palette(menuPalette, "menu_palette.pal", 32, 0);
		debug_register_palette(gamePalette, "game_palette.pal", 32, 0);
		debug_register_copperlist(copper, "display_copper", COPPER_BYTES, 0);
	}

	custom->cop1lc = (ULONG)copper;
	custom->dmacon = DMAF_BLITTER;
	custom->copjmp1 = 0x7fff;
	custom->dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_RASTER | DMAF_COPPER | DMAF_BLITTER | DMAF_SPRITE;

	SetInterruptHandler((APTR)interruptHandler);
	custom->intena = INTF_SETCLR | INTF_INTEN | INTF_VERTB;
	custom->intreq = (1 << INTB_VERTB);

	InitInput();
	WaitFramesOrSelect(140);
	WaitForInputRelease();

	short selected = MENU_ITEM_START;
	short skillLevel = 1;
	short livesSetting = PLAYER_START_LIVES;
	short wingmanControl = WINGMAN_CONTROL_OFF;
	short inGameScene = 0;
	GameState game;
	ULONG highScore = 0;
	UBYTE pendingGameScrollCopperUpdate = 0;
	UBYTE pendingPlayerSpriteUpdate = 0;
	UBYTE pendingCrashSpriteUpdate = 0;
	UBYTE pendingEnemySpriteUpdate = 0;
	UBYTE pendingEnemyMissileSpriteUpdate = 0;
	UBYTE pendingWingmanSpriteUpdate = 0;
	UBYTE activeWorldBuffer = 0;
	UBYTE hudDirty = 0;
	UBYTE telemetryStatsPaused = 0;
	UBYTE gamePaused = 0;
	UBYTE pauseBlinkCounter = 0;
	UBYTE pauseBlinkVisible = 0;
	/* A raw Escape make code can still be present when the menu changes
	 * scene. Do not let that inherited level immediately cancel the new
	 * session: arm cancellation only after Escape has been observed up. */
	UBYTE gameCancelArmed = 0;
	UBYTE debugHubPage = DEBUG_HUB_CLOSED;
	UBYTE debugHubSelected = DEBUG_ITEM_TELEMETRY;
	UWORD debugGraphicIndex = 0;
	UBYTE debugSoundIndex = 0;
	UBYTE debugMusicIndex = 0;
	initGameState(&game);
	InputState input;
	InputState previousInput;
	UBYTE lastInputMask = 0xff;

	ReadInput(&input);
	previousInput = input;
	drawMenuScreen(screenBuffer, selected, skillLevel, livesSetting, wingmanControl, highScore);
	initMenuTicker(menuTickerBitmap);
	drawTelemetryMenuIndicator(screenBuffer);
	drawInputDebugIfEnabled(screenBuffer, &input, 102, MENU_COLOR_PANEL);
	lastInputMask = InputMask(&input);
	buildMenuCopper(copper, screenBuffer, menuTickerBitmap, menuPalette,
		nullSprite);
	startModMusic();

	while (1) {
		WaitVbl();
		modCompletePendingRetriggers();
		updateSfx();
		if (!inGameScene || game.gameOver || game.missionComplete)
			modTick();
		updateCarrierIdleSfx(
			inGameScene && !telemetryStatsPaused && !gamePaused &&
			!modPlaying && !game.gameOver &&
			(game.takeoffState == TAKEOFF_STATE_ROLLING_IN ||
			 game.takeoffState == TAKEOFF_STATE_READY ||
			 (game.missionComplete && game.postLandingSlide)));
		if (!inGameScene && !telemetryStatsPaused &&
			debugHubPage == DEBUG_HUB_CLOSED)
			updateMenuTicker();
		if (inGameScene && !telemetryStatsPaused && !gamePaused) {
			if (pendingGameScrollCopperUpdate) {
				updateGameScrollCopper(worldBuffers[activeWorldBuffer], &game);
				pendingGameScrollCopperUpdate = 0;
			}
		}
		previousInput = input;
		ReadInput(&input);
#if HAR_HEADLESS_AUTOPLAY
		{
			static UBYTE headlessStartSent = 0;
			static UBYTE headlessUpSent = 0;
			if (!headlessStartSent && !inGameScene && frameCounter > 100) {
				input.select = 1;
				headlessStartSent = 1;
			} else if (headlessStartSent && !headlessUpSent && inGameScene && game.takeoffState == TAKEOFF_STATE_READY) {
				input.up = 1;
				headlessUpSent = 1;
			}
			if (headlessUpSent && game.takeoffState == TAKEOFF_STATE_AIRBORNE)
				input.up = 1;
			if (headlessUpSent && frameCounter > 4700)
				break;
		}
#endif
		UBYTE inputMask = InputMask(&input);

		/* Escape is a scene-level command, not a gameplay action. Handle it
		 * before pause, telemetry, game-over and all simulation state
		 * machines so no later feature can accidentally make Escape inert.
		 *
		 * Do not update/save high scores here. An aborted run is not a
		 * completed run, and synchronous AmigaDOS I/O while the custom game
		 * display owns the hardware can stall the transition. Game-over
		 * still records the score through its existing path. */
		if (inGameScene) {
			if (!gameCancelArmed && !input.cancel)
				gameCancelArmed = 1;
			if (gameCancelArmed && input.cancel) {
				inGameScene = 0;
				gameCancelArmed = 0;
				telemetryStatsPaused = 0;
				gamePaused = 0;
				debugHubPage = DEBUG_HUB_CLOSED;
				stopAllSfx();
				stopModMusic();
				pendingGameScrollCopperUpdate = 0;
				pendingPlayerSpriteUpdate = 0;
				pendingCrashSpriteUpdate = 0;
				pendingEnemySpriteUpdate = 0;
				pendingEnemyMissileSpriteUpdate = 0;
				pendingWingmanSpriteUpdate = 0;
				hideHardwareSprite(playerSprite);
				hideHardwareSprite(playerAttachSprite);
				hideHardwareSprite(crashPart1Sprite);
				hideHardwareSprite(enemyAttachSprite);
				hideHardwareSprite(enemySprite);
				hideHardwareSprite(enemyMissileSprite);
				hideHardwareSprite(wingmanSprite);
				hideHardwareSprite(unusedSprite7);
				drawMenuScreen(screenBuffer, selected, skillLevel,
					livesSetting, wingmanControl, highScore);
				drawTelemetryMenuIndicator(screenBuffer);
				buildMenuCopper(copper, screenBuffer, menuTickerBitmap,
					menuPalette, nullSprite);
				custom->copjmp1 = 0x7fff;
				startModMusic();
				drawInputDebugIfEnabled(screenBuffer, &input, 102,
					MENU_COLOR_PANEL);
				lastInputMask = inputMask;
				continue;
			}
		}

		if (!inGameScene && debugHubPage == DEBUG_HUB_CLOSED &&
			(inputMask != lastInputMask ||
			 input.lastRawKey != previousInput.lastRawKey)) {
			drawInputDebugIfEnabled(screenBuffer, &input, 102, MENU_COLOR_PANEL);
			lastInputMask = inputMask;
		}

		if (telemetryStatsPaused) {
			if (Pressed(input.r, previousInput.r)) {
				telemetryReset();
				drawTelemetryStatsScreen(screenBuffer);
			} else if (Pressed(input.space, previousInput.space)) {
				telemetryStatsPaused = 0;
				buildGameHudCopper(copper, worldBuffers[activeWorldBuffer], hudBuffer, (const UWORD*)gamePalette,
					scrollDelayForBplcon1(game.scrollX), displayByteOffsetForGameState(&game),
					playerSprite, playerAttachSprite, crashPart1Sprite, enemyAttachSprite, enemySprite, enemyMissileSprite, wingmanSprite, unusedSprite7);
				custom->copjmp1 = 0x7fff;
				if (!game.gameOver && game.takeoffState == TAKEOFF_STATE_AIRBORNE && !game.crashTimer)
					startEngineSound(scrollPixelsForSpeedLevel(game.speedLevel));
				pendingGameScrollCopperUpdate = 1;
				pendingPlayerSpriteUpdate = 1;
				pendingCrashSpriteUpdate = 1;
				pendingEnemySpriteUpdate = 1;
				pendingEnemyMissileSpriteUpdate = 1;
			}
		} else if (gamePaused) {
			pauseBlinkCounter++;
			if (pauseBlinkCounter >= 25) {
				pauseBlinkCounter = 0;
				pauseBlinkVisible = !pauseBlinkVisible;
				drawPauseHudOverlay(hudBuffer, pauseBlinkVisible);
			}
			if (Pressed(input.space, previousInput.space) ||
				Pressed(input.p, previousInput.p)) {
				gamePaused = 0;
				drawHudBuffer(hudBuffer, &game, highScore, 0);
				if (!game.gameOver &&
					game.takeoffState == TAKEOFF_STATE_AIRBORNE &&
					!game.crashTimer)
					startEngineSound(
						scrollPixelsForSpeedLevel(game.speedLevel));
				pendingGameScrollCopperUpdate = 1;
				pendingPlayerSpriteUpdate = 1;
				pendingCrashSpriteUpdate = 1;
				pendingEnemySpriteUpdate = 1;
				pendingEnemyMissileSpriteUpdate = 1;
				pendingWingmanSpriteUpdate = 1;
			}
		} else if (!inGameScene) {
			if (debugHubPage != DEBUG_HUB_CLOSED) {
				UBYTE debugBack = input.cancel ||
					(input.shift && Pressed(input.d, previousInput.d));
				if (debugBack) {
					if (debugHubPage == DEBUG_HUB_OPTIONS) {
						debugHubPage = DEBUG_HUB_CLOSED;
						drawMenuScreen(screenBuffer, selected, skillLevel,
							livesSetting, wingmanControl, highScore);
						drawTelemetryMenuIndicator(screenBuffer);
						buildMenuCopper(copper, screenBuffer,
							menuTickerBitmap, menuPalette, nullSprite);
						custom->copjmp1 = 0x7fff;
					} else {
						if (debugHubPage == DEBUG_HUB_SOUNDS) {
							stopAllSfx();
							startModMusic();
						} else if (debugHubPage == DEBUG_HUB_MUSIC) {
							stopModMusic();
							startModMusic();
						}
						debugHubPage = DEBUG_HUB_OPTIONS;
						drawDebugHub(screenBuffer, debugHubSelected);
						buildDisplayCopper(copper, screenBuffer,
							menuPalette, nullSprite);
						custom->copjmp1 = 0x7fff;
					}
					lastInputMask = inputMask;
				} else if (debugHubPage == DEBUG_HUB_OPTIONS) {
					if (Pressed(input.menuPrev, previousInput.menuPrev) ||
						Pressed(input.menuNext, previousInput.menuNext)) {
						UBYTE oldSelected = debugHubSelected;
						if (Pressed(input.menuPrev, previousInput.menuPrev))
							debugHubSelected = (UBYTE)((debugHubSelected +
								DEBUG_ITEM_COUNT - 1) % DEBUG_ITEM_COUNT);
						else
							debugHubSelected = (UBYTE)((debugHubSelected + 1) %
								DEBUG_ITEM_COUNT);
						drawDebugHubItem(screenBuffer, oldSelected, 0);
						drawDebugHubItem(screenBuffer, debugHubSelected, 1);
					}

					UBYTE activate = Pressed(input.select, previousInput.select) ||
						Pressed(input.left, previousInput.left) ||
						Pressed(input.right, previousInput.right);
					if (activate) {
						switch (debugHubSelected) {
							case DEBUG_ITEM_TELEMETRY:
								if (telemetryAvailable) {
									telemetryEnabled = telemetryEnabled ? 0 : 1;
									if (telemetryEnabled)
										telemetryReset();
								}
								drawDebugHubItem(screenBuffer,
									debugHubSelected, 1);
								break;
							case DEBUG_ITEM_INFINITE_LIVES:
								debugInfiniteLives = !debugInfiniteLives;
								drawDebugHubItem(screenBuffer,
									debugHubSelected, 1);
								break;
							case DEBUG_ITEM_INFINITE_BOMBS:
								debugInfiniteBombs = !debugInfiniteBombs;
								drawDebugHubItem(screenBuffer,
									debugHubSelected, 1);
								break;
							case DEBUG_ITEM_INFINITE_ROCKETS:
								debugInfiniteRockets = !debugInfiniteRockets;
								drawDebugHubItem(screenBuffer,
									debugHubSelected, 1);
								break;
							case DEBUG_ITEM_INFINITE_FUEL:
								debugInfiniteFuel = !debugInfiniteFuel;
								drawDebugHubItem(screenBuffer,
									debugHubSelected, 1);
								break;
							case DEBUG_ITEM_GRAPHICS:
								debugHubPage = DEBUG_HUB_GRAPHICS;
								drawDebugGraphicsBrowser(screenBuffer,
									debugGraphicIndex);
								buildDisplayCopper(copper, screenBuffer,
									(const UWORD*)gamePalette, nullSprite);
								custom->copjmp1 = 0x7fff;
								break;
							case DEBUG_ITEM_SOUNDS:
								stopModMusic();
								stopAllSfx();
								debugHubPage = DEBUG_HUB_SOUNDS;
								drawDebugSoundBrowser(screenBuffer,
									debugSoundIndex, 0);
								buildDisplayCopper(copper, screenBuffer,
									menuPalette, nullSprite);
								custom->copjmp1 = 0x7fff;
								break;
							case DEBUG_ITEM_MUSIC:
								stopModMusic();
								stopAllSfx();
								debugHubPage = DEBUG_HUB_MUSIC;
								drawDebugMusicBrowser(screenBuffer,
									debugMusicIndex, 0);
								buildDisplayCopper(copper, screenBuffer,
									menuPalette, nullSprite);
								custom->copjmp1 = 0x7fff;
								break;
							default:
								debugHubPage = DEBUG_HUB_CLOSED;
								drawMenuScreen(screenBuffer, selected,
									skillLevel, livesSetting,
									wingmanControl, highScore);
								drawTelemetryMenuIndicator(screenBuffer);
								buildMenuCopper(copper, screenBuffer,
									menuTickerBitmap, menuPalette, nullSprite);
								custom->copjmp1 = 0x7fff;
								break;
						}
					}
				} else if (debugHubPage == DEBUG_HUB_GRAPHICS) {
					if (Pressed(input.left, previousInput.left)) {
						debugGraphicIndex = debugGraphicIndex == 0
							? DEBUG_GRAPHIC_COUNT - 1
							: (UWORD)(debugGraphicIndex - 1);
						drawDebugGraphicsBrowser(screenBuffer,
							debugGraphicIndex);
					} else if (Pressed(input.right, previousInput.right)) {
						debugGraphicIndex =
							(UWORD)((debugGraphicIndex + 1) %
								DEBUG_GRAPHIC_COUNT);
						drawDebugGraphicsBrowser(screenBuffer,
							debugGraphicIndex);
					}
				} else if (debugHubPage == DEBUG_HUB_SOUNDS) {
					if (Pressed(input.left, previousInput.left)) {
						debugSoundIndex = debugSoundIndex == 0
							? SFX_COUNT - 1
							: (UBYTE)(debugSoundIndex - 1);
						drawDebugSoundBrowser(screenBuffer,
							debugSoundIndex, 0);
					} else if (Pressed(input.right, previousInput.right)) {
						debugSoundIndex =
							(UBYTE)((debugSoundIndex + 1) % SFX_COUNT);
						drawDebugSoundBrowser(screenBuffer,
							debugSoundIndex, 0);
					} else if (Pressed(input.select, previousInput.select)) {
						playSfx(debugSoundIndex);
						drawDebugSoundBrowser(screenBuffer,
							debugSoundIndex, 1);
					}
				} else if (debugHubPage == DEBUG_HUB_MUSIC) {
					if (Pressed(input.left, previousInput.left)) {
						debugMusicIndex = debugMusicIndex == 0
							? DEBUG_MUSIC_COUNT - 1
							: (UBYTE)(debugMusicIndex - 1);
						stopModMusic();
						drawDebugMusicBrowser(screenBuffer,
							debugMusicIndex, 0);
					} else if (Pressed(input.right, previousInput.right)) {
						debugMusicIndex = (UBYTE)((debugMusicIndex + 1) %
							DEBUG_MUSIC_COUNT);
						stopModMusic();
						drawDebugMusicBrowser(screenBuffer,
							debugMusicIndex, 0);
					} else if (Pressed(input.select, previousInput.select)) {
						stopModMusic();
						if (debugMusicIndex == 0)
							startModMusic();
						else if (debugMusicIndex == 1)
							startGameOverMusic();
						else
							startCarrierLandingMusic();
						drawDebugMusicBrowser(screenBuffer,
							debugMusicIndex, 1);
					}
				}
			} else if (input.shift &&
				Pressed(input.d, previousInput.d)) {
				debugHubPage = DEBUG_HUB_OPTIONS;
				drawDebugHub(screenBuffer, debugHubSelected);
				buildDisplayCopper(copper, screenBuffer, menuPalette,
					nullSprite);
				custom->copjmp1 = 0x7fff;
			} else {
				if (Pressed(input.menuNext, previousInput.menuNext) ||
					Pressed(input.menuPrev, previousInput.menuPrev)) {
					short oldSelected = selected;
					if (Pressed(input.menuPrev, previousInput.menuPrev))
						selected = (selected + MENU_ITEM_COUNT - 1) %
							MENU_ITEM_COUNT;
					else
						selected = (selected + 1) % MENU_ITEM_COUNT;
					updateMenuSelection(screenBuffer, oldSelected, selected,
						skillLevel, livesSetting, wingmanControl);
					drawInputDebugIfEnabled(screenBuffer, &input, 102,
						MENU_COLOR_PANEL);
				}

				/* Left/right changes the currently selected value in either
				 * direction. It redraws only that row, never the full menu. */
				UBYTE menuAdjusted = 0;
				if (!input.shift && !input.control &&
					(Pressed(input.left, previousInput.left) !=
					 Pressed(input.right, previousInput.right))) {
					short direction = Pressed(input.left, previousInput.left)
						? -1 : 1;
					menuAdjusted = adjustSelectedMenuOption(screenBuffer,
						selected, direction, &skillLevel, &livesSetting,
						&wingmanControl, highScore);
					if (menuAdjusted) {
						drawInputDebugIfEnabled(screenBuffer, &input, 102,
							MENU_COLOR_PANEL);
					}
				}

				if (!menuAdjusted &&
					Pressed(input.select, previousInput.select)) {
					if (selected == MENU_ITEM_START) {
						stopModMusic();
						startGameSession(&game, copper, worldBuffers,
							&activeWorldBuffer, hudBuffer, playerSprite,
							playerAttachSprite, crashPart1Sprite,
							enemyAttachSprite, enemySprite,
							enemyMissileSprite, wingmanSprite,
							unusedSprite7,
							&pendingGameScrollCopperUpdate,
							&pendingPlayerSpriteUpdate,
							&pendingCrashSpriteUpdate,
							&pendingEnemySpriteUpdate,
							&pendingEnemyMissileSpriteUpdate,
							&pendingWingmanSpriteUpdate, &hudDirty,
							highScore, (UBYTE)skillLevel,
							(UBYTE)livesSetting,
							(UBYTE)wingmanControl);
						if (telemetryEnabled)
							telemetryReset();
						lastInputMask = inputMask;
						inGameScene = 1;
						gameCancelArmed = 0;
						gamePaused = 0;
					} else if (adjustSelectedMenuOption(screenBuffer,
							selected, 1, &skillLevel, &livesSetting,
							&wingmanControl, highScore)) {
						drawInputDebugIfEnabled(screenBuffer, &input, 102,
							MENU_COLOR_PANEL);
					}
				}
			}
		} else {
			if (Pressed(input.p, previousInput.p)) {
				gamePaused = 1;
				pauseBlinkCounter = 0;
				pauseBlinkVisible = 1;
				stopAllSfx();
				drawPauseHudOverlay(hudBuffer, pauseBlinkVisible);
			} else if ((input.shift || input.control) && Pressed(input.d, previousInput.d)) {
				telemetryStatsPaused = 1;
				stopAllSfx();
				hideHardwareSprite(playerSprite);
				hideHardwareSprite(playerAttachSprite);
				hideHardwareSprite(crashPart1Sprite);
				hideHardwareSprite(enemyAttachSprite);
				hideHardwareSprite(enemySprite);
				hideHardwareSprite(enemyMissileSprite);
				hideHardwareSprite(wingmanSprite);
				hideHardwareSprite(unusedSprite7);
				drawTelemetryStatsScreen(screenBuffer);
				buildDisplayCopper(copper, screenBuffer, menuPalette, nullSprite);
				custom->copjmp1 = 0x7fff;
			} else if (!game.gameOver) {
				if (game.takeoffState == TAKEOFF_STATE_ROLLING_IN) {
					if (game.scrollX > TAKEOFF_SCROLL_STEP_PIXELS)
						game.scrollX = (UWORD)(game.scrollX - TAKEOFF_SCROLL_STEP_PIXELS);
					else {
						game.scrollX = 0;
						game.takeoffState = TAKEOFF_STATE_READY;
					}
					setTakeoffDeckPosition(&game);
					pendingGameScrollCopperUpdate = 1;
					pendingPlayerSpriteUpdate = 1;
				} else if (game.takeoffState == TAKEOFF_STATE_READY) {
					setTakeoffDeckPosition(&game);
					if (Pressed(input.up, previousInput.up)) {
						game.takeoffState = TAKEOFF_STATE_LIFTING;
						game.scrollX = 0;
						game.playerX = TAKEOFF_PLAYER_DECK_X;
						game.playerY = TAKEOFF_PLAYER_DECK_Y - PLAYER_MOVE_SPEED_PIXELS;
						updatePlayerSprite(playerSprite, playerAttachSprite, &game);
						startEngineSound(scrollPixelsForSpeedLevel(game.speedLevel));
						pendingGameScrollCopperUpdate = 1;
						pendingPlayerSpriteUpdate = 1;
					}
				} else {
				if (game.ejectState) {
					if (updatePlayerEject(&game)) {
						hudDirty = 1;
						pendingPlayerSpriteUpdate = 1;
						pendingWingmanSpriteUpdate = 1;
					}
					if (hudDirty) {
						drawHudValues(hudBuffer, &game, highScore, 0);
						hudDirty = 0;
					}
				} else if (game.crashTimer) {
					if (updatePlayerCrash(&game)) {
						hudDirty = 1;
						pendingPlayerSpriteUpdate = 1;
						pendingCrashSpriteUpdate = 1;
						pendingEnemySpriteUpdate = 1;
						pendingEnemyMissileSpriteUpdate = 1;
					}
					if (hudDirty) {
						drawHudValues(hudBuffer, &game, highScore, 0);
						hudDirty = 0;
					}
				} else {
				if (game.missionComplete) {
					if (game.missionCompleteTimer > 0) {
						game.missionCompleteTimer--;
					} else if (!modPlaying) {
						/* CPC scrollrightfortakeoffloop keeps the current
						 * red landing scene and its already drawn carrier,
						 * then slides the landed aircraft back along the
						 * deck. Do this before preparing the next level, so
						 * there is no intermediate clear/redraw screen. */
						game.postLandingSlide = 1;
						if (game.playerX != LANDING_RESTART_PLAYER_X) {
							if (game.playerX > LANDING_RESTART_PLAYER_X)
								game.playerX -= LANDING_RESTART_SLIDE_PIXELS;
							else
								game.playerX += LANDING_RESTART_SLIDE_PIXELS;
							pendingPlayerSpriteUpdate = 1;
						} else {
						/* CPC beginlandingapproach: after four delays, raise
						 * difficulty (up to 5), replenish, reset level
						 * progress and return to newlevelloop/checkliftoff.
						 * The Amiga landing fanfare also gets to finish
						 * before this transition starts the next sortie.
						 * startGameSession supplies the same complete world/
						 * actor reset; restore run-persistent values afterward
						 * and skip the initial carrier-entry animation. */
						ULONG nextBonusScore = game.bonusScore;
						UWORD nextHitsCount = game.hitsCount;
						UBYTE nextLives = game.lives;
						UBYTE nextSkill = game.skillLevel < 5
							? (UBYTE)(game.skillLevel + 1) : 5;
						UBYTE nextWingmanControl = game.wingmanControl;
						/* A wingman lost earlier in the run must stay lost
						 * into the next mission too - only a Wingman powerup
						 * pickup should bring it back, not a free respawn
						 * every time the carrier resets for the next sortie. */
						UBYTE nextWingmanDestroyed = game.wingman.destroyed;

						skillLevel = nextSkill;
						startGameSession(&game, copper, worldBuffers,
							&activeWorldBuffer, hudBuffer, playerSprite,
							playerAttachSprite, crashPart1Sprite, enemyAttachSprite,
							enemySprite, enemyMissileSprite, wingmanSprite,
							unusedSprite7, &pendingGameScrollCopperUpdate,
							&pendingPlayerSpriteUpdate, &pendingCrashSpriteUpdate,
							&pendingEnemySpriteUpdate,
							&pendingEnemyMissileSpriteUpdate,
							&pendingWingmanSpriteUpdate, &hudDirty, highScore,
							nextSkill, nextLives, nextWingmanControl);
						game.bonusScore = nextBonusScore;
						game.score = nextBonusScore;
						game.hitsCount = nextHitsCount;
						game.wingman.destroyed = nextWingmanDestroyed;
						if (nextWingmanDestroyed)
							game.wingman.mode = WINGMAN_DESTROYED;
						game.takeoffState = TAKEOFF_STATE_READY;
						game.scrollX = 0;
						setTakeoffDeckPosition(&game);
						drawHudValues(hudBuffer, &game, highScore, 0);
						updatePlayerSprite(playerSprite, playerAttachSprite, &game);
						pendingGameScrollCopperUpdate = 1;
						pendingPlayerSpriteUpdate = 0;
						lastInputMask = inputMask;
						gameCancelArmed = 0;
						}
					}
				} else {
				UWORD oldScrollX = game.scrollX;
				WORD oldPlayerX = game.playerX;
				WORD oldPlayerY = game.playerY;
				if (game.respawnSafeTimer > 0)
					game.respawnSafeTimer--;
				if (game.bombLaunchCooldown > 0)
					game.bombLaunchCooldown--;

				if (updateLandingApproach(&game))
					hudDirty = 1;
				if (!game.missionComplete && game.landingState == LANDING_STATE_NONE &&
					updateThrottle(&game, &input))
					hudDirty = 1;

				UBYTE scrollPixels = (game.missionComplete || game.landingState == LANDING_STATE_HOVER) ?
					0 : scrollPixelsForSpeedLevel(game.speedLevel);
				if (!game.missionComplete && game.landingState != LANDING_STATE_HOVER &&
					game.scrollX < GAME_SCROLL_MAX_PIXELS) {
					UWORD nextScrollX = (UWORD)(game.scrollX + scrollPixels);
					if (game.landingState == LANDING_STATE_SLOWING &&
						nextScrollX > LANDING_HOVER_SCROLL_X)
						nextScrollX = LANDING_HOVER_SCROLL_X;
					game.scrollX = nextScrollX > GAME_SCROLL_MAX_PIXELS ? GAME_SCROLL_MAX_PIXELS : nextScrollX;
				}

				if (game.takeoffState == TAKEOFF_STATE_LIFTING) {
					/* CPC begins with a vertical climb from the rear deck.
					 * Do not apply the normal speed-based X anchor until the
					 * Harrier has cleared the carrier superstructure. */
					game.playerY -= PLAYER_MOVE_SPEED_PIXELS;
					if (game.playerY <= TAKEOFF_CLEAR_Y) {
						game.playerY = TAKEOFF_CLEAR_Y;
						game.takeoffState = TAKEOFF_STATE_AIRBORNE;
						/* Replace the baked parked aircraft with a hardware
						 * sprite at the exact same deck position. Its takeoff
						 * state then performs the CPC journey into formation
						 * over subsequent frames. */
						if (game.wingmanControl == WINGMAN_CONTROL_CPU && !game.wingman.active) {
							game.wingman.active = 1;
							game.wingman.mode = WINGMAN_TAKEOFF;
							game.wingman.interceptScreenX =
								WINGMAN_TAKEOFF_DECK_X;
							game.wingman.screenY = WINGMAN_TAKEOFF_DECK_Y;
							game.wingman.row =
								WINGMAN_TAKEOFF_DECK_Y / GAME_TILE_HEIGHT;
							game.wingman.moveTimer = 0;
							/* The old promoted carrier tiles baked the
							 * parked grey Wingman permanently into the
							 * deck. Switch to the aircraft-free carrier
							 * composite as soon as the real hardware
							 * sprite launches, then refresh both carrier
							 * ranges in the ring buffer. */
							carrierParkedWingmanVisible = 0;
							/* Only the start carrier changed from the parked-
							 * Wingman composite. Redrawing every carrier here
							 * mapped the far end carrier into the same ring
							 * slots and made it appear behind the first enemy
							 * frigate until those columns streamed again. */
							dirtyRedrawNativeCarrierAt(worldBuffers, 8);
						}
					}
				} else if (game.landingState == LANDING_STATE_HOVER) {
					/* CPC landinghoverloop releases the speed-derived X
					 * anchor and allows four-direction hover control. */
					if (input.left && !input.right && game.playerX > PLAYER_MIN_X)
						game.playerX -= PLAYER_MOVE_SPEED_PIXELS;
					if (input.right && !input.left && game.playerX < PLAYER_MAX_X)
						game.playerX += PLAYER_MOVE_SPEED_PIXELS;
					if (input.up && game.playerY > PLAYER_MIN_Y)
						game.playerY -= PLAYER_MOVE_SPEED_PIXELS;
					if (input.down && game.playerY < PLAYER_MAX_Y)
						game.playerY += PLAYER_MOVE_SPEED_PIXELS;
				} else {
					WORD targetPlayerX = playerTargetXForSpeedLevel(game.speedLevel);
					if (game.playerX < targetPlayerX && game.playerX < PLAYER_MAX_X) {
						game.playerX += PLAYER_MOVE_SPEED_PIXELS;
						if (game.playerX > targetPlayerX)
							game.playerX = targetPlayerX;
						if (game.playerX > PLAYER_MAX_X)
							game.playerX = PLAYER_MAX_X;
					} else if (game.playerX > targetPlayerX && game.playerX > PLAYER_MIN_X) {
						game.playerX -= PLAYER_MOVE_SPEED_PIXELS;
						if (game.playerX < targetPlayerX)
							game.playerX = targetPlayerX;
						if (game.playerX < PLAYER_MIN_X)
							game.playerX = PLAYER_MIN_X;
					}

					if (input.up && game.playerY > PLAYER_MIN_Y) {
						game.playerY -= PLAYER_MOVE_SPEED_PIXELS;
						if (game.playerY < PLAYER_MIN_Y)
							game.playerY = PLAYER_MIN_Y;
					}
					if (input.down && game.playerY < PLAYER_MAX_Y) {
						game.playerY += PLAYER_MOVE_SPEED_PIXELS;
						if (game.playerY > PLAYER_MAX_Y)
							game.playerY = PLAYER_MAX_Y;
					}
				}
				if (game.scrollX > GAME_SCROLL_MAX_PIXELS)
					game.scrollX = GAME_SCROLL_MAX_PIXELS;

				updateEngineSound(scrollPixels);

				if (game.scrollX != oldScrollX) {
					pendingGameScrollCopperUpdate = 1;
					if (updateHudValues(&game))
						hudDirty = 1;
				}
				if (game.playerX != oldPlayerX || game.playerY != oldPlayerY)
					pendingPlayerSpriteUpdate = 1;
				if (Pressed(input.eject, previousInput.eject) &&
					!input.cancel && game.respawnSafeTimer == 0) {
					startPlayerEject(&game);
					hudDirty = 1;
					pendingPlayerSpriteUpdate = 1;
					pendingEnemyMissileSpriteUpdate = 1;
					pendingWingmanSpriteUpdate = 1;
				}
				/* Weapons are level-triggered on the CPC: holding Fire is
				 * sampled again once the previous rocket has gone.  Using
				 * the menu-oriented select edge here meant that a Fire
				 * press which was already down at the gameplay boundary
				 * (or while the previous rocket was active) was discarded,
				 * and no later launch happened until a perfect new edge.
				 * launchRocket() already limits this to one live shot, so
				 * sampling the actual fire level restores the CPC behaviour
				 * without permitting multiple simultaneous rockets. */
				if (game.landingState != LANDING_STATE_HOVER &&
					input.fire) {
					if (launchRocket(&game, input.left)) {
						hudDirty = 1;
						pendingCrashSpriteUpdate = 1;
					}
				}
				if (game.landingState != LANDING_STATE_HOVER &&
					Pressed(input.bomb, previousInput.bomb)) {
					if (launchBomb(&game)) {
						hudDirty = 1;
						pendingCrashSpriteUpdate = 1;
					}
				}
			if (updateWeapons(&game, scrollPixels, worldBuffers))
				pendingCrashSpriteUpdate = 1;
			trySpawnFlak(&game, worldBuffers);
			trySpawnPowerup(&game);
			updateCityFade(&game);
			updateTargetLock(&game);
			maybeStartWingmanBombingRun(&game);
			updatePowerup(&game);
				if (updateEnemyPlane(&game, scrollPixels))
					pendingEnemySpriteUpdate = 1;
				if (updateEnemyMissile(&game, scrollPixels))
					pendingEnemyMissileSpriteUpdate = 1;
				updateWingmanFormationRow(&game);
				updateWingmanTakeoff(&game);
				updateWingmanIntercept(&game);
				updateWingmanBombingRun(&game, worldBuffers, &hudDirty);
				updateWingmanLanding(&game);
				updateWingmanVisualY(&game);
				if (game.wingman.active)
					pendingWingmanSpriteUpdate = 1;
				{
					UBYTE wingmanRocketHudDirty = 0;
					UBYTE wingmanRocketEnemyDirty = 0;
					updateWingmanRocket(&game, &wingmanRocketHudDirty, &wingmanRocketEnemyDirty);
					if (wingmanRocketHudDirty)
						hudDirty = 1;
					if (wingmanRocketEnemyDirty)
						pendingEnemySpriteUpdate = 1;
				}
				UBYTE collisionHudDirty = 0;
				UBYTE collisionWeaponDirty = 0;
				UBYTE collisionEnemyMissileDirty = 0;
				UBYTE collisionWingmanDirty = 0;
				if (updateGameCollisions(&game, worldBuffers,
					&collisionHudDirty, &collisionWeaponDirty,
					&collisionEnemyMissileDirty, &collisionWingmanDirty))
					pendingEnemySpriteUpdate = 1;
				if (collisionHudDirty) {
					hudDirty = 1;
#if HAR_DEBUG_PERF_LOG
					hudReplenishFires++;
#endif
					pendingPlayerSpriteUpdate = 1;
					pendingCrashSpriteUpdate = 1;
					pendingEnemySpriteUpdate = 1;
					pendingEnemyMissileSpriteUpdate = 1;
				}
				if (collisionWeaponDirty)
					pendingCrashSpriteUpdate = 1;
				if (collisionEnemyMissileDirty)
					pendingEnemyMissileSpriteUpdate = 1;
				if (collisionWingmanDirty)
					pendingWingmanSpriteUpdate = 1;
				if (game.gameOver) {
					updateHighScore(&highScore, &game);
					pendingCrashSpriteUpdate = 1;
					pendingEnemySpriteUpdate = 1;
					pendingEnemyMissileSpriteUpdate = 1;
				}
				if (hudDirty) {
					drawHudValues(hudBuffer, &game, highScore, 0);
					hudDirty = 0;
				}
				}
				}
				}
			} else {
				if (Pressed(input.select, previousInput.select)) {
					stopModMusic();
					startGameSession(&game, copper, worldBuffers, &activeWorldBuffer, hudBuffer, playerSprite, playerAttachSprite, crashPart1Sprite, enemyAttachSprite,
						enemySprite, enemyMissileSprite, wingmanSprite, unusedSprite7,
						&pendingGameScrollCopperUpdate, &pendingPlayerSpriteUpdate,
						&pendingCrashSpriteUpdate, &pendingEnemySpriteUpdate, &pendingEnemyMissileSpriteUpdate, &pendingWingmanSpriteUpdate,
						&hudDirty, highScore, (UBYTE)skillLevel, (UBYTE)livesSetting, (UBYTE)wingmanControl);
					if (telemetryEnabled)
						telemetryReset();
					lastInputMask = inputMask;
					gameCancelArmed = 0;
				}
			}
		}
		if (inGameScene && !telemetryStatsPaused && !gamePaused) {
			if (pendingPlayerSpriteUpdate) {
				updatePlayerSprite(playerSprite, playerAttachSprite, &game);
				pendingPlayerSpriteUpdate = 0;
			}
			if (pendingCrashSpriteUpdate) {
				updateCrashPartSprites(crashPart1Sprite, enemyAttachSprite, &game);
				pendingCrashSpriteUpdate = 0;
			}
			if (pendingEnemySpriteUpdate) {
				updateEnemySprite(enemySprite, enemyAttachSprite, &game);
				pendingEnemySpriteUpdate = 0;
			}
			if (pendingEnemyMissileSpriteUpdate) {
				updateEnemyMissileSprite(enemyMissileSprite, &game);
				pendingEnemyMissileSpriteUpdate = 0;
			}
			if (pendingWingmanSpriteUpdate) {
				updateWingmanSprite(wingmanSprite, unusedSprite7, &game);
				pendingWingmanSpriteUpdate = 0;
			}
			serviceRingWorldStream(worldBuffers[0], &game);
			/* The world is currently single-buffered. Keep erase and redraw
			 * adjacent late in the frame so the raster cannot spend the
			 * whole gameplay update observing a temporarily missing bomb. */
			eraseBombPixelBobFootprint(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, bombShotFootprints);
			eraseBombPixelBobFootprint(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, wingmanBombFootprints);
			eraseRocketPixelBobFootprint(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, rocketShotFootprints);
			eraseRocketPixelBobFootprint(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, wingmanRocketFootprints);
			updateBombImpactBob(worldBuffers[0], &game);
			updatePowerupBob(worldBuffers[0], &game);
			drawBombPixelBob(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, &game.bombShot, bombShotFootprints,
				game.scrollX);
			drawBombPixelBob(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, &game.wingman.bomb,
				wingmanBombFootprints, game.scrollX);
			drawRocketPixelBob(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, &game.rocketShot,
				rocketShotFootprints, game.crashTimer != 0);
			drawRocketPixelBob(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, &game.wingman.rocket,
				wingmanRocketFootprints, game.crashTimer != 0);
			telemetryUpdate(&game, activeWorldBuffer);
#if HAR_DEBUG_PERF_LOG
			perfLogFrame(&game, activeWorldBuffer);
#endif
		}
	}

	stopAllSfx();
	FreeMem(playerSprite, PLAYER_SPRITE_WORDS * sizeof(UWORD));
	FreeMem(playerAttachSprite, PLAYER_SPRITE_WORDS * sizeof(UWORD));
	FreeMem(enemyMissileSprite, ENEMY_MISSILE_SPRITE_WORDS * sizeof(UWORD));
	FreeMem(wingmanSprite, PLAYER_SPRITE_WORDS * sizeof(UWORD));
	FreeMem(unusedSprite7, PLAYER_SPRITE_WORDS * sizeof(UWORD));
	FreeMem(enemySprite, ENEMY_SPRITE_WORDS * sizeof(UWORD));
	FreeMem(enemyAttachSprite, AUXILIARY_SPRITE_WORDS * sizeof(UWORD));
	FreeMem(crashPart1Sprite, AUXILIARY_SPRITE_WORDS * sizeof(UWORD));
	FreeMem(nullSprite, 2 * sizeof(UWORD));
	FreeMem(hudBuffer, HUD_BITMAP_BYTES);
	FreeMem(worldBuffers[0], GAME_WORLD_BITMAP_BYTES);
	FreeMem(engineBuffer, ENGINE_BUFFER_BYTES);
	engineBuffer = 0;
	FreeMem(carrierIdleDecodeBuffer, CARRIER_IDLE_DECODE_BUFFER_BYTES);
	carrierIdleDecodeBuffer = 0;
	if (telemetrySamples)
		FreeMem(telemetrySamples, sizeof(TelemetrySample) * TELEMETRY_SAMPLE_COUNT);
	telemetrySamples = 0;
	FreeMem(menuTickerBitmap, MENU_TICKER_BITMAP_BYTES);
	menuTickerBitmap = 0;
	FreeMem(screenBuffer, SCREEN_BITMAP_BYTES);
	FreeMem(copper, COPPER_BYTES);
	FreeSystem();
#if HAR_DEBUG_PERF_LOG
	perfLogFlushToDisk();
#endif
#if HAR_DEBUG_LAND_LOG
	landLogFlushToDisk();
#endif

	CloseLibrary((struct Library*)DOSBase);
	CloseLibrary((struct Library*)GfxBase);
	return 0;
}
