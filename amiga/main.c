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

#define HAR_BUILD_LABEL "SPRINT 15.88.5"

#define SCREEN_WIDTH 320
#define LOADING_SCREEN_WIDTH 320
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
#define SCREEN_DIWSTRT_Y 24
/* Standard PAL low-resolution display/fetch origin. Menu alignment belongs
 * in its drawing coordinates, not in DIW/DDF timing. */
/* Conservative stock-Amiga low-resolution geometry. Horizontal overscan is
 * deliberately left to COLOR00 until a separate, measured overscan renderer
 * can be introduced without changing the proven 320px scroll path. */
#define SCREEN_DIWSTRT_X 129
#define SCREEN_PLANES 5
#define SCREEN_ROW_BYTES (SCREEN_WIDTH / 8)
#define SCREEN_BPL_MOD ((SCREEN_PLANES - 1) * SCREEN_ROW_BYTES)
#define SCREEN_BITMAP_BYTES (SCREEN_HEIGHT * SCREEN_PLANES * SCREEN_ROW_BYTES)
#define LOADING_SCREEN_HEIGHT 200
#define LOADING_SCREEN_BYTES (LOADING_SCREEN_WIDTH * LOADING_SCREEN_HEIGHT * SCREEN_PLANES / 8)
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
#ifndef HAR_DEBUG_PERF_LOG
#define HAR_DEBUG_PERF_LOG 0
#endif
#define HAR_DEBUG_PERF_OVERLAY 0
#define HAR_DEBUG_HUD_GUARD 0
#ifndef HAR_DEBUG_LAND_LOG
#define HAR_DEBUG_LAND_LOG 0
#endif
#ifndef HAR_DEBUG_ENEMY_PLANE_LOG
#define HAR_DEBUG_ENEMY_PLANE_LOG 0
#endif
#ifndef HAR_ENEMY_PLANE_INTERPOLATION_PIXELS
#define HAR_ENEMY_PLANE_INTERPOLATION_PIXELS 1
#endif
#if HAR_ENEMY_PLANE_INTERPOLATION_PIXELS < 1 || HAR_ENEMY_PLANE_INTERPOLATION_PIXELS > 3
#error "HAR_ENEMY_PLANE_INTERPOLATION_PIXELS must be 1, 2 or 3"
#endif
#ifndef HAR_HEADLESS_AUTOPLAY
#define HAR_HEADLESS_AUTOPLAY 0
#endif
#ifndef HAR_HEADLESS_HIGHSCORE_TEST
#define HAR_HEADLESS_HIGHSCORE_TEST 0
#endif
#ifndef HAR_HEADLESS_CLASSIC_CONTRACT_TEST
#define HAR_HEADLESS_CLASSIC_CONTRACT_TEST 0
#endif
#ifndef HAR_HEADLESS_SKILL_LEVEL
#define HAR_HEADLESS_SKILL_LEVEL 1
#endif
#ifndef HAR_HEADLESS_MAX_FRAMES
#define HAR_HEADLESS_MAX_FRAMES 6500
#endif
#ifndef HAR_HIGHSCORE_DISK_IO
#define HAR_HIGHSCORE_DISK_IO 1
#endif
#ifndef HAR_HEADLESS_CRUISE_SPEED
#define HAR_HEADLESS_CRUISE_SPEED GAME_SPEED_LEVEL_MAX
#endif
#ifndef HAR_HEADLESS_WINGMAN_CONTROL
#define HAR_HEADLESS_WINGMAN_CONTROL WINGMAN_CONTROL_CPU
#endif
#ifndef HAR_HEADLESS_GAME_MODE
/* 0=Classic, 1=Enhanced. Kept numeric because the GameMode enum is declared
 * later in this translation unit. */
#define HAR_HEADLESS_GAME_MODE 1
#endif
#ifndef HAR_HEADLESS_ENEMY_PLANE_EXERCISE
#define HAR_HEADLESS_ENEMY_PLANE_EXERCISE 0
#endif
#ifndef HAR_HEADLESS_WINGMAN_FORMATION_EXERCISE
#define HAR_HEADLESS_WINGMAN_FORMATION_EXERCISE 0
#endif
#ifndef HAR_HEADLESS_WEAPON_STRESS
#define HAR_HEADLESS_WEAPON_STRESS 0
#endif
#ifndef HAR_HEADLESS_PAUSE_TEST
#define HAR_HEADLESS_PAUSE_TEST 0
#endif
#ifndef HAR_VALIDATION_SESSION_SEED
#define HAR_VALIDATION_SESSION_SEED 0
#endif
#define HAR_USE_PROMOTED_CPC_PLUS_ASSETS 1
#define RING_WORLD_STREAM_MAX_AHEAD_TILES 64

#define SFX_CHANNEL_COUNT 4

#define GAME_TILE_WIDTH 8
#define GAME_TILE_HEIGHT 8
#define GAME_TILE_PLANES SCREEN_PLANES
#define GAME_TILE_BYTES (GAME_TILE_HEIGHT * GAME_TILE_PLANES)
#define GAME_TILE_COUNT 102
#define GAME_MAP_WIDTH (SCREEN_WIDTH / GAME_TILE_WIDTH)
#define GAME_MAP_HEIGHT 25
#define GAME_LEVEL_BASE_WIDTH_TILES 709
#define CPC_LEVEL_DIFFICULTY_MAX 5
#define CPC_LAND_EXTENSION_PER_DIFFICULTY 256
#define CPC_LAND_MAX_EXTENSION (CPC_LEVEL_DIFFICULTY_MAX * CPC_LAND_EXTENSION_PER_DIFFICULTY)
#define GAME_LEVEL_WIDTH_TILES (GAME_LEVEL_BASE_WIDTH_TILES + CPC_LAND_MAX_EXTENSION)
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
#ifndef PERF_LOG_INTERVAL_FRAMES
#define PERF_LOG_INTERVAL_FRAMES 500
#endif
#define TELEMETRY_SAMPLE_COUNT 64
#define TELEMETRY_INTERVAL_FRAMES 100
#define TELEMETRY_GAME_EVENT_COUNT 16
#define TELEMETRY_GAME_EVENT_CODE_COUNT 31
#define GAME_SCROLL_SPEED_MIN_PIXELS 1
#define GAME_SCROLL_SPEED_MAX_PIXELS 4
#define GAME_SPEED_LEVEL_MIN 0
#define GAME_SPEED_LEVEL_MAX 15
#define GAME_SPEED_LEVEL_DEFAULT 1
#define GAME_THROTTLE_REPEAT_FRAMES 5
/* Amiga radar extension of CPC's player-height enemy-plane gate. Detection
 * is fixed point (0..1000), updated at 12.5 Hz and follows actual terrain.
 * Enhanced difficulty must lower the masking clearance rather than raise it:
 * a higher skill therefore asks the pilot to fly progressively lower. */
#define RADAR_DETECTION_MAX 1000
#define RADAR_DETECTION_ALARM_START 700
#define RADAR_DETECTION_TICK_FRAMES 4
#define RADAR_ENHANCED_MASKING_MARGIN_PIXELS 6
#define RADAR_ENHANCED_DIFFICULTY_STEP_PIXELS 2
/* Detection should be possible to shed by flying low and fast, but not be
 * erased as quickly as it is accumulated. Keep the existing altitude/speed
 * curves and bias only their final response: +10% gain and -10% drain versus
 * Sprint 15.51's shared 150-percent factor. */
#define RADAR_GAIN_RESPONSE_PERCENT 165
#define RADAR_DRAIN_RESPONSE_PERCENT 135
#define RADAR_ALARM_MIN_VOLUME 14
#define RADAR_ALARM_MAX_VOLUME 28
#define RADAR_ALARM_TONE_FRAMES 5
#define RADAR_ALARM_LOW_PERIOD 160
#define RADAR_ALARM_HIGH_PERIOD 135
/* Final carrier starts at world column 663. Begin when its first pixel reaches
 * the 320px right edge, then keep the approach moving until it sits around
 * screen x=144. CPC can move its hardware-sprite carrier independently of
 * scenery; these two scroll positions reproduce that staging for Amiga's
 * world-anchored carrier. */
#define LANDING_APPROACH_SCROLL_X (((663 + cpcLandRouteExtension + cpcTownRouteOverflow) * GAME_TILE_WIDTH) - SCREEN_WIDTH)
#define LANDING_HOVER_SCROLL_X (((663 + cpcLandRouteExtension + cpcTownRouteOverflow) * GAME_TILE_WIDTH) - 144)
#define LANDING_SLOWDOWN_REPEAT_FRAMES 3
#define LANDING_STATE_NONE 0
#define LANDING_STATE_SLOWING 1
#define LANDING_STATE_HOVER 2
#define LANDING_COMPLETE_HOLD_FRAMES 100
#define LANDING_SCORE_VALUE 200
/* CPC scrollrightfortakeoffloop scrolls the scenery while moving both landed
 * aircraft with the carrier. It does not slide Harrier by itself along a
 * stationary (or mirrored) deck. Move the final carrier from hover X=144 to
 * the opening carrier's X=64 before installing the next sortie. */
#define LANDING_RESTART_SCROLL_PIXELS 80
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
#define GAME_RUNTIME_FLAK_LOOKUP_SIZE 128
#define GAME_DESTROYED_SHIP_CELL_MAX 32
#define GAME_ENEMY_SHIP_GROUP_COUNT 2
#define GAME_SHIP_WRECK_SMOKE_MAX 24
#define GAME_SHIP_WRECK_SMOKE_TILE_A 51
#define GAME_SHIP_WRECK_SMOKE_TILE_B 52
#define GAME_LAND_CRATER_MAX 96
#define GAME_LAND_CRATER_TILE 97
#define GAME_HORIZON_TILE_Y 14
#define GAME_SEA_TOP_TILE_Y 15
/* The authored random-land route is inclusive columns 102..400: 299 columns.
 * This used to remain 295 after CPC_LAND_PROCEDURAL_WORLD_START moved from
 * 106 to 102. The last four route columns then clamped to table entry 294;
 * when that entry was a hill tile, the same slope repeated at one fixed
 * height and produced the visible pre-town terrain "wobble". CPC state 3
 * remains live until the 0x012c timer expires, then state 5 descends toward
 * row 14 one row per column (asm l91e3/checkbuildportstanley). */
#define CPC_LAND_PROCEDURAL_BASE_LENGTH 299
#define CPC_LAND_PROCEDURAL_MAX_LENGTH (CPC_LAND_PROCEDURAL_BASE_LENGTH + CPC_LAND_MAX_EXTENSION)
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
/* Exact CPC carrier composite coordinates (96x24 canvas at screen Y=96).
 * The two 16x16 superstructure pieces occupy x=40..71. The separate grey
 * Wingman is at x=73 on both carriers: the end-frigate data stream enters
 * from the opposite edge, but the completed carrier image is not mirrored. */
#define CARRIER_COMPOSITE_PIXEL_Y 96
#define CARRIER_TOWER_UPPER_LEFT 40
#define CARRIER_TOWER_UPPER_RIGHT 56
#define CARRIER_TOWER_UPPER_TOP 96
#define CARRIER_TOWER_UPPER_BOTTOM 104
#define CARRIER_TOWER_LOWER_LEFT 32
#define CARRIER_TOWER_LOWER_RIGHT 64
#define CARRIER_TOWER_LOWER_TOP 104
#define CARRIER_TOWER_LOWER_BOTTOM CARRIER_DECK_PIXEL_Y
#define CARRIER_PARKED_HARRIER_LEFT_NORMAL 73
#define CARRIER_PARKED_HARRIER_WIDTH 16
#define CARRIER_PARKED_HARRIER_TOP 104
#define CARRIER_PARKED_HARRIER_BOTTOM 112
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
/* CPC checkplayerplanemovement uses tile X = 8 + playerspeed/2. Keep that
 * source mapping available as an executable parity oracle. The Amiga target
 * below deliberately interpolates all sixteen levels across a wider screen
 * band: this is the previously approved pixel-smooth control presentation,
 * not an accidental Classic/Enhanced gameplay split. */
#define CPC_PLAYER_SPEED_TILE_X_BASE 8
#define CPC_PLAYER_SPEED_TILE_X_DIVISOR 2
#define PLAYER_SPEED_ANCHOR_X 96
#define PLAYER_SPEED_ANCHOR_STEP_PIXELS 6
/* Enhanced mode's Amiga rescue extension treats these as complete aircraft,
 * rather than abstract lives. Classic resolves to one aircraft at session
 * start and bypasses the failure/eject state machine entirely. */
#define PLAYER_START_LIVES 3
#define PLAYER_CLASSIC_LIVES 1
#define PLAYER_RESPAWN_SAFE_FRAMES 90
#define CPC_FUEL_CLOCK_HZ 300
#define CPC_FUEL_FRAME_HZ 50
#define CPC_FUEL_TIME_QUANTUM 256
#define CPC_FUEL_SUBCOUNT_FULL 14
#define CPC_FUEL_GAUGE_LEVELS 16
#define CPC_FUEL_TOTAL_QUANTA (CPC_FUEL_SUBCOUNT_FULL * CPC_FUEL_GAUGE_LEVELS)
#define CPC_FUEL_CLOCK_LIMIT (CPC_FUEL_FRAME_HZ * CPC_FUEL_TIME_QUANTUM)
#define PLAYER_CRASH_FRAMES 64
#define PLAYER_CRASH_PART_COUNT 3
#define AIRCRAFT_FAILURE_NONE 0
#define AIRCRAFT_FAILURE_DESCENT 1
#define AIRCRAFT_FAILURE_CAUSE_NONE 0
#define AIRCRAFT_FAILURE_CAUSE_FUEL 1
#define AIRCRAFT_FAILURE_CAUSE_ARMOUR 2
#define AIRCRAFT_FAILURE_CAUSE_MISSILE 3
#define AIRCRAFT_FAILURE_CAUSE_AIRCRAFT 4
#define AIRCRAFT_FAILURE_CAUSE_VOLUNTARY_EJECT 5
#define AIRCRAFT_FAILURE_FALL_START_256 96
#define AIRCRAFT_FAILURE_FALL_ACCEL_256 24
#define AIRCRAFT_FAILURE_FALL_MAX_256 640
#define AIRCRAFT_FAILURE_HORIZONTAL_PIXELS 1
#define AIRCRAFT_FAILURE_SPEED_DECAY_FRAMES 20
#define AIRCRAFT_FAILURE_ALARM_SWEEP_FRAMES 13
#define AIRCRAFT_FAILURE_ALARM_GAP_FRAMES 25
#define AIRCRAFT_FAILURE_ALARM_LOW_PERIOD 520
#define AIRCRAFT_FAILURE_ALARM_HIGH_PERIOD 300
#define AIRCRAFT_FAILURE_ALARM_VOLUME 48
#define EJECT_UPDATE_NONE 0
#define EJECT_UPDATE_CHANGED 1
#define EJECT_UPDATE_CARRIER_RESTART 2
#define PLAYER_OBJECT_COLLISION_SAFE 0

/* Sprint 15.3: CPC's wingman formation offset is "3 tiles left, 3 tiles
 * above or below" the player. WINGMAN_MOVE_FRAME_INTERVAL throttles the
 * wingman's one-tile (8px) row steps to roughly match the player's own
 * PLAYER_MOVE_SPEED_PIXELS(2)/frame - 8px every 4 frames is 100px/s either
 * way, so neither plane looks like it's cheating past the other vertically. */
#define WINGMAN_FORMATION_COLUMNS_BEHIND 3
#define WINGMAN_FORMATION_ROWS_OFFSET 3
#define WINGMAN_TAKEOFF_DECK_X 137
/* Wingman's extracted parked artwork already meets the deck at Y=103.
 * Keep this independent of the player's landing-gear sprite: that sprite
 * needs Y=104 during takeoff staging, while a completed landing continues
 * to use the separate pixel-contact result at Y=105. */
#define WINGMAN_TAKEOFF_DECK_Y 103
#define WINGMAN_TAKEOFF_MOVE_PIXELS 2
#define WINGMAN_MOVE_FRAME_INTERVAL 4
#define WINGMAN_VISUAL_MOVE_PIXELS 2
#define WINGMAN_MAX_ROW ((GAME_WORLD_HEIGHT / GAME_TILE_HEIGHT) - 1)

/* Wingman interception constants, matching
 * HarrierAttackSourceNew2...asm's wingman intercept routines:
 * - WINGMAN_INTERCEPT_CHANCE_MASK: asm:6513-6516 rolls `r & 1` on every new
 *   enemy-plane spawn to decide whether to break formation and intercept.
 *   CPC forces the intercept when the enemy targets Wingman
 *   (asm:6504-6509); this is represented by interceptReason=2.
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
#define WINGMAN_INTERCEPT_FIRST_PASS_COLUMNS 6
#define WINGMAN_EVASION_DIRECTION_COUNT 8
#define WINGMAN_SAFE_CELL_CACHE_SIZE 32
#define WINGMAN_BOMB_CHANCE_MASK 3
#define WINGMAN_BOMB_HEIGHT_TILES 4
#define WINGMAN_BOMB_LEAD_TILES 5
#define WINGMAN_LANDING_WAYPOINT_Y 72
#define WINGMAN_LANDING_MOVE_PIXELS 2
#define WINGMAN_LANDING_DESCEND_PIXELS 1
/* CPC wingman1stwaypoint is X=&18 and the two deck positions are X=&1d
 * (default) and X=&16 (alternate when Player 1 occupies the default pad).
 * The CPC carrier's accepted deck begins at X=&15, so retain those exact
 * relative offsets while the Amiga presentation remains pixel-smooth. */
#define WINGMAN_LANDING_APPROACH_DECK_OFFSET (3 * GAME_TILE_WIDTH)
#define WINGMAN_LANDING_DEFAULT_DECK_OFFSET (8 * GAME_TILE_WIDTH)
#define WINGMAN_LANDING_ALTERNATE_DECK_OFFSET (1 * GAME_TILE_WIDTH)
#define PLAYER_OBJECT_COLLISION_FATAL 1
#define PLAYER_OBJECT_COLLISION_FLAK 2
#define PLAYER_OBJECT_COLLISION_SMOKE 3
#define PLAYER_FRIGATE_STATUS_CLEAR 0
#define PLAYER_FRIGATE_STATUS_HIT 1
#define PLAYER_FRIGATE_STATUS_SERVICED 2

#define ENGINE_CHANNEL 3
#define ENGINE_BUFFER_BYTES 2048
#define ENGINE_MUTATE_BYTES 48

/* Lightweight Amiga-only atmosphere.  None of these values feed gameplay:
 * positions use their own fixed hashes/LFSRs and the Paula voice always has
 * lower priority than weapons, impacts, player cues and the engine. */
#define SEA_AMBIENCE_BUFFER_BYTES 4096
#define SEA_AMBIENCE_PERIOD 430
#define SEA_AMBIENCE_IDLE_VOLUME 11
#define SEA_AMBIENCE_FLIGHT_VOLUME 5
#define SEA_AMBIENCE_VOLUME_STEP_FRAMES 2

#define SEA_WAVE_MAX 16
#define SEA_WAVE_WIDTH 8
#define SEA_WAVE_HEIGHT 2
#define SEA_WAVE_MAX_PLACEMENTS 2
#define SEA_WAVE_MAX_BYTES_PER_ROW 2
#define SEA_WAVE_PHASE_FRAMES 6

#define CARRIER_GULL_MAX 3
#define CARRIER_GULL_WIDTH 16
#define CARRIER_GULL_HEIGHT 9
#define CARRIER_GULL_MAX_PLACEMENTS 2
#define CARRIER_GULL_MAX_BYTES_PER_ROW 3
#define CARRIER_GULL_IDLE_DELAY_FRAMES 100
#define CARRIER_GULL_SPAWN_INTERVAL_MIN_FRAMES 60
#define CARRIER_GULL_SPAWN_INTERVAL_VARIATION 127
#define CARRIER_GULL_CRUISE_X256 77
#define CARRIER_GULL_CRUISE_Y256 14
#define CARRIER_GULL_FLAP_PHASE_FRAMES 9

#define WEAPON_SPRITE_HEIGHT 8
#define AUXILIARY_SPRITE_WORDS (2 + WEAPON_SPRITE_HEIGHT * 2 + 2)
#define ROCKET_SPEED_PIXELS 7
#define ROCKET_RANGE_MIN_TILES 10
#define ROCKET_RANGE_MAX_TILES 20
#define ROCKET_RANGE_DEFAULT_TILES 10
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
#define BOMB_SPEED_Y_PIXELS 2
/* Five vertical pixels per two updates (2.5px/update).  The Harrier can
 * descend by PLAYER_MOVE_SPEED_PIXELS (2) each update, so the bomb must be
 * strictly faster or the player can visibly catch it while diving.  Alternating
 * 2/3 physical pixels keeps the mini-Bob pixel-smooth without tile jumps. */
#define BOMB_EXTRA_FALL_INTERVAL 2
#define BOMB_EXTRA_FALL_PIXELS 1
#if (BOMB_SPEED_Y_PIXELS * BOMB_EXTRA_FALL_INTERVAL + BOMB_EXTRA_FALL_PIXELS) <= \
	(PLAYER_MOVE_SPEED_PIXELS * BOMB_EXTRA_FALL_INTERVAL)
#error "Bomb must fall faster than the Harrier can descend"
#endif
/* CPC dolaunchbomb performs one immediate downward character move, then
 * consumes bombmomentum=3 over three more downward moves while keeping the
 * same screen X against scenery scroll. Once status changes to descent,
 * decreasebombheight decrements screen X with the scroll and advances Y.
 * Thus the first four steps gain world X only from scrolling; later steps
 * retain a fixed world X. */
#define BOMB_MOMENTUM_LOGICAL_STEPS 4
#define BOMB_FORWARD_MOMENTUM_FRAMES BOMB_MOMENTUM_LOGICAL_STEPS
/* Five half-pixels per PAL frame gives the already-approved 2/3-pixel visual
 * cadence (2.5 px/frame). CPC state still advances in complete 8-pixel rows;
 * visible mini-BOB contact is sampled every PAL frame. */
#define BOMB_HALF_PIXELS_PER_FRAME 5
#define BOMB_LOGICAL_STEP_PIXELS GAME_TILE_WIDTH
#define BOMB_LAUNCH_COOLDOWN_FRAMES 18
#define BOMB_IMPACT_SFX_GRACE_FRAMES 8
#define IMPACT_FRAMES 12
#define WATER_SPLASH_FRAME_TICKS 25
#define WATER_SPLASH_FRAMES (WATER_SPLASH_FRAME_TICKS * 2)
#define IMPACT_TYPE_EXPLOSION 0
#define IMPACT_TYPE_WATER_SPLASH 1
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
/* Both profiles use CPC's five-tick/eight-pixel logical descent with smooth
 * Amiga presentation between rows. */
#define POWERUP_LOGICAL_STEP_FRAMES 5
#define POWERUP_SPAWN_COLUMN 38
#define POWERUP_SPAWN_MAX_ROW 3
#define POWERUP_DESPAWN_LEFT_X (-16)
#define POWERUP_ALTITUDE_FLOOR_BASE 11
#define POWERUP_ROCKET_REFILL 16
#define POWERUP_BOMB_REFILL 16
#define POWERUP_SPAWN_ROLL_MASK 0x0f
#define POWERUP_PICKUP_SCORE_VALUE 0
#define POWERUP_EXTRA_AIRCRAFT_SCORE 2000
#define POWERUP_EXTRA_AIRCRAFT_SCROLL_X (LANDING_APPROACH_SCROLL_X - 320)
#define PLAYER_MAX_AIRCRAFT 9

#define ENEMY_SPRITE_WIDTH 16
#define ENEMY_SPRITE_HEIGHT 8
#define ENEMY_SPRITE_WORDS (2 + ENEMY_SPRITE_HEIGHT * 2 + 2)
#define ENEMY_RESPAWN_FRAMES 72
#define ENEMY_PLANE_DAMAGE_NORMAL 0
#define ENEMY_PLANE_DAMAGE_HIT 1
#define ENEMY_PLANE_DAMAGE_BROKEN 2
/* The CPC exposes the broken-plane state for one comparatively expensive
 * game update.  One 50 Hz Amiga frame is too brief to read, so retain that
 * same state for a short presentation-only hold (about 0.24 seconds). */
#define ENEMY_PLANE_BROKEN_HOLD_FRAMES 12
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
/* CPC heatseekposition stops changing altitude once the missile is fewer
 * than five character cells ahead of its selected target.  That dead zone is
 * what lets a late climb/dive evade the shot; horizontal travel continues. */
#define ENEMY_MISSILE_HOMING_CUTOFF_PIXELS (5 * GAME_TILE_WIDTH)
#define ENEMY_TARGET_NONE 0
#define ENEMY_TARGET_PLAYER 1
#define ENEMY_TARGET_WINGMAN 2
#define ENEMY_CPC_FIRE_RANGE_TILES 10
#define ENEMY_CPC_SPAWN_SCREEN_X (0x26 * GAME_TILE_WIDTH)
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
#define HUD_COLOR_POWERUP_ROCKETS GAME_COLOR_POWERUP_BLUE
#define HUD_COLOR_POWERUP_BOMBS GAME_COLOR_POWERUP_GREEN

#define FONT_WIDTH 8
#define FONT_HEIGHT 8
#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR 127
#define TELEMETRY_FONT_WIDTH 4
#define TELEMETRY_FONT_HEIGHT 5

#define MENU_ITEM_START 0
#define MENU_ITEM_SKILL 1
/* Classic keeps CPC gameplay rules; Enhanced enables intentional Amiga
 * extensions such as three aircraft, failure/eject/rescue, Player 2 and the
 * terrain-relative accumulating radar. */
#define MENU_ITEM_GAME_MODE 2
/* Sprint 15.1: real Off/CPU/PLAYER 2 wingman control setting, replacing the
 * old static "Wingman: Off" status line in drawMenuRightSettings() (which
 * never did anything). */
#define MENU_ITEM_WINGMAN 3
#define MENU_ITEM_LOCK_HEIGHT 4
#define MENU_ITEM_ROCKET_RANGE 5
#define MENU_ITEM_CONTROLS 6
#define MENU_ITEM_EXIT_DOS 7
#define MENU_ITEM_COUNT 8
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
#define MENU_TICKER_LEAD_PIXELS SCREEN_WIDTH
/* Keep the editable ASCII text before the size macros: sizeof() is a compile-
 * time constant, so the backing bitplane automatically grows when the text
 * in harrier_menu_text.h gets longer. Besides one complete cycle, Copper
 * needs another visible screen width plus 16px fine-scroll prefetch. The old
 * fixed 2048px buffer was shorter than a user-edited 2232px cycle, causing
 * Copper to display adjacent memory as garbage at the end of the ticker. */
static const char menuTickerText[] = HAR_TEXT_TRIBUTE;
static const char fieldGuideTickerClassicText[] = HAR_TEXT_FIELD_GUIDE_CLASSIC;
static const char fieldGuideTickerEnhancedText[] = HAR_TEXT_FIELD_GUIDE_ENHANCED;
#define MENU_TICKER_MAX_CHARS \
	((sizeof(menuTickerText) > sizeof(fieldGuideTickerEnhancedText) \
		? sizeof(menuTickerText) : sizeof(fieldGuideTickerEnhancedText)) - 1)
#define MENU_TICKER_TEXT_WIDTH (MENU_TICKER_MAX_CHARS * FONT_WIDTH)
#define MENU_TICKER_REQUIRED_WIDTH \
	(MENU_TICKER_MARGIN_PIXELS + MENU_TICKER_TEXT_WIDTH + \
	 MENU_TICKER_GAP_PIXELS + SCREEN_WIDTH + 16)
#define MENU_TICKER_SOURCE_WIDTH \
	((MENU_TICKER_REQUIRED_WIDTH + 15) & ~15)
#define MENU_TICKER_ROW_BYTES (MENU_TICKER_SOURCE_WIDTH / 8)
#define MENU_TICKER_BITMAP_BYTES \
	(FONT_HEIGHT * SCREEN_PLANES * MENU_TICKER_ROW_BYTES)

/* The menu and Field Guide each own one editable ticker. A screen changes
 * only after its final glyph has left the display, rather than after a fixed
 * wall-clock timeout. The backing bitmap is sized for the longer string. */
static UWORD menuTickerScrollX = 0;
static UBYTE menuTickerFrameDivider = 0;
static UBYTE menuTickerCompleted = 0;
static const char* activeMenuTickerText = menuTickerText;
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
/* Stable playfield colours for pickup canopies and their matching HUD bars.
 * Unlike LAND (5) and SEA (15), Copper never repoints these registers between
 * the campaign's day/dusk/night/dawn phases. */
#define GAME_COLOR_POWERUP_GREEN 8
#define GAME_COLOR_POWERUP_BLUE 14
/* Real CPC Mode 1 per-band copper palette (re-derived after the game tile
 * assets were found to have been extracted as Mode 0 instead of Mode 1 and
 * re-extracted correctly) - COLOR00/15 change across the 4 screen bands:
 * upper sky, mid sky, lower play area, instrument panel. COLOR05 (land)
 * follows CPC's campaign phase in the world and is restored before HUD DMA
 * because it doubles as HUD_COLOR_SAFE. COLOR10 (black) never changes.
 * COLOR15 doubles
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
#define GAME_SKY_TOP_DUSK_RGB 0x0a36
#define GAME_SKY_MID_DUSK_RGB 0x0b47
#define GAME_SKY_LOW_DUSK_RGB 0x0c58
#define GAME_SKY_TOP_CLOUD_DUSK_RGB 0x0dfa
#define GAME_SKY_LOW_SEA_DUSK_RGB GAME_SKY_LOW_SEA_RGB
#define GAME_HUD_PANEL_SEA_DUSK_RGB GAME_HUD_PANEL_SEA_RGB
/* CPC advances its day/dusk/night/dawn palette sequence twice per completed
 * route: once at the next takeoff and once when Port Stanley is built.
 * Mission 2 therefore opens at night (the green cast in the CPC reference),
 * then moves toward dawn in town; mission 3 returns to day. These values are
 * Amiga 12-bit approximations. Sea and HUD colours deliberately stay fixed. */
#define GAME_SKY_TOP_NIGHT_RGB 0x0031
#define GAME_SKY_MID_NIGHT_RGB 0x0042
#define GAME_SKY_LOW_NIGHT_RGB 0x0053
#define GAME_SKY_TOP_CLOUD_NIGHT_RGB 0x0aa5
#define GAME_SKY_TOP_DAWN_RGB 0x0565
#define GAME_SKY_MID_DAWN_RGB 0x0576
#define GAME_SKY_LOW_DAWN_RGB 0x0587
#define GAME_SKY_TOP_CLOUD_DAWN_RGB 0x0dfa
#define GAME_LAND_DAY_RGB 0x06a0
#define GAME_LAND_DUSK_RGB 0x0650
#define GAME_LAND_NIGHT_RGB 0x0010
#define GAME_LAND_DAWN_RGB 0x0650
#define MISSION_PALETTE_DAY 0
#define MISSION_PALETTE_DUSK 1
#define MISSION_PALETTE_NIGHT 2
#define MISSION_PALETTE_DAWN 3
#define MISSION_PALETTE_SKY_TOP 0
#define MISSION_PALETTE_SKY_MID 1
#define MISSION_PALETTE_SKY_LOW 2
#define MISSION_PALETTE_CLOUD 3
#define MISSION_PALETTE_LAND 4

#define RAWKEY_E 0x12
#define RAWKEY_W 0x11
#define RAWKEY_A 0x20
#define RAWKEY_S 0x21
#define RAWKEY_D 0x22
#define RAWKEY_P 0x19
#define RAWKEY_R 0x13
#define RAWKEY_B 0x35
#define RAWKEY_SPACE 0x40
#define RAWKEY_BACKSPACE 0x41
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
#define RAWKEY_NONE 0xff
/* Numeric keypad - Player 2's keyboard fallback (see ReadPlayer2Input()),
 * kept clear of every key Player 1 already uses. Standard Amiga keymap raw
 * codes. */
#define RAWKEY_KP_0 0x0f
#define RAWKEY_KP_2 0x1e
#define RAWKEY_KP_4 0x2d
#define RAWKEY_KP_6 0x2f
#define RAWKEY_KP_8 0x3e
#define RAWKEY_KP_ENTER 0x43
#define RAWKEY_KP_DECIMAL 0x3c

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
 * those words; resetCityFade() selects the campaign mission's day or night
 * start phase so stale colours can never leak between sessions. */
static UWORD* activeCopperSkyTopColor = 0;
static UWORD* activeCopperSkyMidColor = 0;
static UWORD* activeCopperSkyLowColor = 0;
static UWORD* activeCopperCloudTopColor = 0;
static UWORD* activeCopperLandColor = 0;
static UWORD* activeCopperSeaLowColor = 0;
static UWORD* activeCopperPanelSeaColor = 0;
static UWORD currentSkyTopRgb = GAME_SKY_TOP_RGB;
static UWORD currentSkyMidRgb = GAME_SKY_MID_RGB;
static UWORD currentSkyLowRgb = GAME_SKY_LOW_RGB;
static UWORD currentCloudTopRgb = GAME_SKY_TOP_CLOUD_RGB;
static UWORD currentLandRgb = GAME_LAND_DAY_RGB;
static UWORD currentSeaLowRgb = GAME_SKY_LOW_SEA_RGB;
static UWORD currentPanelSeaRgb = GAME_HUD_PANEL_SEA_RGB;
static UBYTE currentMissionOpeningPalettePhase = MISSION_PALETTE_DAY;
static UBYTE currentMissionFlightPalettePhase = MISSION_PALETTE_DAY;
static UBYTE currentMissionTownPalettePhase = MISSION_PALETTE_DUSK;

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
	/* Keep each physical bomb source separate through edge detection. A
	 * floating/stuck POT line must not hide a fresh Space or mouse press. */
	UBYTE bombKey;
	UBYTE bombMouse;
	UBYTE bombJoystick;
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

/* Sprint 15.28: Wingman: Player 2's own controller - deliberately separate
 * from InputState rather than extra fields bolted onto it, since it reads a
 * different physical port and only ever matters while wingmanControl ==
 * WINGMAN_CONTROL_PLAYER2. Has a numeric-keypad keyboard fallback (see
 * ReadPlayer2Input()) for the same reason Player 1 does - a second physical
 * joystick isn't something every player (or test setup) has to hand. */
typedef struct Player2InputState {
	UBYTE up;
	UBYTE down;
	UBYTE left;
	UBYTE right;
	UBYTE fire;
	UBYTE bomb;
	UBYTE eject;
} Player2InputState;

enum ControlAction {
	CONTROL_UP = 0,
	CONTROL_DOWN,
	CONTROL_LEFT,
	CONTROL_RIGHT,
	CONTROL_ROCKET,
	CONTROL_BOMB,
	CONTROL_EJECT,
	CONTROL_ACTION_COUNT
};

enum ControlJoystickPort {
	CONTROL_JOY_OFF = 0,
	CONTROL_JOY_PORT_1,
	CONTROL_JOY_PORT_2
};

enum ControlJoystickButton {
	CONTROL_BUTTON_NONE = 0,
	CONTROL_BUTTON_PRIMARY,
	CONTROL_BUTTON_SECONDARY
};

typedef struct ControlProfile {
	UBYTE key[CONTROL_ACTION_COUNT];
	UBYTE joystickPort;
	UBYTE rocketButton;
	UBYTE bombButton;
} ControlProfile;

static ControlProfile controlProfiles[2];

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

typedef struct TelemetryGameEvent {
	UWORD frame;
	UWORD worldColumn;
	UWORD value;
	WORD playerX;
	WORD playerY;
	UWORD fuel;
	UWORD armour;
	UWORD randomState;
	UBYTE code;
	UBYTE reason;
	UBYTE playerRow;
	UBYTE skill;
	UBYTE speed;
	UBYTE gameMode;
	UBYTE repeats;
} TelemetryGameEvent;

enum TelemetryGameEventCode {
	TELEMETRY_GAME_EVENT_NONE = 0,
	TELEMETRY_GAME_EVENT_ENEMY_SPAWN_OK = 1,
	TELEMETRY_GAME_EVENT_ENEMY_SPAWN_NO = 2,
	TELEMETRY_GAME_EVENT_ENEMY_MISSILE = 3,
	TELEMETRY_GAME_EVENT_WING_INTERCEPT_OK = 4,
	TELEMETRY_GAME_EVENT_WING_INTERCEPT_NO = 5,
	TELEMETRY_GAME_EVENT_WING_BOMB_OK = 6,
	TELEMETRY_GAME_EVENT_WING_BOMB_NO = 7,
	TELEMETRY_GAME_EVENT_P2_LEFT_BEHIND = 8,
	TELEMETRY_GAME_EVENT_WING_POWERUP = 9,
	TELEMETRY_GAME_EVENT_TERRAIN_STATE = 10,
	TELEMETRY_GAME_EVENT_CITY_TO_PIER = 11,
	TELEMETRY_GAME_EVENT_ENEMY_PLANE_BLOCKED = 12,
	TELEMETRY_GAME_EVENT_AIRCRAFT_FAILURE = 13,
	TELEMETRY_GAME_EVENT_AIRCRAFT_EJECT = 14,
	TELEMETRY_GAME_EVENT_AIRCRAFT_IMPACT = 15,
	TELEMETRY_GAME_EVENT_AIRCRAFT_RESCUED = 16,
	TELEMETRY_GAME_EVENT_PLAYER_MOVE_LIMIT = 17,
	TELEMETRY_GAME_EVENT_PLAYER_SPEED_CHANGE = 18,
	TELEMETRY_GAME_EVENT_ROCKET_FIRE = 19,
	TELEMETRY_GAME_EVENT_BOMB_RELEASE = 20,
	TELEMETRY_GAME_EVENT_MAVERICK_LOCK = 21,
	TELEMETRY_GAME_EVENT_MAVERICK_LOCK_LOST = 22,
	TELEMETRY_GAME_EVENT_PLAYER_FLAK_HIT = 23,
	TELEMETRY_GAME_EVENT_PLAYER_MISSILE_HIT = 24,
	TELEMETRY_GAME_EVENT_PLAYER_COLLISION = 25,
	TELEMETRY_GAME_EVENT_PLAYER_CRASH = 26,
	TELEMETRY_GAME_EVENT_PLAYER_RESPAWN = 27,
	TELEMETRY_GAME_EVENT_LANDING_START = 28,
	TELEMETRY_GAME_EVENT_LANDING_COMPLETE = 29,
	TELEMETRY_GAME_EVENT_WINGMAN_REVIVED = 30
};

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
 * fallCounter is the shared presentation phase which interpolates between
 * CPC logical rows in both profiles.
 * spawnId tracks position in
 * the deterministic 1..6 type-rotation sequence (1=health, 2=rockets,
 * 3=bombs, 4=rockets, 5=bombs, 6=skip-and-spawn-enemy-plane). */
typedef struct {
	UBYTE active;
	UBYTE type;
	LONG worldX;
	WORD y;             /* interpolated display Y */
	WORD logicalY;      /* CPC collision row; advances 8px every five ticks */
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

/* CPC's menu-time wingmanon setting: Off, CPU formation AI or Player 2. */
typedef enum WingmanControl {
	WINGMAN_CONTROL_OFF = 0,
	WINGMAN_CONTROL_CPU = 1,
	WINGMAN_CONTROL_PLAYER2 = 2
} WingmanControl;

typedef enum GameMode {
	GAME_MODE_CLASSIC = 0,
	GAME_MODE_ENHANCED = 1
} GameMode;

/* Named form of CPC's raw wingmantakeoff state values (see
 * the Sprint 15 roadmap in AMIGA_PORT_PLAN.md for the full ASM-side mapping).
 * Raw CPC value 4 ("documented as landed, but the code normally resets it to
 * 0") is deliberately not represented because CPC never leaves it set. */
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
	WINGMAN_TAKEOFF = 13,
	/* Sprint 15.28: free 4-direction human flight under Wingman: Player 2 -
	 * CPC's checkwingmankeys (asm:2664-2711), an entirely separate control
	 * path from every CPU AI state above, not an overlay on top of one. */
	WINGMAN_PLAYER2_FLIGHT = 14
} WingmanMode;

/* Sprint 15.3: the wingman's own flight state. Deliberately holds only what
 * CPU formation flight needs so far - weapons (own WeaponState rocket,
 * per the roadmap's "explicit struct instances, not a reused/re-pointed
 * shared block" decision) and AI/landing fields arrive with the sprints that
 * actually use them, not ahead of time.
 *
 * Logical formation movement is tile-grid-locked in both axes - matching
 * CPC's 0..8 direction table - while screenX/screenY interpolate the accepted
 * cells for an Amiga-smooth presentation.
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
	WORD formationLogicalX;     /* CPC 8px formation coordinate; presentation glides toward it */
	UBYTE moveTimer;             /* throttles row movement to roughly the player's own vertical speed */
	/* Formation safety depends on tile coordinates, not sub-tile pixels.
	 * Reuse it until either the visible world column or player tile row
	 * changes; the old per-frame recomputation performed dozens of expensive
	 * object-map queries while all inputs were identical. */
	UWORD formationSafetyColumn;
	WORD formationSafetyPlayerRow;
	WORD formationSafetyTargetRow;
	UBYTE formationSafetyValid;

	/* Sprint 15.47: interception AI keeps CPC's three decisions explicit:
	 * fixed first-pass waypoint, live enemy tracking and fire. screenOffsetX
	 * is the wingman's own screen-space X throughout those states; formation
	 * mode derives X directly from the player's current position. */
	WORD interceptScreenX;
	WORD interceptWaypointX;    /* frozen CPC state-5 first-pass waypoint */
	WORD interceptWaypointRow;
	UBYTE interceptReason;      /* 1 = voluntary, 2 = enemy targeted Wingman */
	UBYTE evasionCursor;        /* advances bounded R&7 obstacle choices */
	WeaponState rocket;          /* wingman's own missile - real CPC always fires the
	                              * plain (non-Maverick) missile from interception,
	                              * infinite supply since he's a powerup */
	WeaponState bomb;
	UBYTE bombHalfPixelPhase;
	UBYTE bombStepPixels;
	UBYTE bombMomentumSteps;
	LONG bombTargetWorldX;
	WORD bombTargetY;
	LONG lastBombTargetColumn;
	WORD landingTargetX;
	UBYTE landingTargetSet;

	/* When the chase ends (fired, or the enemy plane
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
	UBYTE gameMode;
	ULONG score;
	ULONG bonusScore;
	ULONG missionStartScore;
	UWORD fuel;
	/* CPC timercountdown uses KL TIME's 300 Hz clock. Every 256 ticks it
	 * decrements a 14-count divider; exhausting 16 gauge levels empties the
	 * tank. Keep that exact integer model and map it onto the Amiga HUD's
	 * existing 0..999 value. */
	UWORD fuelClockAccumulator;
	UBYTE fuelSubCounter;
	UBYTE fuelGaugeLevel;
	UWORD armour;
	UBYTE gameOver;
	/* A completed run may remain on the Game Over screen for many frames.
	 * A qualifying CPC score first enters the six-character name editor, then
	 * commits to the in-memory table exactly once. Disk I/O is deferred to a
	 * safe AmigaDOS window when leaving that screen. */
	UBYTE highScoreCommitted;
	UBYTE highScoreNameEntryActive;
	UBYTE highScoreNameLength;
	UWORD highScoreNameKeySerial;
	char highScoreNameJoyChar;
	char highScoreName[7];
	UBYTE missionComplete;
	UWORD missionCompleteTimer;
	UBYTE postLandingSlide;
	UBYTE landingState;
	UBYTE takeoffState;
	UBYTE lives;
	UBYTE respawnSafeTimer;
	UBYTE flakDamageCount;
	UBYTE smokeDamageContact;
	UBYTE playerFrigateStatus;
	UBYTE rockets;
	UBYTE bombs;
	UBYTE rocketHeightLock;      /* CPC lockinmissileheighttoplayer menu option */
	UBYTE rocketRangeTiles;      /* CPC sidewinder range menu: 10..20 character cells */
	WeaponState rocketShot;
	WeaponState bombShot;
	LONG bombLogicalWorldX;
	WORD bombLogicalY;
	UBYTE bombHalfPixelPhase;
	UBYTE bombStepPixels;
	UBYTE bombMomentumSteps;
	WeaponState impact;
	WeaponState enemyPlane;
	WeaponState enemyMissile;
	WeaponState crashPart[PLAYER_CRASH_PART_COUNT];
	UBYTE enemyRespawnTimer;
	/* Classic's Z80 R roll belongs to the CPC gameplay cadence, not to map
	 * columns.  Keep its phase/RNG wholly separate from Enhanced radar. */
	UBYTE classicEnemySpawnPhase;
	UWORD classicEnemySpawnRandomState;
	UWORD radarDetection;
	UBYTE radarClearance;
	UBYTE radarThreshold;
	UBYTE enemyShipMissileTriggerIndex;
	UBYTE enemyMissileFromShip;
	UBYTE enemyMissileTarget;   /* 1 = player, 2 = Wingman; CPC missiletargetwingman */
	/* CPC enemyplanestatus: once the plane reaches firing range it commits
	 * to firing at most once (asm:6301-6303, "IF WE HAVE LAUNCHED MISSILE
	 * ALREADY, SKIP THIS" gates the actual missile spawn, but the plane's
	 * OWN status still advances to "FIRED MISSILE" the instant range closes,
	 * regardless of whether a missile happened to be free to launch) and
	 * immediately climbs straight up and away instead of continuing to
	 * chase the target's altitude (enemyplaneretreatafterfire, asm:6610-
	 * 6632) until it exits the top of the screen. */
	UBYTE enemyPlaneRetreating;
	/* CPC enemyplanestatus 3/4: a weapon hit is followed by one broken-plane
	 * update before the aircraft is removed. Approach/retreat remain separate
	 * because they control movement before the hit. */
	UBYTE enemyPlaneDamageState;
	UBYTE enemyPlaneBrokenTimer;
	/* 0..7 fixed-point phase. Adding the selected 1x/2x/3x pixel rate
	 * schedules one CPC 8-pixel decision whenever the accumulator crosses 8. */
	UBYTE enemyPlaneLogicPhase;
	UBYTE crashTimer;
	UBYTE crashEndsGame;
	UBYTE aircraftFailureState;
	UBYTE aircraftFailureCause;
	UWORD aircraftFailureTimer;
	UWORD aircraftFailureFallSpeed256;
	LONG aircraftFailureY256;
	UBYTE aircraftFailureAlarmFrame;
	UBYTE abandonedAircraftActive;
	UBYTE abandonedAircraftCrash;
	UBYTE ejectState; /* 0=inactive, 1=seat, 2=parachute, 3=landed/waiting */
	UBYTE ejectTimer;
	WORD ejectX;
	WORD ejectY;
	UBYTE throttleRepeatTimer;
	UBYTE bombLaunchCooldown;
	UBYTE skillLevel;
	UBYTE levelDifficulty; /* CPC leveldifficulty: selected skill + completed boards, capped at 5 */
	UBYTE missionNumber;
	UBYTE extraAircraftBonusSpawned;
	UBYTE takeoffPaletteFadeStep;
	UBYTE takeoffPaletteFadeTimer;
	UBYTE cityFadeStep;
	UBYTE cityFadeTimer;
	UWORD hitsCount;
	TargetLock targetLock;
	PowerupState powerup;
	UBYTE wingmanControl;
	WingmanState wingman;
} GameState;

/* Gameplay policy lives here rather than being rediscovered at individual
 * call sites. CPC rules are the default; a true result for an Enhanced
 * helper names an intentional Amiga gameplay extension. */
static UBYTE gameplayUsesRadar(const GameState* game) {
	return game->gameMode == GAME_MODE_ENHANCED;
}

static UBYTE enhancedRadarClearanceThreshold(const GameState* game) {
	/* Preserve the tuned skill-1 boundary (18px belly clearance), then make
	 * each difficulty step two pixels stricter. The former (2+difficulty)*8
	 * expression moved in the opposite direction and made skill 3 substantially
	 * easier than skill 1. Classic never calls this accumulator. */
	UBYTE difficulty = game->levelDifficulty;
	if (difficulty < 1)
		difficulty = 1;
	else if (difficulty > 5)
		difficulty = 5;
	WORD threshold = (WORD)(3 * GAME_TILE_HEIGHT -
		RADAR_ENHANCED_MASKING_MARGIN_PIXELS) -
		(WORD)(difficulty - 1) * RADAR_ENHANCED_DIFFICULTY_STEP_PIXELS;
	return threshold > 0 ? (UBYTE)threshold : 1;
}

static UBYTE gameplayUsesEnhancedFailure(const GameState* game) {
	return game->gameMode == GAME_MODE_ENHANCED;
}

static UBYTE gameplayUsesSafeRespawn(const GameState* game) {
	return game->gameMode == GAME_MODE_ENHANCED;
}

static UBYTE gameplayStartingAircraft(const GameState* game) {
	return game->gameMode == GAME_MODE_CLASSIC ?
		PLAYER_CLASSIC_LIVES : PLAYER_START_LIVES;
}

static UBYTE gameplayUsesCpcCollisionRules(const GameState* game) {
	(void)game;
	/* Pixel-smooth presentation is shared, but the logical CPC collision
	 * result is the baseline in both modes. No Enhanced exception has been
	 * approved for collision lethality or object classes. */
	return 1;
}

static UBYTE gameplayUsesCpcEjectRules(const GameState* game) {
	return game->gameMode == GAME_MODE_CLASSIC;
}

static void telemetryLogGameEvent(UBYTE code, UBYTE reason,
	UWORD worldColumn, const GameState* game, UWORD value);

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
#define CARRIER_IDLE_VOLUME 29
#define AUDIO_MIX_PERCENT 85
#define AUDIO_MIX_VOLUME(volume) \
	((UWORD)((((ULONG)(volume) * AUDIO_MIX_PERCENT) + 50UL) / 100UL))
#define CARRIER_IDLE_DECODE_BUFFER_BYTES 44100
#define CARRIER_IDLE_PCM_EDGE_FADE_SAMPLES 1024
#define CARRIER_IDLE_DECODE_BYTES_PER_FRAME 48
#define CARRIER_IDLE_FADE_PAIRS_PER_FRAME 64
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
static UBYTE lastKeyboardMakeKey = RAWKEY_NONE;
static UWORD keyboardMakeSerial = 0;
static UBYTE sfxChannelFrames[SFX_CHANNEL_COUNT];
static UBYTE sfxChannelStartDelay[SFX_CHANNEL_COUNT];
static UBYTE sfxChannelSilenceQueueDelay[SFX_CHANNEL_COUNT];
static UBYTE sfxChannelPendingId[SFX_CHANNEL_COUNT];
static UBYTE sfxChannelCurrentId[SFX_CHANNEL_COUNT];
static UBYTE sfxChannelPriority[SFX_CHANNEL_COUNT];
static UWORD sfxChannelPendingPeriod[SFX_CHANNEL_COUNT];
static UWORD sfxChannelPendingVolume[SFX_CHANNEL_COUNT];
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
static UBYTE carrierIdleNextVariant = 0xff;
static UBYTE carrierIdlePreparedVariant = 0xff;
static UWORD carrierIdlePreparedLength = 0;
static UBYTE carrierIdleDecodeActive = 0;
static UBYTE carrierIdleDecodeFading = 0;
static const UBYTE* carrierIdleDecodeSource = 0;
static ULONG carrierIdleDecodeSourceLength = 0;
static ULONG carrierIdleDecodeInputIndex = 0;
static ULONG carrierIdleDecodeOutputIndex = 0;
static ULONG carrierIdleDecodeTargetLength = 0;
static ULONG carrierIdleDecodeFadeIndex = 0;
static LONG carrierIdleDecodePredictor = 0;
static WORD carrierIdleDecodeStepIndex = 0;
static UBYTE modPlaying = 0;
/* CPC menu default is ON. This session setting is copied into GameState at
 * mission start so ordinary rockets can either follow the current Harrier Y
 * or retain their launch height. Maverick guidance is always independent. */
static UBYTE menuRocketHeightLock = 1;
/* CPC rocketrange/rocketrange2a are patched together by the menu. Keep one
 * session setting and copy it into GameState when a mission starts. */
static UBYTE menuRocketRangeTiles = ROCKET_RANGE_DEFAULT_TILES;
static UWORD ringWorldLastStreamedColumn = 0;
static LONG ringStreamColumn = -1;
static UWORD ringStreamRow = 0;
static UWORD destroyedTargetColumns[GAME_DESTROYED_TARGET_MAX];
static UBYTE destroyedTargetCount = 0;
static UWORD runtimeFlakColumns[GAME_RUNTIME_FLAK_MAX];
static UBYTE runtimeFlakRows[GAME_RUNTIME_FLAK_MAX];
static UBYTE runtimeFlakTiles[GAME_RUNTIME_FLAK_MAX];
static UBYTE runtimeFlakCount = 0;
/* Direct rendering/collision lookup. The retained flak window spans fewer
 * than 128 columns, so the low-seven-bit slot is unambiguous after verifying
 * the full column tag. The compact list above remains authoritative for
 * pruning and preserves the existing 64-entry gameplay cap. */
static UWORD runtimeFlakLookupColumns[GAME_RUNTIME_FLAK_LOOKUP_SIZE];
static UBYTE runtimeFlakLookupRows[GAME_RUNTIME_FLAK_LOOKUP_SIZE];
static UBYTE runtimeFlakLookupTiles[GAME_RUNTIME_FLAK_LOOKUP_SIZE];
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
/* Positive-only Wingman passability cache. Keep it outside the query helper
 * so a newly generated session can explicitly invalidate old map answers. */
static UWORD wingmanSafeCellColumn[WINGMAN_SAFE_CELL_CACHE_SIZE];
static UBYTE wingmanSafeCellRow[WINGMAN_SAFE_CELL_CACHE_SIZE];
static UBYTE wingmanSafeCellValid[WINGMAN_SAFE_CELL_CACHE_SIZE];
/* The ring streamer has already resolved every visible column before an
 * enemy plane can fly through it. Retain the resulting clear-sky rows so the
 * CPC plane AI does not repeat the procedural town/object lookup at every
 * 8-pixel decision boundary. That lookup used to create the very regular
 * "eight smooth pixels, one pause" cadence on a stock A500. */
static UWORD enemyPlanePassableMaskByColumn[GAME_LEVEL_WIDTH_TILES];
static UBYTE enemyPlanePassableColumnValid[(GAME_LEVEL_WIDTH_TILES + 7) / 8];
static UBYTE cpcTownGeneratedBlockCount = 0;
static UWORD cpcTownGeneratedBuildingColumns = 0;
static UWORD cpcTownGeneratedFlatColumns = 0;
static UWORD cpcTownGeneratedLength = 0;
static UBYTE cpcTownRouteOverflow = 0;
static UBYTE cpcTownRStart = 0;
static UBYTE cpcTownREnd = 0;
static UWORD cpcTownRSelectionChecksum = 0;
static UBYTE* engineBuffer = 0;
static UWORD engineLfsr = 0xace1;
static UWORD engineWriteOffset = 0;
static UBYTE engineActive = 0;
static UBYTE engineLastSpeed = 0xff;
static UBYTE* seaAmbienceBuffer = 0;
static UBYTE seaAmbienceChannel = 0xff;
static UBYTE seaAmbienceVolume = 0;
static UBYTE seaAmbienceVolumeDivider = 0;
static UBYTE seaAmbienceDriftPhase = 0;
static UBYTE aircraftFailureAlarmDmaActive = 0;
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
static TelemetryGameEvent telemetryGameEvents[TELEMETRY_GAME_EVENT_COUNT];
static UBYTE telemetryGameEventIndex = 0;
static UBYTE telemetryGameEventCount = 0;
static UWORD telemetryGameEventTotal = 0;
static UWORD telemetryGameEventCounters[TELEMETRY_GAME_EVENT_CODE_COUNT];
static UBYTE telemetryLastGameplayStage = 0xff;
static UBYTE telemetryStatsPage = 0;
static ULONG telemetryRadarAboveFrames = 0;
static ULONG telemetryRadarBelowFrames = 0;
static ULONG telemetryRadarGain = 0;
static ULONG telemetryRadarDrain = 0;
static UWORD telemetryRadarAlarmPulses = 0;
static UWORD telemetryRadarLevel = 0;
static UBYTE telemetryRadarClearance = 0;
static UBYTE telemetryRadarThreshold = 0;
/* Classic's air-admission audit.  A fixed-seed parity run must show that one
 * qualifying CPC-shaped decision produces at most one enemy/drop outcome,
 * and that no drop was admitted while an enemy aircraft already existed. */
static UWORD telemetryClassicAirAdmissionTicks = 0;
static UWORD telemetryClassicAirEnemyOutcomes = 0;
static UWORD telemetryClassicAirPowerupOutcomes = 0;
static UWORD telemetryClassicPowerupWhileEnemy = 0;
static UWORD telemetryEnhancedPowerupWhileEnemy = 0;
static UWORD telemetryWingFormationStops = 0;
static UWORD telemetryWingFormationCardinal = 0;
static UWORD telemetryWingFormationDiagonal = 0;
static UWORD telemetryWingFormationEvasive = 0;

enum EnemyPlaneTraceEvent {
	ENEMY_PLANE_TRACE_SPAWN = 1,
	ENEMY_PLANE_TRACE_STEP = 2,
	ENEMY_PLANE_TRACE_FIRE = 3,
	ENEMY_PLANE_TRACE_BLOCKED = 4,
	ENEMY_PLANE_TRACE_DESPAWN = 5
};
#if HAR_DEBUG_ENEMY_PLANE_LOG
#define ENEMY_PLANE_TRACE_COUNT 512
typedef struct EnemyPlaneTraceRecord {
	UWORD sequence;
	UWORD frame;
	UWORD scrollX;
	WORD visualX;
	WORD visualY;
	WORD logicalX;
	WORD logicalY;
	WORD targetX;
	WORD targetY;
	WORD tileDistance;
	WORD lagX;
	WORD lagY;
	UBYTE event;
	UBYTE status;
	UBYTE target;
	UBYTE speed;
	UBYTE blocked;
	UBYTE framesSinceTick;
} EnemyPlaneTraceRecord;
static EnemyPlaneTraceRecord enemyPlaneTrace[ENEMY_PLANE_TRACE_COUNT];
static UWORD enemyPlaneTraceCount = 0;
static UWORD enemyPlaneTraceDropped = 0;
static UWORD enemyPlaneTraceSequence = 0;
static UWORD enemyPlaneTraceLastTickFrame = 0;
#endif

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
static UWORD perfRuntimeFlakSpawns = 0;
static UWORD perfP1RocketLaunches = 0;
static UWORD perfP1BombLaunches = 0;
static UWORD perfP2RocketLaunches = 0;
static UWORD perfP2BombLaunches = 0;
static UWORD perfWingmanWorldProbes = 0;
static UWORD perfWingmanWorldHits = 0;
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

static const EnemyShipGroupDef enemyShipGroupsSource[GAME_ENEMY_SHIP_GROUP_COUNT] = {
	{ 50, 53 },
	{ 625, 628 }
};
static EnemyShipGroupDef enemyShipGroups[GAME_ENEMY_SHIP_GROUP_COUNT];

enum HarObjectId {
	HAR_OBJ_CLOUD = 0,
	HAR_OBJ_SKY = 1,
	HAR_OBJ_SEA = 2,
	HAR_OBJ_LAND = 3,
	HAR_OBJ_OWN_FRIGATE = 4,
	/* CPC object 10 is the pier.  It is solid to aircraft but weapons are
	 * absorbed without setting playerfrigatestatus; it must therefore never
	 * share HAR_OBJ_OWN_FRIGATE with either friendly carrier. */
	HAR_OBJ_PIER = 5,
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
	HAR_OBJECT_FLAG_CPC_TOWN_BLOCK = 8
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
	POWERUP_BOMBS = 4,
	POWERUP_EXTRA_AIRCRAFT = 5
};

#include "assets/level_route.h"
#include "assets/cpc_promoted_assets.h"
#include "assets/cpc_promoted_sprite_tiles.h"
#if WORLD_RENDER_GUNSHIP_WIDTH_TILES != HAR_GUNSHIP_TILES_WIDE
#error "Runtime gunship width must match the promoted CPC+ composite"
#endif

#define HAR_LEVEL_SEGMENT_COUNT (sizeof(harLevelRouteSource) / sizeof(harLevelRouteSource[0]))
#define HAR_LEVEL_OBJECT_COUNT (sizeof(harLevelObjectsSource) / sizeof(harLevelObjectsSource[0]))
#define HAR_ENEMY_SHIP_TRIGGER_COUNT (sizeof(harEnemyShipMissileTriggersSource) / sizeof(harEnemyShipMissileTriggersSource[0]))
static LevelSegmentDef harLevelRoute[HAR_LEVEL_SEGMENT_COUNT];
static LevelObjectDef harLevelObjects[HAR_LEVEL_OBJECT_COUNT];
static UWORD harEnemyShipMissileTriggers[HAR_ENEMY_SHIP_TRIGGER_COUNT];
static UWORD currentGameLevelWidthTiles = GAME_LEVEL_BASE_WIDTH_TILES;
static UWORD cpcLandRouteExtension = 0;
static UWORD gameScrollMaxPixels(void) {
	return (UWORD)((currentGameLevelWidthTiles - GAME_MAP_WIDTH) * GAME_TILE_WIDTH);
}
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

/* The authored route is the compact coordinate baseline. CPC keeps land state
 * 3 active until enemyshiptimer reaches 0x012c + leveldifficulty*0x100, then
 * checks the 200-column town timer only in state 6. Expand the procedural land
 * by 256 columns per difficulty and move every later segment/object/trigger
 * together; then add the measured 0..5-column whole-building town overflow. */
static void configureRuntimeLevelRoute(UWORD landExtension, UBYTE townOverflow) {
	memcpy(harLevelRoute, harLevelRouteSource, sizeof(harLevelRoute));
	memcpy(harLevelObjects, harLevelObjectsSource, sizeof(harLevelObjects));
	memcpy(harEnemyShipMissileTriggers, harEnemyShipMissileTriggersSource,
		sizeof(harEnemyShipMissileTriggers));
	memcpy(enemyShipGroups, enemyShipGroupsSource, sizeof(enemyShipGroups));

	for (UWORD index = 0; index < HAR_LEVEL_SEGMENT_COUNT; index++) {
		LevelSegmentDef* segment = &harLevelRoute[index];
		const LevelSegmentDef* source = &harLevelRouteSource[index];
		if (source->terrainKind == HAR_TERRAIN_CPC_RANDOM_LAND) {
			segment->endColumn = (WORD)(segment->endColumn + landExtension);
		} else if (source->startColumn > 400) {
			segment->startColumn = (WORD)(segment->startColumn + landExtension);
			segment->endColumn = (WORD)(segment->endColumn + landExtension);
		}
		if (source->terrainKind == HAR_TERRAIN_TOWN) {
			segment->endColumn = (WORD)(segment->endColumn + townOverflow);
		} else if (source->startColumn > 610) {
			segment->startColumn = (WORD)(segment->startColumn + townOverflow);
			segment->endColumn = (WORD)(segment->endColumn + townOverflow);
		}
	}
	for (UWORD index = 0; index < HAR_LEVEL_OBJECT_COUNT; index++) {
		WORD sourceColumn = harLevelObjectsSource[index].column;
		if (sourceColumn > 400)
			harLevelObjects[index].column =
				(WORD)(harLevelObjects[index].column + landExtension);
		if (sourceColumn > 610)
			harLevelObjects[index].column =
				(WORD)(harLevelObjects[index].column + townOverflow);
	}
	for (UWORD index = 0; index < HAR_ENEMY_SHIP_TRIGGER_COUNT; index++) {
		UWORD sourceColumn = harEnemyShipMissileTriggersSource[index];
		if (sourceColumn > 400)
			harEnemyShipMissileTriggers[index] =
				(UWORD)(harEnemyShipMissileTriggers[index] + landExtension);
		if (sourceColumn > 610)
			harEnemyShipMissileTriggers[index] =
				(UWORD)(harEnemyShipMissileTriggers[index] + townOverflow);
	}
	for (UWORD index = 0; index < GAME_ENEMY_SHIP_GROUP_COUNT; index++) {
		if (enemyShipGroupsSource[index].startColumn > 400) {
			enemyShipGroups[index].startColumn = (UWORD)(
				enemyShipGroups[index].startColumn + landExtension + townOverflow);
			enemyShipGroups[index].endColumn = (UWORD)(
				enemyShipGroups[index].endColumn + landExtension + townOverflow);
		}
	}

	cpcLandRouteExtension = landExtension;
	cpcTownRouteOverflow = townOverflow;
	currentGameLevelWidthTiles = (UWORD)(GAME_LEVEL_BASE_WIDTH_TILES +
		landExtension);
	/* Object columns changed, so the lazy per-column index must be rebuilt. */
	harLevelObjectIndexReady = 0;
}

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
static UBYTE townHitSmokeTileAtColumnRow(LONG worldColumn, WORD tileY);
static UBYTE runtimeFlakTileAtColumnRow(LONG worldColumn, WORD tileY);
static UBYTE cpcRStateForWorldColumn(LONG worldColumn);
static void resetDestroyedTargets(void);
static void resetRuntimeFlak(void);
static void resetTargetLock(void);
static void resetCpcRandomSequence(void);
static void ammoForSkill(UBYTE skillLevel, UBYTE* bombs, UBYTE* rockets);
static void resetDestroyedShipColumns(void);
static UBYTE isShipCellDestroyed(LONG worldColumn, WORD tileY);
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
static void startPlayerCrashWithSfx(GameState* game, WORD x, WORD y, UBYTE sfxId);
static void modRestoreChannelAfterSfx(UBYTE channel);

#ifdef __INTELLISENSE__
EMBED loadingPalette[] = { 0, 0 };
EMBED cpcFont8x8[] = { 0 };
EMBED gameTiles[] = { 0 };
EMBED gameSceneMap[] = { 0 };
EMBED gamePalette[] = { 0, 0 };
#else
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
EMBED_CHIP sfxFlakHitSample[] = { 0, 0 };
EMBED_CHIP sfxEjectSample[] = { 0, 0 };
EMBED_CHIP sfxPickupPowerupSample[] = { 0, 0 };
EMBED_CHIP sfxFlakGun1Sample[] = { 0, 0 };
EMBED_CHIP sfxFlakGun2Sample[] = { 0, 0 };
EMBED_CHIP sfxGroundTargetHit1Sample[] = { 0, 0 };
EMBED_CHIP sfxGroundTargetHit2Sample[] = { 0, 0 };
EMBED_CHIP sfxGroundTargetHit3Sample[] = { 0, 0 };
EMBED_CHIP sfxGroundTargetHit4Sample[] = { 0, 0 };
EMBED_CHIP sfxGroundMissSample[] = { 0, 0 };
EMBED_CHIP sfxWaterSplashSample[] = { 0, 0 };
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
EMBED_CHIP sfxFlakHitSample[] = {
	#embed "assets/sfx/flak_hit.raw"
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
EMBED_CHIP sfxGroundMissSample[] = {
	#embed "assets/sfx/ground_miss_1.raw"
};
EMBED_CHIP sfxWaterSplashSample[] = {
	#embed "assets/sfx/water_splash.raw"
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

/* Paula has no tone oscillator, but a tiny looping waveform is its native
 * equivalent: DMA repeats these 32 signed 8-bit samples at the requested
 * AUDxPER. This costs 32 chip bytes and needs no authored WAV/RAW asset. */
EMBED_CHIP sfxRadarAlarmWave[] = {
	0x00, 0x18, 0x30, 0x46, 0x59, 0x68, 0x73, 0x79,
	0x7b, 0x79, 0x73, 0x68, 0x59, 0x46, 0x30, 0x18,
	0x00, 0xe8, 0xd0, 0xba, 0xa7, 0x98, 0x8d, 0x87,
	0x85, 0x87, 0x8d, 0x98, 0xa7, 0xba, 0xd0, 0xe8
};

enum {
	SFX_FIRE = 0,
	SFX_BOMB,
	SFX_IMPACT,
	SFX_HIT,
	SFX_FLAK_HIT,
	SFX_EJECT,
	SFX_PICKUP_POWERUP,
	SFX_FLAK_GUN_1,
	SFX_FLAK_GUN_2,
	SFX_GROUND_TARGET_HIT_1,
	SFX_GROUND_TARGET_HIT_2,
	SFX_GROUND_TARGET_HIT_3,
	SFX_GROUND_TARGET_HIT_4,
	SFX_GROUND_MISS,
	SFX_WATER_SPLASH,
	SFX_RADAR_ALARM,
	SFX_CARRIER_IDLE_1,
	SFX_CARRIER_IDLE_2,
	SFX_COUNT
};

/* Keep retrigger state tied to the enum. A separate fixed-size constant
 * previously stayed at 16 after Water Splash and Flak Hit expanded the
 * table, making updateSfx() walk beyond the array. */
static UBYTE sfxRetriggerGuard[SFX_COUNT];

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
	[SFX_FLAK_HIT] = { sfxFlakHitSample, sizeof(sfxFlakHitSample), SFX_PAULA_PERIOD, 50,
		SFX_PRIORITY_PLAYER, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxFlakHitSample)) },
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
	/* Bare-earth miss - a bomb/rocket hitting plain land (not a real ground
	 * target/building/ship) previously reused the same random 1-of-4
	 * ground_target_hit sound as an actual hit; this is its own dedicated
	 * "just dug a hole" sound instead. */
	[SFX_GROUND_MISS] = { sfxGroundMissSample, sizeof(sfxGroundMissSample), SFX_PAULA_PERIOD, 58,
		SFX_PRIORITY_IMPACT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxGroundMissSample)) },
	[SFX_WATER_SPLASH] = { sfxWaterSplashSample, sizeof(sfxWaterSplashSample), SFX_PAULA_PERIOD, 58,
		SFX_PRIORITY_IMPACT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxWaterSplashSample)) },
	/* Native Paula tone: the short waveform loops for a fixed software-timed
	 * beep and remains below flak and every gameplay cue. */
	[SFX_RADAR_ALARM] = { sfxRadarAlarmWave, sizeof(sfxRadarAlarmWave), RADAR_ALARM_LOW_PERIOD, RADAR_ALARM_MAX_VOLUME,
		SFX_PRIORITY_AMBIENT, SFX_PAN_ANY,
		RADAR_ALARM_TONE_FRAMES },
	[SFX_CARRIER_IDLE_1] = { sfxCarrierIdle1Sample, sizeof(sfxCarrierIdle1Sample), SFX_PAULA_PERIOD, 34,
		SFX_PRIORITY_AMBIENT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxCarrierIdle1Sample)) },
	[SFX_CARRIER_IDLE_2] = { sfxCarrierIdle2Sample, sizeof(sfxCarrierIdle2Sample), SFX_PAULA_PERIOD, 34,
		SFX_PRIORITY_AMBIENT, SFX_PAN_ANY,
		SFX_FRAMES_FOR_BYTES(sizeof(sfxCarrierIdle2Sample)) },
};

static const UWORD menuPalette[32] = {
	0x000, 0xffa, 0xf22, 0x026, 0x0f0, 0xff0, 0x05f, 0x0af,
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
	/* The game used to install a level-3 VBL handler whose only job was to
	 * increment this counter.  A CPU interrupt switches from USP to the
	 * machine's supervisor stack before our handler can run.  Some minimal
	 * KS1.3/headless launch configurations leave that SSP at $00c00000, so
	 * the first exception frame was written down through $00bffffe (the CIA
	 * range) and corrupted execution.  We already synchronise every outer
	 * frame and every timed wait here, so advancing the counter after the
	 * observed PAL VBL preserves the exact timing without requiring any CPU
	 * exception or supervisor stack at runtime. */
	frameCounter++;
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
	if (channel == seaAmbienceChannel) {
		seaAmbienceChannel = 0xff;
		seaAmbienceVolume = 0;
	}

	/* Pull the DAC to zero before disabling DMA. Reversing these writes can
	 * leave Paula holding the final non-zero sample for a fraction of a frame,
	 * heard as a click when a quiet ambience cue ends. */
	custom->aud[channel].ac_vol = 0;
	custom->dmacon = sfxDmaBit(channel);
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
	aircraftFailureAlarmDmaActive = 0;
	seaAmbienceChannel = 0xff;
	seaAmbienceVolume = 0;
	seaAmbienceVolumeDivider = 0;
	seaAmbienceDriftPhase = 0;
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
	carrierIdleNextVariant = 0xff;
	carrierIdlePreparedVariant = 0xff;
	carrierIdlePreparedLength = 0;
	carrierIdleDecodeActive = 0;
	carrierIdleDecodeFading = 0;
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
		case SFX_FLAK_HIT:
			return 30;
		case SFX_EJECT:
			return 40;
		case SFX_PICKUP_POWERUP:
			return 16;
		case SFX_FLAK_GUN_1:
		case SFX_FLAK_GUN_2:
			/* Dense flak deliberately restarts its own voice to produce a
			 * continuous ta-ta-ta rhythm. Channel selection still prevents it
			 * from pre-empting any non-flak effect. */
			return 0;
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

static UBYTE isFlakGunSfx(UBYTE sfxId) {
	return sfxId == SFX_FLAK_GUN_1 || sfxId == SFX_FLAK_GUN_2;
}

static UBYTE selectSfxChannel(UBYTE sfxId, const SfxSample* sample, WORD screenX) {
	UBYTE desiredPan = sample->pan;
	if (desiredPan == SFX_PAN_ANY)
		desiredPan = screenX < SFX_POSITION_CENTER ? SFX_PAN_LEFT : SFX_PAN_RIGHT;

	/* Reuse/restart an existing flak-gun voice before claiming a free one.
	 * This gives dense fire a crisp rhythmic restart while guaranteeing that
	 * flak cannot cut off flak-hit, weapons, impacts, engine or music. */
	if (isFlakGunSfx(sfxId)) {
		for (UBYTE channel = 0; channel < SFX_CHANNEL_COUNT; channel++) {
			if (channel == ENGINE_CHANNEL && engineActive)
				continue;
			if (isFlakGunSfx(sfxChannelCurrentId[channel]))
				return channel;
		}
	}

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

/* Prepare the next long deck cue during its 15-20 second idle delay. The old
 * path decoded all 44 KB in the launch frame, visibly pausing animation on a
 * stock 68000. Forty-eight packed bytes per frame keep the work bounded; the
 * longer edge ramp is likewise spread over several frames. */
static void beginCarrierIdleDecode(UBYTE variant) {
	UBYTE sfxId = (UBYTE)(SFX_CARRIER_IDLE_1 + (variant & 1));
	const SfxSample* sample = &sfxSamples[sfxId];
	carrierIdlePreparedVariant = 0xff;
	carrierIdlePreparedLength = 0;
	carrierIdleDecodeActive = 0;
	carrierIdleDecodeFading = 0;
	if (!carrierIdleDecodeBuffer || sample->byteLength < 5)
		return;
	ULONG decodedLength = ((ULONG)sample->data[0] << 24) |
		((ULONG)sample->data[1] << 16) |
		((ULONG)sample->data[2] << 8) | sample->data[3];
	if (decodedLength < 2 || decodedLength > CARRIER_IDLE_DECODE_BUFFER_BYTES ||
		4 + ((decodedLength + 1) >> 1) > sample->byteLength)
		return;
	carrierIdleDecodeSource = sample->data;
	carrierIdleDecodeSourceLength = sample->byteLength;
	carrierIdleDecodeInputIndex = 4;
	carrierIdleDecodeOutputIndex = 0;
	carrierIdleDecodeTargetLength = decodedLength;
	carrierIdleDecodeFadeIndex = 0;
	carrierIdleDecodePredictor = 0;
	carrierIdleDecodeStepIndex = 0;
	carrierIdleDecodeActive = 1;
}

static void serviceCarrierIdleDecode(void) {
	if (!carrierIdleDecodeActive)
		return;
	if (!carrierIdleDecodeFading) {
		UBYTE budget = CARRIER_IDLE_DECODE_BYTES_PER_FRAME;
		while (budget-- &&
			carrierIdleDecodeInputIndex < carrierIdleDecodeSourceLength &&
			carrierIdleDecodeOutputIndex < carrierIdleDecodeTargetLength) {
			UBYTE packed = carrierIdleDecodeSource[carrierIdleDecodeInputIndex++];
			for (UBYTE half = 0; half < 2 &&
				carrierIdleDecodeOutputIndex < carrierIdleDecodeTargetLength; half++) {
				UBYTE nibble = half == 0 ? packed >> 4 : packed & 15;
				LONG step = imaStepTable[carrierIdleDecodeStepIndex];
				LONG difference = step >> 3;
				if (nibble & 1) difference += step >> 2;
				if (nibble & 2) difference += step >> 1;
				if (nibble & 4) difference += step;
				carrierIdleDecodePredictor +=
					(nibble & 8) ? -difference : difference;
				if (carrierIdleDecodePredictor > 32767)
					carrierIdleDecodePredictor = 32767;
				if (carrierIdleDecodePredictor < -32768)
					carrierIdleDecodePredictor = -32768;
				carrierIdleDecodeStepIndex += imaIndexTable[nibble];
				if (carrierIdleDecodeStepIndex < 0)
					carrierIdleDecodeStepIndex = 0;
				if (carrierIdleDecodeStepIndex > 88)
					carrierIdleDecodeStepIndex = 88;
				carrierIdleDecodeBuffer[carrierIdleDecodeOutputIndex++] =
					(UBYTE)((BYTE)(carrierIdleDecodePredictor >> 8));
			}
		}
		if (carrierIdleDecodeOutputIndex < carrierIdleDecodeTargetLength)
			return;
		if (carrierIdleDecodeOutputIndex & 1)
			carrierIdleDecodeBuffer[carrierIdleDecodeOutputIndex++] = 0;
		carrierIdleDecodeFading = 1;
	}

	ULONG fadeSamples = carrierIdleDecodeOutputIndex / 2;
	if (fadeSamples > CARRIER_IDLE_PCM_EDGE_FADE_SAMPLES)
		fadeSamples = CARRIER_IDLE_PCM_EDGE_FADE_SAMPLES;
	UBYTE budget = CARRIER_IDLE_FADE_PAIRS_PER_FRAME;
	while (budget-- && carrierIdleDecodeFadeIndex < fadeSamples) {
		ULONG index = carrierIdleDecodeFadeIndex++;
		LONG first = (BYTE)carrierIdleDecodeBuffer[index];
		LONG last = (BYTE)carrierIdleDecodeBuffer[
			carrierIdleDecodeOutputIndex - 1 - index];
		/* The configured fade is 1024 samples, so the common path is a cheap
		 * multiply/shift. Retain division for unusually short test assets. */
		if (fadeSamples == 1024) {
			carrierIdleDecodeBuffer[index] =
				(UBYTE)((BYTE)((first * (LONG)index) >> 10));
			carrierIdleDecodeBuffer[carrierIdleDecodeOutputIndex - 1 - index] =
				(UBYTE)((BYTE)((last * (LONG)index) >> 10));
		} else {
			carrierIdleDecodeBuffer[index] =
				(UBYTE)((BYTE)((first * (LONG)index) / (LONG)fadeSamples));
			carrierIdleDecodeBuffer[carrierIdleDecodeOutputIndex - 1 - index] =
				(UBYTE)((BYTE)((last * (LONG)index) / (LONG)fadeSamples));
		}
	}
	if (carrierIdleDecodeFadeIndex < fadeSamples)
		return;
	carrierIdleDecodeBuffer[0] = 0;
	carrierIdleDecodeBuffer[carrierIdleDecodeOutputIndex - 1] = 0;
	carrierIdlePreparedVariant = carrierIdleNextVariant;
	carrierIdlePreparedLength = (UWORD)carrierIdleDecodeOutputIndex;
	carrierIdleDecodeActive = 0;
	carrierIdleDecodeFading = 0;
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
	/* Sprint 15.40.1 mix pass: leave the four deliberate bass-boom ground
	 * hits at their authored level; lower every other one-shot by 15%.
	 * Source WAV/RAW data remains untouched. Carrier ambience uses its
	 * reduced envelope constant because its volume is updated after start. */
	if (sfxId < SFX_GROUND_TARGET_HIT_1 ||
		sfxId > SFX_GROUND_TARGET_HIT_4)
		volume = AUDIO_MIX_VOLUME(volume);
	UBYTE channel = selectSfxChannel(sfxId, sample, screenX);
	if (channel >= SFX_CHANNEL_COUNT)
		return;

	if (sfxId >= SFX_CARRIER_IDLE_1 && sfxId <= SFX_CARRIER_IDLE_2) {
		UBYTE variant = (UBYTE)(sfxId - SFX_CARRIER_IDLE_1);
		UWORD decodedLength = carrierIdlePreparedVariant == variant ?
			carrierIdlePreparedLength :
			decodeCarrierIdleSample(sample->data, sample->byteLength);
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
	carrierIdleNextVariant = 0xff;
	carrierIdlePreparedVariant = 0xff;
	carrierIdlePreparedLength = 0;
	carrierIdleDecodeActive = 0;
	carrierIdleDecodeFading = 0;
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
	if (carrierIdleNextVariant == 0xff) {
		carrierIdleNextVariant = nextSfxVariant(&carrierIdleRandomState,
			frameCounter, &carrierIdleLastVariant, 2);
		beginCarrierIdleDecode(carrierIdleNextVariant);
	}
	serviceCarrierIdleDecode();
	if (carrierIdleDelayFrames > 0) {
		carrierIdleDelayFrames--;
		return;
	}
	/* A malformed or unexpectedly long asset must never stall gameplay. It
	 * merely postpones this optional cue until preparation has completed. */
	if (carrierIdlePreparedVariant != carrierIdleNextVariant ||
		!carrierIdlePreparedLength)
		return;

	UBYTE sfxId = (UBYTE)(SFX_CARRIER_IDLE_1 + carrierIdleNextVariant);
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
	/* Ordinary one-shots switch to a silent reload word after their first
	 * pass. The procedural radar tone is intentionally a tiny waveform loop;
	 * its frame timer stops DMA after the short beep. */
	sfxChannelSilenceQueueDelay[channel] =
		sfxChannelCurrentId[channel] == SFX_RADAR_ALARM ? 0 : 1;
}

static void updateSfx(void) {
	for (UBYTE sfxId = 0; sfxId < SFX_COUNT; sfxId++) {
		if (sfxRetriggerGuard[sfxId] > 0)
			sfxRetriggerGuard[sfxId]--;
	}
	for (UBYTE channel = 0; channel < SFX_CHANNEL_COUNT; channel++) {
		/* The generated sea bed is an intentional Paula loop.  It still marks
		 * the channel as ambient-priority below, so any real effect may evict
		 * it through selectSfxChannel()/stopSfxChannel(). */
		if (channel == seaAmbienceChannel)
			continue;
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

static UWORD nextSeaAmbienceNoise(UWORD* state) {
	*state = (UWORD)((*state >> 1) ^ (-(WORD)(*state & 1) & 0xb400));
	if (!*state)
		*state = 0x7d35;
	return *state;
}

/* Build a quiet, deliberately bandwidth-limited wind/sea bed once.  The
 * 128-sample edge fades force a zero crossing at the DMA loop boundary, so
 * Paula can repeat the 4 KiB block without a click.  This is generated from
 * a private fixed seed and therefore cannot perturb CPC/gameplay RNG. */
static void buildSeaAmbienceBuffer(void) {
	if (!seaAmbienceBuffer)
		return;
	UWORD lfsr = 0x4d2b;
	LONG fast = 0;
	LONG slow = 0;
	for (UWORD index = 0; index < SEA_AMBIENCE_BUFFER_BYTES; index++) {
		LONG noise = (BYTE)(nextSeaAmbienceNoise(&lfsr) >> 8);
		fast += (noise - fast) >> 2;
		slow += (noise - slow) >> 6;
		/* Broad surf rather than the old strongly low-passed, motor-like
		 * waveform. The slow component swells while fast-minus-slow supplies
		 * the airy white-water edge; both remain cheap signed 8-bit Paula data. */
		LONG sample = ((fast - slow) >> 1) + (slow >> 3) + (noise >> 4);
		if (sample > 42) sample = 42;
		if (sample < -42) sample = -42;
		seaAmbienceBuffer[index] = (UBYTE)(BYTE)sample;
	}
	/* Crossfade the tail into the start instead of fading both ends to zero.
	 * A periodic silence made the short sample announce every DMA wrap. */
	for (UWORD edge = 0; edge < 256; edge++) {
		UWORD tailIndex = (UWORD)(SEA_AMBIENCE_BUFFER_BYTES - 256 + edge);
		LONG tail = (BYTE)seaAmbienceBuffer[tailIndex];
		LONG head = (BYTE)seaAmbienceBuffer[(edge + 1) & 255];
		seaAmbienceBuffer[tailIndex] = (UBYTE)(BYTE)(
			(tail * (255 - edge) + head * edge) / 255);
	}
	seaAmbienceBuffer[SEA_AMBIENCE_BUFFER_BYTES - 1] = seaAmbienceBuffer[0];
}

static UBYTE findFreeSeaAmbienceChannel(void) {
	/* Prefer a right-side Paula voice: the persistent engine owns left-side
	 * channel 3.  Never evict another sound merely to start ambience. */
	static const UBYTE preference[3] = { 2, 1, 0 };
	for (UBYTE index = 0; index < 3; index++) {
		UBYTE channel = preference[index];
		if (!sfxChannelFrames[channel] &&
			!sfxChannelStartDelay[channel] &&
			!sfxPendingSample[channel])
			return channel;
	}
	return 0xff;
}

static void startSeaAmbience(void) {
	if (!seaAmbienceBuffer || modPlaying ||
		seaAmbienceChannel < SFX_CHANNEL_COUNT)
		return;
	UBYTE channel = findFreeSeaAmbienceChannel();
	if (channel >= SFX_CHANNEL_COUNT)
		return;
	custom->dmacon = sfxDmaBit(channel);
	custom->aud[channel].ac_ptr = (volatile UWORD*)seaAmbienceBuffer;
	custom->aud[channel].ac_len = SEA_AMBIENCE_BUFFER_BYTES >> 1;
	custom->aud[channel].ac_per = SEA_AMBIENCE_PERIOD;
	custom->aud[channel].ac_vol = 0;
	sfxChannelFrames[channel] = 1; /* occupied; updateSfx() special-cases it */
	sfxChannelCurrentId[channel] = 0xff;
	sfxChannelPriority[channel] = SFX_PRIORITY_AMBIENT;
	sfxChannelSequence[channel] = ++sfxVoiceSequence;
	custom->dmacon = DMAF_SETCLR | sfxDmaBit(channel);
	seaAmbienceChannel = channel;
	seaAmbienceVolume = 0;
}

static void updateSeaAmbience(UBYTE targetVolume) {
	if (modPlaying || !seaAmbienceBuffer)
		targetVolume = 0;
	if (targetVolume && seaAmbienceChannel >= SFX_CHANNEL_COUNT)
		startSeaAmbience();
	if (seaAmbienceChannel >= SFX_CHANNEL_COUNT)
		return;
	/* A slow ten-second swell and tiny period drift hide the short low-rate
	 * DMA loop. Volume remains beneath weapons and engine at its crest. */
	if ((frameCounter & 3) == 0) {
		seaAmbienceDriftPhase = (UBYTE)((seaAmbienceDriftPhase + 1) & 127);
		BYTE triangle = seaAmbienceDriftPhase < 64 ?
			(BYTE)seaAmbienceDriftPhase :
			(BYTE)(127 - seaAmbienceDriftPhase);
		custom->aud[seaAmbienceChannel].ac_per = (UWORD)(
			SEA_AMBIENCE_PERIOD - 3 + (triangle >> 4));
	}
	UBYTE pulse = (UBYTE)((seaAmbienceDriftPhase < 64 ?
		seaAmbienceDriftPhase : 127 - seaAmbienceDriftPhase) >> 4);
	UBYTE pulsedTarget = targetVolume ? (UBYTE)(
		targetVolume > 3 ? targetVolume - 3 + pulse : targetVolume) : 0;

	seaAmbienceVolumeDivider++;
	if (seaAmbienceVolumeDivider >= SEA_AMBIENCE_VOLUME_STEP_FRAMES) {
		seaAmbienceVolumeDivider = 0;
		if (seaAmbienceVolume < pulsedTarget)
			seaAmbienceVolume++;
		else if (seaAmbienceVolume > pulsedTarget)
			seaAmbienceVolume--;
		custom->aud[seaAmbienceChannel].ac_vol = seaAmbienceVolume;
	}
	if (!targetVolume && !seaAmbienceVolume)
		stopSfxChannel(seaAmbienceChannel);
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
		custom->aud[channel].ac_vol = AUDIO_MIX_VOLUME(ch->volume);
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
			custom->aud[channel].ac_vol = AUDIO_MIX_VOLUME(ch->volume);
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

/* Menu pages are drawn directly into the displayed buffer and a complete
 * redraw can span more than one PAL frame on a stock 68000. Driving the MOD
 * once per main-loop iteration therefore loses music ticks while text and
 * icons are being rendered. Catch up from the real VBlank counter instead,
 * and allow long menu draw routines to service the replayer between rows. */
static UWORD modLastServicedFrame;

static void serviceModMusicToCurrentVbl(void) {
	UWORD now = frameCounter;
	UWORD elapsed = (UWORD)(now - modLastServicedFrame);
	if (!modPlaying) {
		modLastServicedFrame = now;
		return;
	}
	/* Never replay several elapsed ticks back-to-back. Paula needs real time
	 * between DMA disable/retrigger operations; a burst here is exactly what
	 * makes WinUAE report "DMA wait hack DISABLED" and produces an audible
	 * jump when returning from a costly menu page. The menu draw paths call
	 * this helper frequently, so one tick per observed VBlank stays smooth. */
	if (elapsed) {
		modCompletePendingRetriggers();
		modTick();
	}
	modLastServicedFrame = now;
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
	modLastServicedFrame = frameCounter;
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
	return AUDIO_MIX_VOLUME((UWORD)(12 + speed * 4));
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
			"frame,seconds,loops,minFps,maxFps,avgFps,hitches,maxVblDelta,scroll,speed,origin,job,stage,tileX,tileCols,objCols,pages,fuel,armour,rockets,bombs,p1Rkt,p1Bmb,p2Rkt,p2Bmb,wingWorldProbes,wingWorldHits,hudCalls,hudArmChg,hudFuelChg,hudScoreChg,hudSpdChg,hudRktChg,hudBmbChg,hudGuardHits,hudGuard2Hits,hudRegHits,hudCollisionFires,livBplcon0,expBplcon0,livDdfstrt,expDdfstrt,livDdfstop,expDdfstop,livBpl5pt,expBpl5pt\n";
		perfLogAppend(header, sizeof(header) - 1);
	}
	perfLastLoopFrame = frameCounter;
	perfRuntimeFlakSpawns = 0;
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
	perfP1RocketLaunches = 0;
	perfP1BombLaunches = 0;
	perfP2RocketLaunches = 0;
	perfP2BombLaunches = 0;
	perfWingmanWorldProbes = 0;
	perfWingmanWorldHits = 0;
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
#if HAR_DEBUG_HUD_GUARD
	perfHudGuardCheck();
#endif
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
		c = appendUnsignedLong(c, perfP1RocketLaunches);
		*c++ = ',';
		c = appendUnsignedLong(c, perfP1BombLaunches);
		*c++ = ',';
		c = appendUnsignedLong(c, perfP2RocketLaunches);
		*c++ = ',';
		c = appendUnsignedLong(c, perfP2BombLaunches);
		*c++ = ',';
		c = appendUnsignedLong(c, perfWingmanWorldProbes);
		*c++ = ',';
		c = appendUnsignedLong(c, perfWingmanWorldHits);
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

/* Sprint 15.60: the failed-aircraft warning owns the engine voice only after
 * the engine has been stopped. Paula loops the same tiny signed waveform as
 * the radar beep while AUD3PER sweeps upward in pitch. At PAL 50 Hz the
 * 13-frame bleep is 0.26 s (the nearest whole-frame value to 0.25 s), followed
 * by exactly 25 silent frames/0.5 s. Keeping the silent phase in GameState
 * prevents an orphaned audio timer from restarting the warning after eject,
 * impact, pause or menu return. */
static void stopAircraftFailureAlarm(void) {
	if (!aircraftFailureAlarmDmaActive)
		return;
	stopSfxChannel(ENGINE_CHANNEL);
	aircraftFailureAlarmDmaActive = 0;
}

static void startAircraftFailureAlarmPulse(UWORD period) {
	stopSfxChannel(ENGINE_CHANNEL);
	custom->aud[ENGINE_CHANNEL].ac_ptr = (volatile UWORD*)sfxRadarAlarmWave;
	custom->aud[ENGINE_CHANNEL].ac_len = sizeof(sfxRadarAlarmWave) >> 1;
	custom->aud[ENGINE_CHANNEL].ac_per = period;
	custom->aud[ENGINE_CHANNEL].ac_vol = AIRCRAFT_FAILURE_ALARM_VOLUME;
	custom->dmacon = DMAF_SETCLR | sfxDmaBit(ENGINE_CHANNEL);
	aircraftFailureAlarmDmaActive = 1;
}

static void updateAircraftFailureAlarm(GameState* game) {
	if (game->aircraftFailureState != AIRCRAFT_FAILURE_DESCENT) {
		stopAircraftFailureAlarm();
		return;
	}

	UBYTE frame = game->aircraftFailureAlarmFrame;
	if (frame < AIRCRAFT_FAILURE_ALARM_SWEEP_FRAMES) {
		UWORD period = (UWORD)(AIRCRAFT_FAILURE_ALARM_LOW_PERIOD -
			((ULONG)frame * (AIRCRAFT_FAILURE_ALARM_LOW_PERIOD -
			AIRCRAFT_FAILURE_ALARM_HIGH_PERIOD)) /
			(AIRCRAFT_FAILURE_ALARM_SWEEP_FRAMES - 1));
		if (!aircraftFailureAlarmDmaActive)
			startAircraftFailureAlarmPulse(period);
		else
			custom->aud[ENGINE_CHANNEL].ac_per = period;
	} else if (aircraftFailureAlarmDmaActive) {
		stopAircraftFailureAlarm();
	}

	frame++;
	if (frame >= AIRCRAFT_FAILURE_ALARM_SWEEP_FRAMES +
		AIRCRAFT_FAILURE_ALARM_GAP_FRAMES)
		frame = 0;
	game->aircraftFailureAlarmFrame = frame;
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

/* Sprint 15.28: Wingman: Player 2 reuses these exact same port-1 pins as its
 * fire/bomb buttons - a mouse's buttons and a joystick's buttons share the
 * same electrical pins on this port regardless of which device is actually
 * plugged in, so no new hardware access is needed, only a second name for
 * the same read used when Player 2 control is active. */
static __attribute__((always_inline)) inline short Joy0Fire(void) {
	return MouseLeft();
}

static __attribute__((always_inline)) inline short Joy0Fire1(void) {
	return !((*(volatile UWORD*)0xdff016) & (1 << 10));
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

/* Sprint 15.28: Wingman: Player 2's own joystick, physical port 1 (JOY0DAT,
 * $DFF00A) - the same port the mouse normally occupies. Bit layout/XOR-diagonal
 * decode is identical to Joy1Dat() above, just the other port register. */
static __attribute__((always_inline)) inline UWORD Joy0Dat(void) {
	return *(volatile UWORD*)0xdff00a;
}

static short JoyRight2(void) {
	UWORD joy = Joy0Dat();
	return (joy & 0x0002) != 0;
}

static short JoyLeft2(void) {
	UWORD joy = Joy0Dat();
	return (joy & 0x0200) != 0;
}

static short JoyDown2(void) {
	UWORD joy = Joy0Dat();
	return ((joy & 0x0001) != 0) ^ ((joy & 0x0002) != 0);
}

static short JoyUp2(void) {
	UWORD joy = Joy0Dat();
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
	lastKeyboardMakeKey = RAWKEY_NONE;
	keyboardMakeSerial = 0;
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
		if (!(raw & 0x80)) {
			lastKeyboardMakeKey = code;
			keyboardMakeSerial++;
		}
		KeyboardAck();
	}
}

static UBYTE KeyDown(UBYTE rawKey) {
	return keyboardDown[rawKey & 0x7f] != 0;
}

static void restoreDefaultControlProfile(UBYTE player) {
	ControlProfile* profile = &controlProfiles[player & 1];
	memset(profile, 0, sizeof(*profile));
	if ((player & 1) == 0) {
		profile->key[CONTROL_UP] = RAWKEY_UP;
		profile->key[CONTROL_DOWN] = RAWKEY_DOWN;
		profile->key[CONTROL_LEFT] = RAWKEY_LEFT;
		profile->key[CONTROL_RIGHT] = RAWKEY_RIGHT;
		profile->key[CONTROL_ROCKET] = RAWKEY_CONTROL;
		profile->key[CONTROL_BOMB] = RAWKEY_SPACE;
		profile->key[CONTROL_EJECT] = RAWKEY_E;
		profile->joystickPort = CONTROL_JOY_PORT_2;
	} else {
		profile->key[CONTROL_UP] = RAWKEY_KP_8;
		profile->key[CONTROL_DOWN] = RAWKEY_KP_2;
		profile->key[CONTROL_LEFT] = RAWKEY_KP_4;
		profile->key[CONTROL_RIGHT] = RAWKEY_KP_6;
		profile->key[CONTROL_ROCKET] = RAWKEY_KP_0;
		profile->key[CONTROL_BOMB] = RAWKEY_KP_ENTER;
		profile->key[CONTROL_EJECT] = RAWKEY_KP_DECIMAL;
		profile->joystickPort = CONTROL_JOY_PORT_1;
	}
	profile->rocketButton = CONTROL_BUTTON_PRIMARY;
	profile->bombButton = CONTROL_BUTTON_SECONDARY;
}

static void restoreDefaultControlProfiles(void) {
	restoreDefaultControlProfile(0);
	restoreDefaultControlProfile(1);
}

static UBYTE controlProfileKeyDown(const ControlProfile* profile,
	UBYTE action) {
	UBYTE rawKey = profile->key[action];
	return rawKey != RAWKEY_NONE && KeyDown(rawKey);
}

static UWORD controlJoystickData(UBYTE port) {
	return port == CONTROL_JOY_PORT_1 ? Joy0Dat() : Joy1Dat();
}

static UBYTE controlJoystickDirection(UBYTE port, UBYTE action) {
	if (port == CONTROL_JOY_OFF)
		return 0;
	UWORD joy = controlJoystickData(port);
	switch (action) {
		case CONTROL_UP:
			return (UBYTE)(((joy & 0x0100) != 0) ^ ((joy & 0x0200) != 0));
		case CONTROL_DOWN:
			return (UBYTE)(((joy & 0x0001) != 0) ^ ((joy & 0x0002) != 0));
		case CONTROL_LEFT:
			return (UBYTE)((joy & 0x0200) != 0);
		default:
			return (UBYTE)((joy & 0x0002) != 0);
	}
}

static UBYTE controlJoystickButton(UBYTE port, UBYTE button) {
	if (port == CONTROL_JOY_OFF || button == CONTROL_BUTTON_NONE)
		return 0;
	if (port == CONTROL_JOY_PORT_1)
		return (UBYTE)(button == CONTROL_BUTTON_PRIMARY ? Joy0Fire() : Joy0Fire1());
	return (UBYTE)(button == CONTROL_BUTTON_PRIMARY ? JoyFire() :
		ReadJoyFire1Debounced());
}

/* suppressMouse: a Player 2 profile that owns physical port 1 reuses the
 * mouse-button pins for fire/bomb. Reading them here too would make P2's
 * presses register as P1 weapons. Keyboard-only or port-2 P2 profiles do not
 * own those pins, so they must not unnecessarily disable P1 mouse input. */
static void ReadInput(InputState* input, UBYTE suppressMouse) {
	PollKeyboard();
	memset(input, 0, sizeof(*input));
	const ControlProfile* profile = &controlProfiles[0];
	UBYTE keyShift = KeyDown(RAWKEY_LEFT_SHIFT) || KeyDown(RAWKEY_RIGHT_SHIFT);
	UBYTE keyControl = KeyDown(RAWKEY_CONTROL);
	UBYTE keySpace = KeyDown(RAWKEY_SPACE);
	UBYTE keyReturn = KeyDown(RAWKEY_RETURN) || KeyDown(RAWKEY_KP_ENTER);
	UBYTE keyD = KeyDown(RAWKEY_D);
	UBYTE keyP = KeyDown(RAWKEY_P);
	UBYTE keyR = KeyDown(RAWKEY_R);

	UBYTE mouseLeft = !suppressMouse && MouseLeft();
	UBYTE mouseRight = !suppressMouse && MouseRight();

	input->up = controlProfileKeyDown(profile, CONTROL_UP) ||
		controlJoystickDirection(profile->joystickPort, CONTROL_UP);
	input->down = controlProfileKeyDown(profile, CONTROL_DOWN) ||
		controlJoystickDirection(profile->joystickPort, CONTROL_DOWN);
	input->left = controlProfileKeyDown(profile, CONTROL_LEFT) ||
		controlJoystickDirection(profile->joystickPort, CONTROL_LEFT);
	input->right = controlProfileKeyDown(profile, CONTROL_RIGHT) ||
		controlJoystickDirection(profile->joystickPort, CONTROL_RIGHT);
	/* Profile ROCKET intentionally maps to the existing semantic `fire`
	 * field. There is no parallel legacy weapon signal to double-trigger. */
	input->fire = controlProfileKeyDown(profile, CONTROL_ROCKET) || mouseLeft ||
		controlJoystickButton(profile->joystickPort, profile->rocketButton);
	input->bombKey = controlProfileKeyDown(profile, CONTROL_BOMB);
	input->bombMouse = mouseRight;
	input->bombJoystick = controlJoystickButton(profile->joystickPort,
		profile->bombButton);
	input->bomb = input->bombKey || input->bombMouse || input->bombJoystick;
	input->eject = controlProfileKeyDown(profile, CONTROL_EJECT);
	input->shift = keyShift;
	input->control = keyControl;
	input->space = keySpace;
	input->d = keyD;
	input->p = keyP;
	input->r = keyR;
	input->menuPrev = input->up;
	input->menuNext = input->down || mouseRight;
	/* Return is a menu/UI confirmation key only.  Keep gameplay weapon input
	 * on `fire`, so adding keyboard confirmation cannot launch a rocket. */
	input->select = input->fire || keyReturn;
	input->cancel = KeyDown(RAWKEY_ESCAPE);
	input->any = input->up || input->down || input->left || input->right ||
		input->fire || input->bomb || input->eject || input->select || mouseRight;
	input->lastRawKey = lastKeyboardRawKey;
}

/* Sprint 15.28: Wingman: Player 2's own controller - joystick port 1
 * (Joy0Dat) for direction, the same port's two button pins (already read by
 * MouseLeft()/MouseRight() for the mouse case) for fire/bomb. No debounce on
 * the second button here - ReadJoyFire1Debounced() exists because Player 1's
 * bomb button flickered enough to double-fire; revisit if Player 2 shows the
 * same symptom.
 *
 * Sprint 15.28 follow-up: added a numeric-keypad fallback (kept clear of
 * every key Player 1's own controls already use) after a report of
 * "selected Player 2, nothing happened" that traced back to testing in an
 * emulator with no second joystick actually configured on port 1 - with no
 * device there, Joy0Dat() reads a fixed idle pattern and Up never comes, so
 * the Wingman just waits on deck forever with no on-screen indication why.
 * Real CPC's own Player 2 was always freely rebindable to keyboard or
 * joystick (asm testkeyup2/etc. read a generic matrix, not a fixed port) -
 * this restores that same flexibility rather than hard-requiring a second
 * physical controller, which most players (and this exact bug report)
 * won't have to hand. */
static void ReadPlayer2Input(Player2InputState* input, UBYTE gameMode) {
	/* Classic restores CPC's fixed second-player path.  The Enhanced profile
	 * keeps this port's rebindable controls and eject extension. */
	if (gameMode == GAME_MODE_CLASSIC) {
		input->up = KeyDown(RAWKEY_KP_8) ||
			controlJoystickDirection(CONTROL_JOY_PORT_1, CONTROL_UP);
		input->down = KeyDown(RAWKEY_KP_2) ||
			controlJoystickDirection(CONTROL_JOY_PORT_1, CONTROL_DOWN);
		input->left = KeyDown(RAWKEY_KP_4) ||
			controlJoystickDirection(CONTROL_JOY_PORT_1, CONTROL_LEFT);
		input->right = KeyDown(RAWKEY_KP_6) ||
			controlJoystickDirection(CONTROL_JOY_PORT_1, CONTROL_RIGHT);
		input->fire = KeyDown(RAWKEY_KP_0) ||
			controlJoystickButton(CONTROL_JOY_PORT_1, CONTROL_BUTTON_PRIMARY);
		input->bomb = KeyDown(RAWKEY_KP_ENTER) ||
			controlJoystickButton(CONTROL_JOY_PORT_1, CONTROL_BUTTON_SECONDARY);
		input->eject = 0;
		return;
	}

	const ControlProfile* profile = &controlProfiles[1];
	input->up = controlProfileKeyDown(profile, CONTROL_UP) ||
		controlJoystickDirection(profile->joystickPort, CONTROL_UP);
	input->down = controlProfileKeyDown(profile, CONTROL_DOWN) ||
		controlJoystickDirection(profile->joystickPort, CONTROL_DOWN);
	input->left = controlProfileKeyDown(profile, CONTROL_LEFT) ||
		controlJoystickDirection(profile->joystickPort, CONTROL_LEFT);
	input->right = controlProfileKeyDown(profile, CONTROL_RIGHT) ||
		controlJoystickDirection(profile->joystickPort, CONTROL_RIGHT);
	input->fire = controlProfileKeyDown(profile, CONTROL_ROCKET) ||
		controlJoystickButton(profile->joystickPort, profile->rocketButton);
	input->bomb = controlProfileKeyDown(profile, CONTROL_BOMB) ||
		controlJoystickButton(profile->joystickPort, profile->bombButton);
	input->eject = controlProfileKeyDown(profile, CONTROL_EJECT);
}

static UBYTE Pressed(UBYTE now, UBYTE previous) {
	return now && !previous;
}

static UBYTE BombPressed(const InputState* now,
	const InputState* previous) {
	/* The combined fallback keeps synthetic/headless input working. Normal
	 * controls use their independent edges so one continuously asserted
	 * hardware source cannot mask the keyboard or another controller. */
	return Pressed(now->bombKey, previous->bombKey) ||
		Pressed(now->bombMouse, previous->bombMouse) ||
		Pressed(now->bombJoystick, previous->bombJoystick) ||
		Pressed(now->bomb, previous->bomb);
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
		ReadInput(&input, 0);
		if (input.select)
			break;
		WaitVbl();
	}
}

static void WaitForInputRelease(void) {
	InputState input;
	do {
		ReadInput(&input, 0);
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
	/* OCS DDF fetches must begin on an 8-colour-clock boundary. The centred
	 * 336px DIW starts at x=121, whose raw fetch value is 52; writing that
	 * invalid half-word alignment lets hardware mask the value while our
	 * modulo still assumes the requested width, shifting successive rows by
	 * a word. Align the base first, then add the optional left prefetch. */
	const USHORT fwBase = (USHORT)(((x >> 1) - res) & ~7);
	const USHORT fw = (USHORT)(fwBase - (fetchExtraWordLeft ? 8 : 0));

	*copListEnd++ = offsetof(struct Custom, ddfstrt);
	*copListEnd++ = fw;
	*copListEnd++ = offsetof(struct Custom, ddfstop);
	*copListEnd++ = fw + (((fetchWidth >> 4) - 1) << 3);
	*copListEnd++ = offsetof(struct Custom, diwstrt);
	*copListEnd++ = x + (y << 8);
	*copListEnd++ = offsetof(struct Custom, diwstop);
	/* Keep the full 256-line window; shortening DIWSTOP clips the HUD border. */
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
	activeCopperLandColor = (UWORD*)(copListEnd + 1);
	copListEnd = copSetColor(copListEnd, GAME_COLOR_LAND, currentLandRgb);
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
	const USHORT fwBase = (USHORT)(((x >> 1) - res) & ~7);
	const USHORT fw = (USHORT)(fwBase - (fetchExtraWordLeft ? 8 : 0));

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

	/*
	 * Blank one raster line while changing fetch geometry and all five
	 * bitplane pointers.  At the wide 352px fetch there is not enough time
	 * after DDFSTOP to do this reliably when audio DMA is active.
	 */
	copPtr = copWaitDisplayYAt(copPtr, MENU_TICKER_Y - 2, 0xda);
	*copPtr++ = offsetof(struct Custom, bplcon0);
	*copPtr++ = 0;
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
	copPtr = copWaitDisplayYAt(copPtr, MENU_TICKER_Y - 1, 0xda);
	*copPtr++ = offsetof(struct Custom, bplcon0);
	*copPtr++ = (SCREEN_PLANES << 12) | (1 << 9);

	/* Use another blank raster while restoring the stationary menu. */
	copPtr = copWaitDisplayYAt(copPtr,
		MENU_TICKER_Y + FONT_HEIGHT - 1, 0xda);
	*copPtr++ = offsetof(struct Custom, bplcon0);
	*copPtr++ = 0;
	copPtr = copSetFetch(copPtr, SCREEN_WIDTH, 0);
	copPtr = copSetBplcon1(copPtr, 0, 0);
	copPtr = copSetModulo(copPtr, SCREEN_ROW_BYTES, SCREEN_WIDTH);
	for (int plane = 0; plane < SCREEN_PLANES; plane++)
		planes[plane] = screen +
			(MENU_TICKER_Y + FONT_HEIGHT + 1) *
				SCREEN_PLANES * SCREEN_ROW_BYTES +
			SCREEN_ROW_BYTES * plane;
	copPtr = copSetPlanesEx(0, copPtr, planes, SCREEN_PLANES,
		COPPER_TRACK_NONE);
	copPtr = copWaitDisplayYAt(copPtr,
		MENU_TICKER_Y + FONT_HEIGHT, 0xda);
	*copPtr++ = offsetof(struct Custom, bplcon0);
	*copPtr++ = (SCREEN_PLANES << 12) | (1 << 9);

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
	/* A wide 352px world leaves too little guaranteed bus time to replace all
	 * HUD registers after its final fetch, especially with four Paula DMA
	 * channels active. End the world one raster line early, disable bitplanes,
	 * and use that black separator line as a deterministic transition slot.
	 * HUD DMA is not enabled until every pointer/modulo register is ready. */
	copPtr = copWaitDisplayYAt(copPtr, HUD_TOP - 2, 0xda);
	*copPtr++ = offsetof(struct Custom, bplcon0);
	*copPtr++ = 0;
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
	for (int plane = 0; plane < HUD_PLANES; plane++)
		planes[plane] = hud + SCREEN_ROW_BYTES * plane;
	copPtr = copSetPlanesEx(0, copPtr, planes, HUD_PLANES, COPPER_TRACK_HUD);

	/* All HUD state is now prepared. Wait until the separator line's own
	 * fetch window has passed, then enable the four HUD planes for line 168. */
	copPtr = copWaitDisplayYAt(copPtr, HUD_TOP - 1, 0xda);
	*copPtr++ = offsetof(struct Custom, bplcon0);
	hudCopBplcon0OperandPtr = copPtr;
	*copPtr++ = (HUD_PLANES << 12) | (1 << 9);

	/* Sea colour for the instrument panel band - deliberately placed here
	 * rather than alongside the sky colour right after the WAIT above: that
	 * transition's timing budget is already tight (see the comment on it),
	 * so any register write added there risks reintroducing the Sprint
	 * 14.91.4 HUD "ghost" bug. By the time the HUD's own bitplane pointers
	 * are set (just above), the tight transition is fully complete and
	 * there's no further timing pressure before end of frame. GAME_COLOR_SEA
	 * isn't one of the pens (0/1/5/6/9) the HUD's own graphics use, so this
	 * has no visible effect today - kept only for parity with the reference
	 * table. GAME_COLOR_LAND is restored to the base palette here because its
	 * world-band value now follows CPC's later-mission palette phases while it
	 * still doubles as HUD_COLOR_SAFE (the FUEL/LIVES "ok" green). COLOR10
	 * (black) isn't
	 * written here either - identical to the bulk-loaded value at every
	 * band, never actually changes. */
	copPtr = copSetColor(copPtr, GAME_COLOR_LAND, palette[GAME_COLOR_LAND]);
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
		/* A full five-plane clear spans several VBlanks on a stock A500.
		 * Keep the four-channel menu replayer current while the new page is
		 * visibly filling instead of waiting until all 256 rows are done. */
		if ((y & 7) == 7)
			serviceModMusicToCurrentVbl();
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
		serviceModMusicToCurrentVbl();
	}
}

/* Gameplay overlays do not need to service the menu MOD replayer. Keeping
 * that stateful call out of the pause transition also makes the transition a
 * small, bounded leaf-like draw while all four Paula effect voices are being
 * stopped. */
static void drawTextWithoutMusicService(UBYTE* bitmap, short x, short y,
	const char* text, UBYTE color) {
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
		serviceModMusicToCurrentVbl();
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
	if (x < 0 || x >= (WORD)MENU_TICKER_SOURCE_WIDTH ||
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
		/* Long editable tickers take several PAL frames to rasterise on a
		 * 68000.  Poll the MOD clock after every glyph so Paula never waits
		 * for the complete off-screen string. */
		serviceModMusicToCurrentVbl();
	}
}

static const char* fieldGuideTickerTextForMode(short gameMode) {
	return gameMode == GAME_MODE_CLASSIC ?
		fieldGuideTickerClassicText : fieldGuideTickerEnhancedText;
}

static void initMenuTickerForText(UBYTE* bitmap, const char* text) {
	WORD cycleWidth = (WORD)(strlen(text) * FONT_WIDTH +
		MENU_TICKER_GAP_PIXELS);
	memset(bitmap, 0, MENU_TICKER_BITMAP_BYTES);
	drawMenuTickerText(bitmap,
		MENU_TICKER_MARGIN_PIXELS + MENU_TICKER_LEAD_PIXELS,
		text);
	/* Both attract screens use one-shot text. The visible source after the
	 * final glyph therefore stays blank until the caller changes screen; a
	 * second wrapped copy would otherwise enter from the right before the
	 * first copy had completely left the left edge. */
	(void)cycleWidth;
	activeMenuTickerText = text;
	menuTickerScrollX = 0;
	menuTickerFrameDivider = 0;
	menuTickerCompleted = 0;
}

static void drawMenuTicker(UBYTE* bitmap) {
	/* Copper supplies this band from menuTickerBitmap. */
	fillRect(bitmap, 0, MENU_TICKER_Y, SCREEN_WIDTH, FONT_HEIGHT,
		MENU_COLOR_PANEL);
}

static UBYTE updateMenuTicker(void) {
	menuTickerFrameDivider++;
	if (menuTickerFrameDivider < MENU_TICKER_FRAME_DIVIDER)
		return 0;
	menuTickerFrameDivider = 0;
	UWORD textWidth = (UWORD)(strlen(activeMenuTickerText) * FONT_WIDTH);
	UWORD cycleWidth = (UWORD)(MENU_TICKER_LEAD_PIXELS + textWidth +
		MENU_TICKER_GAP_PIXELS);
	UWORD fine;
	LONG pointerPixelX;
	USHORT byteOffset;
	UWORD delay;

	menuTickerScrollX++;
	UBYTE completedNow = 0;
	/* The Copper source pointer already includes MENU_TICKER_MARGIN_PIXELS,
	 * exactly matching the glyph's source X. Consequently the last glyph
	 * leaves the left edge after textWidth pixels, not margin+textWidth. */
	if (!menuTickerCompleted && menuTickerScrollX >=
		MENU_TICKER_LEAD_PIXELS + textWidth) {
		menuTickerCompleted = 1;
		completedNow = 1;
	}
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
	return completedNow;
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
	/* With the menu-only HUD removed, use the lower PAL area for a roomier,
	 * readable two-column settings block instead of crowding all choices into
	 * the first 160 scanlines.  The high-score table's final 8px glyph row ends
	 * at y=114, so starting at y=124 leaves a visible separator rather than
	 * making START GAME look like an eighth score-table row. */
	static const short itemY[MENU_ITEM_COUNT] = {
		124, 140, 156, 172, 188, 204, 220, 236
	};
	return itemY[item];
}

static void copyMenuText(char* dest, const char* src) {
	while (*src)
		*dest++ = *src++;
	*dest = 0;
}

static void menuItemText(short item, short skillLevel, short gameModeSetting, short wingmanControl, char* text) {
	switch (item) {
		case MENU_ITEM_START:
			copyMenuText(text, HAR_TEXT_START_GAME);
			break;
		case MENU_ITEM_SKILL:
			copyMenuText(text, "Skill level: 1");
			text[13] = (char)('0' + skillLevel);
			break;
		case MENU_ITEM_GAME_MODE:
			copyMenuText(text, gameModeSetting == GAME_MODE_CLASSIC ?
				"Mode: Classic" : "Mode: Enhanced");
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
		case MENU_ITEM_LOCK_HEIGHT:
			copyMenuText(text, menuRocketHeightLock
				? "Lock height: On" : "Lock height: Off");
			break;
		case MENU_ITEM_ROCKET_RANGE:
			copyMenuText(text, "Rocket range: 10");
			text[14] = (char)('0' + (menuRocketRangeTiles / 10));
			text[15] = (char)('0' + (menuRocketRangeTiles % 10));
			break;
		case MENU_ITEM_CONTROLS:
			copyMenuText(text, "Controls...");
			break;
		case MENU_ITEM_EXIT_DOS:
			copyMenuText(text, "Exit to DOS");
			break;
		default:
			text[0] = 0;
			break;
	}
}

static void drawMenuOption(UBYTE* bitmap, short selected, short y, const char* text) {
	fillRect(bitmap, 34 + MENU_CONTENT_X_OFFSET, y - 1, 150, 10, MENU_COLOR_PANEL);
	if (selected)
		drawTextStyled(bitmap, 42 + MENU_CONTENT_X_OFFSET, y, ">", FONT_STYLE_CPC_BLUE);
	drawTextStyled(bitmap, 54 + MENU_CONTENT_X_OFFSET, y, text, FONT_STYLE_CPC_BLUE);
}

static void drawMenuItem(UBYTE* bitmap, short item, short selected, short skillLevel, short gameModeSetting, short wingmanControl) {
	char text[24];
	menuItemText(item, skillLevel, gameModeSetting, wingmanControl, text);
	drawMenuOption(bitmap, selected, menuItemY(item), text);
}

static void drawMenuItems(UBYTE* bitmap, short selected, short skillLevel, short gameModeSetting, short wingmanControl) {
	for (short item = 0; item < MENU_ITEM_COUNT; item++)
		drawMenuItem(bitmap, item, selected == item, skillLevel, gameModeSetting, wingmanControl);
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

/* Fixed control help, deliberately separated from blue selectable settings.
 * ReadInput() really does merge keyboard, joystick and mouse. CPC's L+Fire
 * Maverick chord is likewise a fixed action, not an on/off menu setting. */
static void drawMenuRightSettings(UBYTE* bitmap) {
	drawTextStyled(bitmap, 192 + MENU_CONTENT_X_OFFSET, 124,
		"CONTROL HELP", FONT_STYLE_CPC_GREEN);
	drawText(bitmap, 192 + MENU_CONTENT_X_OFFSET, 148,
		"INPUT: ALL", MENU_COLOR_WHITE);
	drawText(bitmap, 192 + MENU_CONTENT_X_OFFSET, 164,
		"MAV: L+FIRE", MENU_COLOR_WHITE);
}

#define CONTROL_MENU_ROW_COUNT 13
#define CONTROL_MENU_PLAYER_ROW 0
#define CONTROL_MENU_ACTION_FIRST 1
#define CONTROL_MENU_JOYSTICK_ROW 8
#define CONTROL_MENU_ROCKET_BUTTON_ROW 9
#define CONTROL_MENU_BOMB_BUTTON_ROW 10
#define CONTROL_MENU_DEFAULTS_ROW 11
#define CONTROL_MENU_BACK_ROW 12
#define CONTROL_MESSAGE_NONE 0
#define CONTROL_MESSAGE_DUPLICATE 1
#define CONTROL_MESSAGE_PORT_IN_USE 2

static void formatControlKeyName(UBYTE rawKey, char* text) {
	switch (rawKey) {
		case RAWKEY_NONE: copyMenuText(text, "NONE"); return;
		case RAWKEY_UP: copyMenuText(text, "UP"); return;
		case RAWKEY_DOWN: copyMenuText(text, "DOWN"); return;
		case RAWKEY_LEFT: copyMenuText(text, "LEFT"); return;
		case RAWKEY_RIGHT: copyMenuText(text, "RIGHT"); return;
		case RAWKEY_CONTROL: copyMenuText(text, "CTRL"); return;
		case RAWKEY_SPACE: copyMenuText(text, "SPACE"); return;
		case RAWKEY_RETURN: copyMenuText(text, "RETURN"); return;
		case RAWKEY_E: copyMenuText(text, "E"); return;
		case RAWKEY_W: copyMenuText(text, "W"); return;
		case RAWKEY_A: copyMenuText(text, "A"); return;
		case RAWKEY_S: copyMenuText(text, "S"); return;
		case RAWKEY_D: copyMenuText(text, "D"); return;
		case RAWKEY_B: copyMenuText(text, "B"); return;
		case RAWKEY_LEFT_ALT: copyMenuText(text, "L ALT"); return;
		case RAWKEY_RIGHT_ALT: copyMenuText(text, "R ALT"); return;
		case RAWKEY_KP_0: copyMenuText(text, "KP 0"); return;
		case RAWKEY_KP_2: copyMenuText(text, "KP 2"); return;
		case RAWKEY_KP_4: copyMenuText(text, "KP 4"); return;
		case RAWKEY_KP_6: copyMenuText(text, "KP 6"); return;
		case RAWKEY_KP_8: copyMenuText(text, "KP 8"); return;
		case RAWKEY_KP_ENTER: copyMenuText(text, "KP ENTER"); return;
		case RAWKEY_KP_DECIMAL: copyMenuText(text, "KP DEC"); return;
		default:
			text[0] = 'R'; text[1] = 'A'; text[2] = 'W'; text[3] = ' ';
			text[4] = "0123456789ABCDEF"[(rawKey >> 4) & 15];
			text[5] = "0123456789ABCDEF"[rawKey & 15];
			text[6] = 0;
			return;
	}
}

static const char* controlJoystickName(UBYTE port) {
	if (port == CONTROL_JOY_PORT_1)
		return "PORT 1";
	if (port == CONTROL_JOY_PORT_2)
		return "PORT 2";
	return "OFF";
}

static const char* controlButtonName(UBYTE button) {
	if (button == CONTROL_BUTTON_PRIMARY)
		return "BUTTON 1";
	if (button == CONTROL_BUTTON_SECONDARY)
		return "BUTTON 2";
	return "NONE";
}

static const char* const controlActionNames[CONTROL_ACTION_COUNT] = {
	"UP", "DOWN", "LEFT", "RIGHT", "ROCKET", "BOMB", "EJECT"
};

static short controlMenuRowY(UBYTE row) {
	return (short)(42 + row * 14);
}

static void drawControlsCursor(UBYTE* bitmap, UBYTE row, UBYTE visible) {
	short y = controlMenuRowY(row);
	fillRect(bitmap, 8, y, FONT_WIDTH, FONT_HEIGHT, MENU_COLOR_PANEL);
	if (visible)
		drawText(bitmap, 8, y, ">", MENU_COLOR_CYAN);
}

static void drawControlsRow(UBYTE* bitmap, UBYTE player, UBYTE row,
	UBYTE selected, UBYTE capture, UBYTE message) {
	ControlProfile* profile = &controlProfiles[player & 1];
	short y = controlMenuRowY(row);
	char value[12];
	value[0] = 0;

	/* A row is isolated by four blank scanlines. Clear and redraw only this
	 * 10-pixel band when its value/status changes; navigation normally touches
	 * only the cursor cell through drawControlsCursor(). */
	fillRect(bitmap, 8, y - 1, 304, 10, MENU_COLOR_PANEL);
	if (selected)
		drawText(bitmap, 8, y, ">", MENU_COLOR_CYAN);
	if (row == CONTROL_MENU_PLAYER_ROW) {
		drawText(bitmap, 24, y, "PLAYER", MENU_COLOR_WHITE);
		copyMenuText(value, player ? "2" : "1");
	} else if (row >= CONTROL_MENU_ACTION_FIRST &&
		row < CONTROL_MENU_JOYSTICK_ROW) {
		UBYTE action = (UBYTE)(row - CONTROL_MENU_ACTION_FIRST);
		drawText(bitmap, 24, y, controlActionNames[action], MENU_COLOR_WHITE);
		if (capture && selected)
			copyMenuText(value, "PRESS KEY");
		else if (message && selected)
			copyMenuText(value, "DUPLICATE");
		else
			formatControlKeyName(profile->key[action], value);
	} else if (row == CONTROL_MENU_JOYSTICK_ROW) {
		drawText(bitmap, 24, y, "JOYSTICK", MENU_COLOR_WHITE);
		copyMenuText(value, message == CONTROL_MESSAGE_PORT_IN_USE && selected ?
			"PORT IN USE" : controlJoystickName(profile->joystickPort));
	} else if (row == CONTROL_MENU_ROCKET_BUTTON_ROW) {
		drawText(bitmap, 24, y, "ROCKET BUTTON", MENU_COLOR_WHITE);
		copyMenuText(value, message == CONTROL_MESSAGE_DUPLICATE && selected ?
			"DUPLICATE" : controlButtonName(profile->rocketButton));
	} else if (row == CONTROL_MENU_BOMB_BUTTON_ROW) {
		drawText(bitmap, 24, y, "BOMB BUTTON", MENU_COLOR_WHITE);
		copyMenuText(value, message == CONTROL_MESSAGE_DUPLICATE && selected ?
			"DUPLICATE" : controlButtonName(profile->bombButton));
	} else if (row == CONTROL_MENU_DEFAULTS_ROW) {
		drawText(bitmap, 24, y, "RESTORE DEFAULTS", MENU_COLOR_YELLOW);
	} else {
		drawText(bitmap, 24, y, "BACK TO MENU", MENU_COLOR_GREEN);
	}
	if (value[0])
		drawText(bitmap, 176, y, value,
			(message && selected) ? MENU_COLOR_RED : MENU_COLOR_CYAN);
}

static void drawControlsScreen(UBYTE* bitmap, UBYTE player, UBYTE selected,
	UBYTE capture, UBYTE message) {
	fillScreen(bitmap, MENU_COLOR_PANEL);
	drawTextCenteredStyled(bitmap, 12, "CONTROLS", FONT_STYLE_CPC_GREEN);
	drawTextCentered(bitmap, 24,
		"SESSION PROFILE - ESC CANCELS CAPTURE", MENU_COLOR_SHADOW);
	for (UBYTE row = 0; row < CONTROL_MENU_ROW_COUNT; row++) {
		drawControlsRow(bitmap, player, row, row == selected, capture,
			message);
		if ((row & 3) == 3)
			serviceModMusicToCurrentVbl();
	}
	drawTextCentered(bitmap, 230,
		"ARROWS MOVE/CHANGE  FIRE/ENTER SELECT", MENU_COLOR_SHADOW);
	drawTextCentered(bitmap, 242,
		"BUTTON 2 NEEDS A 2-BUTTON PAD", MENU_COLOR_SHADOW);
}

static UBYTE controlKeyIsDuplicate(UBYTE player, UBYTE action,
	UBYTE rawKey) {
	for (UBYTE index = 0; index < CONTROL_ACTION_COUNT; index++) {
		if (index != action && controlProfiles[player].key[index] == rawKey)
			return 1;
	}
	return 0;
}

static UBYTE adjustControlOption(UBYTE player, UBYTE row, short direction,
	UBYTE* message) {
	ControlProfile* profile = &controlProfiles[player];
	*message = CONTROL_MESSAGE_NONE;
	if (row == CONTROL_MENU_PLAYER_ROW)
		return 0;
	if (row == CONTROL_MENU_JOYSTICK_ROW) {
		short next = (short)profile->joystickPort + direction;
		if (next < CONTROL_JOY_OFF) next = CONTROL_JOY_PORT_2;
		if (next > CONTROL_JOY_PORT_2) next = CONTROL_JOY_OFF;
		if (next != CONTROL_JOY_OFF &&
			controlProfiles[player ^ 1].joystickPort == (UBYTE)next) {
			*message = CONTROL_MESSAGE_PORT_IN_USE;
			return 0;
		}
		profile->joystickPort = (UBYTE)next;
		return 1;
	}
	if (row == CONTROL_MENU_ROCKET_BUTTON_ROW ||
		row == CONTROL_MENU_BOMB_BUTTON_ROW) {
		UBYTE* button = row == CONTROL_MENU_ROCKET_BUTTON_ROW ?
			&profile->rocketButton : &profile->bombButton;
		UBYTE other = row == CONTROL_MENU_ROCKET_BUTTON_ROW ?
			profile->bombButton : profile->rocketButton;
		short next = (short)*button + direction;
		if (next < CONTROL_BUTTON_NONE) next = CONTROL_BUTTON_SECONDARY;
		if (next > CONTROL_BUTTON_SECONDARY) next = CONTROL_BUTTON_NONE;
		if (next != CONTROL_BUTTON_NONE && (UBYTE)next == other) {
			*message = CONTROL_MESSAGE_DUPLICATE;
			return 0;
		}
		*button = (UBYTE)next;
		return 1;
	}
	return 0;
}


#define HIGH_SCORE_ENTRY_COUNT 7
#define HIGH_SCORE_NAME_LENGTH 7
#define HIGH_SCORE_SLOT_A_PATH "harrier_scores_a.dat"
#define HIGH_SCORE_SLOT_B_PATH "harrier_scores_b.dat"
#define HIGH_SCORE_LEGACY_PATH "harrier_scores.dat"
#define HIGH_SCORE_FILE_VERSION 1
#define HIGH_SCORE_FILE_HEADER_BYTES 12
#define HIGH_SCORE_FILE_ENTRY_BYTES (HIGH_SCORE_NAME_LENGTH + 1 + 2 + 4)
#define HIGH_SCORE_FILE_PAYLOAD_BYTES (HIGH_SCORE_ENTRY_COUNT * HIGH_SCORE_FILE_ENTRY_BYTES)
#define HIGH_SCORE_FILE_BYTES (HIGH_SCORE_FILE_HEADER_BYTES + HIGH_SCORE_FILE_PAYLOAD_BYTES + 4)

typedef struct HighScoreEntry {
	char name[HIGH_SCORE_NAME_LENGTH];
	UBYTE level;
	UWORD hits;
	ULONG score;
} HighScoreEntry;

static HighScoreEntry highScoreTable[HIGH_SCORE_ENTRY_COUNT];
static ULONG highScoreSaveGeneration = 0;
static UBYTE highScoreSaveNextSlot = 0;
static UBYTE highScoreSaveDirty = 0;

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

static void putHighScoreU16(UBYTE* out, UWORD value) {
	out[0] = (UBYTE)(value >> 8);
	out[1] = (UBYTE)value;
}

static void putHighScoreU32(UBYTE* out, ULONG value) {
	out[0] = (UBYTE)(value >> 24);
	out[1] = (UBYTE)(value >> 16);
	out[2] = (UBYTE)(value >> 8);
	out[3] = (UBYTE)value;
}

static UWORD getHighScoreU16(const UBYTE* in) {
	return (UWORD)(((UWORD)in[0] << 8) | in[1]);
}

static ULONG getHighScoreU32(const UBYTE* in) {
	return ((ULONG)in[0] << 24) | ((ULONG)in[1] << 16) |
		((ULONG)in[2] << 8) | in[3];
}

/* FNV-1a is small on 68000 and catches truncated/torn/corrupt saves. It is
 * not intended as security; its job is to reject bad media data safely. */
static ULONG highScoreChecksum(const UBYTE* data, UWORD length) {
	ULONG hash = 2166136261UL;
	for (UWORD i = 0; i < length; i++) {
		hash ^= data[i];
		hash *= 16777619UL;
	}
	return hash;
}

static UBYTE highScoreEntryIsSane(const HighScoreEntry* entry) {
	UBYTE terminated = 0;
	for (UBYTE i = 0; i < HIGH_SCORE_NAME_LENGTH; i++) {
		UBYTE c = (UBYTE)entry->name[i];
		if (!c) {
			terminated = 1;
			break;
		}
		if (c < 32 || c > 126)
			return 0;
	}
	return terminated && entry->level <= 99 && entry->score <= 999999UL;
}

static UBYTE highScoreTableIsSane(const HighScoreEntry* table) {
	for (UBYTE i = 0; i < HIGH_SCORE_ENTRY_COUNT; i++) {
		if (!highScoreEntryIsSane(&table[i]))
			return 0;
		if (i && table[i].score > table[i - 1].score)
			return 0;
	}
	return 1;
}

static UBYTE highScoreTablesEqual(const HighScoreEntry* left,
	const HighScoreEntry* right) {
	for (UBYTE i = 0; i < HIGH_SCORE_ENTRY_COUNT; i++) {
		for (UBYTE n = 0; n < HIGH_SCORE_NAME_LENGTH; n++) {
			if (left[i].name[n] != right[i].name[n])
				return 0;
		}
		if (left[i].level != right[i].level ||
			left[i].hits != right[i].hits || left[i].score != right[i].score)
			return 0;
	}
	return 1;
}

static void encodeHighScoreFile(UBYTE* out, ULONG generation) {
	out[0] = 'H'; out[1] = 'A'; out[2] = 'R'; out[3] = 'S';
	out[4] = HIGH_SCORE_FILE_VERSION;
	out[5] = HIGH_SCORE_ENTRY_COUNT;
	putHighScoreU16(out + 6, HIGH_SCORE_FILE_PAYLOAD_BYTES);
	putHighScoreU32(out + 8, generation);
	UBYTE* entryOut = out + HIGH_SCORE_FILE_HEADER_BYTES;
	for (UBYTE i = 0; i < HIGH_SCORE_ENTRY_COUNT; i++) {
		memcpy(entryOut, highScoreTable[i].name, HIGH_SCORE_NAME_LENGTH);
		entryOut[7] = highScoreTable[i].level;
		putHighScoreU16(entryOut + 8, highScoreTable[i].hits);
		putHighScoreU32(entryOut + 10, highScoreTable[i].score);
		entryOut += HIGH_SCORE_FILE_ENTRY_BYTES;
	}
	putHighScoreU32(out + HIGH_SCORE_FILE_BYTES - 4,
		highScoreChecksum(out, HIGH_SCORE_FILE_BYTES - 4));
}

static UBYTE decodeHighScoreFile(const UBYTE* in, HighScoreEntry* table,
	ULONG* generation) {
	if (in[0] != 'H' || in[1] != 'A' || in[2] != 'R' || in[3] != 'S' ||
		in[4] != HIGH_SCORE_FILE_VERSION || in[5] != HIGH_SCORE_ENTRY_COUNT ||
		getHighScoreU16(in + 6) != HIGH_SCORE_FILE_PAYLOAD_BYTES ||
		getHighScoreU32(in + HIGH_SCORE_FILE_BYTES - 4) !=
			highScoreChecksum(in, HIGH_SCORE_FILE_BYTES - 4))
		return 0;
	const UBYTE* entryIn = in + HIGH_SCORE_FILE_HEADER_BYTES;
	for (UBYTE i = 0; i < HIGH_SCORE_ENTRY_COUNT; i++) {
		memcpy(table[i].name, entryIn, HIGH_SCORE_NAME_LENGTH);
		table[i].level = entryIn[7];
		table[i].hits = getHighScoreU16(entryIn + 8);
		table[i].score = getHighScoreU32(entryIn + 10);
		entryIn += HIGH_SCORE_FILE_ENTRY_BYTES;
	}
	if (!highScoreTableIsSane(table))
		return 0;
	*generation = getHighScoreU32(in + 8);
	return 1;
}

/* pr_HomeDir is new with AmigaOS 2.0. The NDK header exposes it even when we
 * run on Kickstart/DOS 1.3, where reading past pr_WindowPtr accesses memory
 * outside the real Process structure. Keep the current directory on 1.3.
 * Bartman's startup first changes to dh1:, while a normal CLI/floppy launch
 * already has the launch directory current, so relative save names still end
 * up beside the executable in both supported launch paths. */
static BPTR enterHighScoreDirectory(UBYTE* changedDirectory) {
	*changedDirectory = 0;
	return 0;
}

static void leaveHighScoreDirectory(BPTR oldDirectory,
	UBYTE changedDirectory) {
	(void)oldDirectory;
	(void)changedDirectory;
}

static UBYTE readHighScoreSlot(const char* path, HighScoreEntry* table,
	ULONG* generation) {
	UBYTE bytes[HIGH_SCORE_FILE_BYTES];
	BPTR file = Open((CONST_STRPTR)path, MODE_OLDFILE);
	if (!file)
		return 0;
	LONG bytesRead = Read(file, bytes, HIGH_SCORE_FILE_BYTES);
	LONG extraRead = 0;
	UBYTE extraByte;
	if (bytesRead == HIGH_SCORE_FILE_BYTES)
		extraRead = Read(file, &extraByte, 1);
	Close(file);
	return bytesRead == HIGH_SCORE_FILE_BYTES && extraRead == 0 &&
		decodeHighScoreFile(bytes, table, generation);
}

static UBYTE generationIsNewer(ULONG candidate, ULONG reference) {
	return (LONG)(candidate - reference) > 0;
}

/* Write only one slot, then read and validate it before accepting it. The
 * other slot remains a complete previous generation if the disk fills,
 * becomes write-protected, is removed, or loses power during this write. */
static UBYTE flushHighScoreTable(void) {
	if (!highScoreSaveDirty)
		return 1;
	UBYTE changedDirectory;
	BPTR oldDirectory;
	APTR oldWindowPtr = suppressDosRequesters();
	oldDirectory = enterHighScoreDirectory(&changedDirectory);
	const char* path = highScoreSaveNextSlot ? HIGH_SCORE_SLOT_B_PATH :
		HIGH_SCORE_SLOT_A_PATH;
	ULONG generation = highScoreSaveGeneration + 1;
	UBYTE bytes[HIGH_SCORE_FILE_BYTES];
	encodeHighScoreFile(bytes, generation);
	UBYTE writtenOk = 0;
	BPTR file = Open((CONST_STRPTR)path, MODE_NEWFILE);
	if (file) {
		LONG bytesWritten = Write(file, bytes, HIGH_SCORE_FILE_BYTES);
		/* Close() had no defined return value before DOS V36 (Kickstart 2.0),
		 * so a stock 1.3 build must not inspect it. The reopen + exact read +
		 * checksum + table comparison below is the portable close/flush test. */
		Close(file);
		writtenOk = bytesWritten == HIGH_SCORE_FILE_BYTES;
	}
	if (writtenOk) {
		HighScoreEntry verifyTable[HIGH_SCORE_ENTRY_COUNT];
		ULONG verifyGeneration = 0;
		writtenOk = readHighScoreSlot(path, verifyTable, &verifyGeneration) &&
			verifyGeneration == generation &&
			highScoreTablesEqual(verifyTable, highScoreTable);
	}
	leaveHighScoreDirectory(oldDirectory, changedDirectory);
	restoreDosRequesters(oldWindowPtr);
	if (writtenOk) {
		highScoreSaveGeneration = generation;
		highScoreSaveNextSlot ^= 1;
		highScoreSaveDirty = 0;
	}
	return writtenOk;
}

/* Loaded exactly once before TakeSystem(). Both slots are independently
 * validated and the newest complete generation wins. A legacy raw table is
 * accepted only after strict sanity checks and migrated on the next save. */
static void loadHighScoreTable(void) {
	resetHighScoreTableToDefaults();
	highScoreSaveGeneration = 0;
	highScoreSaveNextSlot = 0;
	highScoreSaveDirty = 0;
	HighScoreEntry slotA[HIGH_SCORE_ENTRY_COUNT];
	HighScoreEntry slotB[HIGH_SCORE_ENTRY_COUNT];
	ULONG generationA = 0, generationB = 0;
	APTR oldWindowPtr = suppressDosRequesters();
	UBYTE changedDirectory;
	BPTR oldDirectory = enterHighScoreDirectory(&changedDirectory);
	UBYTE validA = readHighScoreSlot(HIGH_SCORE_SLOT_A_PATH, slotA,
		&generationA);
	UBYTE validB = readHighScoreSlot(HIGH_SCORE_SLOT_B_PATH, slotB,
		&generationB);
	if (validA || validB) {
		UBYTE useB = validB && (!validA ||
			generationIsNewer(generationB, generationA));
		memcpy(highScoreTable, useB ? slotB : slotA,
			sizeof(highScoreTable));
		highScoreSaveGeneration = useB ? generationB : generationA;
		highScoreSaveNextSlot = useB ? 0 : 1;
	} else {
		BPTR legacy = Open((CONST_STRPTR)HIGH_SCORE_LEGACY_PATH, MODE_OLDFILE);
		if (legacy) {
			HighScoreEntry legacyTable[HIGH_SCORE_ENTRY_COUNT];
			LONG bytesRead = Read(legacy, legacyTable, sizeof(legacyTable));
			UBYTE extraByte;
			LONG extraRead = bytesRead == (LONG)sizeof(legacyTable) ?
				Read(legacy, &extraByte, 1) : -1;
			Close(legacy);
			if (bytesRead == (LONG)sizeof(legacyTable) && extraRead == 0 &&
				highScoreTableIsSane(legacyTable)) {
				memcpy(highScoreTable, legacyTable, sizeof(highScoreTable));
				highScoreSaveDirty = 1;
			}
		}
	}
	leaveHighScoreDirectory(oldDirectory, changedDirectory);
	restoreDosRequesters(oldWindowPtr);
}

static void drawMenuHighScore(UBYTE* bitmap) {
	drawTextStyled(bitmap, 40 + MENU_CONTENT_X_OFFSET, 44, "NAME", FONT_STYLE_CPC_GREEN);
	drawTextStyled(bitmap, 128 + MENU_CONTENT_X_OFFSET, 44, "LEVEL", FONT_STYLE_CPC_GREEN);
	drawTextStyled(bitmap, 212 + MENU_CONTENT_X_OFFSET, 44, "HITS", FONT_STYLE_CPC_GREEN);
	drawTextStyled(bitmap, 268 + MENU_CONTENT_X_OFFSET, 44, "SCORE", FONT_STYLE_CPC_GREEN);

	/* Row pitch was exactly FONT_HEIGHT (8px), leaving zero blank scanline
	 * between rows - the 8px-tall glyphs touched top-to-bottom with no gap.
	 * Reported on real Amiga 600 hardware as descenders being overwritten by
	 * the row below (not corrupted, just no separation) - fine on Amiga 1200
	 * and in WinUAE, but real CRT/scandoubler scanline bleed makes a 0px gap
	 * risky. 53+row*9 keeps the same start/end bounds as before (row 0 still
	 * clears the y=44 header by 1px, the last row still clears itemY[0]=116
	 * by 1px) while inserting a 1px blank line between every row. */
	for (short row = 0; row < HIGH_SCORE_ENTRY_COUNT; row++) {
		short y = (short)(53 + row * 9);
		drawTextStyled(bitmap, 40 + MENU_CONTENT_X_OFFSET, y, highScoreTable[row].name, FONT_STYLE_CPC_GREEN);
		drawUnsignedPaddedStyled(bitmap, 136 + MENU_CONTENT_X_OFFSET, y, highScoreTable[row].level, 2, FONT_STYLE_CPC_GREEN);
		drawUnsignedPaddedStyled(bitmap, 212 + MENU_CONTENT_X_OFFSET, y, highScoreTable[row].hits, 5, FONT_STYLE_CPC_GREEN);
		drawUnsignedPaddedStyled(bitmap, 268 + MENU_CONTENT_X_OFFSET, y, highScoreTable[row].score, 6, FONT_STYLE_CPC_GREEN);
	}
}

static UWORD cpcFuelHudValue(const GameState* game) {
	UWORD remainingQuanta;
	if (game->fuelGaugeLevel == 0)
		return 0;
	remainingQuanta = (UWORD)((game->fuelGaugeLevel - 1) *
		CPC_FUEL_SUBCOUNT_FULL + game->fuelSubCounter);
	return (UWORD)(((ULONG)remainingQuanta * 999UL +
		(CPC_FUEL_TOTAL_QUANTA / 2)) / CPC_FUEL_TOTAL_QUANTA);
}

static void resetPlayerFuel(GameState* game) {
	game->fuelClockAccumulator = 0;
	game->fuelSubCounter = CPC_FUEL_SUBCOUNT_FULL;
	game->fuelGaugeLevel = CPC_FUEL_GAUGE_LEVELS;
	game->fuel = 999;
}

/* CPC timercountdown compares successive KL TIME values. Its high byte
 * changes every 256 ticks of the 300 Hz firmware clock, so a full 14x16 tank
 * lasts about 191.15 seconds. The PAL game loop is the clock here: adding 300
 * against a 50*256 limit preserves the same rational cadence without floats. */
static UBYTE updatePlayerFuel(GameState* game) {
	UWORD oldFuel = game->fuel;
	if (debugInfiniteFuel) {
		if (game->fuel != 999)
			resetPlayerFuel(game);
		return oldFuel != game->fuel;
	}
	if (game->fuelGaugeLevel == 0) {
		game->fuel = 0;
		return oldFuel != 0;
	}

	game->fuelClockAccumulator = (UWORD)(game->fuelClockAccumulator +
		CPC_FUEL_CLOCK_HZ);
	if (game->fuelClockAccumulator >= CPC_FUEL_CLOCK_LIMIT) {
		game->fuelClockAccumulator = (UWORD)(game->fuelClockAccumulator -
			CPC_FUEL_CLOCK_LIMIT);
		if (game->fuelSubCounter > 1) {
			game->fuelSubCounter--;
		} else {
			game->fuelSubCounter = CPC_FUEL_SUBCOUNT_FULL;
			if (game->fuelGaugeLevel > 0)
				game->fuelGaugeLevel--;
		}
		game->fuel = cpcFuelHudValue(game);
	}
	return oldFuel != game->fuel;
}

static void initGameState(GameState* game) {
	resetDestroyedTargets();
	resetCpcRandomSequence();
	resetRuntimeFlak();
	resetTargetLock();
	resetDestroyedShipColumns();
	resetLandCraters();
	resetPowerup(game);
	game->scrollX = 0;
	game->playerX = PLAYER_START_X;
	game->playerY = PLAYER_START_Y;
	game->speedLevel = GAME_SPEED_LEVEL_DEFAULT;
	game->gameMode = GAME_MODE_ENHANCED;
	game->score = 0;
	game->bonusScore = 0;
	game->missionStartScore = 0;
	game->hitsCount = 0;
	game->targetLock.active = 0;
	game->targetLock.worldX = 0;
	game->targetLock.y = 0;
	game->targetLock.targetType = 0; /* matches CPC_LAND_TARGET_NONE, defined later in the file */
	resetPlayerFuel(game);
	game->armour = 100;
	game->gameOver = 0;
	game->highScoreCommitted = 0;
	game->highScoreNameEntryActive = 0;
	game->highScoreNameLength = 0;
	game->highScoreNameKeySerial = keyboardMakeSerial;
	game->highScoreNameJoyChar = 0;
	memset(game->highScoreName, 0, sizeof(game->highScoreName));
	game->missionComplete = 0;
	game->missionCompleteTimer = 0;
	game->postLandingSlide = 0;
	game->landingState = LANDING_STATE_NONE;
	game->takeoffState = TAKEOFF_STATE_AIRBORNE;
	game->lives = PLAYER_START_LIVES;
	game->respawnSafeTimer = 0;
	game->flakDamageCount = 0;
	game->smokeDamageContact = 0;
	game->playerFrigateStatus = PLAYER_FRIGATE_STATUS_CLEAR;
	game->rockets = 12;
	game->bombs = 6;
	/* GameState lives on the 68000 stack and is not blanket-zeroed. Leaving
	 * this byte untouched gave a new session an arbitrary bomb lockout which
	 * only disappeared after that many gameplay frames had elapsed. */
	game->bombLaunchCooldown = 0;
	game->rocketHeightLock = menuRocketHeightLock;
	game->rocketRangeTiles = menuRocketRangeTiles;
	game->missionNumber = 1;
	resetCityFade(game);
	game->extraAircraftBonusSpawned = 0;
	memset(&game->rocketShot, 0, sizeof(game->rocketShot));
	memset(&game->bombShot, 0, sizeof(game->bombShot));
	memset(&game->impact, 0, sizeof(game->impact));
	memset(&game->enemyPlane, 0, sizeof(game->enemyPlane));
	memset(&game->enemyMissile, 0, sizeof(game->enemyMissile));
	memset(game->crashPart, 0, sizeof(game->crashPart));
	memset(&game->wingman, 0, sizeof(game->wingman));
	game->wingman.lastBombTargetColumn = -1;
	game->enemyRespawnTimer = 0;
	game->classicEnemySpawnPhase = 0;
	game->classicEnemySpawnRandomState = 0x6d2b;
	game->radarDetection = 0;
	game->radarClearance = 0;
	game->radarThreshold = 0;
	game->enemyShipMissileTriggerIndex = 0;
	game->enemyMissileFromShip = 0;
	game->enemyMissileTarget = ENEMY_TARGET_NONE;
	game->enemyPlaneRetreating = 0;
	game->enemyPlaneLogicPhase = 0;
	game->crashTimer = 0;
	game->crashEndsGame = 0;
	game->aircraftFailureState = AIRCRAFT_FAILURE_NONE;
	game->aircraftFailureCause = AIRCRAFT_FAILURE_CAUSE_NONE;
	game->aircraftFailureTimer = 0;
	game->aircraftFailureFallSpeed256 = 0;
	game->aircraftFailureY256 = 0;
	game->aircraftFailureAlarmFrame = 0;
	game->abandonedAircraftActive = 0;
	game->abandonedAircraftCrash = 0;
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

	/* Real CPC only awards score via explosionnoise() (HarrierAttackSourceNew2...
	 * asm:8250-8266), called exclusively on actual hits/kills - there is no
	 * distance/survival term anywhere in its scoring. game->bonusScore
	 * already accumulates only from hit events (see the *_SCORE_VALUE
	 * constants below); the previous `+ (scrollX >> 4)` term here awarded
	 * points for sheer distance flown, which doesn't exist on the real
	 * hardware and also meant the HUD redrew the score field constantly
	 * during normal flight instead of only on actual hits. */
	game->score = game->bonusScore;
	return oldScore != game->score;
}

static UBYTE highScoreTableQualifies(ULONG score) {
	ULONG lowestScore = highScoreTable[0].score;
	for (UBYTE i = 1; i < HIGH_SCORE_ENTRY_COUNT; i++) {
		if (highScoreTable[i].score < lowestScore)
			lowestScore = highScoreTable[i].score;
	}
	/* CPC enterhighscore rejects only scores below lastscore. Equal scores
	 * enter the table too (asm:3814-3820). */
	return score >= lowestScore;
}

static UBYTE updateHighScoreNamed(ULONG* highScore, const GameState* game,
	const char* playerName) {
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
	if (game->score >= highScoreTable[lowestIndex].score) {
		copyMenuText(highScoreTable[lowestIndex].name, playerName);
		/* CPC enterhighscore stores leveldifficulty, not gamelevelprogress or
		 * the world stage reached before death (asm:3863-3865). Sprint 15.75
		 * made this the authoritative selected-skill-plus-mission value. */
		highScoreTable[lowestIndex].level = game->levelDifficulty;
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

		highScoreSaveDirty = 1;
		changed = 1;
	}
	return changed;
}

/* Deterministic/headless callers still need a one-call commit. Interactive
 * runs use beginHighScoreNameEntry()/commitHighScoreNameEntry() below. */
static UBYTE updateHighScore(ULONG* highScore, const GameState* game) {
	return updateHighScoreNamed(highScore, game, "PLAYER");
}

static UBYTE beginHighScoreNameEntry(ULONG* highScore, GameState* game) {
	if (game->score > *highScore)
		*highScore = game->score;
	if (!highScoreTableQualifies(game->score)) {
		game->highScoreCommitted = 1;
		return 0;
	}
	game->highScoreNameEntryActive = 1;
	game->highScoreNameLength = 0;
	game->highScoreNameKeySerial = keyboardMakeSerial;
	game->highScoreNameJoyChar = 0;
	memset(game->highScoreName, 0, sizeof(game->highScoreName));
	return 1;
}

static void commitHighScoreNameEntry(ULONG* highScore, GameState* game) {
	if (!game->highScoreNameEntryActive)
		return;
	if (!game->highScoreNameLength)
		copyMenuText(game->highScoreName, "PLAYER");
	else
		game->highScoreName[game->highScoreNameLength] = 0;
	updateHighScoreNamed(highScore, game, game->highScoreName);
	game->highScoreNameEntryActive = 0;
	game->highScoreCommitted = 1;
}

static UBYTE rawKeyToHighScoreChar(UBYTE rawKey, char* value) {
	static const char numberRow[] = "`1234567890";
	static const char qwertyRow[] = "QWERTYUIOP";
	static const char homeRow[] = "ASDFGHJKL";
	static const char bottomRow[] = "ZXCVBNM";
	if (rawKey <= 0x0a) {
		*value = numberRow[rawKey];
		return rawKey != 0;
	}
	if (rawKey >= 0x10 && rawKey <= 0x19) {
		*value = qwertyRow[rawKey - 0x10];
		return 1;
	}
	if (rawKey >= 0x20 && rawKey <= 0x28) {
		*value = homeRow[rawKey - 0x20];
		return 1;
	}
	if (rawKey >= 0x31 && rawKey <= 0x37) {
		*value = bottomRow[rawKey - 0x31];
		return 1;
	}
	if (rawKey == RAWKEY_SPACE) {
		*value = ' ';
		return 1;
	}
	return 0;
}

static UBYTE appendHighScoreNameChar(ULONG* highScore, GameState* game,
	char value) {
	if (game->highScoreNameLength >= 6)
		return 0;
	game->highScoreName[game->highScoreNameLength++] = value;
	game->highScoreName[game->highScoreNameLength] = 0;
	game->highScoreNameJoyChar = 0;
	if (game->highScoreNameLength == 6) {
		commitHighScoreNameEntry(highScore, game);
		return 2;
	}
	return 1;
}

/* CPC printscores accepts direct keyboard characters and also exposes its
 * currentjoykey editor: Up/Down select space..Z, Left deletes and Right/Fire
 * accepts the selected character. Return completes a shorter name. Return
 * values: 0=no change, 1=redraw name editor, 2=entry committed. */
static UBYTE updateHighScoreNameEntry(ULONG* highScore, GameState* game,
	const InputState* input, const InputState* previousInput) {
	if (!game->highScoreNameEntryActive)
		return 0;

	if (game->highScoreNameKeySerial != keyboardMakeSerial) {
		game->highScoreNameKeySerial = keyboardMakeSerial;
		UBYTE rawKey = lastKeyboardMakeKey;
		if (rawKey == RAWKEY_RETURN || rawKey == RAWKEY_KP_ENTER) {
			commitHighScoreNameEntry(highScore, game);
			return 2;
		}
		if (rawKey == RAWKEY_BACKSPACE) {
			game->highScoreNameJoyChar = 0;
			if (game->highScoreNameLength) {
				game->highScoreNameLength--;
				game->highScoreName[game->highScoreNameLength] = 0;
				return 1;
			}
			return 0;
		}
		char value;
		if (rawKeyToHighScoreChar(rawKey, &value))
			return appendHighScoreNameChar(highScore, game, value);
	}

	if (Pressed(input->up, previousInput->up)) {
		if (!game->highScoreNameJoyChar)
			game->highScoreNameJoyChar = 'A';
		else if (game->highScoreNameJoyChar < 'Z')
			game->highScoreNameJoyChar++;
		return 1;
	}
	if (Pressed(input->down, previousInput->down)) {
		if (!game->highScoreNameJoyChar)
			game->highScoreNameJoyChar = 'A';
		else if (game->highScoreNameJoyChar > ' ')
			game->highScoreNameJoyChar--;
		return 1;
	}
	if (Pressed(input->left, previousInput->left)) {
		game->highScoreNameJoyChar = 0;
		if (game->highScoreNameLength) {
			game->highScoreNameLength--;
			game->highScoreName[game->highScoreNameLength] = 0;
			return 1;
		}
		return 0;
	}
	if (Pressed(input->right, previousInput->right) ||
		Pressed(input->select, previousInput->select)) {
		if (!game->highScoreNameJoyChar) {
			commitHighScoreNameEntry(highScore, game);
			return 2;
		}
		return appendHighScoreNameChar(highScore, game,
			game->highScoreNameJoyChar);
	}
	return 0;
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

static UBYTE cpcPlayerTileXForSpeedLevel(UBYTE speedLevel) {
	if (speedLevel > GAME_SPEED_LEVEL_MAX)
		speedLevel = GAME_SPEED_LEVEL_MAX;
	return (UBYTE)(CPC_PLAYER_SPEED_TILE_X_BASE +
		(speedLevel / CPC_PLAYER_SPEED_TILE_X_DIVISOR));
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
			telemetryLogGameEvent(TELEMETRY_GAME_EVENT_PLAYER_SPEED_CHANGE,
				1, (UWORD)(((LONG)game->scrollX + game->playerX) >> 3),
				game, game->speedLevel);
			return 1;
		}
		if (input->left && !input->right && game->speedLevel > GAME_SPEED_LEVEL_MIN) {
			game->speedLevel--;
			game->throttleRepeatTimer = GAME_THROTTLE_REPEAT_FRAMES;
			telemetryLogGameEvent(TELEMETRY_GAME_EVENT_PLAYER_SPEED_CHANGE,
				2, (UWORD)(((LONG)game->scrollX + game->playerX) >> 3),
				game, game->speedLevel);
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
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_LANDING_START, 0,
			(UWORD)(((LONG)game->scrollX + game->playerX) >> 3), game,
			game->speedLevel);
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
			game->wingman.returningToFormation = 0;
			game->wingman.interceptReason = 0;
			game->wingman.landingTargetSet = 0;
			game->wingman.rocket.active = 0;
			game->wingman.bomb.active = 0;
		}
	}
	return 1;
}

/* CPC draws SPEED/FUEL/ROCKETS/BOMBS as tick-segmented
 * gauge bars (drawgauge, HarrierAttackSourceNew2_alt_CRTC_CART16.asm:5249-5261
 * - 15 "empty gauge" tiles plus one "marker" tile) and ARMOUR as a bar that
 * erases one segment per hit (updatehealth, :2963-2985). SCORE and LIV remain
 * numeric; LIV is an Enhanced-mode extension. */
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
static void drawHudStatic(UBYTE* hud, UBYTE gameMode) {
	fillRect(hud, 0, 0, SCREEN_WIDTH, HUD_HEIGHT, HUD_COLOR_BACKGROUND);
	fillRect(hud, 0, 0, SCREEN_WIDTH, 1, GAME_COLOR_WHITE);

	drawTextStyled(hud, 8, 4, "SCORE", FONT_STYLE_CPC_HUD);
	drawTextStyled(hud, 104, 4, "HIGH", FONT_STYLE_CPC_HUD);
	drawTextStyled(hud, 200, 4, "AIR", FONT_STYLE_CPC_HUD);
	drawTextStyled(hud, 252, 4, "SK", FONT_STYLE_CPC_HUD);
	drawTextStyled(hud, 284, 4, "LV", FONT_STYLE_CPC_HUD);

	drawTextStyled(hud, 8, 19, "ARMOUR", FONT_STYLE_CPC_HUD);

	drawTextStyled(hud, 36, 36, "SPEED", FONT_STYLE_CPC_HUD);
	drawTextStyled(hud, 144, 36, "FUEL", FONT_STYLE_CPC_HUD);
	if (gameMode == GAME_MODE_ENHANCED)
		drawTextStyled(hud, 240, 36, "RADAR", FONT_STYLE_CPC_HUD);

	drawTextStyled(hud, 58, 66, "ROCKETS", FONT_STYLE_CPC_HUD);
	drawTextStyled(hud, 214, 66, "BOMBS", FONT_STYLE_CPC_HUD);
}

/* Per-buffer tracked "last drawn" state. The gameplay HUD is now the only
 * authoritative HUD; Sprint 15.39 deliberately removed the differently-
 * paletted decorative copy from the main menu. */
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
	UWORD radarDetection;
	UBYTE radarColor;
	UBYTE lives;
	UBYTE livesColor;
	UBYTE skillLevel;
	UBYTE missionNumber;
	UBYTE overlayMode; /* 0=none, 1=game over, 2=mission complete, 3=name entry */
} HudRenderState;

static HudRenderState hudRenderState[HUD_BUFFER_COUNT];

static void drawHudValues(UBYTE* hud, const GameState* game, ULONG highScore, UBYTE hudBufferIndex) {
	HudRenderState* state = &hudRenderState[hudBufferIndex];
	UBYTE fullBombs;
	UBYTE fullRockets;
	ammoForSkill(game->levelDifficulty, &fullBombs, &fullRockets);
	UBYTE armourColor = (UBYTE)(game->armour == 0 ? HUD_COLOR_WARN : HUD_COLOR_VALUE);
	UBYTE fuelColor = (UBYTE)(game->fuel < 100 ? HUD_COLOR_WARN :
		HUD_COLOR_POWERUP_HEALTH);
	UBYTE livesColor = (UBYTE)(game->lives == 0 ? HUD_COLOR_WARN : HUD_COLOR_SAFE);
	UBYTE radarColor = (UBYTE)(game->radarDetection >= 900 ? HUD_COLOR_WARN :
		(game->radarDetection >= RADAR_DETECTION_ALARM_START ?
		HUD_COLOR_VALUE : HUD_COLOR_SAFE));
	UBYTE overlayMode = (UBYTE)(game->highScoreNameEntryActive ? 3 :
		(game->gameOver ? 1 : (game->missionComplete ? 2 : 0)));

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
		/* Pull the score digits half a character left. At x=56 the sixth
		 * digit touched HIGH at x=104, visually merging the two fields. */
		fillRect(hud, 52, 4, 48, 8, HUD_COLOR_BACKGROUND);
		drawUnsignedPaddedStyled(hud, 52, 4, game->score, 6, FONT_STYLE_CPC_HUD);
		fillRect(hud, 144, 4, 48, 8, HUD_COLOR_BACKGROUND);
		drawUnsignedPaddedStyled(hud, 144, 4, highScore, 6, FONT_STYLE_CPC_HUD);
		fillRect(hud, 232, 4, 16, 8, HUD_COLOR_BACKGROUND);
		drawUnsignedPadded(hud, 232, 4, game->lives, 2, livesColor);
		fillRect(hud, 268, 4, 8, 8, HUD_COLOR_BACKGROUND);
		drawUnsignedPaddedStyled(hud, 268, 4, game->skillLevel, 1,
			FONT_STYLE_CPC_HUD);
		fillRect(hud, 300, 4, 16, 8, HUD_COLOR_BACKGROUND);
		drawUnsignedPaddedStyled(hud, 300, 4, game->missionNumber, 2,
			FONT_STYLE_CPC_HUD);
		drawHudGaugeBar(hud, 64, 17, 232, 10, armourColor, game->armour, 100);
		drawHudGaugeBar(hud, 8, 50, 96, 10, HUD_COLOR_VALUE, game->speedLevel, GAME_SPEED_LEVEL_MAX);
		drawHudGaugeBar(hud, 112, 50, 96, 10, fuelColor, game->fuel, 999);
		if (game->gameMode == GAME_MODE_ENHANCED)
			drawHudGaugeBar(hud, 216, 50, 96, 10, radarColor,
				game->radarDetection, RADAR_DETECTION_MAX);
		drawHudGaugeBar(hud, 16, 80, 140, 8,
			HUD_COLOR_POWERUP_ROCKETS, game->rockets, fullRockets);
		drawHudGaugeBar(hud, 164, 80, 140, 8,
			HUD_COLOR_POWERUP_BOMBS, game->bombs, fullBombs);
	} else {
		if (state->score != game->score)
			drawUnsignedPaddedDelta(hud, 52, 4, state->score, game->score, 6, FONT_STYLE_CPC_HUD);
		if (state->highScore != highScore)
			drawUnsignedPaddedDelta(hud, 144, 4, state->highScore, highScore, 6, FONT_STYLE_CPC_HUD);
		if (state->lives != game->lives || state->livesColor != livesColor) {
			fillRect(hud, 232, 4, 16, 8, HUD_COLOR_BACKGROUND);
			drawUnsignedPadded(hud, 232, 4, game->lives, 2, livesColor);
		}
		if (state->skillLevel != game->skillLevel)
			drawUnsignedPaddedDelta(hud, 268, 4, state->skillLevel,
				game->skillLevel, 1, FONT_STYLE_CPC_HUD);
		if (state->missionNumber != game->missionNumber)
			drawUnsignedPaddedDelta(hud, 300, 4, state->missionNumber,
				game->missionNumber, 2, FONT_STYLE_CPC_HUD);
		drawHudGaugeBarDelta(hud, 64, 17, 232, 10, armourColor, state->armourColor, state->armour, game->armour, 100);
		/* SPEED/FUEL sit under the GAME OVER/LANDED overlay rect - skip
		 * updating them while it's showing, no point drawing what the
		 * overlay immediately covers; restored when the overlay clears. */
		if (overlayMode == 0) {
			drawHudGaugeBarDelta(hud, 8, 50, 96, 10, HUD_COLOR_VALUE, HUD_COLOR_VALUE, state->speedLevel, game->speedLevel, GAME_SPEED_LEVEL_MAX);
			drawHudGaugeBarDelta(hud, 112, 50, 96, 10, fuelColor, state->fuelColor, state->fuel, game->fuel, 999);
			if (game->gameMode == GAME_MODE_ENHANCED)
				drawHudGaugeBarDelta(hud, 216, 50, 96, 10, radarColor,
					state->radarColor, state->radarDetection,
					game->radarDetection, RADAR_DETECTION_MAX);
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
			/* Match the GAME OVER panel's stable footprint so both status
			 * transitions restore the same HUD rectangle. Green/yellow marks a
			 * successful carrier recovery without borrowing the fatal red box. */
			fillRect(hud, 42, 31, 236, 35, HUD_COLOR_SAFE);
			fillRect(hud, 44, 33, 232, 32, HUD_COLOR_VALUE);
			fillRect(hud, 46, 35, 228, 28, HUD_COLOR_BACKGROUND);
			drawTextCentered(hud, 45, "LANDED", HUD_COLOR_SAFE);
		} else if (overlayMode == 3) {
			char entryText[7];
			for (UBYTE i = 0; i < 6; i++) {
				if (i < game->highScoreNameLength)
					entryText[i] = game->highScoreName[i];
				else if (i == game->highScoreNameLength &&
					game->highScoreNameJoyChar)
					entryText[i] = game->highScoreNameJoyChar;
				else
					entryText[i] = '_';
			}
			entryText[6] = 0;
			fillRect(hud, 42, 31, 236, 35, HUD_COLOR_SAFE);
			fillRect(hud, 44, 33, 232, 32, HUD_COLOR_VALUE);
			fillRect(hud, 46, 35, 228, 28, HUD_COLOR_BACKGROUND);
			drawTextCentered(hud, 38, "HIGH SCORE", HUD_COLOR_SAFE);
			drawTextCentered(hud, 52, entryText, HUD_COLOR_VALUE);
		} else {
			/* Overlay just cleared - restore what's normally there. */
			drawTextStyled(hud, 36, 36, "SPEED", FONT_STYLE_CPC_HUD);
			drawTextStyled(hud, 144, 36, "FUEL", FONT_STYLE_CPC_HUD);
			if (game->gameMode == GAME_MODE_ENHANCED)
				drawTextStyled(hud, 240, 36, "RADAR", FONT_STYLE_CPC_HUD);
			drawHudGaugeBar(hud, 8, 50, 96, 10, HUD_COLOR_VALUE, game->speedLevel, GAME_SPEED_LEVEL_MAX);
			drawHudGaugeBar(hud, 112, 50, 96, 10, fuelColor, game->fuel, 999);
			if (game->gameMode == GAME_MODE_ENHANCED)
				drawHudGaugeBar(hud, 216, 50, 96, 10, radarColor,
					game->radarDetection, RADAR_DETECTION_MAX);
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
	state->radarDetection = game->radarDetection;
	state->radarColor = radarColor;
	state->lives = game->lives;
	state->livesColor = livesColor;
	state->skillLevel = game->skillLevel;
	state->missionNumber = game->missionNumber;
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
	drawHudStatic(hud, game->gameMode);
	drawHudValues(hud, game, highScore, hudBufferIndex);
}

static void drawMenuScreen(UBYTE* bitmap, short selected, short skillLevel, short gameModeSetting, short wingmanControl, ULONG highScore) {
	(void)highScore;
	/* Every entry to the menu is a fresh one-shot ticker pass.  Its text is
	 * pre-rendered one full screen beyond the right edge, so the visible band
	 * starts empty even when returning early from another page. */
	initMenuTickerForText(menuTickerBitmap,
		menuTickerText);
	fillScreen(bitmap, MENU_COLOR_PANEL);
	serviceModMusicToCurrentVbl();

	drawMenuTicker(bitmap);
	drawMenuNotice(bitmap, "", MENU_COLOR_WHITE);
	drawTextCenteredStyled(bitmap, 28, HAR_TEXT_TITLE, FONT_STYLE_CPC_GREEN);
	serviceModMusicToCurrentVbl();
	drawMenuHighScore(bitmap);
	serviceModMusicToCurrentVbl();
	drawMenuItems(bitmap, selected, skillLevel, gameModeSetting, wingmanControl);
	drawMenuRightSettings(bitmap);
	serviceModMusicToCurrentVbl();
	/* Deliberate Amiga menu direction: no preview HUD. The gameplay HUD uses
	 * gamePalette and live state; duplicating it under menuPalette produced
	 * misleading colours and coupled every HUD change to a second renderer. */
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

static void telemetryLogGameEvent(UBYTE code, UBYTE reason,
	UWORD worldColumn, const GameState* game, UWORD value) {
	if ((!telemetryEnabled && !HAR_DEBUG_PERF_LOG) || !telemetryAvailable ||
		code == TELEMETRY_GAME_EVENT_NONE ||
		code >= TELEMETRY_GAME_EVENT_CODE_COUNT)
		return;
	/* One rejected enemy-spawn audit is produced per generated column. Keep
	 * that useful counter, but coalesce consecutive equal rejection reasons
	 * so the 16-entry history is not completely hidden by ESP NO rows. */
	if ((code == TELEMETRY_GAME_EVENT_ENEMY_SPAWN_NO ||
		code == TELEMETRY_GAME_EVENT_ENEMY_PLANE_BLOCKED) &&
		telemetryGameEventCount > 0) {
		UBYTE previousIndex = (UBYTE)((telemetryGameEventIndex +
			TELEMETRY_GAME_EVENT_COUNT - 1) % TELEMETRY_GAME_EVENT_COUNT);
		TelemetryGameEvent* previous = &telemetryGameEvents[previousIndex];
		if (previous->code == code && previous->reason == reason) {
			previous->frame = frameCounter;
			previous->worldColumn = worldColumn;
			previous->value = value;
			previous->playerX = game ? game->playerX : 0;
			previous->playerY = game ? game->playerY : 0;
			previous->fuel = game ? game->fuel : 0;
			previous->armour = game ? game->armour : 0;
			previous->randomState = game ? game->classicEnemySpawnRandomState : 0;
			previous->playerRow = game ? (UBYTE)(game->playerY >> 3) : 0;
			previous->skill = game ? game->skillLevel : 0;
			previous->speed = game ? game->speedLevel : 0;
			previous->gameMode = game ? game->gameMode : GAME_MODE_CLASSIC;
			if (previous->repeats < 255)
				previous->repeats++;
			telemetryGameEventTotal++;
			telemetryGameEventCounters[code]++;
			return;
		}
	}
	TelemetryGameEvent* event = &telemetryGameEvents[telemetryGameEventIndex];
	event->frame = frameCounter;
	event->worldColumn = worldColumn;
	event->value = value;
	event->playerX = game ? game->playerX : 0;
	event->playerY = game ? game->playerY : 0;
	event->fuel = game ? game->fuel : 0;
	event->armour = game ? game->armour : 0;
	event->randomState = game ? game->classicEnemySpawnRandomState : 0;
	event->code = code;
	event->reason = reason;
	event->playerRow = game ? (UBYTE)(game->playerY >> 3) : 0;
	event->skill = game ? game->skillLevel : 0;
	event->speed = game ? game->speedLevel : 0;
	event->gameMode = game ? game->gameMode : GAME_MODE_CLASSIC;
	event->repeats = 1;
	telemetryGameEventIndex = (UBYTE)((telemetryGameEventIndex + 1) %
		TELEMETRY_GAME_EVENT_COUNT);
	if (telemetryGameEventCount < TELEMETRY_GAME_EVENT_COUNT)
		telemetryGameEventCount++;
	telemetryGameEventTotal++;
	telemetryGameEventCounters[code]++;
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
	memset(telemetryGameEvents, 0, sizeof(telemetryGameEvents));
	memset(telemetryGameEventCounters, 0,
		sizeof(telemetryGameEventCounters));
	telemetryGameEventIndex = 0;
	telemetryGameEventCount = 0;
	telemetryGameEventTotal = 0;
	telemetryLastGameplayStage = 0xff;
	telemetryRadarAboveFrames = 0;
	telemetryRadarBelowFrames = 0;
	telemetryRadarGain = 0;
	telemetryRadarDrain = 0;
	telemetryRadarAlarmPulses = 0;
	telemetryRadarLevel = 0;
	telemetryRadarClearance = 0;
	telemetryRadarThreshold = 0;
	telemetryClassicAirAdmissionTicks = 0;
	telemetryClassicAirEnemyOutcomes = 0;
	telemetryClassicAirPowerupOutcomes = 0;
	telemetryClassicPowerupWhileEnemy = 0;
	telemetryEnhancedPowerupWhileEnemy = 0;
	telemetryWingFormationStops = 0;
	telemetryWingFormationCardinal = 0;
	telemetryWingFormationDiagonal = 0;
	telemetryWingFormationEvasive = 0;
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
	drawTelemetryTextCentered(bitmap, 188,
		"LEFT/RIGHT PAGE  R RESET  SPACE BACK", MENU_COLOR_YELLOW);

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
		drawTelemetryTextCentered(bitmap, 88, "WAITING FOR FIRST 2S SAMPLE", MENU_COLOR_CYAN);
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

static const char* telemetryGameEventName(UBYTE code) {
	switch (code) {
		case TELEMETRY_GAME_EVENT_ENEMY_SPAWN_OK: return "ESP OK";
		case TELEMETRY_GAME_EVENT_ENEMY_SPAWN_NO: return "ESP NO";
		case TELEMETRY_GAME_EVENT_ENEMY_MISSILE: return "MISSIL";
		case TELEMETRY_GAME_EVENT_WING_INTERCEPT_OK: return "INT OK";
		case TELEMETRY_GAME_EVENT_WING_INTERCEPT_NO: return "INT NO";
		case TELEMETRY_GAME_EVENT_WING_BOMB_OK: return "BMB OK";
		case TELEMETRY_GAME_EVENT_WING_BOMB_NO: return "BMB NO";
		case TELEMETRY_GAME_EVENT_P2_LEFT_BEHIND: return "P2LEFT";
		case TELEMETRY_GAME_EVENT_WING_POWERUP: return "WPOWER";
		case TELEMETRY_GAME_EVENT_TERRAIN_STATE: return "STAGE";
		case TELEMETRY_GAME_EVENT_CITY_TO_PIER: return "PIER";
		case TELEMETRY_GAME_EVENT_ENEMY_PLANE_BLOCKED: return "EPL BL";
		case TELEMETRY_GAME_EVENT_AIRCRAFT_FAILURE: return "FAIL";
		case TELEMETRY_GAME_EVENT_AIRCRAFT_EJECT: return "EJECT";
		case TELEMETRY_GAME_EVENT_AIRCRAFT_IMPACT: return "IMPACT";
		case TELEMETRY_GAME_EVENT_AIRCRAFT_RESCUED: return "RESCUE";
		case TELEMETRY_GAME_EVENT_PLAYER_MOVE_LIMIT: return "MOV LIM";
		case TELEMETRY_GAME_EVENT_PLAYER_SPEED_CHANGE: return "SPEED";
		case TELEMETRY_GAME_EVENT_ROCKET_FIRE: return "ROCKET";
		case TELEMETRY_GAME_EVENT_BOMB_RELEASE: return "BOMB";
		case TELEMETRY_GAME_EVENT_MAVERICK_LOCK: return "MAV LK";
		case TELEMETRY_GAME_EVENT_MAVERICK_LOCK_LOST: return "MAV NO";
		case TELEMETRY_GAME_EVENT_PLAYER_FLAK_HIT: return "FLAK";
		case TELEMETRY_GAME_EVENT_PLAYER_MISSILE_HIT: return "MSL HIT";
		case TELEMETRY_GAME_EVENT_PLAYER_COLLISION: return "COLL";
		case TELEMETRY_GAME_EVENT_PLAYER_CRASH: return "CRASH";
		case TELEMETRY_GAME_EVENT_PLAYER_RESPAWN: return "RESPWN";
		case TELEMETRY_GAME_EVENT_LANDING_START: return "LAND ST";
		case TELEMETRY_GAME_EVENT_LANDING_COMPLETE: return "LANDED";
		case TELEMETRY_GAME_EVENT_WINGMAN_REVIVED: return "WREVIV";
		default: return "------";
	}
}

static void drawTelemetryGameEventsScreen(UBYTE* bitmap) {
	fillScreen(bitmap, MENU_COLOR_PANEL);
	drawTelemetryTextCentered(bitmap, 6, "GAME EVENTS", MENU_COLOR_GREEN);
	drawTelemetryTextCentered(bitmap, 16, HAR_BUILD_LABEL, MENU_COLOR_CYAN);
	drawTelemetryText(bitmap, 8, 30, "EVENT FRM COL Y S R N VAL", MENU_COLOR_WHITE);

	for (UBYTE row = 0; row < telemetryGameEventCount && row < 12; row++) {
		UBYTE index = (UBYTE)((telemetryGameEventIndex +
			TELEMETRY_GAME_EVENT_COUNT - 1 - row) %
			TELEMETRY_GAME_EVENT_COUNT);
		const TelemetryGameEvent* event = &telemetryGameEvents[index];
		short y = (short)(42 + row * 11);
		drawTelemetryText(bitmap, 8, y,
			telemetryGameEventName(event->code), MENU_COLOR_CYAN);
		drawTelemetryUnsignedPadded(bitmap, 45, y, event->frame, 5,
			MENU_COLOR_YELLOW);
		drawTelemetryUnsignedPadded(bitmap, 75, y, event->worldColumn, 4,
			MENU_COLOR_YELLOW);
		drawTelemetryUnsignedPadded(bitmap, 100, y, event->playerRow, 2,
			MENU_COLOR_YELLOW);
		drawTelemetryUnsignedPadded(bitmap, 115, y, event->skill, 1,
			MENU_COLOR_YELLOW);
		drawTelemetryUnsignedPadded(bitmap, 125, y, event->reason, 2,
			MENU_COLOR_YELLOW);
		drawTelemetryUnsignedPadded(bitmap, 140, y, event->repeats, 3,
			MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 160, y, event->value, 5,
			MENU_COLOR_YELLOW);
	}

	drawTelemetryText(bitmap, 8, 168, "RADAR% CLR THR UP DN AL", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 58, 168,
		telemetryRadarLevel / 10, 3, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 100, 168,
		telemetryRadarClearance, 3, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 140, 168,
		telemetryRadarThreshold, 3, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 180, 168,
		telemetryRadarAboveFrames / 50, 4, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 230, 168,
		telemetryRadarBelowFrames / 50, 4, MENU_COLOR_YELLOW);
	drawTelemetryUnsignedPadded(bitmap, 282, 168,
		telemetryRadarAlarmPulses, 3, MENU_COLOR_YELLOW);

	drawTelemetryText(bitmap, 8, 180, "TOTAL", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 48, 180, telemetryGameEventTotal, 5,
		MENU_COLOR_YELLOW);
	drawTelemetryText(bitmap, 88, 180, "SP", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 104, 180,
		telemetryGameEventCounters[TELEMETRY_GAME_EVENT_ENEMY_SPAWN_OK], 3,
		MENU_COLOR_YELLOW);
	drawTelemetryText(bitmap, 132, 180, "MS", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 148, 180,
		telemetryGameEventCounters[TELEMETRY_GAME_EVENT_ENEMY_MISSILE], 3,
		MENU_COLOR_YELLOW);
	drawTelemetryText(bitmap, 176, 180, "BI", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 192, 180,
		(UWORD)(telemetryGameEventCounters[TELEMETRY_GAME_EVENT_WING_BOMB_OK] +
		telemetryGameEventCounters[TELEMETRY_GAME_EVENT_WING_INTERCEPT_OK]), 3,
		MENU_COLOR_YELLOW);
	drawTelemetryText(bitmap, 220, 180, "F", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 230, 180,
		telemetryGameEventCounters[TELEMETRY_GAME_EVENT_AIRCRAFT_FAILURE], 2,
		MENU_COLOR_YELLOW);
	drawTelemetryText(bitmap, 252, 180, "E", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 262, 180,
		telemetryGameEventCounters[TELEMETRY_GAME_EVENT_AIRCRAFT_EJECT], 2,
		MENU_COLOR_YELLOW);
	drawTelemetryText(bitmap, 284, 180, "R", MENU_COLOR_WHITE);
	drawTelemetryUnsignedPadded(bitmap, 294, 180,
		telemetryGameEventCounters[TELEMETRY_GAME_EVENT_AIRCRAFT_RESCUED], 2,
		MENU_COLOR_YELLOW);
	drawTelemetryTextCentered(bitmap, 198,
		"LEFT/RIGHT PAGE  R RESET  SPACE BACK", MENU_COLOR_YELLOW);
}

static void drawPauseHudOverlay(UBYTE* hud, UBYTE visible) {
	static const char pauseText[] = "PAUSED - SPACE TO CONTINUE";
	/* Temporarily replace only the HUD's bottom row. The playfield, Copper
	 * split and hardware sprites remain untouched while paused. */
	fillRect(hud, 0, 64, SCREEN_WIDTH, HUD_HEIGHT - 64,
		HUD_COLOR_BACKGROUND);
	if (visible) {
		const short width = (short)((sizeof(pauseText) - 1) * FONT_WIDTH);
		drawTextWithoutMusicService(hud, (SCREEN_WIDTH - width) / 2, 72,
			pauseText, HUD_COLOR_VALUE);
	}
}

static void updateMenuSelection(UBYTE* bitmap, short oldSelected, short newSelected, short skillLevel, short gameModeSetting, short wingmanControl) {
	if (oldSelected == newSelected)
		return;

	(void)skillLevel;
	(void)gameModeSetting;
	(void)wingmanControl;
	drawMenuCursor(bitmap, oldSelected, 0);
	drawMenuCursor(bitmap, newSelected, 1);
}

/* direction: -1 for left, +1 for right/Select. Returns 1 only when a setting
 * changed. The selected row and (for Lives) the one live HUD value are the
 * only pixels redrawn. */
static UBYTE adjustSelectedMenuOption(UBYTE* bitmap, short selected, short direction,
	short* skillLevel, short* gameModeSetting, short* wingmanControl,
	ULONG highScore) {
	(void)highScore;
	if (selected == MENU_ITEM_SKILL) {
		*skillLevel += direction;
		if (*skillLevel < 1)
			*skillLevel = 5;
		else if (*skillLevel > 5)
			*skillLevel = 1;
	} else if (selected == MENU_ITEM_GAME_MODE) {
		*gameModeSetting = (*gameModeSetting == GAME_MODE_ENHANCED) ?
			GAME_MODE_CLASSIC : GAME_MODE_ENHANCED;
	} else if (selected == MENU_ITEM_WINGMAN) {
		*wingmanControl += direction;
		if (*wingmanControl < WINGMAN_CONTROL_OFF)
			*wingmanControl = WINGMAN_CONTROL_PLAYER2;
		else if (*wingmanControl > WINGMAN_CONTROL_PLAYER2)
			*wingmanControl = WINGMAN_CONTROL_OFF;
	} else if (selected == MENU_ITEM_LOCK_HEIGHT) {
		menuRocketHeightLock = !menuRocketHeightLock;
	} else if (selected == MENU_ITEM_ROCKET_RANGE) {
		if (direction < 0) {
			if (menuRocketRangeTiles <= ROCKET_RANGE_MIN_TILES)
				return 0;
			menuRocketRangeTiles--;
		} else {
			if (menuRocketRangeTiles >= ROCKET_RANGE_MAX_TILES)
				return 0;
			menuRocketRangeTiles++;
		}
	} else {
		return 0;
	}

	drawMenuItem(bitmap, selected, 1, *skillLevel, *gameModeSetting, *wingmanControl);
	/* Changing mode may also have coerced the Wingman row. */
	if (selected == MENU_ITEM_GAME_MODE)
		drawMenuItem(bitmap, MENU_ITEM_WINGMAN, 0, *skillLevel,
			*gameModeSetting, *wingmanControl);
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
#define BOMB_IMPACT_BOB_KIND_WATER_SMOKE_1 4
#define BOMB_IMPACT_BOB_KIND_WATER_SMOKE_2 5
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
	LONG worldX;
	UWORD byteX[ROCKET_PIXEL_BOB_MAX_PLACEMENTS];
	UBYTE byteCount[ROCKET_PIXEL_BOB_MAX_PLACEMENTS];
	UBYTE background[ROCKET_PIXEL_BOB_MAX_PLACEMENTS]
		[ROCKET_PIXEL_BOB_HEIGHT]
		[GAME_WORLD_DISPLAY_PLANES]
		[ROCKET_PIXEL_BOB_MAX_BYTES_PER_ROW];
} RocketShotFootprint;

static RocketShotFootprint rocketShotFootprints[GAME_WORLD_BUFFER_COUNT];
static RocketShotFootprint wingmanRocketFootprints[GAME_WORLD_BUFFER_COUNT];
static RocketShotFootprint enemyMissileFootprints[GAME_WORLD_BUFFER_COUNT];

typedef struct SeaWaveFootprint {
	UBYTE valid;
	UBYTE placementCount;
	UBYTE phase;
	WORD y;
	LONG worldColumn;
	UWORD byteX[SEA_WAVE_MAX_PLACEMENTS];
	UBYTE byteCount[SEA_WAVE_MAX_PLACEMENTS];
	UBYTE background[SEA_WAVE_MAX_PLACEMENTS][SEA_WAVE_HEIGHT]
		[GAME_WORLD_DISPLAY_PLANES][SEA_WAVE_MAX_BYTES_PER_ROW];
} SeaWaveFootprint;

typedef struct CarrierGull {
	UBYTE active;
	UBYTE scattering;
	UBYTE variant;
	UBYTE flapOffset;
	UBYTE scale;
	UBYTE maxScale;
	UBYTE scatterFrames;
	LONG worldX256;
	LONG y256;
	WORD velocityX256;
	WORD velocityY256;
} CarrierGull;

typedef struct CarrierGullFootprint {
	UBYTE valid;
	UBYTE placementCount;
	UBYTE phase;
	UBYTE variant;
	UBYTE scale;
	WORD y;
	LONG worldX;
	UWORD byteX[CARRIER_GULL_MAX_PLACEMENTS];
	UBYTE byteCount[CARRIER_GULL_MAX_PLACEMENTS];
	UBYTE background[CARRIER_GULL_MAX_PLACEMENTS][CARRIER_GULL_HEIGHT]
		[GAME_WORLD_DISPLAY_PLANES][CARRIER_GULL_MAX_BYTES_PER_ROW];
} CarrierGullFootprint;

static SeaWaveFootprint seaWaveFootprints[GAME_WORLD_BUFFER_COUNT]
	[SEA_WAVE_MAX];
static CarrierGull carrierGulls[CARRIER_GULL_MAX];
static CarrierGullFootprint carrierGullFootprints[GAME_WORLD_BUFFER_COUNT]
	[CARRIER_GULL_MAX];
static UBYTE carrierGullIdleFrames = 0;
static UBYTE carrierGullSpawnFrames = 0;
static UBYTE carrierGullSpawnInterval = CARRIER_GULL_SPAWN_INTERVAL_MIN_FRAMES;
static UBYTE carrierGullTargetCount = 0;
static UWORD carrierGullFlockFrames = 0;
static UWORD carrierGullFlockLifetime = 0;
static UWORD carrierGullLfsr = 0x593d;

/* Dirty-BOB counters are deliberately kept outside TelemetrySample so the
 * optional extended-memory telemetry ABI does not grow. They are process
 * totals that can be inspected directly in the debugger. */
static volatile ULONG bobWaveRedraws = 0;
static volatile ULONG bobWaveUnchangedSkips = 0;
static volatile ULONG bobGullRedraws = 0;
static volatile ULONG bobGullUnchangedSkips = 0;
static volatile ULONG bobImpactRedraws = 0;
static volatile ULONG bobImpactUnchangedSkips = 0;

#define AIRCRAFT_FAILURE_SMOKE_MAX 6
#define AIRCRAFT_FAILURE_SMOKE_LIFETIME 22
#define AIRCRAFT_FAILURE_SMOKE_SPAWN_FRAMES 4

typedef struct AircraftFailureSmokeParticle {
	UBYTE active;
	UBYTE age;
	LONG worldX;
	WORD y;
} AircraftFailureSmokeParticle;

/* The footprint is a conservative tile rectangle around every particle drawn
 * into one ring buffer. Erase rebuilds that rectangle from authoritative world
 * data rather than restoring cached bytes that may have gone stale after a
 * streamed column, flak spawn or weapon impact. */
typedef struct AircraftFailureSmokeFootprint {
	UBYTE valid;
	LONG firstWorldColumn;
	UBYTE columnCount;
	WORD firstTileRow;
	UBYTE rowCount;
	ULONG renderSignature;
} AircraftFailureSmokeFootprint;

static AircraftFailureSmokeParticle
	aircraftFailureSmoke[AIRCRAFT_FAILURE_SMOKE_MAX];
static AircraftFailureSmokeFootprint
	aircraftFailureSmokeFootprints[GAME_WORLD_BUFFER_COUNT];
static UBYTE aircraftFailureSmokeCursor = 0;

static void resetAircraftFailureSmoke(void) {
	memset(aircraftFailureSmoke, 0, sizeof(aircraftFailureSmoke));
	memset(aircraftFailureSmokeFootprints, 0,
		sizeof(aircraftFailureSmokeFootprints));
	aircraftFailureSmokeCursor = 0;
}

static void stopAircraftFailureSmokeEmission(void) {
	/* Keep the old footprint valid until the late renderer has reconstructed
	 * the world underneath it. Only particle simulation is stopped here. */
	memset(aircraftFailureSmoke, 0, sizeof(aircraftFailureSmoke));
}

static void spawnAircraftFailureSmoke(const GameState* game) {
	AircraftFailureSmokeParticle* particle =
		&aircraftFailureSmoke[aircraftFailureSmokeCursor];
	aircraftFailureSmokeCursor = (UBYTE)((aircraftFailureSmokeCursor + 1) %
		AIRCRAFT_FAILURE_SMOKE_MAX);
	particle->active = 1;
	particle->age = 0;
	/* The Harrier faces right: its exhaust/nozzle is near the rear-left edge.
	 * Store absolute world X so the puff remains behind while the camera moves. */
	particle->worldX = (LONG)game->scrollX + game->playerX + 1;
	particle->y = (WORD)(game->playerY + 4);
}

static void updateAircraftFailureSmoke(GameState* game) {
	if (game->aircraftFailureState != AIRCRAFT_FAILURE_DESCENT)
		return;
	if ((game->aircraftFailureTimer % AIRCRAFT_FAILURE_SMOKE_SPAWN_FRAMES) == 0)
		spawnAircraftFailureSmoke(game);

	for (UBYTE index = 0; index < AIRCRAFT_FAILURE_SMOKE_MAX; index++) {
		AircraftFailureSmokeParticle* particle = &aircraftFailureSmoke[index];
		if (!particle->active)
			continue;
		particle->age++;
		if ((particle->age & 1) == 0)
			particle->worldX--;
		if ((particle->age % 4) == 0)
			particle->y--;
		if (particle->age >= AIRCRAFT_FAILURE_SMOKE_LIFETIME)
			particle->active = 0;
	}
}

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
	} else if (kind == BOMB_IMPACT_BOB_KIND_IMPACT_SMALL ||
		kind == BOMB_IMPACT_BOB_KIND_IMPACT_LARGE) {
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
	} else {
		/* Preserve the CPC Smoke 1/Smoke 2 silhouettes, but render their
		 * opaque pixels in white as temporary water spray. Persistent smoke
		 * over destroyed land objects keeps its normal palette mapping. */
		UBYTE tileId = (kind == BOMB_IMPACT_BOB_KIND_WATER_SMOKE_1) ? 51 : 52;
		for (UBYTE row = 0; row < GAME_TILE_HEIGHT; row++) {
			UBYTE* dest = bombImpactBobTile + row * (GAME_WORLD_DISPLAY_PLANES + 1);
			UBYTE mask = 0;
			for (UBYTE col = 0; col < GAME_TILE_WIDTH; col++) {
				UBYTE color = gameTilePixelColor(tileId, (short)col, (short)row);
				if (color != GAME_COLOR_SKY)
					mask |= (UBYTE)(0x80 >> col);
			}
			for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++) {
				if (GAME_COLOR_WHITE & (1 << plane))
					dest[plane] = mask;
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
	static const UBYTE typeColor[6] = {
		GAME_COLOR_SKY,    /* NONE (unused) */
		GAME_COLOR_RED,    /* WINGMAN */
		GAME_COLOR_YELLOW, /* HEALTH */
		GAME_COLOR_POWERUP_BLUE,  /* ROCKETS: stable across mission palettes */
		GAME_COLOR_POWERUP_GREEN, /* BOMBS: stable across mission palettes */
		GAME_COLOR_WHITE   /* EXTRA AIRCRAFT */
	};
	UBYTE canopyColor = typeColor[type < 6 ? type : 0];
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
	buildAttachedSpriteFromCpcPlusHalves(sprite, attachSprite,
		ENEMY_SPRITE_HEIGHT, x, y,
		harCpcEnemyPlaneFlyingLeftPixels,
		harCpcEnemyPlaneFlyingRightPixels);
}

static void buildEnemyBrokenPlaneSprite(UWORD* sprite, UWORD* attachSprite,
	WORD x, WORD y) {
	buildAttachedSpriteFromCpcPlusHalves(sprite, attachSprite,
		ENEMY_SPRITE_HEIGHT, x, y,
		harCpcEnemyPlaneBrokenLeftPixels,
		harCpcEnemyPlaneBrokenRightPixels);
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
/* Both native carriers use the same assembled orientation. CPC's
 * endfrigatesprite stream enters from the opposite side; the completed ship
 * art itself is not mirrored, and Wingman remains on the forward deck. */
static void drawPromotedCpcCarrierRangeRowAt(UBYTE* bitmap,
	UWORD physicalTileX, UWORD compositeColumn, UBYTE row) {
	if (compositeColumn >= HAR_CARRIER_TILES_WIDE)
		return;
	const UBYTE* tileData;
	const UBYTE* tileSkip;
	if (carrierParkedWingmanVisible) {
		tileData = harCarrierTileData;
		tileSkip = harCarrierTileSkip;
	} else {
		tileData = harCarrierWithoutWingmanTileData;
		tileSkip = harCarrierWithoutWingmanTileSkip;
	}
	if (row >= HAR_CARRIER_TILES_TALL)
		return;
	UWORD gridIndex = (UWORD)(row * HAR_CARRIER_TILES_WIDE + compositeColumn);
	if (tileSkip[gridIndex])
		return;
	/* Canvas row 0 in the generator corresponds to world tile row 12
	 * (pixel Y 96 = the caller's old fixed y=80 base + the 16px/2-tile
	 * shift the generator applied so its own canvas starts at row 0 -
	 * see tools/cpc_promoted_sprites_to_tiles.py's carrier canvas
	 * comment). Only ever called with that same fixed base today. */
	drawGameScrollTileMasked(bitmap, (short)physicalTileX, (short)(12 + row),
		tileData + (ULONG)gridIndex * HAR_CARRIER_TILE_BYTES);
}

static void drawPromotedCpcCarrierRangeAt(UBYTE* bitmap, UWORD physicalTileX,
	UWORD compositeColumn) {
	for (UBYTE row = 0; row < HAR_CARRIER_TILES_TALL; row++)
		drawPromotedCpcCarrierRangeRowAt(bitmap, physicalTileX,
			compositeColumn, row);
}

static void drawPromotedCpcGunshipAt(UBYTE* bitmap, short x, short y) {
	drawCpcPlusSpriteScroll(bitmap, x, y, harCpcGunshipLeftPixels, HAR_CPC_GUNSHIP_LEFT_WIDTH, HAR_CPC_GUNSHIP_LEFT_HEIGHT, 1);
	drawCpcPlusSpriteScroll(bitmap, (short)(x + 16), y, harCpcGunshipRightPixels, HAR_CPC_GUNSHIP_RIGHT_WIDTH, HAR_CPC_GUNSHIP_RIGHT_HEIGHT, 1);
}

/* Shift+D debug hub -------------------------------------------------------
 * This is deliberately a normal planar screen, not an overlay on the menu:
 * it cannot corrupt the ticker's independently-scrolled bitplane pointers,
 * and graphics/SFX inspection never needs gameplay buffers or sprite DMA.
 * The graphics catalogue includes every converted 8x8 game tile, every
 * promoted CPC Plus sprite component, then all eight complete sourced town
 * blocks. Objects can therefore be audited both by source piece and by the
 * exact composite column sequence used by gameplay. */
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
#define DEBUG_TOWN_BLOCK_BASE ((UWORD)(GAME_TILE_COUNT + DEBUG_PROMOTED_GRAPHIC_COUNT))
#define DEBUG_GRAPHIC_COUNT ((UWORD)(DEBUG_TOWN_BLOCK_BASE + HAR_CPC_TOWN_BLOCK_COUNT))

static const char* const debugTownBlockNames[HAR_CPC_TOWN_BLOCK_COUNT] = {
	"TOWN BLOCK 0", "TOWN BLOCK 1", "TOWN BLOCK 2", "TOWN BLOCK 3",
	"TOWN BLOCK 4", "TOWN BLOCK 5", "TOWN BLOCK 6", "TOWN BLOCK 7"
};

static const char* const debugSfxNames[SFX_COUNT] = {
	"ROCKET FIRE",
	"BOMB DROP",
	"IMPACT",
	"PLAYER HIT",
	"FLAK HIT",
	"EJECT",
	"PICKUP POWERUP",
	"FLAK GUN 1",
	"FLAK GUN 2",
	/* Match the source WAV suffixes exactly. The old 1..4 labels made
	 * GROUND HIT 2 select ground_hit_1.wav, which was especially confusing
	 * while auditioning replacement masters. */
	"GROUND HIT 0 WAV",
	"GROUND HIT 1 WAV",
	"GROUND HIT 2 WAV",
	"GROUND HIT 3 WAV",
	"GROUND MISS",
	"WATER SPLASH",
	"RADAR ALARM",
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

static void drawDebugTownBlockScaled(UBYTE* bitmap, UBYTE blockId) {
	const UBYTE scale = 2;
	UBYTE widthTiles = harCpcTownBlockWidths[blockId];
	short width = (short)(widthTiles * GAME_TILE_WIDTH * scale);
	short height = (short)(HAR_CPC_TOWN_BLOCK_HEIGHT * GAME_TILE_HEIGHT * scale);
	short startX = (short)((SCREEN_WIDTH - width) / 2);
	short startY = (short)(122 - height / 2);
	const UBYTE* block = harCpcTownBlockTiles[blockId];
	for (UBYTE tileX = 0; tileX < widthTiles; tileX++) {
		for (UBYTE tileY = 0; tileY < HAR_CPC_TOWN_BLOCK_HEIGHT; tileY++) {
			UBYTE tileId = block[tileX * HAR_CPC_TOWN_BLOCK_HEIGHT + tileY];
			for (UBYTE y = 0; y < GAME_TILE_HEIGHT; y++) {
				for (UBYTE x = 0; x < GAME_TILE_WIDTH; x++) {
					UBYTE color = gameTilePixelColor(tileId, x, y);
					fillRect(bitmap,
						(short)(startX + (tileX * GAME_TILE_WIDTH + x) * scale),
						(short)(startY + (tileY * GAME_TILE_HEIGHT + y) * scale),
						scale, scale, color);
				}
			}
		}
	}
}

static void drawDebugGraphicsBrowser(UBYTE* bitmap, UWORD index) {
	drawDebugPageHeader(bitmap, "GRAPHICS BROWSER");
	if (index < GAME_TILE_COUNT) {
		drawTextCentered(bitmap, 68, "CONVERTED GAME TILE", MENU_COLOR_CYAN);
		drawDebugGameTileScaled(bitmap, (UBYTE)index);
		drawText(bitmap, 112, 168, "TILE", MENU_COLOR_WHITE);
		drawUnsignedPadded(bitmap, 160, 168, index, 3, MENU_COLOR_YELLOW);
	} else if (index < DEBUG_TOWN_BLOCK_BASE) {
		const DebugGraphicAsset* asset =
			&debugPromotedGraphics[index - GAME_TILE_COUNT];
		drawTextCentered(bitmap, 68, asset->name, MENU_COLOR_CYAN);
		drawDebugPromotedGraphicScaled(bitmap, asset);
		drawTextCentered(bitmap, 174, "PROMOTED CPC SPRITE PART",
			MENU_COLOR_WHITE);
	} else {
		UBYTE blockId = (UBYTE)(index - DEBUG_TOWN_BLOCK_BASE);
		drawTextCentered(bitmap, 68, debugTownBlockNames[blockId],
			MENU_COLOR_CYAN);
		drawDebugTownBlockScaled(bitmap, blockId);
		drawTextCentered(bitmap, 174, "COMPLETE CPC TOWN BLOCK",
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

/* Attract-mode Field Guide, entered after the main ticker completes and left
 * after its own gameplay/rules ticker completes.
 * Point values reuse the exact same constants the collision/hit code awards
 * (GROUND_TARGET_SCORE_VALUE/ENEMY_SHIP_SCORE_VALUE/TOWN_BLOCK_SCORE_VALUE/
 * ENEMY_SCORE_VALUE), so this can never drift out of sync with real scoring.
 * Every object here is one-hit destructible and instant death on player
 * contact except flak (Sprint 14.91/14.95): flak absorbs the player's own
 * weapons harmlessly (never destroyed) and instead chips armour per contact
 * frame up to flakDamageThresholdForSkill()'s skill-scaled hit budget, and
 * the promoted gunship is only a presentation overlay: its destructible CPC
 * enemyshipsprite cells remain authoritative for weapon contact, 500-point
 * scoring, persistent smoke and removal of the struck visual section. Powerup
 * colours match buildPowerupBobTileIfNeeded()'s typeColor[] table exactly.
 *
 * Icons reuse the exact same graphics the game actually draws in the world,
 * the same way the debug hub's own GRAPHICS BROWSER does just above (see
 * drawDebugGameTileScaled()/drawDebugPromotedGraphicScaled()) - either one
 * of the 102 converted 8x8 CPC tiles (gameTilePixelColor(), tile IDs taken
 * directly from the real spawn/placement code: 45 is a TANK_FRONT ground
 * target per targetTiles[] in objectCellForWorldColumnTile(), 20 is an
 * ENEMY_SHIP hull tile from level_route.h, 66 is town_block_0's one real
 * building tile in cpc_promoted_assets.h, 57 is a flak tile per
 * trySpawnFlak()'s tile choice), or a promoted CPC sprite part
 * (cpcPlusPenToGameColor(), same pen data debugPromotedGraphics[] already
 * names "ENEMY FLYING LEFT"/"GUNSHIP LEFT"/"PARACHUTE"). Composite sprites
 * are assembled from both CPC source pieces at natural pixel size. */
#define FIELD_GUIDE_ICON_BOX 16
#define FIELD_GUIDE_ICON_WIDTH 32

/* Field Guide is displayed with menuPalette, while promoted CPC graphics are
 * converted to game-palette indices. Translate by colour meaning instead of
 * copying the numeric index (index 5, for example, is land green in-game but
 * yellow in the menu). */
static UBYTE fieldGuideMenuColorForGameColor(UBYTE color) {
	switch (color) {
		case GAME_COLOR_WHITE:      return MENU_COLOR_WHITE;
		case GAME_COLOR_LIGHT_GREY:
		case GAME_COLOR_MID_GREY:
		case GAME_COLOR_DARK_GREY:  return 8; /* menuPalette: neutral grey */
		case GAME_COLOR_LAND:       return MENU_COLOR_GREEN;
		case GAME_COLOR_YELLOW:     return MENU_COLOR_YELLOW;
		case GAME_COLOR_RED:        return MENU_COLOR_RED;
		case GAME_COLOR_BLACK:      return MENU_COLOR_PANEL;
		case GAME_COLOR_SEA:        return 15; /* menuPalette: dark blue */
		default:                    return MENU_COLOR_CYAN;
	}
}

static void drawFieldGuideTileIcon(UBYTE* bitmap, short x, short y, UBYTE tileId) {
	for (UBYTE dy = 0; dy < FIELD_GUIDE_ICON_BOX; dy++) {
		UBYTE sy = (UBYTE)(dy * GAME_TILE_HEIGHT / FIELD_GUIDE_ICON_BOX);
		for (UBYTE dx = 0; dx < FIELD_GUIDE_ICON_BOX; dx++) {
			UBYTE sx = (UBYTE)(dx * GAME_TILE_WIDTH / FIELD_GUIDE_ICON_BOX);
			putPixel(bitmap, (short)(x + dx), (short)(y + dy),
				gameTilePixelColor(tileId, sx, sy));
		}
		serviceModMusicToCurrentVbl();
	}
}

/* CPC's tank is a two-character object: tile 45 is explicitly TANK FRONT
 * and tile 46 TANK REAR in the extracted asset manifest. Keep them together
 * and enlarge both 2x, yielding one readable 32x16 guide illustration. */
static void drawFieldGuideDoubleTileIcon(UBYTE* bitmap, short x, short y,
	UBYTE leftTileId, UBYTE rightTileId) {
	for (UBYTE dy = 0; dy < FIELD_GUIDE_ICON_BOX; dy++) {
		UBYTE sy = (UBYTE)(dy >> 1);
		for (UBYTE dx = 0; dx < FIELD_GUIDE_ICON_WIDTH; dx++) {
			UBYTE tileId = dx < FIELD_GUIDE_ICON_BOX
				? leftTileId : rightTileId;
			UBYTE sx = (UBYTE)((dx & (FIELD_GUIDE_ICON_BOX - 1)) >> 1);
			putPixel(bitmap, (short)(x + dx), (short)(y + dy),
				gameTilePixelColor(tileId, sx, sy));
		}
		serviceModMusicToCurrentVbl();
	}
}

/* The enemy plane's CPC "left"/"right" arrays are the left and right 8px
 * halves of one 16x8 ASIC sprite (each source row is padded to 16 bytes).
 * This is the same composition buildAttachedSpriteFromCpcPlusHalves() uses
 * during gameplay, rendered here at its natural two-tile size. */
static void drawFieldGuideHalvedSpriteIcon(UBYTE* bitmap, short x, short y,
	const UBYTE* leftPixels, const UBYTE* rightPixels, UBYTE height,
	UBYTE forceGrey) {
	short top = (short)(y + (FIELD_GUIDE_ICON_BOX - height) / 2);
	for (UBYTE row = 0; row < height; row++) {
		for (UBYTE col = 0; col < 16; col++) {
			const UBYTE* source = col < 8 ? leftPixels : rightPixels;
			UBYTE pen = source[(UWORD)row * 16 + (col & 7)] & 15;
			if (pen)
				putPixel(bitmap, (short)(x + col), (short)(top + row),
					forceGrey ? 8 : fieldGuideMenuColorForGameColor(
						cpcPlusPenToGameColor(pen)));
		}
		serviceModMusicToCurrentVbl();
	}
}

/* Gunship is two complete 16x16 CPC sprites placed side by side. */
static void drawFieldGuideGunshipIcon(UBYTE* bitmap, short x, short y,
	const UBYTE* leftPixels, const UBYTE* rightPixels, UBYTE forceGrey) {
	for (UBYTE row = 0; row < 16; row++) {
		for (UBYTE col = 0; col < 32; col++) {
			const UBYTE* source = col < 16 ? leftPixels : rightPixels;
			UBYTE pen = source[(UWORD)row * 16 + (col & 15)] & 15;
			if (pen)
				putPixel(bitmap, (short)(x + col), (short)(y + row),
					forceGrey ? 8 : fieldGuideMenuColorForGameColor(
						cpcPlusPenToGameColor(pen)));
		}
		serviceModMusicToCurrentVbl();
	}
}

/* Same 8x8 parachute art buildPowerupBobTileIfNeeded() bakes into the in-game
 * drop Bob, enlarged 2x for the guide. The extracted CPC rows are padded to
 * HAR_CPC_PARACHUTE_WIDTH (16), but only their first 8 columns contain the
 * actual graphic. Treating that padding as image content previously squeezed
 * the chute into the left half of the icon. Rigging is grey here rather than
 * gameplay black so it remains visible against the guide's black panel. */
static void drawFieldGuideParachuteIcon(UBYTE* bitmap, short x, short y,
	UBYTE canopyColor) {
	for (UBYTE dy = 0; dy < FIELD_GUIDE_ICON_BOX; dy++) {
		UBYTE sy = (UBYTE)((UWORD)dy * HAR_CPC_PARACHUTE_HEIGHT / FIELD_GUIDE_ICON_BOX);
		for (UBYTE dx = 0; dx < FIELD_GUIDE_ICON_BOX; dx++) {
			UBYTE sx = (UBYTE)(dx >> 1); /* 8 real CPC pixels -> 16 guide pixels */
			UBYTE pen = harCpcParachutePixels[(UWORD)sy * HAR_CPC_PARACHUTE_WIDTH + sx];
			if (pen == 15)
				putPixel(bitmap, (short)(x + dx), (short)(y + dy), canopyColor);
			else if (pen == 1)
				putPixel(bitmap, (short)(x + dx), (short)(y + dy), 8);
		}
		serviceModMusicToCurrentVbl();
	}
}

typedef struct FieldGuideEntry {
	const char* name;
	UWORD points;
	const char* note;
	UBYTE isSprite;
	UBYTE tileId;
	UBYTE tileId2;
	const UBYTE* pixels;
	const UBYTE* pixels2;
	UBYTE spriteWidth;
	UBYTE spriteHeight;
} FieldGuideEntry;

#define FIELD_GUIDE_TILE_ICON(tile) 0, tile, 0, 0, 0, 0, 0
#define FIELD_GUIDE_DOUBLE_TILE_ICON(left, right) 3, left, right, 0, 0, 32, 16
#define FIELD_GUIDE_HALVED_ICON(left, right, h) 1, 0, 0, left, right, 16, h
#define FIELD_GUIDE_GUNSHIP_ICON(left, right) 2, 0, 0, left, right, 32, 16

static const FieldGuideEntry fieldGuideEnemies[] = {
	{ "GROUND TARGET", GROUND_TARGET_SCORE_VALUE, "1 HIT",
		FIELD_GUIDE_DOUBLE_TILE_ICON(45, 46) },
	{ "ENEMY SHIP",     ENEMY_SHIP_SCORE_VALUE,    "1 HIT",
		FIELD_GUIDE_GUNSHIP_ICON(harCpcGunshipLeftPixels,
			harCpcGunshipRightPixels) },
	{ "TOWN BLOCK",     TOWN_BLOCK_SCORE_VALUE,    "1 HIT",
		FIELD_GUIDE_TILE_ICON(66) },
	{ "ENEMY PLANE",    ENEMY_SCORE_VALUE,         "RADAR DETECT",
		FIELD_GUIDE_HALVED_ICON(harCpcEnemyPlaneFlyingLeftPixels,
			harCpcEnemyPlaneFlyingRightPixels,
			HAR_CPC_ENEMY_PLANE_FLYING_LEFT_HEIGHT) },
	{ "FLAK GUN",       0,                         "CHIPS ARMOUR",
		FIELD_GUIDE_TILE_ICON(57) }
};
#define FIELD_GUIDE_ENEMY_COUNT \
	(sizeof(fieldGuideEnemies) / sizeof(fieldGuideEnemies[0]))

typedef struct FieldGuidePowerup {
	const char* label;
	UBYTE color;
	const char* meaning;
} FieldGuidePowerup;

static const FieldGuidePowerup fieldGuidePowerups[] = {
	{ "RED",    MENU_COLOR_RED,    "WINGMAN REVIVED" },
	{ "YELLOW", MENU_COLOR_YELLOW, "FULL ARMOUR" },
	{ "BLUE",   MENU_COLOR_CYAN,   "ROCKETS REFILLED" },
	{ "GREEN",  MENU_COLOR_GREEN,  "BOMBS REFILLED" },
	{ "WHITE",  MENU_COLOR_WHITE,  "EXTRA AIRCRAFT (ENHANCED)" }
};
#define FIELD_GUIDE_POWERUP_COUNT \
	(sizeof(fieldGuidePowerups) / sizeof(fieldGuidePowerups[0]))

static void drawFieldGuideScreen(UBYTE* bitmap, short gameMode) {
	/* Do not inherit the main ticker's pointer.  Start with a completely
	 * empty band, then let the guide text enter smoothly from the right. */
	initMenuTickerForText(menuTickerBitmap,
		fieldGuideTickerTextForMode(gameMode));
	fillScreen(bitmap, MENU_COLOR_PANEL);
	serviceModMusicToCurrentVbl();
	drawMenuTicker(bitmap);

	short y = 26;
	drawText(bitmap, 48, y, "ENEMY", MENU_COLOR_CYAN);
	drawText(bitmap, 164, y, "PTS", MENU_COLOR_CYAN);
	drawText(bitmap, 204, y, "NOTE", MENU_COLOR_CYAN);
	y += 12;
	for (UBYTE i = 0; i < FIELD_GUIDE_ENEMY_COUNT; i++) {
		const FieldGuideEntry* entry = &fieldGuideEnemies[i];
		if (entry->isSprite == 1)
			drawFieldGuideHalvedSpriteIcon(bitmap, 16, y, entry->pixels,
				entry->pixels2, entry->spriteHeight, 1);
		else if (entry->isSprite == 2)
			drawFieldGuideGunshipIcon(bitmap, 8, y, entry->pixels,
				entry->pixels2, 1);
		else if (entry->isSprite == 3)
			drawFieldGuideDoubleTileIcon(bitmap, 8, y, entry->tileId,
				entry->tileId2);
		else
			drawFieldGuideTileIcon(bitmap, 16, y, entry->tileId);
		drawText(bitmap, 48, (short)(y + 4), entry->name, MENU_COLOR_WHITE);
		if (entry->points)
			drawUnsignedPadded(bitmap, 164, (short)(y + 4), entry->points, 3,
				MENU_COLOR_YELLOW);
		else
			drawText(bitmap, 164, (short)(y + 4), "--", MENU_COLOR_SHADOW);
		drawText(bitmap, 204, (short)(y + 4), entry->note, MENU_COLOR_WHITE);
		y += FIELD_GUIDE_ICON_BOX + 2;
		serviceModMusicToCurrentVbl();
	}

	y += 6;
	drawTextCentered(bitmap, y, "PARACHUTE DROP COLOURS", MENU_COLOR_CYAN);
	y += 14;
	UBYTE powerupCount = gameMode == GAME_MODE_CLASSIC ?
		FIELD_GUIDE_POWERUP_COUNT - 1 : FIELD_GUIDE_POWERUP_COUNT;
	for (UBYTE i = 0; i < powerupCount; i++) {
		const FieldGuidePowerup* powerup = &fieldGuidePowerups[i];
		drawFieldGuideParachuteIcon(bitmap, 16, y, powerup->color);
		drawText(bitmap, 40, (short)(y + 4), powerup->label, MENU_COLOR_WHITE);
		drawText(bitmap, 100, (short)(y + 4), powerup->meaning,
			MENU_COLOR_WHITE);
		y += FIELD_GUIDE_ICON_BOX + 2;
		serviceModMusicToCurrentVbl();
	}

}

static void drawDebugSoundBrowser(UBYTE* bitmap, UBYTE index,
	UBYTE playing) {
	drawDebugPageHeader(bitmap, "SOUND BROWSER");
	drawTextCentered(bitmap, 88, debugSfxNames[index], MENU_COLOR_CYAN);
	drawUnsignedPadded(bitmap, 120, 112, (ULONG)(index + 1), 2,
		MENU_COLOR_YELLOW);
	drawText(bitmap, 144, 112, "/", MENU_COLOR_WHITE);
	drawUnsignedPadded(bitmap, 160, 112, SFX_COUNT, 2, MENU_COLOR_YELLOW);
	drawTextCentered(bitmap, 142, playing ? "PLAYING" : "FIRE/ENTER TO PLAY",
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
	drawTextCentered(bitmap, 220, "FIRE/ENTER PLAY SAMPLE", MENU_COLOR_WHITE);
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
	drawTextCentered(bitmap, 148, playing ? "PLAYING" : "FIRE/ENTER TO PLAY",
		playing ? MENU_COLOR_GREEN : MENU_COLOR_WHITE);
	drawTextCentered(bitmap, 174, "FOUR PAULA CHANNEL MOD",
		MENU_COLOR_SHADOW);
	drawTextCentered(bitmap, 204, "LEFT/RIGHT MUSIC", MENU_COLOR_WHITE);
	drawTextCentered(bitmap, 220, "FIRE/ENTER PLAY FROM START", MENU_COLOR_WHITE);
	drawTextCentered(bitmap, 236, "ESC OR SHIFT+D BACK", MENU_COLOR_SHADOW);
}

/* Sprint 14.94 Part 6: same idea as drawPromotedCpcCarrierRangeAt() above,
 * for the 2-piece (left/right) gunship. baseTileRow is already in tile units
 * (levelObjectRowForColumnObject()'s result, passed straight through by the
 * caller) - the gunship canvas has no vertical shift baked in (unlike the
 * carrier's +16px/2-tile one), since both pieces share the same y in the
 * original per-pixel calls. */
static void drawPromotedCpcGunshipRangeRowAt(UBYTE* bitmap,
	UWORD physicalTileX, UWORD worldColumn, UWORD compositeColumn,
	short baseTileRow, UBYTE row) {
	if (compositeColumn >= HAR_GUNSHIP_TILES_WIDE)
		return;
	if (row >= HAR_GUNSHIP_TILES_TALL)
		return;
	/* enemyshipsprite supplies the collision cells behind this CPC+ sprite
	 * pair. A hit replaces exactly one such cell with persistent smoke, so
	 * omit the matching presentation tile rather than drawing an apparently
	 * indestructible gunship over the damaged object map. */
	if (isShipCellDestroyed((LONG)worldColumn,
		(WORD)(baseTileRow + row)))
		return;
	UWORD gridIndex = (UWORD)(row * HAR_GUNSHIP_TILES_WIDE + compositeColumn);
	if (harGunshipTileSkip[gridIndex])
		return;
	drawGameScrollTileMasked(bitmap, (short)physicalTileX,
		(short)(baseTileRow + row),
		harGunshipTileData + (ULONG)gridIndex * HAR_CARRIER_TILE_BYTES);
}

static void drawPromotedCpcGunshipRangeAt(UBYTE* bitmap, UWORD physicalTileX,
	UWORD worldColumn, UWORD compositeColumn, short baseTileRow) {
	for (UBYTE row = 0; row < HAR_GUNSHIP_TILES_TALL; row++)
		drawPromotedCpcGunshipRangeRowAt(bitmap, physicalTileX, worldColumn,
			compositeColumn, baseTileRow, row);
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

/* Observe stage transitions at the right edge without affecting gameplay. */
static void telemetryTrackGameplayStage(const GameState* game) {
	if ((!telemetryEnabled && !HAR_DEBUG_PERF_LOG) || !telemetryAvailable)
		return;
	UWORD worldColumn = (UWORD)((game->scrollX >> 3) + GAME_MAP_WIDTH);
	const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
	UBYTE stage = stageForWorldColumn(worldColumn, segment);
	if (stage == telemetryLastGameplayStage)
		return;
	UBYTE previousStage = telemetryLastGameplayStage;
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_TERRAIN_STATE,
		(previousStage == 0xff) ? 0 : previousStage,
		worldColumn, game, stage);
	if (previousStage == HAR_STAGE_FLAT_TOWNLAND &&
		stage == HAR_STAGE_START_PIER)
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_CITY_TO_PIER,
			previousStage, worldColumn, game, stage);
	telemetryLastGameplayStage = stage;
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
/* Sprint 15.45: gameplay decisions live together and are finalized before
 * any cosmetic tile is selected.  This makes the ownership boundary
 * explicit: changing art variation cannot move terrain, targets or events. */
typedef struct CpcLandGameplayState {
	UBYTE height;
	UBYTE target;
	UBYTE transition;
} CpcLandGameplayState;
static CpcLandGameplayState cpcLandGameplayTable[CPC_LAND_PROCEDURAL_MAX_LENGTH];
/* Cosmetic surface tile chosen after gameplay generation for each column,
 * stored directly instead of re-derived later by comparing
 * neighbouring columns' heights (landSurfaceTileForColumn()'s old approach)
 * - that re-derivation could tag two consecutive columns as a slope tile for
 * a single real height change (e.g. heights 14,14,13,13 read as "hill up" at
 * both the transition column and the column after it), which the real CPC
 * never does (only the column where the height dispatcher's mode 1/2 branch
 * actually fires gets a slope tile). */
static UBYTE cpcLandSurfaceTable[CPC_LAND_PROCEDURAL_MAX_LENGTH];
static UWORD cpcLandProceduralLength = CPC_LAND_PROCEDURAL_BASE_LENGTH;

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

/* Coverage-ordered fallback used by deterministic non-procedural transitions.
 * CPC-procedural hills select their 24-27/28-31 variant randomly at the
 * actual height-change event, matching `ld a,r; rra; and 3`. */
static const UBYTE hillPhaseByCoverage[4] = { 2, 3, 0, 1 };

/* Compile-time cosmetic seed: deliberately independent from the session RNG
 * that owns CPC gameplay.  Override with -DHAR_COSMETIC_SEED=... to compare
 * appearances while retaining identical terrain/target/flak decisions. */
#ifndef HAR_COSMETIC_SEED
#define HAR_COSMETIC_SEED 0x6d35U
#endif

static UBYTE cosmeticVariantForColumn(UWORD column, UWORD salt) {
	/* 16-bit xorshift-style mixer. It runs only during precomputation (or at
	 * the rare live flak spawn), avoids 32-bit division/modulo on 68000 and
	 * never writes any gameplay RNG state. */
	UWORD value = (UWORD)(HAR_COSMETIC_SEED + column * 257U + salt);
	value ^= (UWORD)(value << 7);
	value ^= (UWORD)(value >> 9);
	value ^= (UWORD)(value << 5);
	return (UBYTE)(value & 3);
}

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

/* CPC keeps the menu choice as the campaign's starting difficulty, then
 * increments leveldifficulty after every successful landing, capped at 5.
 * Do not mutate the menu option itself: rescue and retry must reconstruct the
 * same effective board difficulty from the selected skill and mission number. */
static UBYTE cpcDifficultyForMission(UBYTE selectedSkill, UBYTE missionNumber) {
	UWORD difficulty = selectedSkill;
	if (missionNumber > 1)
		difficulty += (UWORD)(missionNumber - 1);
	if (difficulty > CPC_LEVEL_DIFFICULTY_MAX)
		difficulty = CPC_LEVEL_DIFFICULTY_MAX;
	return (UBYTE)difficulty;
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
typedef struct CpcColumnGameplayState {
	UWORD randomState; /* currtime/l8859 source used by terrain and flak */
	UBYTE rState;      /* modeled CPC R used by target/flak decisions */
} CpcColumnGameplayState;
static CpcColumnGameplayState cpcGameplayStateByColumn[GAME_LEVEL_WIDTH_TILES];
static UBYTE cpcRandomSequenceReady = 0;
static void resetCpcTownBlockTable(void);
static void generateCpcTownBlockTable(void);

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
 * shape. cpcGameplayStateByColumn.rState stores R at the start of the generated column,
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
#define CPC_R_COST_DEFAULT 63  /* non-town fallback - UNMEASURED, assumed */
#define CPC_R_COST_FLAT 63     /* land: flat, or a blocked hill/target degenerating to flat */
#define CPC_R_COST_HILL 63     /* land: hill up/down actually stepping */
#define CPC_R_COST_TARGET 63   /* land: successful target insertion */
#define CPC_R_COST_TANK_REAR 63 /* CPC's next-tick continuation of the tank sprite block */
/* Sprint 15.71.0: town R ownership is modeled explicitly. CPC reads R only
 * in buildportstanley/state 6, after its flat-column work; state 7 then
 * streams every column of the selected building before another choice is
 * made. The existing LOGGEN land sample found a median column cost of 60
 * and a rounded representative cost of 63. Until a town-only LOGGEN trace
 * supplies separate measured values, retain that calibrated land magnitude
 * for both town paths, but expose one persistent town-local R stream rather
 * than hiding its progression behind absolute per-column lookups. This makes
 * the state-6/state-7 ownership auditable and gives town-specific
 * start/end/checksum telemetry for later calibration. */
#define CPC_R_COST_TOWN_FLAT_CHOICE 63
#define CPC_R_COST_TOWN_BUILDING_COLUMN 63

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

	for (UWORD column = 0; column < currentGameLevelWidthTiles; column++) {
		UBYTE terrainKind = terrainKindForCloudColumn((LONG)column);
		UWORD rng = cpcGameplayStateByColumn[column].randomState;

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
 * flak/town read the frameCounter-seeded cpcGameplayStateByColumn table) -
 * that made the two only superficially similar,
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
		static const char header[] = "index,height,surfaceTile,transition,target\n";
		landLogAppend(header, sizeof(header) - 1);
	}
	for (UWORD i = 0; i < cpcLandProceduralLength; i++) {
		char line[24];
		char* out = line;
		out = appendUnsignedLong(out, i);
		*out++ = ',';
		out = appendUnsignedLong(out, cpcLandGameplayTable[i].height);
		*out++ = ',';
		out = appendUnsignedLong(out, cpcLandSurfaceTable[i]);
		*out++ = ',';
		out = appendUnsignedLong(out, cpcLandGameplayTable[i].transition);
		*out++ = ',';
		out = appendUnsignedLong(out, cpcLandGameplayTable[i].target);
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
	/* A retry may produce a different final town block. Restore the authored
	 * coordinates before generating the new session, then apply its measured
	 * overflow after the CPC state tables are complete. */
	configureRuntimeLevelRoute(cpcLandRouteExtension, 0);
	/* Clear positive answers before procedural terrain is rebuilt. A retry may
	 * use a different session seed, so carrying safe cells across games could
	 * otherwise let Wingman pass through a newly generated block. */
	memset(wingmanSafeCellValid, 0, sizeof(wingmanSafeCellValid));
	memset(enemyPlanePassableColumnValid, 0,
		sizeof(enemyPlanePassableColumnValid));
#if HAR_VALIDATION_SESSION_SEED
	/* Diagnostic/headless builds can pin the modeled CPC session state so
	 * A/B performance runs exercise identical terrain, clouds, targets and
	 * flak. Normal F5/release builds leave this at zero and retain the
	 * menu-time-derived per-session variation above. */
	state = HAR_VALIDATION_SESSION_SEED;
#endif
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

	for (UWORD column = 0; column < currentGameLevelWidthTiles; column++) {
		/* CPC calls genrandomhl before generating the newly revealed column,
		 * unconditionally, every tick regardless of sea/land/town stage. */
		state = (UWORD)(state * 1509U + 0x0029U);
		UBYTE l8859 = (UBYTE)(state >> 8);
		UBYTE rCost = (terrainKindForCloudColumn((LONG)column) == HAR_TERRAIN_SEA)
			? CPC_R_COST_SEA : CPC_R_COST_DEFAULT;

		LONG landLocalColumn = (LONG)column - CPC_LAND_PROCEDURAL_WORLD_START;
		if (landLocalColumn >= 0 && landLocalColumn < cpcLandProceduralLength) {
			UWORD i = (UWORD)landLocalColumn;
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
			cpcLandGameplayTable[i].target = CPC_LAND_TARGET_NONE;

			if (landPendingTankRear) {
				/* CPC continuation, not an Amiga invention. The first
				 * insertenemylandtile call draws bytes 00,2d vertically and
				 * stores the advanced DE in l885e. On the next scroll tick
				 * gamelevelprogress=4 resumes at l9206/l91cb and draws
				 * 00,2e at the fresh right edge. The visible tank therefore
				 * spans two world columns even though drawspriteblock3 itself
				 * advances rows within each column. */
				landPendingTankRear = 0;
				cpcLandGameplayTable[i].target = CPC_LAND_TARGET_TANK_REAR;
				rCost = CPC_R_COST_TANK_REAR;
				transition = CPC_LAND_TARGET;
			} else if (i == 0) {
				/* asm:5409-5431 startoffalklandisland: one-shot land-entry
				 * transition column - draws fixed join tiles, not mode
				 * dispatch. */
				landHeight = CPC_LAND_PROCEDURAL_BASELINE;
				drawHeight = landHeight;
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
						rCost = CPC_R_COST_HILL;
						transition = CPC_LAND_DESCEND;
					} else {
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
						rCost = CPC_R_COST_HILL;
						transition = CPC_LAND_CLIMB;
					} else {
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
					if (type == 3 && i + 1 < cpcLandProceduralLength) {
						cpcLandGameplayTable[i].target = CPC_LAND_TARGET_TANK_FRONT;
						landPendingTankRear = 1;
					} else {
						cpcLandGameplayTable[i].target = (UBYTE)(CPC_LAND_TARGET_RADAR + type);
					}
					landJustInserted = 1;
					rCost = CPC_R_COST_TARGET;
					transition = CPC_LAND_TARGET;
				} else {
					mode = 0;
					rCost = CPC_R_COST_FLAT;
				}
				landLastMode = mode;
			}

			cpcLandGameplayTable[i].height = drawHeight;
			cpcLandGameplayTable[i].transition = transition;
		}

		cpcGameplayStateByColumn[column].randomState = state;
		/* Store the value actually visible to this column's terrain path.
		 * The former ordering stored the post-column value, shifting every
		 * R-based lookup away from the terrain that selected it. */
		cpcGameplayStateByColumn[column].rState = rState;
		/* R advances independently of genrandomhl, by however much code ran
		 * this tick - see the CPC_R_COST_* comment above. */
		rState = (UBYTE)((rState + rCost) & CPC_R_MASK);
	}

	/* Cosmetic pass runs only after every gameplay decision is complete.
	 * Its seed and queries therefore cannot feed back into height, target
	 * placement, R costs, flak gates or the event sequence. */
	for (UWORD i = 0; i < cpcLandProceduralLength; i++) {
		UBYTE variant = cosmeticVariantForColumn(i, 0x134bU);
		switch (cpcLandGameplayTable[i].transition) {
			case CPC_LAND_CLIMB:
				cpcLandSurfaceTable[i] = (UBYTE)(24 + variant);
				break;
			case CPC_LAND_DESCEND:
				cpcLandSurfaceTable[i] = (UBYTE)(28 + variant);
				break;
			default:
				cpcLandSurfaceTable[i] = (UBYTE)(32 + variant);
				break;
		}
	}
	cpcRandomSequenceReady = 1;
	resetCpcTownBlockTable();
	generateCpcTownBlockTable();
	configureRuntimeLevelRoute(cpcLandRouteExtension, cpcTownRouteOverflow);
	generateCpcCloudTable();
#if HAR_DEBUG_LAND_LOG
	landLogBuild();
#endif

#if HAR_DEBUG_PERF_LOG
	/* Sanity check: CPC's real height table only ever steps by 1 per
	 * column (hill up/down), so any larger jump means this reconstruction
	 * has a bug - not something that should ever legitimately fire. */
	for (UWORD i = 1; i < cpcLandProceduralLength; i++) {
		WORD delta = (WORD)cpcLandGameplayTable[i].height - (WORD)cpcLandGameplayTable[i - 1].height;
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
			out = appendUnsignedLong(out, cpcLandGameplayTable[i - 1].height);
			*out++ = '-'; *out++ = '>';
			out = appendUnsignedLong(out, cpcLandGameplayTable[i].height);
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
	 * Sprint 14.105: this now checks the precomputed gameplay transition
	 * instead of re-deriving the transition from a height delta. Once
	 * descend was fixed to draw at the OLD height (matching asm:5500-5525 -
	 * see drawHeight's own comment above), the *visible* height change for a
	 * descend column shows up one column later than the transition that
	 * caused it, so a delta-based check here would misfire on every real
	 * descend. The explicit transition table isn't affected by that timing
	 * shift - it's set at the same index as the tile choice either way. */
	for (UWORD i = 0; i < cpcLandProceduralLength; i++) {
		UBYTE transition = cpcLandGameplayTable[i].transition;
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

#if HAR_DEBUG_PERF_LOG
/* One compact, machine-readable end-of-run summary for Sprint 15.48.  It is
 * deliberately assembled and written only after FreeSystem(): during play,
 * AmigaDOS cannot safely run while the game owns interrupts/task scheduling.
 * Terrain values come from the authoritative gameplay table, while encounter
 * values come from the same event counters used by the in-game telemetry. */
static void parityLogFlushToDisk(const GameState* game) {
	UBYTE minHeight = 0xff;
	UBYTE maxHeight = 0;
	UWORD flat = 0;
	UWORD climbs = 0;
	UWORD descends = 0;
	UWORD targets = 0;
	for (UWORD i = 0; i < cpcLandProceduralLength; i++) {
		const CpcLandGameplayState* state = &cpcLandGameplayTable[i];
		if (state->height < minHeight)
			minHeight = state->height;
		if (state->height > maxHeight)
			maxHeight = state->height;
		if (state->transition == CPC_LAND_CLIMB)
			climbs++;
		else if (state->transition == CPC_LAND_DESCEND)
			descends++;
		else if (state->transition == CPC_LAND_FLAT)
			flat++;
		if (state->target != CPC_LAND_TARGET_NONE)
			targets++;
	}

	char report[896];
	char* out = report;
	static const char header[] =
		"skill,difficulty,mission,landLength,minY,maxY,flat,climb,descend,targets,enemySpawnOk,enemySpawnNo,enemyMissiles,flakSpawns,wingInterceptOk,wingInterceptNo,wingBombOk,wingBombNo,terrainEvents,pierEvents,enemyPlaneBlocked,townBlocks,townBuildingCols,townFlatCols,townLength,townOverflowCols,townRStart,townREnd,townRChecksum,townClippedCols,classicAirTicks,classicEnemyOutcomes,classicPowerupOutcomes,classicPowerupWhileEnemy,enhancedPowerupWhileEnemy,wingFormationStops,wingFormationCardinal,wingFormationDiagonal,wingFormationEvasive,highScoreLevel,finalScroll,landingState,missionComplete,reachedFinalCarrier\n";
	for (UWORD i = 0; i < sizeof(header) - 1; i++)
		*out++ = header[i];
#define PARITY_VALUE(value) do { out = appendUnsignedLong(out, (ULONG)(value)); *out++ = ','; } while (0)
	PARITY_VALUE(game->skillLevel);
	PARITY_VALUE(game->levelDifficulty);
	PARITY_VALUE(game->missionNumber);
	PARITY_VALUE(cpcLandProceduralLength);
	PARITY_VALUE(minHeight);
	PARITY_VALUE(maxHeight);
	PARITY_VALUE(flat);
	PARITY_VALUE(climbs);
	PARITY_VALUE(descends);
	PARITY_VALUE(targets);
	PARITY_VALUE(telemetryGameEventCounters[TELEMETRY_GAME_EVENT_ENEMY_SPAWN_OK]);
	PARITY_VALUE(telemetryGameEventCounters[TELEMETRY_GAME_EVENT_ENEMY_SPAWN_NO]);
	PARITY_VALUE(telemetryGameEventCounters[TELEMETRY_GAME_EVENT_ENEMY_MISSILE]);
	PARITY_VALUE(perfRuntimeFlakSpawns);
	PARITY_VALUE(telemetryGameEventCounters[TELEMETRY_GAME_EVENT_WING_INTERCEPT_OK]);
	PARITY_VALUE(telemetryGameEventCounters[TELEMETRY_GAME_EVENT_WING_INTERCEPT_NO]);
	PARITY_VALUE(telemetryGameEventCounters[TELEMETRY_GAME_EVENT_WING_BOMB_OK]);
	PARITY_VALUE(telemetryGameEventCounters[TELEMETRY_GAME_EVENT_WING_BOMB_NO]);
	PARITY_VALUE(telemetryGameEventCounters[TELEMETRY_GAME_EVENT_TERRAIN_STATE]);
	PARITY_VALUE(telemetryGameEventCounters[TELEMETRY_GAME_EVENT_CITY_TO_PIER]);
	PARITY_VALUE(telemetryGameEventCounters[TELEMETRY_GAME_EVENT_ENEMY_PLANE_BLOCKED]);
	PARITY_VALUE(cpcTownGeneratedBlockCount);
	PARITY_VALUE(cpcTownGeneratedBuildingColumns);
	PARITY_VALUE(cpcTownGeneratedFlatColumns);
	PARITY_VALUE(cpcTownGeneratedLength);
	PARITY_VALUE(cpcTownRouteOverflow);
	PARITY_VALUE(cpcTownRStart);
	PARITY_VALUE(cpcTownREnd);
	PARITY_VALUE(cpcTownRSelectionChecksum);
	PARITY_VALUE(0);
	PARITY_VALUE(telemetryClassicAirAdmissionTicks);
	PARITY_VALUE(telemetryClassicAirEnemyOutcomes);
	PARITY_VALUE(telemetryClassicAirPowerupOutcomes);
	PARITY_VALUE(telemetryClassicPowerupWhileEnemy);
	PARITY_VALUE(telemetryEnhancedPowerupWhileEnemy);
	PARITY_VALUE(telemetryWingFormationStops);
	PARITY_VALUE(telemetryWingFormationCardinal);
	PARITY_VALUE(telemetryWingFormationDiagonal);
	PARITY_VALUE(telemetryWingFormationEvasive);
	PARITY_VALUE(highScoreTable[0].level);
	PARITY_VALUE(game->scrollX);
	PARITY_VALUE(game->landingState);
	PARITY_VALUE(game->missionComplete);
	out = appendUnsignedLong(out,
		(game->landingState == LANDING_STATE_HOVER ||
		 game->scrollX >= LANDING_HOVER_SCROLL_X) ? 1 : 0);
	*out++ = '\n';
#undef PARITY_VALUE

	BPTR file = Open((CONST_STRPTR)"DH1:parity_log.csv", MODE_NEWFILE);
	if (!file)
		return;
	Write(file, (APTR)report, (LONG)(out - report));
	Close(file);
}
#endif

#if HAR_DEBUG_ENEMY_PLANE_LOG
static char* enemyTraceAppendUnsigned(char* out, ULONG value) {
	char reversed[10];
	UBYTE count = 0;
	if (!value) {
		*out++ = '0';
		return out;
	}
	while (value && count < sizeof(reversed)) {
		reversed[count++] = (char)('0' + value % 10);
		value /= 10;
	}
	while (count)
		*out++ = reversed[--count];
	return out;
}

static char* enemyTraceAppendSigned(char* out, LONG value) {
	if (value < 0) {
		*out++ = '-';
		value = -value;
	}
	return enemyTraceAppendUnsigned(out, (ULONG)value);
}

/* Runtime only records compact structs in RAM. The AmigaDOS file is created
 * after FreeSystem(), when filesystem task switching is safe again. */
static void enemyPlaneTraceFlushToDisk(const GameState* game) {
	BPTR file = Open((CONST_STRPTR)"DH1:enemy_plane_log.csv", MODE_NEWFILE);
	if (!file)
		return;
	static const char header[] =
		"rate,seed,skill,seq,frame,event,status,target,scroll,speed,visualX,visualY,logicalX,logicalY,targetX,targetY,tileDistance,lagX,lagY,blocked,framesSinceTick,dropped\n";
	Write(file, (APTR)header, sizeof(header) - 1);
	for (UWORD index = 0; index < enemyPlaneTraceCount; index++) {
		const EnemyPlaneTraceRecord* record = &enemyPlaneTrace[index];
		char line[192];
		char* out = line;
#define ENEMY_TRACE_UNSIGNED(value) do { out = enemyTraceAppendUnsigned(out, (ULONG)(value)); *out++ = ','; } while (0)
#define ENEMY_TRACE_SIGNED(value) do { out = enemyTraceAppendSigned(out, (LONG)(value)); *out++ = ','; } while (0)
		ENEMY_TRACE_UNSIGNED(HAR_ENEMY_PLANE_INTERPOLATION_PIXELS);
		ENEMY_TRACE_UNSIGNED(HAR_VALIDATION_SESSION_SEED);
		ENEMY_TRACE_UNSIGNED(game->skillLevel);
		ENEMY_TRACE_UNSIGNED(record->sequence);
		ENEMY_TRACE_UNSIGNED(record->frame);
		ENEMY_TRACE_UNSIGNED(record->event);
		ENEMY_TRACE_UNSIGNED(record->status);
		ENEMY_TRACE_UNSIGNED(record->target);
		ENEMY_TRACE_UNSIGNED(record->scrollX);
		ENEMY_TRACE_UNSIGNED(record->speed);
		ENEMY_TRACE_SIGNED(record->visualX);
		ENEMY_TRACE_SIGNED(record->visualY);
		ENEMY_TRACE_SIGNED(record->logicalX);
		ENEMY_TRACE_SIGNED(record->logicalY);
		ENEMY_TRACE_SIGNED(record->targetX);
		ENEMY_TRACE_SIGNED(record->targetY);
		ENEMY_TRACE_SIGNED(record->tileDistance);
		ENEMY_TRACE_SIGNED(record->lagX);
		ENEMY_TRACE_SIGNED(record->lagY);
		ENEMY_TRACE_UNSIGNED(record->blocked);
		ENEMY_TRACE_UNSIGNED(record->framesSinceTick);
		out = enemyTraceAppendUnsigned(out, enemyPlaneTraceDropped);
		*out++ = '\n';
		Write(file, (APTR)line, (LONG)(out - line));
#undef ENEMY_TRACE_UNSIGNED
#undef ENEMY_TRACE_SIGNED
	}
	Close(file);
}
#endif

static UWORD cpcRandomStateForWorldColumn(LONG worldColumn) {
	if (!cpcRandomSequenceReady)
		resetCpcRandomSequence();
	if (worldColumn < 0)
		worldColumn = 0;
	if (worldColumn >= currentGameLevelWidthTiles)
		worldColumn = currentGameLevelWidthTiles - 1;
	return cpcGameplayStateByColumn[worldColumn].randomState;
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
	if (worldColumn >= currentGameLevelWidthTiles)
		worldColumn = currentGameLevelWidthTiles - 1;
	return cpcGameplayStateByColumn[worldColumn].rState;
}

static UBYTE cpcCloudTileAtColumnRow(LONG worldColumn, WORD tileY) {
	UBYTE topRow;
	UBYTE encodedColumn;
	UBYTE blockColumn;
	UBYTE row;
	if (!cpcRandomSequenceReady)
		resetCpcRandomSequence();
	if (worldColumn < 0 || worldColumn >= currentGameLevelWidthTiles)
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
	return cpcLandGameplayTable[index].height;
}

static UBYTE cpcLandProceduralTarget(UWORD index) {
	if (!cpcRandomSequenceReady)
		resetCpcRandomSequence();
	return cpcLandGameplayTable[index].target;
}

static UBYTE cpcLandProceduralSurface(UWORD index) {
	if (!cpcRandomSequenceReady)
		resetCpcRandomSequence();
	return cpcLandSurfaceTable[index];
}

/* CPC town state machine (asm buildportstanley/checkbuildpier): state 6 emits
 * one ordinary flat-terrain column and selects one of eight sprite blocks;
 * state 7 then emits that block one column per tick before returning to state
 * 6. The resulting pattern is always [flat][whole block][flat][whole block],
 * including the first building. Precompute the same local-column table for
 * rendering and collision queries. */
#define CPC_TOWN_TIMER_LENGTH 200
/* State 7 can finish a width-five block selected by the final state-6 tick. */
#define CPC_TOWN_PROCEDURAL_CAPACITY (CPC_TOWN_TIMER_LENGTH + 5)
#define CPC_TOWN_BLOCK_NONE 0xff
#define CPC_TOWN_SMOKE_ROWS (HAR_CPC_TOWN_BLOCK_HEIGHT + 1)
#define CPC_TOWN_SMOKE_NONE 0
#define CPC_TOWN_SMOKE_A 1
#define CPC_TOWN_SMOKE_B 2
static UBYTE townBlockForColumn[CPC_TOWN_PROCEDURAL_CAPACITY];
static UBYTE townBlockLocalColumnForColumn[CPC_TOWN_PROCEDURAL_CAPACITY];
/* Two bits per possible smoke row.  Town destruction used to consume the
 * shared 24-entry ship/target smoke list, so a busy Enhanced sortie could
 * exhaust it before (or inside) the town: score and sound still happened,
 * but the facade was rebuilt intact.  A complete town needs only 205 words
 * (410 bytes) and can now represent every destructible cell without a list
 * capacity or per-frame scan. Row zero is the sky cell immediately above the
 * five-row block, used by CPC Smoke 1. */
static UWORD townHitSmokeByColumn[CPC_TOWN_PROCEDURAL_CAPACITY];
static UBYTE townBlockTableReady = 0;

static void resetCpcTownBlockTable(void) {
	townBlockTableReady = 0;
	memset(townHitSmokeByColumn, 0, sizeof(townHitSmokeByColumn));
	cpcTownGeneratedBlockCount = 0;
	cpcTownGeneratedBuildingColumns = 0;
	cpcTownGeneratedFlatColumns = 0;
	cpcTownGeneratedLength = 0;
	cpcTownRouteOverflow = 0;
	cpcTownRStart = 0;
	cpcTownREnd = 0;
	cpcTownRSelectionChecksum = 0;
}

static void generateCpcTownBlockTable(void) {
	/* CPC selects town buildings from R:
	 * `ld a,r; rra; rra; and #07` → blockId = (R >> 2) & 7.
	 * Previously used a separate local LCG (seed 0x3c91, * 25173 + 13849),
	 * which was Amiga-specific and had no correlation to CPC's R-driven
	 * sequence. Sprint 15.71 makes the town's single sequential R ownership
	 * explicit: initialise it once from the shared mission stream, then advance
	 * it for every state-6/state-7 town tick. */
	memset(townBlockForColumn, CPC_TOWN_BLOCK_NONE, sizeof(townBlockForColumn));
	memset(townBlockLocalColumnForColumn, 0, sizeof(townBlockLocalColumnForColumn));
	cpcTownGeneratedBlockCount = 0;
	cpcTownGeneratedBuildingColumns = 0;
	cpcTownGeneratedFlatColumns = 0;
	cpcTownGeneratedLength = 0;
	cpcTownRouteOverflow = 0;
	cpcTownRSelectionChecksum = 0;

	/* Town segment starts at column 411 in the authored route. The R value at
	 * that absolute column already contains all preceding sea/land opcode
	 * costs. From here the town owns one continuous local cursor. */
	const LONG townWorldStart = (LONG)(411 + cpcLandRouteExtension);
	UBYTE townRState = cpcRStateForWorldColumn(townWorldStart);
	cpcTownRStart = townRState;

	UWORD i = 0;
	while (i < CPC_TOWN_TIMER_LENGTH) {
		/* State 6: drawflatterrain. NONE leaves the already generated flat town
		 * surface visible for exactly one column, including at town entry. */
		UBYTE blockId = (UBYTE)((townRState >> 2) & 7);
		cpcTownRSelectionChecksum = (UWORD)(
			(cpcTownRSelectionChecksum * 33U) ^ townRState ^ blockId);
		i++;
		cpcTownGeneratedFlatColumns++;
		cpcTownGeneratedBlockCount++;
		townRState = (UBYTE)((townRState + CPC_R_COST_TOWN_FLAT_CHOICE) & CPC_R_MASK);
		UBYTE width = harCpcTownBlockWidths[blockId];
		/* State 7: CPC finishes the chosen sprite block without another timer
		 * check. Preserve every column, including those beyond the timer. */
		UBYTE col = 0;
		for (; col < width && i < CPC_TOWN_PROCEDURAL_CAPACITY; col++, i++) {
			townBlockForColumn[i] = blockId;
			townBlockLocalColumnForColumn[i] = col;
			cpcTownGeneratedBuildingColumns++;
			townRState = (UBYTE)((townRState + CPC_R_COST_TOWN_BUILDING_COLUMN) & CPC_R_MASK);
		}
	}
	cpcTownGeneratedLength = i;
	cpcTownREnd = townRState;
	cpcTownRouteOverflow = (i > CPC_TOWN_TIMER_LENGTH) ?
		(UBYTE)(i - CPC_TOWN_TIMER_LENGTH) : 0;
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
			/* Sprint 15.46: setcloudcolourtosea's C=3 is object ID, while
			 * B=2 is the vertical draw count. This is one column containing
			 * solid land at rows 14-15, immediately followed by pendata. */
			if (localColumn == 0)
				return 14;
			return 255;
		case HAR_TERRAIN_CPC_RANDOM_LAND: {
			if (localColumn >= cpcLandProceduralLength)
				localColumn = cpcLandProceduralLength - 1;
			return cpcLandProceduralProfile((UWORD)localColumn);
		}
		case HAR_TERRAIN_CPC_DESCEND_TO_TOWN: {
			/* Sprint 14.97 PRI 3: CPC continues from the last generated
			 * land height and steps one row down per column until reaching
			 * 14, rather than a fixed 10-column script. The descend length
			 * adapts to how high the terrain was when land ended - skill 5
			 * with row 7 terrain needs 7 descend columns, not the same 3
			 * that skill 1 with row 11 terrain does. */
			UBYTE lastLandHeight = cpcLandProceduralProfile(cpcLandProceduralLength - 1);
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
		if (localColumn >= cpcLandProceduralLength)
			localColumn = cpcLandProceduralLength - 1;
		return cpcLandProceduralSurface((UWORD)localColumn);
	}
	/* CPC's coast-rise sequence starts with one solid block on row 15 and
	 * only then emits the hill-up block on row 14. Returning a slope tile for
	 * that first row-15 column left its transparent upper pixels showing sea,
	 * which made the land appear one tile below the waterline at every
	 * sea-to-land seam even though terrainY itself was correct. */
	if (terrainKind == HAR_TERRAIN_COAST_RISE) {
		const LevelSegmentDef* coastSegment = levelSegmentForWorldColumn(worldColumn);
		LONG coastColumn = coastSegment ? worldColumn - coastSegment->startColumn : 0;
		if (coastColumn == 0)
			return 1;
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

static WORD terrainSurfacePixelYForWorldColumn(LONG worldColumn) {
	/* landSurfaceYForWorldColumn() deliberately returns a tile row because
	 * the renderer and terrain diagnostics consume rows. Failure/ejection
	 * physics operate in screen pixels and must not compare against that row
	 * directly. Sea uses the visible water-line tile as its surface. */
	WORD surfaceRow = landSurfaceYForWorldColumn(worldColumn);
	if (surfaceRow < 0)
		surfaceRow = GAME_SEA_TOP_TILE_Y;
	return (WORD)(surfaceRow * GAME_TILE_HEIGHT);
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
	memset(townHitSmokeByColumn, 0, sizeof(townHitSmokeByColumn));
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

static BYTE townHitSmokeSlotForColumnRow(LONG worldColumn, WORD tileY,
	UWORD* localColumn) {
	const LevelSegmentDef* segment = levelSegmentForWorldColumn(worldColumn);
	if (!segment || segment->terrainKind != HAR_TERRAIN_TOWN)
		return -1;
	LONG local = worldColumn - segment->startColumn;
	if (local < 0 || local >= CPC_TOWN_PROCEDURAL_CAPACITY)
		return -1;
	UBYTE terrainY = terrainYForWorldColumn(worldColumn, segment,
		HAR_TERRAIN_TOWN);
	if (terrainY == 255)
		return -1;
	WORD firstSmokeRow = (WORD)(terrainY - 4);
	WORD slot = (WORD)(tileY - firstSmokeRow);
	if (slot < 0 || slot >= CPC_TOWN_SMOKE_ROWS)
		return -1;
	if (localColumn)
		*localColumn = (UWORD)local;
	return (BYTE)slot;
}

static UBYTE townHitSmokeTileAtColumnRow(LONG worldColumn, WORD tileY) {
	UWORD localColumn;
	BYTE slot = townHitSmokeSlotForColumnRow(worldColumn, tileY,
		&localColumn);
	if (slot < 0)
		return 0;
	UBYTE kind = (UBYTE)((townHitSmokeByColumn[localColumn] >>
		((UBYTE)slot * 2)) & 3);
	if (kind == CPC_TOWN_SMOKE_A)
		return GAME_SHIP_WRECK_SMOKE_TILE_A;
	if (kind == CPC_TOWN_SMOKE_B)
		return GAME_SHIP_WRECK_SMOKE_TILE_B;
	return 0;
}

static UBYTE markTownHitSmokeAtColumnRow(LONG worldColumn, WORD tileY,
	UBYTE kind) {
	UWORD localColumn;
	BYTE slot = townHitSmokeSlotForColumnRow(worldColumn, tileY,
		&localColumn);
	if (slot < 0 || kind == CPC_TOWN_SMOKE_NONE || kind > CPC_TOWN_SMOKE_B)
		return 0;
	UBYTE shift = (UBYTE)slot * 2;
	UWORD oldBits = townHitSmokeByColumn[localColumn];
	UWORD newBits = (UWORD)((oldBits & (UWORD)~(3U << shift)) |
		((UWORD)kind << shift));
	townHitSmokeByColumn[localColumn] = newBits;
	return newBits != oldBits;
}

static void addCpcTownHitSmokeAtColumnRow(LONG worldColumn, WORD tileY) {
	ObjectCell aboveCell;
	markTownHitSmokeAtColumnRow(worldColumn, tileY, CPC_TOWN_SMOKE_B);
	if (tileY > 0 && objectCellForWorldColumnTile(worldColumn, tileY - 1,
		&aboveCell) && aboveCell.id == HAR_OBJ_SKY) {
		markTownHitSmokeAtColumnRow(worldColumn, tileY - 1,
			CPC_TOWN_SMOKE_A);
	}
}

static UBYTE persistentHitSmokeTileAtColumnRow(LONG worldColumn,
	WORD tileY) {
	UBYTE townTile = townHitSmokeTileAtColumnRow(worldColumn, tileY);
	return townTile ? townTile :
		shipWreckSmokeTileAtColumnRow(worldColumn, tileY);
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
		if (localColumn == 0 && (tileY == 14 || tileY == 15)) {
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

	UBYTE smokeTile = persistentHitSmokeTileAtColumnRow(worldColumn, tileY);
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
		if (localColumn >= 0 && localColumn < cpcLandProceduralLength) {
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
		if (playerRightWorld < deckLeftWorld || playerLeftWorld >= deckRightWorld)
			continue;

		/* CPC checkwingmanlanded/wingmanlandoncarrier2 keeps the two deck
		 * positions distinct and selects an alternate pad if one is occupied.
		 * In single-player-without-Wingman the grey aircraft is still parked
		 * there as carrier scenery, so that part of the deck is not a valid
		 * Player 1 touchdown zone. */
		if (carrierParkedWingmanVisible) {
			LONG parkedLeft = deckLeftWorld +
				CARRIER_PARKED_HARRIER_LEFT_NORMAL;
			LONG parkedRight = parkedLeft + CARRIER_PARKED_HARRIER_WIDTH;
			if (playerRightWorld >= parkedLeft && playerLeftWorld < parkedRight)
				continue;
		}
		return 1;
	}

	return 0;
}

/* Pixel-precise carrier obstructions above the otherwise landable deck.
 * The object map represents the native carrier only as a horizon-row deck,
 * while CPC draws/collides with its tower and parked second Harrier too.
 * Keep these small rectangles separate from the deck test so sea/hull and
 * normal terrain collision continue through the existing object map. */
static UBYTE playerHitsNativeCarrierObstruction(const GameState* game,
	LONG* hitWorldColumn, WORD* hitTileY) {
	if (game->takeoffState != TAKEOFF_STATE_AIRBORNE || game->crashTimer || game->gameOver)
		return 0;

	/* CPC landinghoverloop tests exactly the two object-map cells occupied by
	 * currentplayerlocation on its current character row.  Testing the smooth
	 * Amiga sprite's pixel rectangle instead reached into the next CPC row up
	 * to seven pixels early, so clear air immediately behind/above the tower
	 * could be fatal.  Keep pixel-smooth presentation, but quantize this one
	 * gameplay decision to CPC's 2x1 character footprint. */
	LONG playerWorldColumn = ((LONG)game->scrollX + game->playerX) >> 3;
	WORD playerTileY = game->playerY >> 3;

	if (!harLevelObjectIndexReady)
		buildHarLevelObjectIndex();
	for (UBYTE wideIndex = 0; wideIndex < harWideObjectCount; wideIndex++) {
		const LevelObjectDef* object = &harLevelObjects[harWideObjectIndex[wideIndex]];
		if (object->id != HAR_OBJ_OWN_FRIGATE || !(object->flags & HAR_OBJECT_FLAG_NATIVE_CARRIER))
			continue;
		LONG carrierColumn = object->column;
		/* CPC writefrigatetilemap is an exact three-row mask:
		 *   top    1,1,1,1,1,4,4,1,1,1,1,1
		 *   middle 1,1,1,1,4,4,4,4,1,1,1,1
		 *   deck   4,4,4,4,4,4,4,4,4,4,4,4
		 * Keep the two tower steps separate. The former single x=40..71
		 * rectangle was shifted eight pixels right and made clear air behind
		 * the visible/CPC tower fatal during landing. */
		WORD carrierTileY = CARRIER_COMPOSITE_PIXEL_Y >> 3;
		for (UBYTE playerCell = 0; playerCell < 2; playerCell++) {
			LONG relativeColumn = playerWorldColumn + playerCell - carrierColumn;
			UBYTE towerHit =
				(playerTileY == carrierTileY &&
				 relativeColumn >= CARRIER_TOWER_UPPER_LEFT / GAME_TILE_WIDTH &&
				 relativeColumn < CARRIER_TOWER_UPPER_RIGHT / GAME_TILE_WIDTH) ||
				(playerTileY == carrierTileY + 1 &&
				 relativeColumn >= CARRIER_TOWER_LOWER_LEFT / GAME_TILE_WIDTH &&
				 relativeColumn < CARRIER_TOWER_LOWER_RIGHT / GAME_TILE_WIDTH);
			if (towerHit) {
				*hitWorldColumn = playerWorldColumn + playerCell;
				*hitTileY = playerTileY;
				return 1;
			}
		}

		if (carrierParkedWingmanVisible &&
			game->playerY + PLAYER_SPRITE_HEIGHT - 1 >= CARRIER_PARKED_HARRIER_TOP &&
			game->playerY + 1 < CARRIER_PARKED_HARRIER_BOTTOM) {
			LONG playerLeft = (LONG)game->scrollX + game->playerX + 2;
			LONG playerRight = (LONG)game->scrollX + game->playerX +
				PLAYER_SPRITE_WIDTH - 3;
			LONG parkedLeft = carrierColumn * GAME_TILE_WIDTH +
				CARRIER_PARKED_HARRIER_LEFT_NORMAL;
			LONG parkedRight = parkedLeft + CARRIER_PARKED_HARRIER_WIDTH - 1;
			if (playerRight >= parkedLeft && playerLeft <= parkedRight) {
				*hitWorldColumn = parkedLeft >> 3;
				*hitTileY = CARRIER_PARKED_HARRIER_TOP >> 3;
				return 1;
			}
		}
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
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_LANDING_COMPLETE, 0,
			(UWORD)(((LONG)game->scrollX + game->playerX) >> 3), game,
			game->missionNumber);
		/* Landing is a one-shot state transition, so the fanfare starts
		 * exactly once here rather than being retriggered while LANDED is
		 * displayed. Give its four MOD voices sole ownership of Paula. */
		stopAllSfx();
		startCarrierLandingMusic();
		changed = 1;
	}

	if (game->fuel != 999 ||
		game->fuelGaugeLevel != CPC_FUEL_GAUGE_LEVELS ||
		game->fuelSubCounter != CPC_FUEL_SUBCOUNT_FULL ||
		game->fuelClockAccumulator != 0) {
		resetPlayerFuel(game);
		changed = 1;
	}
	{
		UBYTE fullBombs, fullRockets;
		ammoForSkill(game->levelDifficulty, &fullBombs, &fullRockets);
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
			if (localColumn == 0 && (tileY == 14 || tileY == 15)) {
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

	/* Priority 2: persistent hit smoke. A town block's bottom visible tile
	 * can share terrainY with the solid land base. The old `if (claimed)`
	 * rejected smoke on that row because terrain had claim 1, after which
	 * the procedural facade simply drew the intact building back over it.
	 * CPC drawsmokesprite replaces the struck object-map cell, including a
	 * building cell at ground level, so smoke may replace base terrain
	 * (claim 0/1) and then protects the row with claim 2. */
	for (UWORD tileY = 0; tileY < GAME_OBJECT_MAP_HEIGHT_TILES; tileY++) {
		UBYTE smokeTile = persistentHitSmokeTileAtColumnRow(worldColumn, tileY);
		if (smokeTile && claimed[tileY] < 2) {
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
		if (localColumn >= 0 && localColumn < CPC_TOWN_PROCEDURAL_CAPACITY) {
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
		if (localColumn >= 0 && localColumn < cpcLandProceduralLength) {
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

	/* A row is passable to the CPC enemy plane only while it is unresolved
	 * sky/cloud. All terrain, town cells, targets, flak and carrier geometry
	 * claim their row above. Clouds deliberately remain unclaimed because the
	 * CPC obstruction test accepts both object IDs 0 and 1. Store only the
	 * upper playfield rows; sea and anything below it are never valid lanes. */
	if (worldColumn >= 0 && worldColumn < currentGameLevelWidthTiles) {
		UWORD passableMask = 0;
		for (UBYTE tileY = 0; tileY < GAME_SEA_TOP_TILE_Y; tileY++) {
			if (!claimed[tileY])
				passableMask |= (UWORD)(1U << tileY);
		}
		enemyPlanePassableMaskByColumn[worldColumn] = passableMask;
		enemyPlanePassableColumnValid[(UWORD)worldColumn >> 3] |=
			(UBYTE)(1U << ((UWORD)worldColumn & 7));
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
			drawPromotedCpcCarrierRangeAt(bitmap, physicalTileX,
				(UWORD)(worldColumn - objectColumn));
			continue;
		}

		if (object->id == HAR_OBJ_GUNSHIP && (object->flags & HAR_OBJECT_FLAG_CPC_GUNSHIP)) {
			if (worldColumn < objectColumn || worldColumn >= objectColumn + WORLD_RENDER_GUNSHIP_WIDTH_TILES)
				continue;
			row = levelObjectRowForColumnObject(object);
			if (row < 0)
				continue;
			drawPromotedCpcGunshipRangeAt(bitmap, physicalTileX,
				(UWORD)worldColumn,
				(UWORD)(worldColumn - objectColumn), row);
			continue;
		}
	}
}

/* Reapply only one row of a promoted wide object after a transient BOB has
 * restored the base tile underneath it. The enemy ship collision cells are
 * intentionally absent from buildWorldTileColumn(); its visible CPC+ art is
 * this masked overlay, so rebuilding only the base row would otherwise make
 * a ship section disappear until the ring column is streamed again. */
static void drawDirectColumnRangeObjectRow(UBYTE* bitmap,
	UWORD physicalTileX, LONG worldColumn, WORD tileRow) {
	if (!harLevelObjectIndexReady)
		buildHarLevelObjectIndex();
	for (UBYTE wideIndex = 0; wideIndex < harWideObjectCount; wideIndex++) {
		const LevelObjectDef* object =
			&harLevelObjects[harWideObjectIndex[wideIndex]];
		LONG objectColumn = object->column;
		if (object->id == HAR_OBJ_OWN_FRIGATE &&
			(object->flags & HAR_OBJECT_FLAG_NATIVE_CARRIER)) {
			if (worldColumn < objectColumn ||
				worldColumn >= objectColumn + WORLD_RENDER_CARRIER_WIDTH_TILES ||
				tileRow < 12 || tileRow >= 12 + HAR_CARRIER_TILES_TALL)
				continue;
			drawPromotedCpcCarrierRangeRowAt(bitmap, physicalTileX,
				(UWORD)(worldColumn - objectColumn), (UBYTE)(tileRow - 12));
			continue;
		}

		if (object->id == HAR_OBJ_GUNSHIP &&
			(object->flags & HAR_OBJECT_FLAG_CPC_GUNSHIP)) {
			if (worldColumn < objectColumn ||
				worldColumn >= objectColumn + WORLD_RENDER_GUNSHIP_WIDTH_TILES)
				continue;
			WORD baseTileRow = levelObjectRowForColumnObject(object);
			if (baseTileRow < 0 || tileRow < baseTileRow ||
				tileRow >= baseTileRow + HAR_GUNSHIP_TILES_TALL)
				continue;
			drawPromotedCpcGunshipRangeRowAt(bitmap, physicalTileX,
				(UWORD)worldColumn, (UWORD)(worldColumn - objectColumn),
				baseTileRow, (UBYTE)(tileRow - baseTileRow));
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
	if (localColumn < 0 || localColumn >= CPC_TOWN_PROCEDURAL_CAPACITY)
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
	if (townHitSmokeTileAtColumnRow(centerColumn, centerTileY))
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
	memset(bitmap, 0, GAME_WORLD_BITMAP_BYTES);
	ringWorldLastStreamedColumn = 0xffff;
	ringStreamColumn = -1;
	ringStreamRow = 0;
	/* The opening carrier scroll can expose only columns startColumn through
	 * startColumn + 51 (96px initial camera offset plus the 336px fetch).
	 * Building all 86 ring-page columns synchronously here needlessly redrew
	 * another full screen before takeoff. Prime the visible union plus two
	 * hidden safety columns; serviceRingWorldStream() fills the remainder in
	 * order while the aircraft lifts and normal scrolling begins. */
	LONG lastInitialColumn = (LONG)startColumn +
		(TAKEOFF_SCROLL_START_PIXELS / GAME_TILE_WIDTH) + GAME_FETCH_BYTES + 1;
	for (LONG worldColumn = startColumn;
		worldColumn <= lastInitialColumn; worldColumn++) {
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
static LONG ringStreamTouchedFirstColumn = -1;
static LONG ringStreamTouchedLastColumn = -1;

/* Conservative pre-stream test used by retained BOBs. It simulates only the
 * columns the fixed row budget can touch this frame. False positives merely
 * cause one harmless redraw; false negatives would leave a BOB partially
 * overwritten by a recycled ring column. */
static UBYTE ringStreamMayTouchColumnRange(const GameState* game,
	LONG firstColumn, LONG lastColumn) {
	if (useFixedTakeoffWorldWindow(game))
		return 0;
	UBYTE scrollPixels = (game->missionComplete ||
		game->landingState == LANDING_STATE_HOVER) ? 0 :
		scrollPixelsForSpeedLevel(game->speedLevel);
	UWORD rows = (UWORD)((scrollPixels * GAME_OBJECT_MAP_HEIGHT_TILES +
		(GAME_TILE_WIDTH - 1)) / GAME_TILE_WIDTH);
	if (!rows)
		return 0;
	LONG streamFirst = ringStreamColumn >= 0 ? ringStreamColumn :
		(LONG)ringWorldLastStreamedColumn + 1;
	UWORD firstRows = ringStreamColumn >= 0 ?
		(UWORD)(GAME_OBJECT_MAP_HEIGHT_TILES - ringStreamRow) :
		GAME_OBJECT_MAP_HEIGHT_TILES;
	LONG streamLast = streamFirst;
	if (rows > firstRows) {
		rows = (UWORD)(rows - firstRows);
		streamLast += (LONG)((rows + GAME_OBJECT_MAP_HEIGHT_TILES - 1) /
			GAME_OBJECT_MAP_HEIGHT_TILES);
	}
	return !(lastColumn < streamFirst || firstColumn > streamLast);
}

static void serviceRingWorldStream(UBYTE* bitmap, const GameState* game) {
	ringStreamTouchedFirstColumn = -1;
	ringStreamTouchedLastColumn = -1;
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
		if (ringStreamTouchedFirstColumn < 0 ||
			ringStreamColumn < ringStreamTouchedFirstColumn)
			ringStreamTouchedFirstColumn = ringStreamColumn;
		if (ringStreamColumn > ringStreamTouchedLastColumn)
			ringStreamTouchedLastColumn = ringStreamColumn;

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

/* --- Lightweight sea/carrier ambience overlays -------------------------
 * These are CPU-masked mini-BOBs.  At these tiny sizes the setup traffic for
 * the Blitter costs more than the handful of bytes touched by the 68000.
 * Saved backgrounds are kept per world buffer and erased before streaming,
 * exactly like the projectile BOBs later in the frame. */
static void eraseSeaWaves(UBYTE* bitmap, UBYTE bufferIndex) {
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT)
		return;
	for (UBYTE index = 0; index < SEA_WAVE_MAX; index++) {
		SeaWaveFootprint* footprint = &seaWaveFootprints[bufferIndex][index];
		if (!footprint->valid)
			continue;
		for (UBYTE placement = 0; placement < footprint->placementCount;
			placement++) {
			for (UBYTE row = 0; row < SEA_WAVE_HEIGHT; row++) {
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
	}
}

static void drawSeaWavePlacement(UBYTE* bitmap, SeaWaveFootprint* footprint,
	UBYTE placement, UWORD bufferPixelX, WORD y, UBYTE phase) {
	static const UBYTE shape[4][SEA_WAVE_HEIGHT] = {
		{ 0x18, 0x00 }, { 0x3c, 0x08 },
		{ 0x7e, 0x18 }, { 0x3c, 0x10 }
	};
	UBYTE bitOffset = (UBYTE)(bufferPixelX & 7);
	UWORD byteX = bufferPixelX >> 3;
	UBYTE byteCount = bitOffset ? 2 : 1;
	if (byteX + byteCount > GAME_WORLD_ROW_BYTES)
		byteCount = (UBYTE)(GAME_WORLD_ROW_BYTES - byteX);
	if (!byteCount)
		return;
	footprint->byteX[placement] = byteX;
	footprint->byteCount[placement] = byteCount;

	for (UBYTE row = 0; row < SEA_WAVE_HEIGHT; row++) {
		UWORD mask16 = (UWORD)shape[phase & 3][row] << (8 - bitOffset);
		UBYTE masks[2] = { (UBYTE)(mask16 >> 8), (UBYTE)mask16 };
		UBYTE color = row ? GAME_COLOR_LIGHT_GREY : GAME_COLOR_WHITE;
		UBYTE* dest = bitmap + (y + row) * SCREEN_PLANES *
			GAME_WORLD_ROW_BYTES + byteX;
		for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++) {
			for (UBYTE byte = 0; byte < byteCount; byte++) {
				UBYTE* target = dest + plane * GAME_WORLD_ROW_BYTES + byte;
				UBYTE old = *target;
				footprint->background[placement][row][plane][byte] = old;
				*target = (UBYTE)((old & ~masks[byte]) |
					((color & (1 << plane)) ? masks[byte] : 0));
			}
		}
	}
}

static UWORD ambienceHashForColumn(LONG worldColumn) {
	UWORD value = (UWORD)worldColumn;
	value ^= (UWORD)(value << 7);
	value ^= (UWORD)(value >> 9);
	value = (UWORD)(value * 40503U + 21713U);
	return value;
}

enum {
	SEA_WAVE_UPDATE_NONE = 0,
	SEA_WAVE_UPDATE_PHASE = 1,
	SEA_WAVE_UPDATE_FULL = 2
};

static UBYTE seaWavesUpdateKind(const GameState* game) {
	UBYTE count = 0;
	UBYTE phaseChanged = 0;
	LONG firstColumn = ((LONG)game->scrollX >> 3) - 2;
	LONG lastColumn = firstColumn + GAME_FETCH_BYTES + 5;
	UBYTE basePhase = (UBYTE)((frameCounter / SEA_WAVE_PHASE_FRAMES) & 3);
	for (LONG worldColumn = firstColumn;
		worldColumn <= lastColumn && count < SEA_WAVE_MAX; worldColumn++) {
		UWORD hash = ambienceHashForColumn(worldColumn);
		if ((hash & 3) != 0)
			continue;
		WORD y = (WORD)(SEA_SURFACE_Y + 4 + (((hash >> 4) & 3) * 8));
		ObjectCell cell;
		if (!objectCellForWorldColumnTile(worldColumn, y >> 3, &cell) ||
			cell.id != HAR_OBJ_SEA)
			continue;
		SeaWaveFootprint* footprint = &seaWaveFootprints[0][count++];
		UBYTE phase = (UBYTE)((basePhase + ((hash >> 2) & 3)) & 3);
		if (!footprint->valid || footprint->worldColumn != worldColumn ||
			footprint->y != y)
			return SEA_WAVE_UPDATE_FULL;
		if (footprint->phase != phase)
			phaseChanged = 1;
		LONG pixelOffset = (LONG)((hash >> 8) & 3);
		LONG firstTouched = worldColumn;
		LONG lastTouched = (worldColumn * GAME_TILE_WIDTH + pixelOffset +
			SEA_WAVE_WIDTH - 1) >> 3;
		if (ringStreamMayTouchColumnRange(game, firstTouched, lastTouched))
			return SEA_WAVE_UPDATE_FULL;
	}
	for (; count < SEA_WAVE_MAX; count++)
		if (seaWaveFootprints[0][count].valid)
			return SEA_WAVE_UPDATE_FULL;
	return phaseChanged ? SEA_WAVE_UPDATE_PHASE : SEA_WAVE_UPDATE_NONE;
}

/* Phase-only animation keeps each footprint's authoritative saved
 * background and replaces old/new masks in one byte pass. The previous
 * erase-whole-group/draw-whole-group sequence exposed bare sea to the beam
 * and looked like a flash in the single-buffered playfield. */
static void updateSeaWavePhasesInPlace(UBYTE* bitmap, UBYTE bufferIndex) {
	static const UBYTE shape[4][SEA_WAVE_HEIGHT] = {
		{ 0x18, 0x00 }, { 0x3c, 0x08 },
		{ 0x7e, 0x18 }, { 0x3c, 0x10 }
	};
	UBYTE basePhase = (UBYTE)((frameCounter / SEA_WAVE_PHASE_FRAMES) & 3);
	for (UBYTE index = 0; index < SEA_WAVE_MAX; index++) {
		SeaWaveFootprint* footprint = &seaWaveFootprints[bufferIndex][index];
		if (!footprint->valid)
			continue;
		UWORD hash = ambienceHashForColumn(footprint->worldColumn);
		UBYTE phase = (UBYTE)((basePhase + ((hash >> 2) & 3)) & 3);
		for (UBYTE placement = 0; placement < footprint->placementCount;
			placement++) {
			UBYTE bitOffset = (UBYTE)((footprint->worldColumn *
				GAME_TILE_WIDTH + ((hash >> 8) & 3)) & 7);
			for (UBYTE row = 0; row < SEA_WAVE_HEIGHT; row++) {
				UWORD mask16 = (UWORD)shape[phase][row] << (8 - bitOffset);
				UBYTE masks[2] = { (UBYTE)(mask16 >> 8), (UBYTE)mask16 };
				UBYTE color = row ? GAME_COLOR_LIGHT_GREY : GAME_COLOR_WHITE;
				UBYTE* dest = bitmap + (footprint->y + row) * SCREEN_PLANES *
					GAME_WORLD_ROW_BYTES + footprint->byteX[placement];
				for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++)
					for (UBYTE byte = 0;
						byte < footprint->byteCount[placement]; byte++) {
						UBYTE base = footprint->background[placement][row][plane][byte];
						dest[plane * GAME_WORLD_ROW_BYTES + byte] =
							(UBYTE)((base & ~masks[byte]) |
							((color & (1 << plane)) ? masks[byte] : 0));
					}
			}
		}
		footprint->phase = phase;
	}
}

static void drawSeaWaves(UBYTE* bitmap, UBYTE bufferIndex,
	const GameState* game) {
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT)
		return;
	UBYTE count = 0;
	LONG firstColumn = ((LONG)game->scrollX >> 3) - 2;
	LONG lastColumn = firstColumn + GAME_FETCH_BYTES + 5;
	UBYTE phase = (UBYTE)((frameCounter / SEA_WAVE_PHASE_FRAMES) & 3);
	const LONG pagePixels =
		(LONG)GAME_WORLD_SCROLL_PAGE_BYTES * GAME_TILE_WIDTH;

	for (LONG worldColumn = firstColumn;
		worldColumn <= lastColumn && count < SEA_WAVE_MAX; worldColumn++) {
		UWORD hash = ambienceHashForColumn(worldColumn);
		if ((hash & 3) != 0)
			continue;
		WORD y = (WORD)(SEA_SURFACE_Y + 4 + (((hash >> 4) & 3) * 8));
		ObjectCell cell;
		if (!objectCellForWorldColumnTile(worldColumn, y >> 3, &cell) ||
			cell.id != HAR_OBJ_SEA)
			continue;

		LONG worldPixelX = worldColumn * GAME_TILE_WIDTH +
			((hash >> 8) & 3);
		LONG localPixelX = worldPixelX % pagePixels;
		if (localPixelX < 0)
			localPixelX += pagePixels;
		UWORD primaryPixelX =
			(UWORD)(GAME_WORLD_BUFFER_MARGIN_PIXELS + localPixelX);
		SeaWaveFootprint* footprint =
			&seaWaveFootprints[bufferIndex][count++];
		footprint->valid = 1;
		footprint->placementCount = 1;
		footprint->phase = (UBYTE)((phase + ((hash >> 2) & 3)) & 3);
		footprint->y = y;
		footprint->worldColumn = worldColumn;
		drawSeaWavePlacement(bitmap, footprint, 0, primaryPixelX, y,
			footprint->phase);
		if (primaryPixelX <
			(GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) *
			GAME_TILE_WIDTH) {
			footprint->placementCount = 2;
			drawSeaWavePlacement(bitmap, footprint, 1,
				(UWORD)(primaryPixelX + pagePixels), y,
				footprint->phase);
		}
	}
}

static void eraseCarrierGulls(UBYTE* bitmap, UBYTE bufferIndex) {
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT)
		return;
	/* Gulls can overlap. They are drawn from index 0 upwards, so restore in
	 * reverse order. Forward restoration can copy pixels from a later gull
	 * (captured as saved background) back into the sky as a white trail. */
	for (UBYTE remaining = CARRIER_GULL_MAX; remaining > 0; remaining--) {
		UBYTE index = (UBYTE)(remaining - 1);
		CarrierGullFootprint* footprint =
			&carrierGullFootprints[bufferIndex][index];
		if (!footprint->valid)
			continue;
		for (UBYTE placement = 0; placement < footprint->placementCount;
			placement++) {
			for (UBYTE row = 0; row < CARRIER_GULL_HEIGHT; row++) {
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
	}
}

static void resetCarrierGullActors(void) {
	memset(carrierGulls, 0, sizeof(carrierGulls));
	carrierGullIdleFrames = 0;
	carrierGullSpawnFrames = 0;
	carrierGullSpawnInterval = CARRIER_GULL_SPAWN_INTERVAL_MIN_FRAMES;
	carrierGullTargetCount = 0;
	carrierGullFlockFrames = 0;
	carrierGullFlockLifetime = 0;
	carrierGullLfsr = 0x593d;
}

static void resetCarrierAmbienceVisuals(void) {
	memset(seaWaveFootprints, 0, sizeof(seaWaveFootprints));
	memset(carrierGullFootprints, 0, sizeof(carrierGullFootprints));
	resetCarrierGullActors();
}

static UBYTE carrierGullsNeedRedraw(const GameState* game) {
	const LONG pagePixels =
		(LONG)GAME_WORLD_SCROLL_PAGE_BYTES * GAME_TILE_WIDTH;
	(void)pagePixels;
	for (UBYTE index = 0; index < CARRIER_GULL_MAX; index++) {
		CarrierGull* gull = &carrierGulls[index];
		CarrierGullFootprint* footprint = &carrierGullFootprints[0][index];
		LONG worldX = gull->worldX256 >> 8;
		WORD y = (WORD)(gull->y256 >> 8);
		LONG screenX = worldX - game->scrollX;
		UBYTE visible = gull->active && screenX > -CARRIER_GULL_WIDTH &&
			screenX < SCREEN_WIDTH && y >= 0 &&
			y + CARRIER_GULL_HEIGHT <= GAME_WORLD_HEIGHT;
		if (!visible) {
			if (footprint->valid)
				return 1;
			continue;
		}
		UBYTE phase = gull->scattering ? 0 :
			(UBYTE)(((frameCounter / CARRIER_GULL_FLAP_PHASE_FRAMES) +
				gull->flapOffset) % 3);
		if (!footprint->valid || footprint->worldX != worldX ||
			footprint->y != y || footprint->phase != phase ||
			footprint->variant != gull->variant ||
			footprint->scale != gull->scale)
			return 1;
		if (ringStreamMayTouchColumnRange(game, worldX >> 3,
			(worldX + CARRIER_GULL_WIDTH - 1) >> 3))
			return 1;
	}
	return 0;
}

static UWORD nextCarrierGullRandom(void) {
	carrierGullLfsr = (UWORD)((carrierGullLfsr >> 1) ^
		(-(WORD)(carrierGullLfsr & 1) & 0xb400));
	if (!carrierGullLfsr)
		carrierGullLfsr = 0x593d;
	return carrierGullLfsr;
}

static void spawnCarrierGull(const GameState* game, UBYTE index) {
	CarrierGull* gull = &carrierGulls[index];
	UWORD random = nextCarrierGullRandom();
	/* Enter from outside the visible fetch instead of materialising around the
	 * carrier. This also gives each deterministic bird a visibly different
	 * travel distance before it settles into the carrier orbit. */
	UBYTE enterFromRight = (UBYTE)((random >> 7) & 1);
	WORD screenX = enterFromRight ? SCREEN_WIDTH : -CARRIER_GULL_WIDTH;
	gull->active = 1;
	gull->scattering = 0;
	gull->variant = (UBYTE)((random >> 8) & 1);
	gull->flapOffset = (UBYTE)((random >> 10) & 7);
	/* Birds enter as distant silhouettes and grow only at two discrete
	 * approach thresholds. Some remain mid-distance, which avoids a flock of
	 * identical full-size BOBs and costs no runtime scaling. */
	gull->scale = 0;
	gull->maxScale = (UBYTE)(1 + ((random >> 12) & 1));
	gull->scatterFrames = 0;
	gull->worldX256 = ((LONG)game->scrollX + screenX) << 8;
	gull->y256 = (LONG)(30 + ((random >> 2) & 55)) << 8;
	gull->velocityX256 = enterFromRight ?
		-CARRIER_GULL_CRUISE_X256 : CARRIER_GULL_CRUISE_X256;
	gull->velocityY256 = (random & 0x100) ?
		CARRIER_GULL_CRUISE_Y256 : -CARRIER_GULL_CRUISE_Y256;
}

static void updateCarrierGulls(const GameState* game, UBYTE idleEligible,
	UBYTE sceneActive) {
	if (!sceneActive || game->gameOver) {
		/* Keep the old footprints valid until the late renderer restores them.
		 * Clearing footprint metadata here used to strand the final gull pixels
		 * in the single world buffer when Game Over deactivated the actors. */
		resetCarrierGullActors();
		return;
	}
	if (idleEligible) {
		if (carrierGullIdleFrames < CARRIER_GULL_IDLE_DELAY_FRAMES)
			carrierGullIdleFrames++;
		else if (!carrierGullTargetCount) {
			UWORD random = nextCarrierGullRandom();
			/* An idle deck is allowed to have no birds at all. Zero restarts
			 * the quiet wait; one to three build a temporary flock. */
			carrierGullTargetCount =
				(UBYTE)(random % (CARRIER_GULL_MAX + 1));
			carrierGullSpawnInterval = (UBYTE)(
				CARRIER_GULL_SPAWN_INTERVAL_MIN_FRAMES +
				((random >> 4) & CARRIER_GULL_SPAWN_INTERVAL_VARIATION));
			carrierGullFlockFrames = 0;
			carrierGullFlockLifetime =
				(UWORD)(400 + ((random >> 6) & 255));
			if (!carrierGullTargetCount)
				carrierGullIdleFrames = 0;
		} else if (++carrierGullSpawnFrames >= carrierGullSpawnInterval) {
			carrierGullSpawnFrames = 0;
			UBYTE activeCount = 0;
			for (UBYTE index = 0; index < CARRIER_GULL_MAX; index++)
				if (carrierGulls[index].active)
					activeCount++;
			if (activeCount < carrierGullTargetCount) {
				for (UBYTE index = 0; index < CARRIER_GULL_MAX; index++) {
					if (!carrierGulls[index].active) {
						spawnCarrierGull(game, index);
						break;
					}
				}
				carrierGullSpawnInterval = (UBYTE)(
					CARRIER_GULL_SPAWN_INTERVAL_MIN_FRAMES +
					(nextCarrierGullRandom() &
					 CARRIER_GULL_SPAWN_INTERVAL_VARIATION));
			}
		}
		if (carrierGullTargetCount) {
			UBYTE activeCount = 0;
			for (UBYTE index = 0; index < CARRIER_GULL_MAX; index++)
				if (carrierGulls[index].active &&
					!carrierGulls[index].scattering)
					activeCount++;
			if (activeCount >= carrierGullTargetCount &&
				++carrierGullFlockFrames >= carrierGullFlockLifetime) {
				/* Let the flock leave instead of orbiting forever. Reuse the
				 * proven takeoff-scatter exit so footprints are retired safely. */
				for (UBYTE index = 0; index < CARRIER_GULL_MAX; index++) {
					CarrierGull* gull = &carrierGulls[index];
					if (!gull->active || gull->scattering)
						continue;
					gull->scattering = 1;
					gull->scatterFrames = 0;
					gull->velocityX256 = gull->velocityX256 < 0 ? -192 : 192;
					gull->velocityY256 = -96;
				}
				carrierGullTargetCount = 0;
				carrierGullIdleFrames = 0;
				carrierGullSpawnFrames = 0;
			}
		}
	} else {
		carrierGullIdleFrames = 0;
		carrierGullSpawnFrames = 0;
		carrierGullTargetCount = 0;
		carrierGullFlockFrames = 0;
		carrierGullFlockLifetime = 0;
		for (UBYTE index = 0; index < CARRIER_GULL_MAX; index++) {
			CarrierGull* gull = &carrierGulls[index];
			if (!gull->active || gull->scattering)
				continue;
			gull->scattering = 1;
			gull->scatterFrames = 0;
			gull->velocityX256 =
				((gull->worldX256 >> 8) <
				 (LONG)game->scrollX + game->playerX) ? -384 : 384;
			gull->velocityY256 = -256;
		}
	}

	LONG anchor = (LONG)game->scrollX + game->playerX;
	for (UBYTE index = 0; index < CARRIER_GULL_MAX; index++) {
		CarrierGull* gull = &carrierGulls[index];
		if (!gull->active)
			continue;
		gull->worldX256 += gull->velocityX256;
		gull->y256 += gull->velocityY256;
		if (!gull->scattering) {
			LONG worldX = gull->worldX256 >> 8;
			LONG distance = worldX - anchor;
			if (distance < 0)
				distance = -distance;
			if (distance < 112 && gull->scale < 1)
				gull->scale = 1;
			if (distance < 56 && gull->maxScale > 1)
				gull->scale = 2;
			if (worldX < anchor - 72)
				gull->velocityX256 = CARRIER_GULL_CRUISE_X256;
			else if (worldX > anchor + 88)
				gull->velocityX256 = -CARRIER_GULL_CRUISE_X256;
			WORD y = (WORD)(gull->y256 >> 8);
			if (y < 30)
				gull->velocityY256 = CARRIER_GULL_CRUISE_Y256;
			else if (y > 86)
				gull->velocityY256 = -CARRIER_GULL_CRUISE_Y256;
		} else {
			gull->scatterFrames++;
			/* Take-off scares the birds away into the distance: shrink in
			 * stepped banks while accelerating upward and outward. */
			if (gull->scatterFrames == 14 && gull->scale > 1)
				gull->scale = 1;
			else if (gull->scatterFrames == 30)
				gull->scale = 0;
			gull->velocityY256 -= 10;
			LONG screenX = (gull->worldX256 >> 8) - game->scrollX;
			if (screenX < -CARRIER_GULL_WIDTH ||
				screenX > SCREEN_WIDTH || (gull->y256 >> 8) < -8)
				gull->active = 0;
		}
	}
}

static void drawCarrierGullPlacement(UBYTE* bitmap,
	CarrierGullFootprint* footprint, UBYTE placement, UWORD bufferPixelX,
	WORD y, UBYTE phase, UBYTE variant, UBYTE scale) {
	(void)variant;
	/* Three hand-authored OCS banks: distant 4x3, middle 6x4 and near
	 * 8x5. Keeping the perspective banks precomputed is cheaper than runtime
	 * scaling, while the closest gull is now half the former 16px width. */
	static const UWORD shape[3][3][CARRIER_GULL_HEIGHT] = {
		{
			{ 0, 0, 0, 0x0240, 0x0180, 0, 0, 0, 0 },
			{ 0, 0, 0, 0x0180, 0x0180, 0, 0, 0, 0 },
			{ 0, 0, 0, 0x0180, 0x0240, 0, 0, 0, 0 }
		}, {
			{ 0, 0, 0, 0x0420, 0x0240, 0x0180, 0, 0, 0 },
			{ 0, 0, 0, 0x0420, 0x0240, 0x0180, 0, 0, 0 },
			{ 0, 0, 0, 0x0180, 0x0240, 0x0420, 0, 0, 0 }
		}, {
			{ 0, 0, 0x0810, 0x0420, 0x0240, 0x0180, 0, 0, 0 },
			{ 0, 0, 0x0810, 0x0420, 0x0240, 0x0180, 0, 0, 0 },
			{ 0, 0, 0x0180, 0x0240, 0x0420, 0x0810, 0, 0, 0 }
		}
	};
	UBYTE bitOffset = (UBYTE)(bufferPixelX & 7);
	UWORD byteX = bufferPixelX >> 3;
	UBYTE byteCount = (UBYTE)(bitOffset ? 3 : 2);
	if (byteX + byteCount > GAME_WORLD_ROW_BYTES)
		byteCount = (UBYTE)(GAME_WORLD_ROW_BYTES - byteX);
	if (!byteCount)
		return;
	footprint->byteX[placement] = byteX;
	footprint->byteCount[placement] = byteCount;
	for (UBYTE row = 0; row < CARRIER_GULL_HEIGHT; row++) {
		ULONG shifted = (ULONG)shape[scale % 3][phase % 3][row] <<
			(8 - bitOffset);
		UBYTE masks[3] = {
			(UBYTE)(shifted >> 16), (UBYTE)(shifted >> 8), (UBYTE)shifted
		};
		/* Distance is expressed by silhouette size, not a different palette
		 * colour. The old per-bird grey variant made gull #2 look faded and its
		 * sparse flap phase read as a blink against the bright sky. */
		UBYTE color = GAME_COLOR_WHITE;
		UBYTE* dest = bitmap + (y + row) * SCREEN_PLANES *
			GAME_WORLD_ROW_BYTES + byteX;
		for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++) {
			for (UBYTE byte = 0; byte < byteCount; byte++) {
				UBYTE* target = dest + plane * GAME_WORLD_ROW_BYTES + byte;
				UBYTE old = *target;
				footprint->background[placement][row][plane][byte] = old;
				*target = (UBYTE)((old & ~masks[byte]) |
					((color & (1 << plane)) ? masks[byte] : 0));
			}
		}
	}
}

static void drawCarrierGulls(UBYTE* bitmap, UBYTE bufferIndex,
	const GameState* game) {
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT)
		return;
	const LONG pagePixels =
		(LONG)GAME_WORLD_SCROLL_PAGE_BYTES * GAME_TILE_WIDTH;
	for (UBYTE index = 0; index < CARRIER_GULL_MAX; index++) {
		CarrierGull* gull = &carrierGulls[index];
		if (!gull->active)
			continue;
		LONG worldX = gull->worldX256 >> 8;
		WORD y = (WORD)(gull->y256 >> 8);
		LONG screenX = worldX - game->scrollX;
		if (screenX <= -CARRIER_GULL_WIDTH || screenX >= SCREEN_WIDTH ||
			y < 0 || y + CARRIER_GULL_HEIGHT > GAME_WORLD_HEIGHT)
			continue;
		LONG localPixelX = worldX % pagePixels;
		if (localPixelX < 0)
			localPixelX += pagePixels;
		UWORD primaryPixelX =
			(UWORD)(GAME_WORLD_BUFFER_MARGIN_PIXELS + localPixelX);
		CarrierGullFootprint* footprint =
			&carrierGullFootprints[bufferIndex][index];
		footprint->valid = 1;
		footprint->placementCount = 1;
		footprint->y = y;
		UBYTE phase = gull->scattering ? 0 :
			(UBYTE)(((frameCounter / CARRIER_GULL_FLAP_PHASE_FRAMES) +
				gull->flapOffset) % 3);
		footprint->worldX = worldX;
		footprint->phase = phase;
		footprint->variant = gull->variant;
		footprint->scale = gull->scale;
		drawCarrierGullPlacement(bitmap, footprint, 0, primaryPixelX, y,
			phase, gull->variant, gull->scale);
		if (primaryPixelX <
			(GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) *
			GAME_TILE_WIDTH) {
			footprint->placementCount = 2;
			drawCarrierGullPlacement(bitmap, footprint, 1,
				(UWORD)(primaryPixelX + pagePixels), y, phase,
				gull->variant, gull->scale);
		}
	}
}

static UBYTE seaAmbienceTargetForGame(const GameState* game,
	UBYTE carrierIdleEligible, UBYTE sceneActive) {
	if (!sceneActive || game->gameOver || game->crashTimer)
		return 0;
	if (carrierIdleEligible)
		return SEA_AMBIENCE_IDLE_VOLUME;
	if (game->takeoffState != TAKEOFF_STATE_AIRBORNE ||
		game->missionComplete)
		return 0;
	LONG worldColumn = ((LONG)game->scrollX + game->playerX +
		(PLAYER_SPRITE_WIDTH / 2)) >> 3;
	ObjectCell cell;
	if (!objectCellForWorldColumnTile(worldColumn, GAME_SEA_TOP_TILE_Y,
		&cell) || cell.id != HAR_OBJ_SEA)
		return 0;
	WORD clearance = (WORD)(SEA_SURFACE_Y -
		(game->playerY + PLAYER_SPRITE_HEIGHT));
	if (clearance <= 40)
		return SEA_AMBIENCE_FLIGHT_VOLUME;
	if (clearance <= 64)
		return (UBYTE)(SEA_AMBIENCE_FLIGHT_VOLUME - 2);
	return 0;
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
 * overlay rebuild at that rate would be far too expensive. Promoted carrier
 * and gunship art is reapplied for this exact row after the base tile, since
 * those masked layers are not part of buildWorldTileColumn(). */
static void bobCompositorErase(UBYTE* bitmap, LONG worldColumnLeft, WORD tileRow, UBYTE columnCount) {
	for (UBYTE column = 0; column < columnCount; column++) {
		RenderColumn rebuilt;
		LONG worldColumn = worldColumnLeft + column;
		buildWorldTileColumn(worldColumn, &rebuilt);
		UWORD tileX = ringWorldTileXForColumn(worldColumn);
		drawGameScrollTile(bitmap, (short)tileX, (short)tileRow, rebuilt.tile[tileRow]);
		drawDirectColumnRangeObjectRow(bitmap, tileX, worldColumn, tileRow);
		if (tileX < GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) {
			UWORD duplicateTileX =
				(UWORD)(tileX + GAME_WORLD_SCROLL_PAGE_BYTES);
			drawGameScrollTile(bitmap, (short)(tileX + GAME_WORLD_SCROLL_PAGE_BYTES),
				(short)tileRow, rebuilt.tile[tileRow]);
			drawDirectColumnRangeObjectRow(bitmap, duplicateTileX,
				worldColumn, tileRow);
		}
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

static void eraseAircraftFailureSmokeFootprint(UBYTE* bitmap,
	UBYTE bufferIndex) {
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT)
		return;
	AircraftFailureSmokeFootprint* footprint =
		&aircraftFailureSmokeFootprints[bufferIndex];
	if (!footprint->valid)
		return;

	/* Resolve a whole column once, then restore each touched row. Calling the
	 * generic one-row eraser repeatedly would rebuild the same procedural
	 * column up to three times per frame on a 68000. */
	for (UBYTE column = 0; column < footprint->columnCount; column++) {
		LONG worldColumn = footprint->firstWorldColumn + column;
		RenderColumn rebuilt;
		buildWorldTileColumn(worldColumn, &rebuilt);
		UWORD tileX = ringWorldTileXForColumn(worldColumn);
		for (UBYTE row = 0; row < footprint->rowCount; row++) {
			WORD tileRow = (WORD)(footprint->firstTileRow + row);
			if (tileRow < 0 || tileRow >= GAME_OBJECT_MAP_HEIGHT_TILES)
				continue;
			drawGameScrollTile(bitmap, (short)tileX, (short)tileRow,
				rebuilt.tile[tileRow]);
			drawDirectColumnRangeObjectRow(bitmap, tileX, worldColumn,
				tileRow);
			if (tileX < GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) {
				UWORD duplicateTileX =
					(UWORD)(tileX + GAME_WORLD_SCROLL_PAGE_BYTES);
				drawGameScrollTile(bitmap,
					(short)duplicateTileX,
					(short)tileRow, rebuilt.tile[tileRow]);
				drawDirectColumnRangeObjectRow(bitmap, duplicateTileX,
					worldColumn, tileRow);
			}
		}
	}
	footprint->valid = 0;
}

static void plotAircraftFailureSmokePixelAt(UBYTE* bitmap,
	UWORD bufferPixelX, WORD y, UBYTE color) {
	if (y < 0 || y >= GAME_WORLD_HEIGHT ||
		bufferPixelX >= GAME_WORLD_BUFFER_WIDTH)
		return;
	UWORD byteX = bufferPixelX >> 3;
	UBYTE bit = (UBYTE)(0x80 >> (bufferPixelX & 7));
	UBYTE* row = bitmap + y * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + byteX;
	for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++) {
		UBYTE* target = row + plane * GAME_WORLD_ROW_BYTES;
		if (color & (1 << plane))
			*target |= bit;
		else
			*target &= (UBYTE)~bit;
	}
}

static void plotAircraftFailureSmokePixel(UBYTE* bitmap, LONG worldX,
	WORD y, UBYTE color) {
	const LONG pagePixels =
		(LONG)GAME_WORLD_SCROLL_PAGE_BYTES * GAME_TILE_WIDTH;
	LONG localPixelX = worldX % pagePixels;
	if (localPixelX < 0)
		localPixelX += pagePixels;
	UWORD primaryPixelX =
		(UWORD)(GAME_WORLD_BUFFER_MARGIN_PIXELS + localPixelX);
	plotAircraftFailureSmokePixelAt(bitmap, primaryPixelX, y, color);
	if (primaryPixelX <
		(GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) * GAME_TILE_WIDTH)
		plotAircraftFailureSmokePixelAt(bitmap,
			(UWORD)(primaryPixelX + pagePixels), y, color);
}

static UBYTE aircraftFailureSmokeSize(UBYTE age) {
	if (age < 10)
		return 2;
	return 3;
}

static UBYTE aircraftFailureSmokeVisualPhase(UBYTE age) {
	if (age < 4)
		return age;
	if (age < 12)
		return 4;
	if (age < 21)
		return (UBYTE)(5 + ((age - 12) >> 2));
	return 8;
}

static ULONG aircraftFailureSmokeSignature(void) {
	ULONG signature = 2166136261UL;
	UBYTE any = 0;
	for (UBYTE index = 0; index < AIRCRAFT_FAILURE_SMOKE_MAX; index++) {
		const AircraftFailureSmokeParticle* particle =
			&aircraftFailureSmoke[index];
		signature ^= particle->active;
		signature *= 16777619UL;
		if (!particle->active)
			continue;
		any = 1;
		signature ^= (ULONG)particle->worldX;
		signature *= 16777619UL;
		signature ^= (UWORD)particle->y;
		signature *= 16777619UL;
		signature ^= aircraftFailureSmokeVisualPhase(particle->age);
		signature *= 16777619UL;
	}
	return any ? signature : 0;
}

static UBYTE aircraftFailureSmokeNeedsRedraw(const GameState* game) {
	AircraftFailureSmokeFootprint* footprint =
		&aircraftFailureSmokeFootprints[0];
	ULONG signature = aircraftFailureSmokeSignature();
	if (!footprint->valid)
		return signature != 0;
	if (footprint->renderSignature != signature)
		return 1;
	return ringStreamMayTouchColumnRange(game, footprint->firstWorldColumn,
		footprint->firstWorldColumn + footprint->columnCount - 1);
}

static void drawAircraftFailureSmoke(UBYTE* bitmap, UBYTE bufferIndex) {
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT)
		return;
	LONG firstColumn = 0x7fffffff;
	LONG lastColumn = -1;
	WORD firstRow = 32767;
	WORD lastRow = -1;
	UBYTE any = 0;

	for (UBYTE index = 0; index < AIRCRAFT_FAILURE_SMOKE_MAX; index++) {
		AircraftFailureSmokeParticle* particle = &aircraftFailureSmoke[index];
		if (!particle->active)
			continue;
		UBYTE size = aircraftFailureSmokeSize(particle->age);
		if (particle->y + size <= 0 || particle->y >= GAME_WORLD_HEIGHT)
			continue;
		LONG leftColumn = particle->worldX >> 3;
		LONG rightColumn = (particle->worldX + size - 1) >> 3;
		WORD topRow = particle->y >> 3;
		WORD bottomRow = (particle->y + size - 1) >> 3;
		if (!any || leftColumn < firstColumn)
			firstColumn = leftColumn;
		if (!any || rightColumn > lastColumn)
			lastColumn = rightColumn;
		if (!any || topRow < firstRow)
			firstRow = topRow;
		if (!any || bottomRow > lastRow)
			lastRow = bottomRow;
		any = 1;

		UBYTE color = particle->age < 2 ? GAME_COLOR_YELLOW :
			(particle->age < 4 ? GAME_COLOR_RED :
			(particle->age < 12 ? GAME_COLOR_DARK_GREY :
			(particle->age < 21 ? GAME_COLOR_MID_GREY :
			GAME_COLOR_LIGHT_GREY)));
		for (UBYTE y = 0; y < size; y++) {
			for (UBYTE x = 0; x < size; x++) {
				/* Two alternating deterministic dither masks keep expanding smoke
				 * irregular without consuming the CPC gameplay RNG sequence. */
				if (particle->age >= 12 &&
					(((x + y + aircraftFailureSmokeVisualPhase(
						particle->age) + index) & 3) == 0))
					continue;
				plotAircraftFailureSmokePixel(bitmap,
					particle->worldX + x, (WORD)(particle->y + y), color);
			}
		}
	}

	AircraftFailureSmokeFootprint* footprint =
		&aircraftFailureSmokeFootprints[bufferIndex];
	if (!any) {
		footprint->valid = 0;
		return;
	}
	footprint->valid = 1;
	footprint->firstWorldColumn = firstColumn;
	footprint->columnCount = (UBYTE)(lastColumn - firstColumn + 1);
	footprint->firstTileRow = firstRow;
	footprint->rowCount = (UBYTE)(lastRow - firstRow + 1);
	footprint->renderSignature = aircraftFailureSmokeSignature();
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
static UBYTE bombImpactBobFootprintKind = 0xff;

static void eraseBombImpactBobFootprint(UBYTE* bitmap) {
	if (!bombImpactBobFootprintValid)
		return;
	bobCompositorErase(bitmap, bombImpactBobFootprintWorldColumn, bombImpactBobFootprintRow, 1);
	bombImpactBobFootprintValid = 0;
	bombImpactBobFootprintKind = 0xff;
}

static void drawBombImpactBobAt(UBYTE* bitmap, LONG worldColumn, WORD row, UBYTE kind) {
	buildBombImpactBobTileIfNeeded(kind);
	if (bombImpactBobFootprintValid &&
		(bombImpactBobFootprintWorldColumn != worldColumn ||
		 bombImpactBobFootprintRow != row ||
		 bombImpactBobFootprintKind != kind))
		eraseBombImpactBobFootprint(bitmap);
	else if (bombImpactBobFootprintValid &&
		!(ringStreamTouchedFirstColumn >= 0 &&
		  worldColumn >= ringStreamTouchedFirstColumn &&
		  worldColumn <= ringStreamTouchedLastColumn)) {
		bobImpactUnchangedSkips++;
		return;
	} else if (bombImpactBobFootprintValid) {
		/* Streaming may have replaced only part of the masked tile. Rebuild it
		 * from world truth before applying the same impact frame again. */
		eraseBombImpactBobFootprint(bitmap);
	}
	bobCompositorDrawMasked(bitmap, worldColumn, row, bombImpactBobTile);
	bombImpactBobFootprintWorldColumn = worldColumn;
	bombImpactBobFootprintRow = row;
	bombImpactBobFootprintKind = kind;
	bombImpactBobFootprintValid = 1;
	bobImpactRedraws++;
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
			UBYTE kind;
			if (game->impact.type == IMPACT_TYPE_WATER_SPLASH)
				kind = game->impact.timer >= WATER_SPLASH_FRAME_TICKS ?
					BOMB_IMPACT_BOB_KIND_WATER_SMOKE_1 :
					BOMB_IMPACT_BOB_KIND_WATER_SMOKE_2;
			else
				kind = (game->impact.timer & 2) ?
					BOMB_IMPACT_BOB_KIND_IMPACT_LARGE :
					BOMB_IMPACT_BOB_KIND_IMPACT_SMALL;
			drawBombImpactBobAt(bitmap, worldColumn, row, kind);
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

/* A retained projectile normally gets erased immediately before all missiles
 * are redrawn near the end of the frame. Only erase it early when this frame's
 * ring streamer may recycle one of its saved-background columns; otherwise
 * the old erase-to-redraw gap included streaming, waves, gulls and smoke and
 * was long enough to show as a flicker on the single-buffered playfield. */
static UBYTE rocketPixelBobNeedsPreStreamErase(const GameState* game,
	UBYTE bufferIndex, const RocketShotFootprint* footprints) {
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT)
		return 0;
	const RocketShotFootprint* footprint = &footprints[bufferIndex];
	if (!footprint->valid)
		return 0;
	LONG firstColumn = footprint->worldX >> 3;
	LONG lastColumn =
		(footprint->worldX + ROCKET_PIXEL_BOB_WIDTH - 1) >> 3;
	return ringStreamMayTouchColumnRange(game, firstColumn, lastColumn);
}

static void resetRocketShotPixelBobFootprints(void) {
	memset(rocketShotFootprints, 0, sizeof(rocketShotFootprints));
	memset(wingmanRocketFootprints, 0, sizeof(wingmanRocketFootprints));
	memset(enemyMissileFootprints, 0, sizeof(enemyMissileFootprints));
}

/* A projectile hit can redraw persistent world state before the normal
 * late-frame BOB erase. Restore its saved background first, otherwise that
 * later erase writes stale pre-impact bytes over smoke/craters/damage. */
static void retireBombPixelBobBeforeWorldMutation(UBYTE** worldBuffers,
	BombShotFootprint* footprints) {
	for (UBYTE bufferIndex = 0; bufferIndex < GAME_WORLD_BUFFER_COUNT;
		bufferIndex++)
		eraseBombPixelBobFootprint(worldBuffers[bufferIndex], bufferIndex,
			footprints);
}

static void retireRocketPixelBobBeforeWorldMutation(UBYTE** worldBuffers,
	RocketShotFootprint* footprints) {
	for (UBYTE bufferIndex = 0; bufferIndex < GAME_WORLD_BUFFER_COUNT;
		bufferIndex++)
		eraseRocketPixelBobFootprint(worldBuffers[bufferIndex], bufferIndex,
			footprints);
}

static void drawRocketShotPixelBobPlacement(UBYTE* bitmap,
	RocketShotFootprint* footprint, UBYTE placement, UWORD bufferPixelX,
	WORD screenY, UBYTE tileId, UBYTE monochromeBlack) {
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
			UBYTE pixelColor = monochromeBlack ? GAME_COLOR_BLACK :
				(color == GAME_COLOR_BLACK ? GAME_COLOR_BLACK : GAME_COLOR_YELLOW);
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
	footprint->worldX = rocket->worldX;
	UBYTE tileId = rocketTileForState(rocket);
	drawRocketShotPixelBobPlacement(bitmap, footprint, 0, primaryPixelX,
		rocket->y, tileId, 0);
	if (primaryPixelX <
		(GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) *
			GAME_TILE_WIDTH) {
		footprint->placementCount = 2;
		drawRocketShotPixelBobPlacement(bitmap, footprint, 1,
			(UWORD)(primaryPixelX + pagePixels), rocket->y,
			tileId, 0);
	}
}

/* CPC heatseekposition selects Mode-1 tiles 53/54/55 for ascending,
 * descending and level flight. Hardware sprite 4 can only address COLOR25-27,
 * which are simultaneously pens 9-11 of the player's attached 16-colour
 * sprite; recolouring them black would corrupt the Harrier. Composite the
 * exact directional silhouettes into the playfield instead, forcing every
 * non-transparent source pixel to stable GAME_COLOR_BLACK. */
static void drawEnemyMissilePixelBob(UBYTE* bitmap, UBYTE bufferIndex,
	const GameState* game) {
	const WeaponState* missile = &game->enemyMissile;
	if (bufferIndex >= GAME_WORLD_BUFFER_COUNT || !missile->active)
		return;
	if (missile->y < 0 ||
		missile->y + ROCKET_PIXEL_BOB_HEIGHT > GAME_WORLD_HEIGHT ||
		missile->x <= -ROCKET_PIXEL_BOB_WIDTH || missile->x >= SCREEN_WIDTH)
		return;

	const LONG pagePixels =
		(LONG)GAME_WORLD_SCROLL_PAGE_BYTES * GAME_TILE_WIDTH;
	LONG localPixelX = missile->worldX % pagePixels;
	if (localPixelX < 0)
		localPixelX += pagePixels;
	UWORD primaryPixelX =
		(UWORD)(GAME_WORLD_BUFFER_MARGIN_PIXELS + localPixelX);
	RocketShotFootprint* footprint = &enemyMissileFootprints[bufferIndex];
	footprint->valid = 1;
	footprint->placementCount = 1;
	footprint->y = missile->y;
	footprint->worldX = missile->worldX;
	UBYTE tileId = missile->dy < 0 ? 53 : (missile->dy > 0 ? 54 : 55);
	drawRocketShotPixelBobPlacement(bitmap, footprint, 0, primaryPixelX,
		missile->y, tileId, 1);
	if (primaryPixelX <
		(GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) * GAME_TILE_WIDTH) {
		footprint->placementCount = 2;
		drawRocketShotPixelBobPlacement(bitmap, footprint, 1,
			(UWORD)(primaryPixelX + pagePixels), missile->y, tileId, 1);
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
 * row even though updatePowerup() already moves at a smooth pixel cadence.
 * Draw the two masked bytes at the real Y and restore only the touched
 * scanlines from world truth before moving it. */
static UBYTE powerupBobFootprintValid = 0;
static LONG powerupBobFootprintWorldColumnLeft = 0;
static WORD powerupBobFootprintY = 0;

static void erasePowerupBobFootprint(UBYTE* bitmap) {
	if (!powerupBobFootprintValid)
		return;
	/* Restore only the eight scanlines actually touched by the old canopy.
	 * Redrawing both complete containing tiles used to overwrite 16 scanlines
	 * per column (32 when crossing a tile boundary), making the single-buffered
	 * erase interval visible as a faint flash. */
	for (UBYTE column = 0; column < 2; column++) {
		LONG worldColumn = powerupBobFootprintWorldColumnLeft + column;
		RenderColumn rebuilt;
		buildWorldTileColumn(worldColumn, &rebuilt);
		UWORD tileX = ringWorldTileXForColumn(worldColumn);
		for (UBYTE pixelRow = 0; pixelRow < POWERUP_SPRITE_HEIGHT;
			pixelRow++) {
			WORD screenY = (WORD)(powerupBobFootprintY + pixelRow);
			if (screenY < 0 || screenY >= GAME_WORLD_HEIGHT)
				continue;
			WORD tileRow = screenY >> 3;
			if (tileRow < 0 || tileRow >= GAME_OBJECT_MAP_HEIGHT_TILES)
				continue;
			UBYTE tileId = rebuilt.tile[tileRow];
			if (tileId >= GAME_TILE_COUNT)
				tileId = 0;
			const UBYTE* src = gameTiles + tileId * GAME_TILE_BYTES +
				(screenY & 7) * GAME_TILE_PLANES;
			UBYTE* dest = bitmap +
				screenY * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + tileX;
			for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++)
				dest[plane * GAME_WORLD_ROW_BYTES] = src[plane];
			if (tileX < GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) {
				dest += GAME_WORLD_SCROLL_PAGE_BYTES;
				for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++)
					dest[plane * GAME_WORLD_ROW_BYTES] = src[plane];
			}
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

/* The world playfield is intentionally single-buffered. Erasing all eight
 * old scanlines and only then drawing all eight new ones exposed a short blank
 * interval every frame, which read as a flickering parachute. For the common
 * one-pixel fall with unchanged world columns, rebuild each union scanline and
 * immediately composite the new row before touching the next scanline. */
static void redrawPowerupBobVerticalTransition(UBYTE* bitmap,
	LONG worldColumnLeft, WORD oldY, WORD newY) {
	RenderColumn rebuilt[2];
	for (UBYTE column = 0; column < 2; column++)
		buildWorldTileColumn(worldColumnLeft + column, &rebuilt[column]);

	WORD firstY = oldY < newY ? oldY : newY;
	WORD oldBottom = (WORD)(oldY + POWERUP_SPRITE_HEIGHT - 1);
	WORD newBottom = (WORD)(newY + POWERUP_SPRITE_HEIGHT - 1);
	WORD lastY = oldBottom > newBottom ? oldBottom : newBottom;
	for (WORD screenY = firstY; screenY <= lastY; screenY++) {
		if (screenY < 0 || screenY >= GAME_WORLD_HEIGHT)
			continue;
		WORD tileRow = screenY >> 3;
		if (tileRow < 0 || tileRow >= GAME_OBJECT_MAP_HEIGHT_TILES)
			continue;

		for (UBYTE column = 0; column < 2; column++) {
			LONG worldColumn = worldColumnLeft + column;
			UWORD tileX = ringWorldTileXForColumn(worldColumn);
			UBYTE tileId = rebuilt[column].tile[tileRow];
			if (tileId >= GAME_TILE_COUNT)
				tileId = 0;
			const UBYTE* worldSrc = gameTiles + tileId * GAME_TILE_BYTES +
				(screenY & 7) * GAME_TILE_PLANES;
			const UBYTE* bobTile = column ? powerupBobTileRight :
				powerupBobTileLeft;
			const UBYTE* bobSrc = 0;
			UBYTE mask = 0;
			if (screenY >= newY && screenY <= newBottom) {
				bobSrc = bobTile + (screenY - newY) *
					(GAME_WORLD_DISPLAY_PLANES + 1);
				mask = bobSrc[GAME_WORLD_DISPLAY_PLANES];
			}

			UBYTE* dest = bitmap +
				screenY * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + tileX;
			for (UBYTE placement = 0; placement < 2; placement++) {
				for (UBYTE plane = 0; plane < GAME_WORLD_DISPLAY_PLANES; plane++) {
					UBYTE value = worldSrc[plane];
					if (mask)
						value = (UBYTE)((value & ~mask) |
							(bobSrc[plane] & mask));
					dest[plane * GAME_WORLD_ROW_BYTES] = value;
				}
				if (tileX >= GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES)
					break;
				dest += GAME_WORLD_SCROLL_PAGE_BYTES;
			}
		}
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
	if (powerupBobFootprintValid &&
		powerupBobFootprintWorldColumnLeft == worldColumnLeft) {
		redrawPowerupBobVerticalTransition(bitmap, worldColumnLeft,
			powerupBobFootprintY, pixelY);
		powerupBobFootprintY = pixelY;
		return;
	}
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
	LONG worldColumnLeft, UBYTE* targetIsSafe) {
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
	if (targetIsSafe)
		*targetIsSafe = game->wingman.formationBelow ? belowSafe : aboveSafe;
	if (targetRow < 0)
		targetRow = 0;
	if (targetRow > WINGMAN_MAX_ROW)
		targetRow = WINGMAN_MAX_ROW;
	return targetRow;
}

static UBYTE wingmanCellIsPassable(LONG worldColumn, WORD tileRow) {
	/* Intercept checks a six-cell corridor while advancing by less than one
	 * tile per frame; normally five cells overlap the previous query. Cache
	 * only positive answers so a cell that was blocked and later destroyed is
	 * always resolved again immediately. Generated terrain never turns a
	 * previously clear sky cell solid within one session, and runtime flak is
	 * passable by CPC's own Wingman radar rule. */
	UBYTE cacheIndex = (UBYTE)(((UWORD)worldColumn ^
		((UWORD)tileRow << 3)) & (WINGMAN_SAFE_CELL_CACHE_SIZE - 1));
	if (wingmanSafeCellValid[cacheIndex] &&
		wingmanSafeCellColumn[cacheIndex] == (UWORD)worldColumn &&
		wingmanSafeCellRow[cacheIndex] == (UBYTE)tileRow)
		return 1;

	ObjectCell cell;
	if (townBlockCellAtWorldColumnRow(worldColumn, tileRow, &cell))
		return 0;
	if (!objectCellForWorldColumnTile(worldColumn, tileRow, &cell))
		return 0;
	if (cell.id != HAR_OBJ_SKY && cell.id != HAR_OBJ_CLOUD &&
		cell.id != HAR_OBJ_FLAK)
		return 0;
	wingmanSafeCellColumn[cacheIndex] = (UWORD)worldColumn;
	wingmanSafeCellRow[cacheIndex] = (UBYTE)tileRow;
	wingmanSafeCellValid[cacheIndex] = 1;
	return 1;
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
	while (row > 1) {
		if (wingmanFormationRowIsSafe(worldColumnLeft, row))
			return row;
		row--;
	}
	/* CPC's bounded fallback is the top playable row even when no tested
	 * footprint is clear.  Rechecking the same final row cannot change that
	 * result, so avoid another six object-map queries here. */
	return 1;
}

static WORD cachedWingmanFormationTargetRow(GameState* game,
	LONG worldColumnLeft) {
	WingmanState* wingman = &game->wingman;
	WORD playerRow = (WORD)(game->playerY / GAME_TILE_HEIGHT);
	if (!wingman->formationSafetyValid ||
		wingman->formationSafetyColumn != (UWORD)worldColumnLeft ||
		wingman->formationSafetyPlayerRow != playerRow) {
		UBYTE targetIsSafe;
		WORD targetRow = updateWingmanFormationTargetRow(game,
			worldColumnLeft, &targetIsSafe);
		wingman->formationSafetyTargetRow = targetIsSafe ? targetRow :
			wingmanSafeTargetRow(worldColumnLeft, targetRow);
		wingman->formationSafetyColumn = (UWORD)worldColumnLeft;
		wingman->formationSafetyPlayerRow = playerRow;
		wingman->formationSafetyValid = 1;
	}
	return wingman->formationSafetyTargetRow;
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

	WORD targetX = (WORD)(game->playerX -
		WINGMAN_FORMATION_COLUMNS_BEHIND * GAME_TILE_WIDTH);
	if (targetX < 0)
		targetX = 0;
	else if (targetX > SCREEN_WIDTH - PLAYER_SPRITE_WIDTH)
		targetX = SCREEN_WIDTH - PLAYER_SPRITE_WIDTH;

	LONG targetWorldColumn = ((LONG)game->scrollX + targetX) >> 3;
	WORD targetRow = cachedWingmanFormationTargetRow(game, targetWorldColumn);
	WORD xDirection = (wingman->formationLogicalX < targetX) ? 1 :
		((wingman->formationLogicalX > targetX) ? -1 : 0);
	WORD yDirection = (wingman->row < targetRow) ? 1 :
		((wingman->row > targetRow) ? -1 : 0);

	if (xDirection != 0 || yDirection != 0) {
		static const BYTE directionX[WINGMAN_EVASION_DIRECTION_COUNT] =
			{ 0, 1, 1, 1, 0, -1, -1, -1 };
		static const BYTE directionY[WINGMAN_EVASION_DIRECTION_COUNT] =
			{ -1, -1, 0, 1, 1, 1, 0, -1 };
		wingman->moveTimer++;
		if (wingman->moveTimer >= WINGMAN_MOVE_FRAME_INTERVAL) {
			wingman->moveTimer = 0;
			WORD previousX = wingman->formationLogicalX;
			WORD previousRow = wingman->row;
			WORD candidateX = (WORD)(wingman->formationLogicalX +
				xDirection * GAME_TILE_WIDTH);
			WORD candidateRow = (WORD)(wingman->row + yDirection);
			LONG candidateWorldColumn = ((LONG)game->scrollX + candidateX) >> 3;
			UBYTE accepted = candidateX >= 0 &&
				candidateX <= SCREEN_WIDTH - PLAYER_SPRITE_WIDTH &&
				candidateRow >= 0 && candidateRow <= WINGMAN_MAX_ROW &&
				wingmanFormationRowIsSafe(candidateWorldColumn, candidateRow);

			/* CPC checkwingmanradar tries a bounded R&7 direction when the
			 * preferred 0..8 vector is obstructed. Preserve that decision,
			 * but keep it finite so a dense city column cannot stall a frame. */
			UBYTE usedEvasion = 0;
			if (!accepted) {
				UBYTE firstDirection = (UBYTE)((cpcRStateForWorldColumn(
					(UWORD)targetWorldColumn) + wingman->evasionCursor++) & 7);
				for (UBYTE attempt = 0; attempt < WINGMAN_EVASION_DIRECTION_COUNT;
					attempt++) {
					UBYTE direction = (UBYTE)((firstDirection + attempt) & 7);
					candidateX = (WORD)(wingman->formationLogicalX +
						directionX[direction] * GAME_TILE_WIDTH);
					candidateRow = (WORD)(wingman->row + directionY[direction]);
					candidateWorldColumn = ((LONG)game->scrollX + candidateX) >> 3;
					if (candidateX >= 0 &&
						candidateX <= SCREEN_WIDTH - PLAYER_SPRITE_WIDTH &&
						candidateRow >= 0 && candidateRow <= WINGMAN_MAX_ROW &&
						wingmanFormationRowIsSafe(candidateWorldColumn, candidateRow)) {
						accepted = 1;
						usedEvasion = 1;
						break;
					}
				}
			}

			if (accepted) {
				wingman->formationLogicalX = candidateX;
				wingman->row = candidateRow;
				if (candidateX != previousX && candidateRow != previousRow) {
					if (telemetryWingFormationDiagonal < 0xffff)
						telemetryWingFormationDiagonal++;
				} else if (telemetryWingFormationCardinal < 0xffff) {
					telemetryWingFormationCardinal++;
				}
				if (usedEvasion && telemetryWingFormationEvasive < 0xffff)
					telemetryWingFormationEvasive++;
			}
		}
	} else {
		wingman->moveTimer = 0;
		if (telemetryWingFormationStops < 0xffff)
			telemetryWingFormationStops++;
	}

	/* Hardware sprites are not tile bound. Interpolate the accepted CPC
	 * coordinate so diagonal and horizontal formation corrections remain
	 * pixel smooth on Amiga while retaining CPC timing and collision cells. */
	if (wingman->interceptScreenX < wingman->formationLogicalX) {
		WORD stepped = (WORD)(wingman->interceptScreenX + WINGMAN_VISUAL_MOVE_PIXELS);
		wingman->interceptScreenX = stepped > wingman->formationLogicalX ?
			wingman->formationLogicalX : stepped;
	} else if (wingman->interceptScreenX > wingman->formationLogicalX) {
		WORD stepped = (WORD)(wingman->interceptScreenX - WINGMAN_VISUAL_MOVE_PIXELS);
		wingman->interceptScreenX = stepped < wingman->formationLogicalX ?
			wingman->formationLogicalX : stepped;
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
		wingman->mode == WINGMAN_LANDING_DECK ||
		wingman->mode == WINGMAN_PLAYER2_FLIGHT)
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
	UBYTE targetIsSafe;
	WORD targetRow = updateWingmanFormationTargetRow(game,
		targetWorldColumn, &targetIsSafe);
	if (!targetIsSafe)
		targetRow = wingmanSafeTargetRow(targetWorldColumn, targetRow);
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
		wingman->formationLogicalX = targetX;
		wingman->mode = WINGMAN_FORMATION;
		wingman->moveTimer = 0;
	}
}

/* CPC wingmantakeoff=254 is the single authoritative "lost but recoverable"
 * state. Keep all ways of losing the Wingman on the same transition so the
 * ordinary powerup path can revive CPU and Player-2 Wingmen identically. */
static void setWingmanDestroyedState(GameState* game) {
#if HAR_HEADLESS_AUTOPLAY && HAR_HEADLESS_WEAPON_STRESS
	/* The weapon-stress profile measures rendering/collision load across the
	 * complete route.  Keep P2 present after hostile/friendly contacts so the
	 * second rocket and bomb streams do not silently disappear mid-sample. */
	(void)game;
	return;
#endif
	WingmanState* wingman = &game->wingman;
	wingman->active = 0;
	wingman->destroyed = 1;
	wingman->mode = WINGMAN_DESTROYED;
	wingman->rocket.active = 0;
	wingman->bomb.active = 0;
	wingman->returningToFormation = 0;
	wingman->interceptReason = 0;
	wingman->interceptWaypointX = 0;
	wingman->interceptWaypointRow = 0;
	if (game->enemyMissileTarget == ENEMY_TARGET_WINGMAN)
		game->enemyMissileTarget = ENEMY_TARGET_PLAYER;
}

/* Sprint 15.28: Wingman: Player 2's own control - CPC checkwingmankeys
 * (asm:2664-2711) moves the wingman by a fixed 1 character cell per key-test
 * call with no CPU AI involved at all once selected (controlwingmanfunc
 * dispatches straight here, asm:2732-2734, skipping every formation/
 * intercept/bombing-run state above entirely). This port gives Player 2 the
 * same continuous per-frame pixel speed as the player's own plane
 * (PLAYER_MOVE_SPEED_PIXELS) instead of CPC's coarser per-call tile jump -
 * matching this project's established practice of keeping the logical rule
 * (free 4-direction movement, no formation/intercept AI) while rendering it
 * pixel-smooth, the same choice already made for the player's own motion and
 * the CPU Wingman's row-following. Screen bounds reuse the player's own
 * PLAYER_MIN/MAX_X/Y - CPC clamps to its own screen's character-grid edges
 * (asm:2673-2702), this is that same rule translated to this port's actual
 * play area. */
static void resetWingmanBombMotion(WingmanState* wingman);

static void updateWingmanPlayer2Control(GameState* game, UBYTE** worldBuffers,
	const Player2InputState* input2, const Player2InputState* previousInput2,
	UBYTE* hudDirty) {
	WingmanState* wingman = &game->wingman;
	if (game->wingmanControl != WINGMAN_CONTROL_PLAYER2)
		return;

	if (!wingman->active || wingman->mode == WINGMAN_ON_DECK) {
		/* CPC: CPU auto-launches once the player passes the parked Wingman
		 * on deck; Player 2 must press Up before the carrier scrolls out of
		 * reach instead (asm:2611-2625, "OTHERWISE PLAYER 2 MUST TAKEOFF
		 * BEFORE OUT OF VIEW"). The parked aircraft is baked into the carrier
		 * columns, so its visible position already scrolls with the world; keep
		 * the live state at that same carrier-relative X for a seamless late
		 * launch and mark it CPC-destroyed (254) exactly as it leaves at X=0. */
		if (wingman->mode != WINGMAN_ON_DECK || wingman->destroyed)
			return;
		LONG deckScreenX = (LONG)WINGMAN_TAKEOFF_DECK_X - (LONG)game->scrollX;
		wingman->interceptScreenX = (WORD)deckScreenX;
		if (deckScreenX <= 0) {
			setWingmanDestroyedState(game);
			carrierParkedWingmanVisible = 0;
			dirtyRedrawNativeCarrierAt(worldBuffers, 8);
			telemetryLogGameEvent(TELEMETRY_GAME_EVENT_P2_LEFT_BEHIND, 1,
				(UWORD)(game->scrollX >> 3), game, game->scrollX);
			return;
		}
		if (!input2->up)
			return;
		wingman->active = 1;
		wingman->mode = WINGMAN_PLAYER2_FLIGHT;
		if (carrierParkedWingmanVisible) {
			carrierParkedWingmanVisible = 0;
			dirtyRedrawNativeCarrierAt(worldBuffers, 8);
		}
		return;
	}

	if (wingman->mode != WINGMAN_PLAYER2_FLIGHT)
		return;

	if (input2->up && wingman->screenY > PLAYER_MIN_Y)
		wingman->screenY = (WORD)(wingman->screenY - PLAYER_MOVE_SPEED_PIXELS);
	if (input2->down && wingman->screenY < PLAYER_MAX_Y)
		wingman->screenY = (WORD)(wingman->screenY + PLAYER_MOVE_SPEED_PIXELS);
	if (input2->left && wingman->interceptScreenX > PLAYER_MIN_X)
		wingman->interceptScreenX = (WORD)(wingman->interceptScreenX - PLAYER_MOVE_SPEED_PIXELS);
	if (input2->right && wingman->interceptScreenX < PLAYER_MAX_X)
		wingman->interceptScreenX = (WORD)(wingman->interceptScreenX + PLAYER_MOVE_SPEED_PIXELS);
	wingman->row = (WORD)(wingman->screenY / GAME_TILE_HEIGHT);

	/* CPC exposes the same seven bindable actions to Player 2. This port has
	 * no second eject-seat sprite channel, so P2 Eject deliberately performs
	 * the existing Wingman abandon/destroy transition: the aircraft leaves
	 * play and can be recovered through the normal Wingman powerup. */
	if (game->gameMode == GAME_MODE_ENHANCED &&
		Pressed(input2->eject, previousInput2->eject)) {
		setWingmanDestroyedState(game);
		return;
	}

	/* CPC checkfirewingmanmissile: Fire launches the plain missile - no
	 * Maverick lock-on exists for the Wingman's weapon in this port (it
	 * always reuses the player's plain, non-Maverick rocket art/behaviour),
	 * so Left+Fire isn't given a separate meaning here. */
	UBYTE sharedInventory = game->gameMode == GAME_MODE_ENHANCED;
	if (!wingman->rocket.active && (!sharedInventory || game->rockets > 0) &&
		Pressed(input2->fire, previousInput2->fire)) {
		memset(&wingman->rocket, 0, sizeof(wingman->rocket));
		wingman->rocket.active = 1;
		wingman->rocket.x = wingman->interceptScreenX;
		wingman->rocket.y = wingman->screenY;
		wingman->rocket.worldX = (LONG)game->scrollX + wingman->rocket.x;
		wingman->rocket.worldAnchored = 1;
		wingman->rocket.dx = ROCKET_SPEED_PIXELS;
		if (sharedInventory && !debugInfiniteRockets)
			game->rockets--;
#if HAR_DEBUG_PERF_LOG
		perfP2RocketLaunches++;
#endif
		if (sharedInventory) {
			updateHudValues(game);
			*hudDirty = 1;
		}
		playSfxAt(SFX_FIRE, wingman->rocket.x);
	}
	/* Enhanced's two humans share the visible ordnance pool. Classic keeps
	 * the support aircraft's ammunition independent of Player 1's HUD. */
	if (!wingman->bomb.active && (!sharedInventory || game->bombs > 0) &&
		Pressed(input2->bomb, previousInput2->bomb)) {
		memset(&wingman->bomb, 0, sizeof(wingman->bomb));
		wingman->bomb.active = 1;
		wingman->bomb.x = (WORD)(wingman->interceptScreenX + 6);
		wingman->bomb.y = (WORD)(wingman->screenY + PLAYER_SPRITE_HEIGHT - 1);
		wingman->bomb.worldX = (LONG)game->scrollX + wingman->bomb.x;
		wingman->bomb.dy = BOMB_SPEED_Y_PIXELS;
		resetWingmanBombMotion(wingman);
		if (sharedInventory && !debugInfiniteBombs)
			game->bombs--;
#if HAR_DEBUG_PERF_LOG
		perfP2BombLaunches++;
#endif
		if (sharedInventory) {
			updateHudValues(game);
			*hudDirty = 1;
		}
		playSfxAt(SFX_BOMB, wingman->bomb.x);
	}
}

static WORD wingmanScreenX(const GameState* game) {
	const WingmanState* wingman = &game->wingman;
	return (wingman->mode == WINGMAN_ON_DECK ||
		wingman->mode == WINGMAN_TAKEOFF ||
		wingman->mode == WINGMAN_FORMATION ||
		wingman->mode == WINGMAN_INTERCEPT_APPROACH ||
		wingman->mode == WINGMAN_INTERCEPT_TRACK ||
		wingman->mode == WINGMAN_INTERCEPT_FIRE ||
		wingman->mode == WINGMAN_BOMB_APPROACH ||
		wingman->mode == WINGMAN_LANDING_APPROACH ||
		wingman->mode == WINGMAN_LANDING_DECK ||
		wingman->mode == WINGMAN_PLAYER2_FLIGHT)
		? wingman->interceptScreenX
		: (WORD)(game->playerX -
			WINGMAN_FORMATION_COLUMNS_BEHIND * GAME_TILE_WIDTH);
}

/* Wingman uses channel 6. Channel 7 is independent and is used by the
 * player's ejector-seat/parachute sequence. */
static void updateWingmanSprite(UWORD* sprite, UWORD* unusedSprite7, const GameState* game) {
	/* The CPC pixels and pen mapping are immutable.  Converting all 16x16
	 * pixels through a function pointer on every movement frame used to cost
	 * 256 mapping calls plus all sprite-plane packing each time.  Hardware
	 * sprite movement only requires rewriting the two control words; build
	 * the payload once for this allocation and retain it while hidden. CPC
	 * setlandingsprite switches Wingman to sprite_pixel_data19/20 as soon as
	 * the final approach begins, so rebuild only on that state transition. */
	static UWORD* payloadSprite = 0;
	static UBYTE payloadLanding = 0xff;
	const WingmanState* wingman = &game->wingman;
	UBYTE landingArtwork =
		(wingman->mode == WINGMAN_ON_DECK ||
		 wingman->mode == WINGMAN_LANDING_APPROACH ||
		 wingman->mode == WINGMAN_LANDING_DECK) ? 1 : 0;
	(void)unusedSprite7;
	if (payloadSprite != sprite || payloadLanding != landingArtwork) {
		if (landingArtwork)
			buildSpriteFromCpcPlusHalves(sprite, PLAYER_SPRITE_HEIGHT, 0, 0,
				harCpcWingmanLandedLeftPixels, harCpcWingmanLandedRightPixels,
				cpcPlusPenToWingmanHardwareColor);
		else
			buildSpriteFromCpcPlusHalves(sprite, PLAYER_SPRITE_HEIGHT, 0, 0,
				harCpcWingmanFlyingLeftPixels, harCpcWingmanFlyingRightPixels,
				cpcPlusPenToWingmanHardwareColor);
		payloadSprite = sprite;
		payloadLanding = landingArtwork;
	}
	if (!wingman->active ||
		(wingman->mode != WINGMAN_ON_DECK &&
		 wingman->mode != WINGMAN_TAKEOFF &&
		 wingman->mode != WINGMAN_FORMATION &&
		 wingman->mode != WINGMAN_INTERCEPT_APPROACH &&
		 wingman->mode != WINGMAN_INTERCEPT_TRACK &&
		 wingman->mode != WINGMAN_INTERCEPT_FIRE &&
		 wingman->mode != WINGMAN_BOMB_APPROACH &&
		 wingman->mode != WINGMAN_LANDING_APPROACH &&
		 wingman->mode != WINGMAN_LANDING_DECK &&
		 wingman->mode != WINGMAN_PLAYER2_FLIGHT)) {
		hideHardwareSprite(sprite);
		return;
	}

	WORD screenX = wingmanScreenX(game);
	WORD screenY = wingman->screenY;
	setHardwareSpritePosition(sprite, PLAYER_SPRITE_HEIGHT, screenX, screenY);
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
	UBYTE smokeTile = persistentHitSmokeTileAtColumnRow(worldColumn, tileY);
	if (smokeTile)
		dirtyRedrawWorldTile(worldBuffers, worldColumn, tileY, smokeTile);
}

static LONG scrollPointerPixelX(UWORD scrollX) {
	UWORD fine = scrollX & 15;
	/* DDF already fetches one extra 16px word to the left. Point at the
	 * preceding word only on an exact word boundary; intermediate phases use
	 * the containing word plus BPLCON1 delay. Moving both cases another word
	 * backwards double-counts the DDF prefetch: it exposes a 16px empty strip
	 * at the left and the wrong final word at the right edge. */
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

static UWORD displayScrollXForGameState(const GameState* game) {
	/* Presentation follows the real camera phase. */
	return game->scrollX;
}

static USHORT displayByteOffsetForGameState(const GameState* game) {
	UWORD displayScrollX = displayScrollXForGameState(game);
	if (useFixedTakeoffWorldWindow(game))
		return scrollAbsoluteByteOffset(displayScrollX);
	return scrollLocalByteOffset(displayScrollX);
}

/* CPC keeps the completed carrier picture on screen when it advances to the
 * next sortie. The old Amiga transition called initRingWorldBuffer(), which
 * cleared the live single-buffer playfield and synchronously rebuilt 56
 * columns after the carrier slide. Besides the cost, that made the entire
 * landscape visibly appear a second time.
 *
 * At the end of the scripted slide the final carrier occupies the same screen
 * position as the opening carrier. Copy the 320 visible pixels of every
 * bitplane row into the logical column-0 window, then let the ordinary ring
 * streamer create only the still-hidden columns during the next lift. The
 * landed scene therefore survives the mission reset without a second bitmap
 * or a full redraw. The scripted scroll is always byte-aligned (fine 0 or 8);
 * retain a guarded fallback to the old initializer if that invariant changes. */
static UBYTE rebaseLandedWorldForNextMission(UBYTE* bitmap,
	const GameState* game) {
	const UWORD visibleBytes = SCREEN_WIDTH / 8;
	UWORD fine = (UWORD)(displayScrollXForGameState(game) & 15);
	if (fine != 0 && fine != 8)
		return 0;

	USHORT sourceFetchOffset = displayByteOffsetForGameState(game);
	/* On an exact 16px boundary the DDF prefetch consumes the two preceding
	 * bytes. At phase 8 it consumes one. In both cases this selects the first
	 * pixel that is actually visible, rather than the Copper fetch guard. */
	USHORT sourceVisibleOffset = (USHORT)(sourceFetchOffset +
		(fine == 0 ? 2 : 1));
	USHORT destinationVisibleOffset = GAME_WORLD_BUFFER_MARGIN_TILES;
	if (sourceVisibleOffset + visibleBytes > GAME_WORLD_ROW_BYTES ||
		destinationVisibleOffset + visibleBytes > GAME_WORLD_ROW_BYTES)
		return 0;

	UBYTE rowCopy[SCREEN_WIDTH / 8];
	for (UWORD y = 0; y < GAME_WORLD_HEIGHT; y++) {
		UBYTE* interleavedRow = bitmap +
			(ULONG)y * SCREEN_PLANES * GAME_WORLD_ROW_BYTES;
		for (UBYTE plane = 0; plane < SCREEN_PLANES; plane++) {
			UBYTE* planeRow = interleavedRow +
				(ULONG)plane * GAME_WORLD_ROW_BYTES;
			memcpy(rowCopy, planeRow + sourceVisibleOffset, visibleBytes);
			memcpy(planeRow + destinationVisibleOffset, rowCopy, visibleBytes);
		}
	}

	/* Columns 0..39 are now the retained visible scene. Column 40 onward is
	 * filled incrementally by serviceRingWorldStream() once lift-off begins. */
	ringWorldLastStreamedColumn = GAME_MAP_WIDTH - 1;
	ringStreamColumn = -1;
	ringStreamRow = 0;
	ringStreamTouchedFirstColumn = -1;
	ringStreamTouchedLastColumn = -1;
	return 1;
}

static void updateGameScrollCopper(const UBYTE* worldBuffer, const GameState* game) {
	UWORD displayScrollX = displayScrollXForGameState(game);
	setCopperPlanePointers(worldBuffer, GAME_WORLD_ROW_BYTES, displayByteOffsetForGameState(game));
	setCopperFineScroll(horizontalScrollDelayToBplcon1(scrollDelayForBplcon1(displayScrollX)));
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
	game->impact.type = IMPACT_TYPE_EXPLOSION;
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
	game->impact.type = IMPACT_TYPE_EXPLOSION;
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

/* Amiga presentation upgrade: a sea hit uses CPC Smoke 1 followed by
 * Smoke 2 as white spray. Keep it world-anchored so scrolling cannot make
 * it follow the aircraft. SEA_SURFACE_Y is not tile-aligned, so the tile is
 * placed immediately above the surface rather than partly inside the sea. */
static void startWaterSplash(GameState* game, WORD x) {
	startImpactQuiet(game, x, (WORD)(SEA_SURFACE_Y - GAME_TILE_HEIGHT));
	game->impact.timer = WATER_SPLASH_FRAMES;
	game->impact.type = IMPACT_TYPE_WATER_SPLASH;
	game->impact.worldAnchored = 1;
	game->impact.worldX = (LONG)game->scrollX + x;
	playSfxAt(SFX_WATER_SPLASH, x);
}

static UBYTE objectUsesGroundTargetHitSfx(UBYTE objectId) {
	return objectId == HAR_OBJ_GROUND_TARGET ||
		objectId == HAR_OBJ_ENEMY_SHIP ||
		objectId == HAR_OBJ_LAND ||
		objectId == HAR_OBJ_TOWN_BLOCK;
}

/* The projectile's own y at the moment of collision is deliberately NOT
 * used for the explosion's position: hit detection probes a point offset
 * from the projectile's sprite (e.g. its bottom edge for a falling bomb,
 * its centre for a rocket - see the probe math at each call site), so the
 * raw sprite y can sit in the tile row ABOVE the one that was actually
 * detected as hit. Since updateBombImpactBob() places the explosion at
 * `impact.y / GAME_TILE_HEIGHT`, re-deriving y from the already-known
 * correct tileY instead guarantees the explosion lands on the struck tile
 * itself, never one row short. */
static void startGroundTargetHitImpact(GameState* game, WORD x,
	LONG worldColumn, WORD tileY, UBYTE objectId) {
	startWorldImpactQuiet(game, x, (WORD)(tileY * GAME_TILE_HEIGHT));
	/* A miss on bare land gets its own dedicated sound instead of reusing
	 * one of the "actually hit a target/building/ship" variants. */
	if (objectId == HAR_OBJ_LAND)
		playSfxAt(SFX_GROUND_MISS, x);
	else
		playGroundTargetHitSfx((UWORD)(worldColumn ^
			((LONG)tileY << 8) ^ frameCounter), x);
}

/* Match collision to the one opaque pixel on the bomb's lowest row. CPC
 * launch art ends at x+3 during the short diagonal phase; the descending
 * silhouette is a vertical line at x+1. Earlier probes out to x+5 could hit
 * the next tile before the visible bomb reached it, while an x+3 land probe
 * could mask a real target contact at x+1 on the previous tile boundary. */
static WORD bombShotContactX(const WeaponState* bomb) {
	return (WORD)(bomb->x +
		(bomb->timer < BOMB_FORWARD_MOMENTUM_FRAMES ? 3 : 1));
}

static WORD bombShotContactY(const WeaponState* bomb) {
	return (WORD)(bomb->y + BOMB_SHOT_PIXEL_BOB_HEIGHT - 1);
}

static void clearTargetLockWithTelemetry(GameState* game, UBYTE reason) {
	if (!game->targetLock.active)
		return;
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_MAVERICK_LOCK_LOST,
		reason ? reason : game->targetLock.targetType,
		(UWORD)(game->targetLock.worldX / GAME_TILE_WIDTH), game, 0);
	game->targetLock.active = 0;
}

/* A Player 2-dropped bomb has no target lock (unlike the CPU bombing run),
 * so it uses the player's generic world collision. Probe the complete narrow
 * footprint and use the same near-cell helpers as P1: procedural town art is
 * wider than a single object-map cell and a centre-only probe let P2 bombs
 * pass through visible building edges. Resolve solid objects before the sea
 * cutoff so a low building/ship contact cannot be turned into a splash. */
/* CPC routes Player 1, CPU Wingman and Player 2 through dolaunchbomb. Keep a
 * separate tiny interpolator state for Wingman's shared bomb instance, but
 * preserve the same four logical momentum rows: screen X stays fixed while
 * the world scrolls, then world X stays fixed during the remaining descent. */
static void resetWingmanBombMotion(WingmanState* wingman) {
	wingman->bombHalfPixelPhase = 0;
	wingman->bombStepPixels = 0;
	wingman->bombMomentumSteps = 0;
	wingman->bomb.timer = 0;
	wingman->bomb.worldAnchored = 0;
}

static void advanceWingmanBombMotion(GameState* game, UBYTE scrollPixels) {
	WingmanState* wingman = &game->wingman;
	WeaponState* bomb = &wingman->bomb;
	UBYTE newLaunchFrame = wingman->bombMomentumSteps == 0 &&
		wingman->bombStepPixels == 0 && wingman->bombHalfPixelPhase == 0 &&
		bomb->timer == 0;
	UBYTE momentumPhase =
		wingman->bombMomentumSteps < BOMB_MOMENTUM_LOGICAL_STEPS;
	UBYTE pixels = (UBYTE)(BOMB_HALF_PIXELS_PER_FRAME >> 1);
	wingman->bombHalfPixelPhase = (UBYTE)(wingman->bombHalfPixelPhase +
		(BOMB_HALF_PIXELS_PER_FRAME & 1));
	if (wingman->bombHalfPixelPhase >= 2) {
		wingman->bombHalfPixelPhase -= 2;
		pixels++;
	}

	bomb->y = (WORD)(bomb->y + pixels);
	if (momentumPhase && !newLaunchFrame)
		bomb->worldX += scrollPixels;
	bomb->x = (WORD)(bomb->worldX - game->scrollX);

	wingman->bombStepPixels = (UBYTE)(wingman->bombStepPixels + pixels);
	if (wingman->bombStepPixels >= BOMB_LOGICAL_STEP_PIXELS) {
		wingman->bombStepPixels = (UBYTE)(wingman->bombStepPixels -
			BOMB_LOGICAL_STEP_PIXELS);
		if (momentumPhase)
			wingman->bombMomentumSteps++;
	}
	if (wingman->bombMomentumSteps < BOMB_MOMENTUM_LOGICAL_STEPS)
		bomb->timer = wingman->bombMomentumSteps;
	else if (bomb->timer < 255)
		bomb->timer++;
	bomb->worldAnchored = 1;
}

static void updateWingmanPlayer2Bomb(GameState* game, UBYTE scrollPixels,
	UBYTE** worldBuffers, UBYTE* hudDirty) {
	WingmanState* wingman = &game->wingman;
	if (!wingman->bomb.active || game->wingmanControl != WINGMAN_CONTROL_PLAYER2)
		return;

	advanceWingmanBombMotion(game, scrollPixels);

	if (wingman->bomb.x < -16) {
		wingman->bomb.active = 0;
		return;
	}

	ObjectCell cell;
	LONG worldColumn = -1;
	WORD tileY = -1;
	WORD probeX = bombShotContactX(&wingman->bomb);
	WORD probeY = bombShotContactY(&wingman->bomb);
	UBYTE bombHitObject = objectCellForWorldPoint(game,
		probeX, probeY, &cell, &worldColumn, &tileY) &&
		(cell.id == HAR_OBJ_LAND || cell.id == HAR_OBJ_GROUND_TARGET ||
		 cell.id == HAR_OBJ_ENEMY_SHIP || cell.id == HAR_OBJ_FLAK ||
		 cell.id == HAR_OBJ_SMOKE || cell.id == HAR_OBJ_OWN_FRIGATE ||
		 cell.id == HAR_OBJ_PIER || cell.id == HAR_OBJ_TOWN_BLOCK);
	if (!bombHitObject)
		bombHitObject = ownFrigateCellNearWorldPoint(game,
			probeX, probeY,
			&cell, &worldColumn, &tileY);
	if (!bombHitObject)
		bombHitObject = townBlockCellNearWorldPoint(game,
			probeX, probeY,
			&cell, &worldColumn, &tileY);
	if (!bombHitObject) {
		if (wingman->bomb.y >= SEA_SURFACE_Y) {
			startWaterSplash(game, wingman->bomb.x);
			wingman->bomb.active = 0;
		}
		return;
	}
	retireBombPixelBobBeforeWorldMutation(worldBuffers,
		wingmanBombFootprints);

	if (cell.id == HAR_OBJ_GROUND_TARGET) {
		game->bonusScore += GROUND_TARGET_SCORE_VALUE;
		game->hitsCount++;
		updateHudValues(game);
		markTargetDestroyedAtColumn(worldColumn);
		if (game->targetLock.active &&
			game->targetLock.worldX / GAME_TILE_WIDTH == worldColumn)
			clearTargetLockWithTelemetry(game, cell.id);
		addCpcHitSmokeAtColumnRow(worldColumn, tileY);
		dirtyRedrawWorldColumn(worldBuffers, worldColumn);
		*hudDirty = 1;
	} else if (cell.id == HAR_OBJ_ENEMY_SHIP) {
		UBYTE shipChanged = damageEnemyShipAtColumnRow(worldColumn, tileY);
		game->bonusScore += ENEMY_SHIP_SCORE_VALUE;
		game->hitsCount++;
		updateHudValues(game);
		dirtyRedrawWorldColumn(worldBuffers, worldColumn);
		if (shipChanged)
			dirtyRedrawWorldColumn(worldBuffers, worldColumn - 1);
		*hudDirty = 1;
	} else if (cell.id == HAR_OBJ_OWN_FRIGATE) {
		game->playerFrigateStatus = PLAYER_FRIGATE_STATUS_HIT;
		addCpcHitSmokeAtColumnRow(worldColumn, tileY);
		dirtyRedrawWorldColumn(worldBuffers, worldColumn);
		dirtyRedrawWorldColumn(worldBuffers, worldColumn - 1);
	} else if (cell.id == HAR_OBJ_LAND) {
		if (markLandCraterAtColumnRow(worldColumn, tileY))
			dirtyRedrawWorldColumn(worldBuffers, worldColumn);
	} else if (cell.id == HAR_OBJ_TOWN_BLOCK) {
		game->bonusScore += TOWN_BLOCK_SCORE_VALUE;
		game->hitsCount++;
		updateHudValues(game);
		addCpcTownHitSmokeAtColumnRow(worldColumn, tileY);
		dirtyRedrawWorldColumn(worldBuffers, worldColumn);
		*hudDirty = 1;
	}

	if (cell.id == HAR_OBJ_FLAK || cell.id == HAR_OBJ_SMOKE ||
		cell.id == HAR_OBJ_PIER) {
		/* Absorbed with no visible/audible effect, matching the player's
		 * own bomb against the same object types. CPC's pier follows the
		 * bombhitsealand path and does not damage the friendly carrier. */
	} else if (objectUsesGroundTargetHitSfx(cell.id)) {
		startGroundTargetHitImpact(game, probeX, worldColumn, tileY, cell.id);
	} else {
		startWorldImpact(game, probeX, (WORD)(tileY * GAME_TILE_HEIGHT));
	}
	wingman->bomb.active = 0;
}

static void destroyWingman(GameState* game) {
	WingmanState* wingman = &game->wingman;
	if (!wingman->active)
		return;
	WORD impactX = wingmanScreenX(game);
	WORD impactY = wingman->screenY;
	setWingmanDestroyedState(game);
	startImpact(game, impactX, impactY);
}

/* CPC drawwingmanplane resolves the two object-map cells occupied by the
 * 16-pixel aircraft immediately before drawing it. IDs 0/1/10/12/20/21 pass;
 * enemy aircraft destroys both (handled by the pixel-accurate rectangle test
 * below), and every other world cell destroys the Wingman. Keep the same two
 * horizontal cell probes here rather than copying Player 1's wider six-cell
 * collision box: this is gameplay parity, not a new Enhanced safety net. */
static UBYTE wingmanObjectMapCollision(const GameState* game) {
	const WingmanState* wingman = &game->wingman;
	if (!wingman->active || wingman->destroyed)
		return 0;

	WORD screenX = wingmanScreenX(game);
	LONG firstWorldColumn = ((LONG)game->scrollX + screenX) >> 3;
	WORD tileY = wingman->screenY >> 3;
	for (UBYTE offset = 0; offset < 2; offset++) {
		ObjectCell cell;
		LONG worldColumn = firstWorldColumn + offset;
#if HAR_DEBUG_PERF_LOG
		perfWingmanWorldProbes++;
#endif
		if (!townBlockCellAtWorldColumnRow(worldColumn, tileY, &cell) &&
			!objectCellForWorldColumnTile(worldColumn, tileY, &cell))
			continue;
		switch (cell.id) {
			case HAR_OBJ_CLOUD:
			case HAR_OBJ_SKY:
			case HAR_OBJ_FLAK:
			case HAR_OBJ_SMOKE: /* CPC smoke shares FLAK's passable ID 10. */
			case HAR_OBJ_PLAYER_PLANE:
			case HAR_OBJ_WINGMAN:
			case HAR_OBJ_POWERUP:
				break;
			default:
#if HAR_DEBUG_PERF_LOG
				perfWingmanWorldHits++;
#endif
				return 1;
		}
	}
	return 0;
}

static UBYTE targetLockIsAvailable(const GameState* game) {
	/* CPC checkfireplayermissile tests only enemylandlocationlock != 0.
	 * scrollenemylandlocationlock already owns the screen-edge expiry at
	 * character column 5; adding a second player-relative "ahead" test made
	 * a still-valid CPC lock unusable when the Harrier had overtaken it. */
	return game->targetLock.active;
}

static UWORD standardRocketRangePixels(const GameState* game) {
	return (UWORD)(game->rocketRangeTiles * GAME_TILE_WIDTH);
}

static UBYTE launchRocket(GameState* game, UBYTE requestMaverick) {
	/* CPC checkfireplayermissile/checklaunchbomb do not suppress weapons over
	 * the start carrier. They stop them only from gamelevelprogress 11, when
	 * the intact final carrier has started its landing approach. The former
	 * playerOnOwnFrigateDeck() test covered the complete start-carrier deck and
	 * made the weapon input appear dead until that carrier had scrolled away. */
	UBYTE finalLandingWeaponsLocked =
		game->playerFrigateStatus == PLAYER_FRIGATE_STATUS_CLEAR &&
		game->landingState != LANDING_STATE_NONE;
	if (game->rocketShot.active || game->rockets == 0 ||
		finalLandingWeaponsLocked)
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
	if (requestMaverick && targetLockIsAvailable(game)) {
		game->rocketShot.type = ROCKET_SHOT_MAVERICK_LAUNCH;
		game->rocketShot.targetWorldX = game->targetLock.worldX + GAME_TILE_WIDTH / 2;
		game->rocketShot.targetY = (WORD)(game->targetLock.y + GAME_TILE_HEIGHT / 2);
	} else {
		/* CPC falls back to an ordinary rocket when Left+Fire has no lock. */
		game->rocketShot.type = ROCKET_SHOT_STANDARD;
		game->rocketShot.targetWorldX = 0;
	game->rocketShot.targetY = 0;
	}
#if HAR_DEBUG_PERF_LOG
	perfP1RocketLaunches++;
#endif
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_ROCKET_FIRE,
		game->rocketShot.type, (UWORD)(game->rocketShot.worldX >> 3), game,
		game->rockets);
	playSfxAt(SFX_FIRE, game->rocketShot.x);
	return 1;
}

static UBYTE directionToMaverickTarget(const WeaponState* rocket) {
	LONG dx = rocket->targetWorldX - (rocket->worldX + 8);
	WORD dy = (WORD)(rocket->targetY - (rocket->y + 4));
	/* CPC getdirectionfromcoords compares both coordinates exactly. Its nine
	 * entries select a direction even for a one-character difference and
	 * return NONE only at the exact target. Do not hide a 1..4 pixel error in
	 * a capture band; the pixel-smooth Amiga mover clamps its final step below
	 * instead, avoiding overshoot without changing the steering decision. */
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
	WORD stepX = MAVERICK_GUIDED_SPEED_PIXELS;
	WORD stepY = MAVERICK_GUIDED_SPEED_PIXELS;
	if (lockStillActive) {
		UBYTE newDirection = directionToMaverickTarget(rocket);
		LONG dx = rocket->targetWorldX - (rocket->worldX + 8);
		WORD dy = (WORD)(rocket->targetY - (rocket->y + 4));
		/* Preserve CPC's last direction when getdirectionfromcoords returns 0.
		 * For a non-zero residual, shorten only the final pixel step so the
		 * four-pixel presentation cannot oscillate around an exact CPC axis. */
		if (newDirection != MAVERICK_DIRECTION_NONE)
			rocket->direction = newDirection;
		if (dx != 0 && dx > -MAVERICK_GUIDED_SPEED_PIXELS &&
			dx < MAVERICK_GUIDED_SPEED_PIXELS)
			stepX = (WORD)(dx < 0 ? -dx : dx);
		if (dy != 0 && dy > -MAVERICK_GUIDED_SPEED_PIXELS &&
			dy < MAVERICK_GUIDED_SPEED_PIXELS)
			stepY = (WORD)(dy < 0 ? -dy : dy);
	}

	switch (rocket->direction) {
		case MAVERICK_DIRECTION_UP:
			rocket->y -= stepY;
			break;
		case MAVERICK_DIRECTION_UP_RIGHT:
			rocket->worldX += stepX;
			rocket->y -= stepY;
			break;
		case MAVERICK_DIRECTION_RIGHT:
			rocket->worldX += stepX;
			break;
		case MAVERICK_DIRECTION_DOWN_RIGHT:
			rocket->worldX += stepX;
			rocket->y += stepY;
			break;
		case MAVERICK_DIRECTION_DOWN:
			rocket->y += stepY;
			break;
		case MAVERICK_DIRECTION_DOWN_LEFT:
			rocket->worldX -= stepX;
			rocket->y += stepY;
			break;
		case MAVERICK_DIRECTION_LEFT:
			rocket->worldX -= stepX;
			break;
		case MAVERICK_DIRECTION_UP_LEFT:
			rocket->worldX -= stepX;
			rocket->y -= stepY;
			break;
	}
}

static UBYTE launchBomb(GameState* game) {
	/* Match CPC checklaunchbomb: bombing is legal from the first airborne
	 * frame, including while the start carrier is still beneath the Harrier.
	 * Only the intact final-carrier landing approach locks the weapon. */
	UBYTE finalLandingWeaponsLocked =
		game->playerFrigateStatus == PLAYER_FRIGATE_STATUS_CLEAR &&
		game->landingState != LANDING_STATE_NONE;
	if (game->bombLaunchCooldown > 0 || game->bombShot.active ||
		game->bombs == 0 || finalLandingWeaponsLocked)
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
	game->bombLogicalWorldX = game->bombShot.worldX;
	game->bombLogicalY = game->bombShot.y;
	game->bombHalfPixelPhase = 0;
	game->bombStepPixels = 0;
	game->bombMomentumSteps = 0;
#if HAR_DEBUG_PERF_LOG
	perfP1BombLaunches++;
#endif
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_BOMB_RELEASE, 0,
		(UWORD)(game->bombShot.worldX >> 3), game, game->bombs);
	playSfxAt(SFX_BOMB, game->bombShot.x);
	return 1;
}

/* Advance only the player bomb's CPC-derived motion. The caller has already
 * advanced scrollX for this frame. A newly launched bomb was positioned after
 * that scroll, so compensation begins on its second update. During the four
 * momentum rows, following scroll keeps screen X steady and increases world
 * X; after that, fixed world X naturally moves left on screen with scenery.
 * Returns true only when a complete CPC 8-pixel logical row was crossed. */
static UBYTE advancePlayerBombMotion(GameState* game, UBYTE scrollPixels) {
	WeaponState* bomb = &game->bombShot;
	UBYTE newLaunchFrame = game->bombMomentumSteps == 0 &&
		game->bombStepPixels == 0 && game->bombHalfPixelPhase == 0 &&
		bomb->timer == 0;
	UBYTE momentumPhase =
		game->bombMomentumSteps < BOMB_MOMENTUM_LOGICAL_STEPS;
	UBYTE pixels = (UBYTE)(BOMB_HALF_PIXELS_PER_FRAME >> 1);
	game->bombHalfPixelPhase = (UBYTE)(game->bombHalfPixelPhase +
		(BOMB_HALF_PIXELS_PER_FRAME & 1));
	if (game->bombHalfPixelPhase >= 2) {
		game->bombHalfPixelPhase -= 2;
		pixels++;
	}

	/* CPC increases H in both movebombmomentum and decreasebombheight. */
	bomb->y = (WORD)(bomb->y + pixels);
	if (momentumPhase && !newLaunchFrame)
		bomb->worldX += scrollPixels;
	bomb->x = (WORD)(bomb->worldX - game->scrollX);

	game->bombStepPixels = (UBYTE)(game->bombStepPixels + pixels);
	if (game->bombStepPixels < BOMB_LOGICAL_STEP_PIXELS)
		return 0;

	game->bombStepPixels = (UBYTE)(game->bombStepPixels -
		BOMB_LOGICAL_STEP_PIXELS);
	game->bombLogicalY += GAME_TILE_HEIGHT;
	if (momentumPhase) {
		game->bombMomentumSteps++;
		game->bombLogicalWorldX = bomb->worldX;
	}
	return 1;
}

static UBYTE updateWeapons(GameState* game, UBYTE scrollPixels, UBYTE** worldBuffers) {
	UBYTE changed = 0;

	if (game->rocketShot.active) {
		if (game->rocketShot.type == ROCKET_SHOT_STANDARD) {
			game->rocketShot.worldX += game->rocketShot.dx;
			game->rocketShot.guidanceDistance +=
				(UWORD)game->rocketShot.dx;
			/* Real CPC (lockinmissileheighttoplayer, :6994-7003): with the
			 * menu option enabled, every in-flight standard rocket is moved
			 * to the player's CURRENT height. With it disabled, the launch Y
			 * remains unchanged. Mavericks use their separate guidance path. */
			if (game->rocketHeightLock)
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
			(rocketCell.id == HAR_OBJ_LAND || rocketCell.id == HAR_OBJ_GROUND_TARGET || rocketCell.id == HAR_OBJ_ENEMY_SHIP || rocketCell.id == HAR_OBJ_FLAK || rocketCell.id == HAR_OBJ_SMOKE || rocketCell.id == HAR_OBJ_OWN_FRIGATE || rocketCell.id == HAR_OBJ_PIER);
		if (!rocketHitObject)
			rocketHitObject = enemyShipCellNearWorldPoint(game, rocketProbeX, rocketProbeY, -1, 1, -1, 1, &rocketCell, &rocketWorldColumn, &rocketTileY);
		if (!rocketHitObject)
			rocketHitObject = ownFrigateCellNearWorldPoint(game, rocketProbeX, rocketProbeY, &rocketCell, &rocketWorldColumn, &rocketTileY);
		if (!rocketHitObject)
			rocketHitObject = townBlockCellNearWorldPoint(game, rocketProbeX, rocketProbeY, &rocketCell, &rocketWorldColumn, &rocketTileY);
		if (!rocketHitObject &&
			game->rocketShot.type == ROCKET_SHOT_MAVERICK_GUIDED) {
			/* CPC collision works in character cells. The Amiga guidance now
			 * clamps the final pixel step to an exact axis, while this one-step
			 * proximity fuse maps entry into the locked CPC cell to the target
			 * object before the BOB can visually pass through it. */
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
			retireRocketPixelBobBeforeWorldMutation(worldBuffers,
				rocketShotFootprints);
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
				 * addCpcTownHitSmokeAtColumnRow() two-tile pattern. Menu review:
				 * single-tile redraw
				 * instead of the whole column. */
				game->bonusScore += TOWN_BLOCK_SCORE_VALUE;
				game->hitsCount++;
				updateHudValues(game);
				addCpcTownHitSmokeAtColumnRow(rocketWorldColumn, rocketTileY);
				dirtyRedrawWorldTileIfSmoke(worldBuffers, rocketWorldColumn, rocketTileY);
				dirtyRedrawWorldTileIfSmoke(worldBuffers, rocketWorldColumn, rocketTileY - 1);
			}
			if (rocketCell.id == HAR_OBJ_FLAK || rocketCell.id == HAR_OBJ_SMOKE ||
				rocketCell.id == HAR_OBJ_PIER) {
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
						rocketWorldColumn, rocketTileY, rocketCell.id);
				else
					startWorldImpact(game, game->rocketShot.x,
						(WORD)(rocketTileY * GAME_TILE_HEIGHT));
				game->rocketShot.active = 0;
			}
			clearTargetLockWithTelemetry(game, rocketCell.id);
		} else if (game->rocketShot.type == ROCKET_SHOT_STANDARD &&
			game->rocketShot.guidanceDistance >=
				standardRocketRangePixels(game)) {
			/* CPC checkplayermissilemove retires the ordinary rocket when its
			 * character-range counter reaches the menu value. No impact occurs. */
			game->rocketShot.active = 0;
		} else if (game->rocketShot.x >= SCREEN_WIDTH - 18) {
			startImpact(game, (WORD)(SCREEN_WIDTH - 28), game->rocketShot.y);
			game->rocketShot.active = 0;
		} else if (game->rocketShot.x < -16) {
			game->rocketShot.active = 0;
		}
		changed = 1;
	}

	if (game->bombShot.active) {
		/* CPC changes logical rows; the mini-BOB interpolates between them. */
		advancePlayerBombMotion(game, scrollPixels);
		game->bombShot.worldAnchored = 1;
		if (game->bombMomentumSteps < BOMB_MOMENTUM_LOGICAL_STEPS) {
			/* The mini-BOB uses timer<4 to select CPC's horizontal bomb art.
			 * Keep that presentation phase tied to logical momentum steps, not
			 * PAL frames. */
			game->bombShot.timer = game->bombMomentumSteps;
		} else if (game->bombShot.timer < 255) {
			game->bombShot.timer++;
		}
		ObjectCell bombCell;
		LONG bombWorldColumn = -1;
		WORD bombTileY = -1;
		/* Physics remains on CPC's eight-pixel logical rows, but collision must
		 * follow the interpolated mini-BOB every frame. The old logical-row-only
		 * probe could visibly pass through a target for up to seven pixels,
		 * especially during the diagonal momentum phase. */
		WORD bombProbeX = bombShotContactX(&game->bombShot);
		WORD bombProbeY = bombShotContactY(&game->bombShot);
		UBYTE bombHitObject = objectCellForWorldPoint(game, bombProbeX,
			bombProbeY, &bombCell, &bombWorldColumn, &bombTileY) &&
			(bombCell.id == HAR_OBJ_LAND || bombCell.id == HAR_OBJ_GROUND_TARGET || bombCell.id == HAR_OBJ_ENEMY_SHIP || bombCell.id == HAR_OBJ_FLAK || bombCell.id == HAR_OBJ_SMOKE || bombCell.id == HAR_OBJ_OWN_FRIGATE || bombCell.id == HAR_OBJ_PIER);
		if (!bombHitObject)
			bombHitObject = ownFrigateCellNearWorldPoint(game, bombProbeX,
				bombProbeY, &bombCell, &bombWorldColumn, &bombTileY);
		if (!bombHitObject)
			bombHitObject = townBlockCellNearWorldPoint(game, bombProbeX,
				bombProbeY, &bombCell, &bombWorldColumn, &bombTileY);
		if (bombHitObject) {
			retireBombPixelBobBeforeWorldMutation(worldBuffers,
				bombShotFootprints);
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
					clearTargetLockWithTelemetry(game, bombCell.id);
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
				addCpcTownHitSmokeAtColumnRow(bombWorldColumn, bombTileY);
				dirtyRedrawWorldColumn(worldBuffers, bombWorldColumn);
			}
			if (bombCell.id == HAR_OBJ_FLAK || bombCell.id == HAR_OBJ_SMOKE ||
				bombCell.id == HAR_OBJ_PIER) {
				/* Sprint 14.95 Part 2: see the matching rocket branch above -
				 * flak/smoke absorb the weapon with no visible/audible
				 * effect. */
				game->bombShot.active = 0;
			} else {
				if (objectUsesGroundTargetHitSfx(bombCell.id))
					startGroundTargetHitImpact(game, bombProbeX,
						bombWorldColumn, bombTileY, bombCell.id);
				else if (game->bombShot.timer <= BOMB_IMPACT_SFX_GRACE_FRAMES)
					startWorldImpactQuiet(game, bombProbeX,
						(WORD)(bombTileY * GAME_TILE_HEIGHT));
				else
					startWorldImpact(game, bombProbeX,
						(WORD)(bombTileY * GAME_TILE_HEIGHT));
				game->bombShot.active = 0;
			}
		} else if (game->bombShot.y >= SEA_SURFACE_Y) {
			startWaterSplash(game, game->bombShot.x);
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
static void updateCrashPartSprites(UWORD* crashPart1Sprite,
	UWORD* enemySprite, UWORD* enemyAttachSprite, const GameState* game) {
	if (game->crashTimer) {
		/* During eject, sprite 0 remains the seat/parachute. The abandoned
		 * Harrier already owned enemy pair 2/3, so split that pair and put
		 * fragment zero on channel 2 while channels 5 and 3 carry the rest. */
		if (game->abandonedAircraftCrash) {
			if (game->crashPart[0].active)
				buildPlayerCrashPartSprite(enemySprite,
					game->crashPart[0].x, game->crashPart[0].y, 0);
			else
				hideHardwareSprite(enemySprite);
		}
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

	/* Player rocket is a playfield Bob; channel 5 is reserved for crash debris. */
	hideHardwareSprite(crashPart1Sprite);
}

static UBYTE rectsOverlap(WORD ax, WORD ay, WORD aw, WORD ah, WORD bx, WORD by, WORD bw, WORD bh) {
	return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static UBYTE runtimeFlakTileAtColumnRow(LONG worldColumn, WORD tileY) {
	if (worldColumn < 0 || tileY < 0)
		return 0;
	UBYTE slot = (UBYTE)((UWORD)worldColumn &
		(GAME_RUNTIME_FLAK_LOOKUP_SIZE - 1));
	if (runtimeFlakLookupColumns[slot] == (UWORD)worldColumn &&
		runtimeFlakLookupRows[slot] == (UBYTE)tileY)
		return runtimeFlakLookupTiles[slot];
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
			UBYTE slot = (UBYTE)(runtimeFlakColumns[index] &
				(GAME_RUNTIME_FLAK_LOOKUP_SIZE - 1));
			if (runtimeFlakLookupColumns[slot] == runtimeFlakColumns[index])
				runtimeFlakLookupColumns[slot] = 0xffff;
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
	UBYTE slot = (UBYTE)((UWORD)worldColumn &
		(GAME_RUNTIME_FLAK_LOOKUP_SIZE - 1));
	if (runtimeFlakLookupColumns[slot] != (UWORD)worldColumn ||
		runtimeFlakLookupRows[slot] != (UBYTE)tileY)
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
		runtimeFlakLookupColumns[slot] = 0xffff;
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
	UBYTE slot = (UBYTE)((UWORD)worldColumn &
		(GAME_RUNTIME_FLAK_LOOKUP_SIZE - 1));
	runtimeFlakLookupColumns[slot] = (UWORD)worldColumn;
	runtimeFlakLookupRows[slot] = (UBYTE)tileY;
	runtimeFlakLookupTiles[slot] = tile;
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
	if (localColumn < 0 || localColumn >= cpcLandProceduralLength)
		return 0xFF;
	UBYTE target = cpcLandProceduralTarget((UWORD)localColumn);
	if (target == CPC_LAND_TARGET_NONE || target == CPC_LAND_TARGET_TANK_REAR)
		return 0xFF;
	return (UBYTE)(target - CPC_LAND_TARGET_RADAR);
}

static void resetRuntimeFlak(void) {
	runtimeFlakCount = 0;
	for (UWORD slot = 0; slot < GAME_RUNTIME_FLAK_LOOKUP_SIZE; slot++)
		runtimeFlakLookupColumns[slot] = 0xffff;
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

	/* CPC uses R bit 0 for sprite 57/58 (asm:6082-6087). On Amiga that choice
	 * is cosmetic only, so Sprint 15.45 deliberately moves it to the isolated
	 * cosmetic seed. The modeled R value remains available below for the SFX
	 * variation key, which also cannot affect spawning or collision. */
	UBYTE rState = cpcRStateForWorldColumn((LONG)checkColumn);
	/* Sprite 57/58 is presentation only. Keep it off the modeled Z80 R
	 * stream so art variation can never affect flak placement or timing. */
	UBYTE tile = (UBYTE)(57 + (cosmeticVariantForColumn(checkColumn,
		(UWORD)(0x5a31U + (UWORD)flakRow)) & 1));
	if (addRuntimeFlak((LONG)checkColumn, flakRow, tile)) {
#if HAR_DEBUG_PERF_LOG
		if (perfRuntimeFlakSpawns < 0xffff)
			perfRuntimeFlakSpawns++;
#endif
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
		game->targetLock.worldX - game->scrollX <= 5 * GAME_TILE_WIDTH) {
		clearTargetLockWithTelemetry(game, 0);
	}

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
	if (localColumn < 0 || localColumn >= cpcLandProceduralLength)
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
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_MAVERICK_LOCK, target,
		checkColumn, game, terrainY);
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

static UWORD missionPalettePhaseColor(UBYTE phase, UBYTE band) {
	if (phase == MISSION_PALETTE_DUSK) {
		if (band == MISSION_PALETTE_SKY_TOP) return GAME_SKY_TOP_DUSK_RGB;
		if (band == MISSION_PALETTE_SKY_MID) return GAME_SKY_MID_DUSK_RGB;
		if (band == MISSION_PALETTE_SKY_LOW) return GAME_SKY_LOW_DUSK_RGB;
		if (band == MISSION_PALETTE_LAND) return GAME_LAND_DUSK_RGB;
		return GAME_SKY_TOP_CLOUD_DUSK_RGB;
	}
	if (phase == MISSION_PALETTE_NIGHT) {
		if (band == MISSION_PALETTE_SKY_TOP) return GAME_SKY_TOP_NIGHT_RGB;
		if (band == MISSION_PALETTE_SKY_MID) return GAME_SKY_MID_NIGHT_RGB;
		if (band == MISSION_PALETTE_SKY_LOW) return GAME_SKY_LOW_NIGHT_RGB;
		if (band == MISSION_PALETTE_LAND) return GAME_LAND_NIGHT_RGB;
		return GAME_SKY_TOP_CLOUD_NIGHT_RGB;
	}
	if (phase == MISSION_PALETTE_DAWN) {
		if (band == MISSION_PALETTE_SKY_TOP) return GAME_SKY_TOP_DAWN_RGB;
		if (band == MISSION_PALETTE_SKY_MID) return GAME_SKY_MID_DAWN_RGB;
		if (band == MISSION_PALETTE_SKY_LOW) return GAME_SKY_LOW_DAWN_RGB;
		if (band == MISSION_PALETTE_LAND) return GAME_LAND_DAWN_RGB;
		return GAME_SKY_TOP_CLOUD_DAWN_RGB;
	}
	if (band == MISSION_PALETTE_SKY_TOP) return GAME_SKY_TOP_RGB;
	if (band == MISSION_PALETTE_SKY_MID) return GAME_SKY_MID_RGB;
	if (band == MISSION_PALETTE_SKY_LOW) return GAME_SKY_LOW_RGB;
	if (band == MISSION_PALETTE_LAND) return GAME_LAND_DAY_RGB;
	return GAME_SKY_TOP_CLOUD_RGB;
}

static void applyMissionPaletteFadeStep(UBYTE fromPhase, UBYTE toPhase,
	UBYTE step) {
	currentSkyTopRgb = lerpFadeRgb(
		missionPalettePhaseColor(fromPhase, MISSION_PALETTE_SKY_TOP),
		missionPalettePhaseColor(toPhase, MISSION_PALETTE_SKY_TOP), step);
	currentSkyMidRgb = lerpFadeRgb(
		missionPalettePhaseColor(fromPhase, MISSION_PALETTE_SKY_MID),
		missionPalettePhaseColor(toPhase, MISSION_PALETTE_SKY_MID), step);
	currentSkyLowRgb = lerpFadeRgb(
		missionPalettePhaseColor(fromPhase, MISSION_PALETTE_SKY_LOW),
		missionPalettePhaseColor(toPhase, MISSION_PALETTE_SKY_LOW), step);
	currentCloudTopRgb = lerpFadeRgb(
		missionPalettePhaseColor(fromPhase, MISSION_PALETTE_CLOUD),
		missionPalettePhaseColor(toPhase, MISSION_PALETTE_CLOUD), step);
	currentLandRgb = lerpFadeRgb(
		missionPalettePhaseColor(fromPhase, MISSION_PALETTE_LAND),
		missionPalettePhaseColor(toPhase, MISSION_PALETTE_LAND), step);
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
	if (activeCopperLandColor)
		*activeCopperLandColor = currentLandRgb;
	if (activeCopperSeaLowColor)
		*activeCopperSeaLowColor = currentSeaLowRgb;
	if (activeCopperPanelSeaColor)
		*activeCopperPanelSeaColor = currentPanelSeaRgb;
}

static void applyCityFadeStep(UBYTE step) {
	applyMissionPaletteFadeStep(currentMissionFlightPalettePhase,
		currentMissionTownPalettePhase, step);
}

/* CPC advances five entries both at newlevelloop and when Port Stanley is
 * built. Mission 1 deliberately skips its takeoff fade. Thereafter the exact
 * repeating campaign is day->dusk, dusk->night, night->dawn, dawn->day. */
static void resetCityFade(GameState* game) {
	if (game->missionNumber == 1) {
		currentMissionOpeningPalettePhase = MISSION_PALETTE_DAY;
		currentMissionFlightPalettePhase = MISSION_PALETTE_DAY;
		currentMissionTownPalettePhase = MISSION_PALETTE_DUSK;
	} else if ((game->missionNumber & 1) == 0) {
		currentMissionOpeningPalettePhase = MISSION_PALETTE_DUSK;
		currentMissionFlightPalettePhase = MISSION_PALETTE_NIGHT;
		currentMissionTownPalettePhase = MISSION_PALETTE_DAWN;
	} else {
		currentMissionOpeningPalettePhase = MISSION_PALETTE_DAWN;
		currentMissionFlightPalettePhase = MISSION_PALETTE_DAY;
		currentMissionTownPalettePhase = MISSION_PALETTE_DUSK;
	}
	game->takeoffPaletteFadeStep =
		currentMissionOpeningPalettePhase == currentMissionFlightPalettePhase ?
			CITY_FADE_STEP_COUNT : 0;
	game->takeoffPaletteFadeTimer = CITY_FADE_STEP_FRAMES;
	game->cityFadeStep = 0;
	game->cityFadeTimer = CITY_FADE_STEP_FRAMES;
	currentSkyTopRgb = missionPalettePhaseColor(
		currentMissionOpeningPalettePhase, MISSION_PALETTE_SKY_TOP);
	currentSkyMidRgb = missionPalettePhaseColor(
		currentMissionOpeningPalettePhase, MISSION_PALETTE_SKY_MID);
	currentSkyLowRgb = missionPalettePhaseColor(
		currentMissionOpeningPalettePhase, MISSION_PALETTE_SKY_LOW);
	currentCloudTopRgb = missionPalettePhaseColor(
		currentMissionOpeningPalettePhase, MISSION_PALETTE_CLOUD);
	currentLandRgb = missionPalettePhaseColor(
		currentMissionOpeningPalettePhase, MISSION_PALETTE_LAND);
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
 * palette still active. The successful landing's next newlevelloop advances
 * the campaign palette again. Therefore the first non-zero fade step latches
 * the mission's town target for the rest of this GameState; resetCityFade()
 * selects the next mission's day/night base in startGameSession(). */
static void updateCityFade(GameState* game) {
	/* CPC starts the between-mission five-step fade only after checkliftoff.
	 * Keep the retained landing palette while the aircraft wait on deck, then
	 * advance it during the real lift-off instead of popping directly to the
	 * next phase when startGameSession() rebuilds the Copper list. */
	if (game->takeoffPaletteFadeStep < CITY_FADE_STEP_COUNT) {
		if (game->takeoffState < TAKEOFF_STATE_LIFTING)
			return;
		if (game->takeoffPaletteFadeTimer > 0) {
			game->takeoffPaletteFadeTimer--;
			return;
		}
		game->takeoffPaletteFadeStep++;
		game->takeoffPaletteFadeTimer = CITY_FADE_STEP_FRAMES;
		applyMissionPaletteFadeStep(currentMissionOpeningPalettePhase,
			currentMissionFlightPalettePhase,
			game->takeoffPaletteFadeStep);
		return;
	}
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
	UBYTE floor = (UBYTE)(POWERUP_ALTITUDE_FLOOR_BASE - game->levelDifficulty);
	return (UBYTE)(game->playerY >> 3) < floor;
}

static void spawnPowerup(GameState* game, UBYTE type, UBYTE startRow) {
	PowerupState* p = &game->powerup;
	/* Audit every Enhanced entry point, including the end-of-board aircraft
	 * bonus. The caller-side arbitration should keep this at zero. */
	if (game->gameMode == GAME_MODE_ENHANCED && game->enemyPlane.active &&
		telemetryEnhancedPowerupWhileEnemy < 0xffff)
		telemetryEnhancedPowerupWhileEnemy++;
	p->active = 1;
	p->type = type;
	p->worldX = (LONG)game->scrollX + (POWERUP_SPAWN_COLUMN << 3);
	p->y = (WORD)(startRow << 3);
	p->logicalY = p->y;
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
	/* Classic owns one ordered launchenemyplane decision in
	 * updateClassicAirAdmission().  Running this older column-driven path as
	 * well allowed a drop and an enemy to be admitted independently. */
	if (game->gameMode == GAME_MODE_CLASSIC)
		return;
	/* Enhanced keeps its own radar/column timing, but follows the same clean
	 * ownership rule: an admitted enemy aircraft blocks any new drop. Do not
	 * consume the column while blocked; after the aircraft leaves, the current
	 * column receives one normal opportunity. */
	if (game->enemyPlane.active)
		return;
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
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_WING_POWERUP, 0,
			checkColumn, game, rState);
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

/* Amiga campaign extension: a strong sortie earns one visible extra-aircraft
 * pickup on the final run toward the carrier.  It is deliberately outside
 * CPC's random 1..6 powerup rotation, deterministic, and one-shot per board.
 * missionStartScore makes the threshold depend on this board's work rather
 * than the cumulative campaign score. */
static void trySpawnExtraAircraftBonus(GameState* game) {
	/* White extra-aircraft drops are an Enhanced-only reward. Classic never
	 * creates the object, rather than spawning an inert pickup. */
	if (game->gameMode != GAME_MODE_ENHANCED ||
		game->extraAircraftBonusSpawned || game->powerup.active ||
		game->enemyPlane.active ||
		game->gameOver || game->crashTimer || game->ejectState ||
		game->missionComplete || game->landingState != LANDING_STATE_NONE ||
		game->lives >= PLAYER_MAX_AIRCRAFT)
		return;
	if (game->scrollX < POWERUP_EXTRA_AIRCRAFT_SCROLL_X ||
		game->scrollX >= LANDING_APPROACH_SCROLL_X)
		return;
	if (game->bonusScore - game->missionStartScore <
		POWERUP_EXTRA_AIRCRAFT_SCORE)
		return;

	game->extraAircraftBonusSpawned = 1;
	spawnPowerup(game, POWERUP_EXTRA_AIRCRAFT, 2);
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
			/* CPC checkactivaterockets writes literal &10 to
			 * numberofrockets. This is a 16-shot pickup, not the
			 * skill-scaled full inventory used at takeoff/landing. */
			game->rockets = POWERUP_ROCKET_REFILL;
			break;
		case POWERUP_BOMBS:
			/* CPC checkactivatebombs has the same literal &10 rule. */
			game->bombs = POWERUP_BOMB_REFILL;
			break;
		case POWERUP_WINGMAN:
			if (game->wingman.destroyed) {
				/* Revive at the pickup point. CPU reuses WINGMAN_TAKEOFF's
				 * existing smooth converge-into-formation logic (its own
				 * "climb clear of the carrier" phase is a no-op here since
				 * a mid-air pickup already starts above TAKEOFF_CLEAR_Y).
				 * Player 2 must stay under the human's control instead of
				 * silently becoming a CPU-flown Wingman - go straight to
				 * WINGMAN_PLAYER2_FLIGHT at the pickup point. */
				WingmanState* wingman = &game->wingman;
				WORD pickupScreenX = (WORD)(game->powerup.worldX - (LONG)game->scrollX);
				wingman->active = 1;
				wingman->destroyed = 0;
				wingman->mode = (game->wingmanControl == WINGMAN_CONTROL_PLAYER2) ?
					WINGMAN_PLAYER2_FLIGHT : WINGMAN_TAKEOFF;
				wingman->interceptScreenX = pickupScreenX;
				WORD pickupY = game->powerup.logicalY;
				wingman->screenY = pickupY;
				wingman->row = (WORD)(pickupY / GAME_TILE_HEIGHT);
				wingman->moveTimer = 0;
				wingman->returningToFormation = 0;
				wingman->rocket.active = 0;
				wingman->bomb.active = 0;
				telemetryLogGameEvent(TELEMETRY_GAME_EVENT_WINGMAN_REVIVED,
					game->wingmanControl,
					(UWORD)(game->powerup.worldX / GAME_TILE_WIDTH), game,
					(UWORD)pickupY);
			} else {
				/* CPC's own "or just give full health" fallback for the
				 * (normally unreachable, since trySpawnPowerup() only ever
				 * emits this type while a wingman is actually destroyed)
				 * case of collecting this type with nothing to revive. */
				game->flakDamageCount = 0;
				game->armour = 100;
			}
			break;
		case POWERUP_EXTRA_AIRCRAFT:
			if (game->gameMode != GAME_MODE_ENHANCED) {
				collected = 0;
				break;
			}
			if (!debugInfiniteLives && game->lives < PLAYER_MAX_AIRCRAFT)
				game->lives++;
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
	WORD collisionY = p->logicalY;
	WORD probeY = (WORD)(collisionY + POWERUP_SPRITE_HEIGHT);
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
	WORD collisionY = p->logicalY;
	if (rectsOverlap(screenX, collisionY, POWERUP_COLLISION_WIDTH,
		POWERUP_SPRITE_HEIGHT, game->playerX, game->playerY,
		PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT))
		return 1;
	/* In Enhanced two-player mode either aircraft may collect a pickup. Rockets and
	 * bombs refill the shared HUD inventory; health remains the shared team
	 * armour model already exposed by the game state. Classic retains CPC's
	 * player-only pickup ownership. */
	if (game->gameMode == GAME_MODE_ENHANCED &&
		game->wingmanControl == WINGMAN_CONTROL_PLAYER2 &&
		game->wingman.active && !game->wingman.destroyed)
		return rectsOverlap(screenX, collisionY, POWERUP_COLLISION_WIDTH,
			POWERUP_SPRITE_HEIGHT, wingmanScreenX(game),
			game->wingman.screenY, PLAYER_SPRITE_WIDTH,
			PLAYER_SPRITE_HEIGHT);
	return 0;
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

	/* CPC checks contact at its current character row on every call, then
	 * advances one complete row only when wingmanpowerupspeed reaches five.
	 * Both profiles share that logic and the smooth 1/2-pixel presentation. */
	if (powerupHitsPlayer(game, p)) {
		activatePowerup(game, p->type);
		destroyPowerup(game, 0);
		return 1;
	}

	if (powerupHitsSolidWorld(game, p)) {
		destroyPowerup(game, 1);
		return 1;
	}

	static const UBYTE displayOffset[POWERUP_LOGICAL_STEP_FRAMES + 1] = {
		0, 1, 3, 4, 6, 8
	};
	p->fallCounter++;
	p->y = (WORD)(p->logicalY + displayOffset[p->fallCounter]);
	if (p->fallCounter >= POWERUP_LOGICAL_STEP_FRAMES) {
		p->fallCounter = 0;
		p->logicalY = (WORD)(p->logicalY + GAME_TILE_HEIGHT);
		p->y = p->logicalY;
	}

	return 1;
}

/* Exact outcome table from CPC checkplayeragainstobjectmap (:7525-7544).
 * Cloud/sky and Wingman are passable; powerup contact is handled by the
 * pickup path; object ID 10 is accumulated flak damage; every other occupied
 * object enters planehitbyobject. Smoke has its own Amiga ID only so its
 * persistent artwork is not removed as a live flak cell, but shares the CPC
 * non-fatal damage outcome. */
static UBYTE cpcPlayerCollisionForObjectId(UBYTE objectId) {
	switch (objectId) {
		case HAR_OBJ_CLOUD:
		case HAR_OBJ_SKY:
		case HAR_OBJ_WINGMAN:
		case HAR_OBJ_POWERUP:
			return PLAYER_OBJECT_COLLISION_SAFE;
		case HAR_OBJ_FLAK:
			return PLAYER_OBJECT_COLLISION_FLAK;
		case HAR_OBJ_SMOKE:
			return PLAYER_OBJECT_COLLISION_SMOKE;
		default:
			return PLAYER_OBJECT_COLLISION_FATAL;
	}
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

	/* Must precede the deck-safe exception: a touchdown over the carrier
	 * tower or parked second Harrier is a collision, not a valid landing. */
	if (playerHitsNativeCarrierObstruction(game, hitWorldColumn, hitTileY))
		return PLAYER_OBJECT_COLLISION_FATAL;

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
	UBYTE smokeContact = 0;

	for (WORD tileY = topTileY; tileY <= bottomTileY; tileY++) {
		for (LONG worldColumn = leftWorldColumn;
			worldColumn <= rightWorldColumn; worldColumn++) {
			ObjectCell cell;
			UBYTE collisionClass;
			/* Procedural town facades are rendered outside the base object
			 * resolver. Include their exact opaque cells here as well, otherwise
			 * a normal or failing aircraft can pass through a building while
			 * bombs still hit the same visible tile. Destroyed facade cells are
			 * skipped by townBlockCellAtWorldColumnRow(); the fallback then sees
			 * their persistent smoke and preserves CPC's non-fatal smoke contact. */
			if (!townBlockCellAtWorldColumnRow(worldColumn, tileY, &cell) &&
				!objectCellForWorldColumnTile(worldColumn, tileY, &cell))
				continue;
			collisionClass = cpcPlayerCollisionForObjectId(cell.id);
			if (collisionClass == PLAYER_OBJECT_COLLISION_SAFE)
				continue;
			if (collisionClass == PLAYER_OBJECT_COLLISION_FLAK) {
				if (!flakContact) {
					*hitWorldColumn = worldColumn;
					*hitTileY = tileY;
					flakContact = 1;
				}
				continue;
			}
			/* CPC drawsmokesprite assigns the same object ID (10) as live
			 * flak, so crossing persistent hit smoke follows the non-fatal
			 * flak-damage path too. Keep the separate Amiga ID only so smoke
			 * remains persistent and is never removed as a live flak cell. */
			if (collisionClass == PLAYER_OBJECT_COLLISION_SMOKE) {
				if (!flakContact && !smokeContact) {
					*hitWorldColumn = worldColumn;
					*hitTileY = tileY;
					smokeContact = 1;
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
	if (smokeContact)
		return PLAYER_OBJECT_COLLISION_SMOKE;

	return PLAYER_OBJECT_COLLISION_SAFE;
}

static void maybeStartWingmanIntercept(GameState* game); /* Sprint 15.5, defined below */

/* CPC spawns at exactly currtime&3 (rows 0..3) and screen column $26.
 * Never relocate a blocked spawn to a lower lane: that was the source of
 * Amiga enemies appearing to descend from high terrain. A blocked authentic
 * lane is simply retried later. */
static UBYTE enemyPlaneRowIsPassable(LONG worldColumn, WORD tileRow) {
	ObjectCell cell;
	if (tileRow < 0 || tileRow >= GAME_SEA_TOP_TILE_Y)
		return 0;
	for (UBYTE offset = 0; offset < 2; offset++) {
		LONG probeColumn = worldColumn + offset;
		if (probeColumn >= 0 && probeColumn < currentGameLevelWidthTiles &&
			(enemyPlanePassableColumnValid[(UWORD)probeColumn >> 3] &
			 (UBYTE)(1U << ((UWORD)probeColumn & 7)))) {
			if (!(enemyPlanePassableMaskByColumn[probeColumn] &
				(UWORD)(1U << tileRow)))
				return 0;
			continue;
		}
		/* Startup and diagnostic callers can probe a column before it has
		 * entered the ring cache. Preserve the exact old resolver as a safe
		 * fallback; ordinary gameplay should almost never take this branch. */
		if (townBlockCellAtWorldColumnRow(probeColumn, tileRow, &cell))
			return 0;
		if (!objectCellForWorldColumnTile(probeColumn, tileRow, &cell))
			return 0;
		/* Unlike Wingman radar, CPC enemy-plane obstruction does not permit
		 * flak: only object ids 0 (cloud) and 1 (sky) pass. */
		if (cell.id != HAR_OBJ_CLOUD && cell.id != HAR_OBJ_SKY)
			return 0;
	}
	return 1;
}

#if HAR_DEBUG_ENEMY_PLANE_LOG
static void enemyPlaneTraceRecordEvent(const GameState* game, UBYTE event,
	WORD tileDistance, UBYTE blocked) {
	if (enemyPlaneTraceCount >= ENEMY_PLANE_TRACE_COUNT) {
		enemyPlaneTraceDropped++;
		return;
	}
	EnemyPlaneTraceRecord* record = &enemyPlaneTrace[enemyPlaneTraceCount++];
	WORD targetX = game->playerX;
	WORD targetY = game->playerY;
	if (game->enemyMissileTarget == ENEMY_TARGET_WINGMAN &&
		game->wingman.active) {
		targetX = wingmanScreenX(game);
		targetY = game->wingman.screenY;
	}
	record->sequence = enemyPlaneTraceSequence++;
	record->frame = frameCounter;
	record->scrollX = game->scrollX;
	record->visualX = game->enemyPlane.x;
	record->visualY = game->enemyPlane.y;
	record->logicalX = (WORD)(game->enemyPlane.targetWorldX - game->scrollX);
	record->logicalY = game->enemyPlane.targetY;
	record->targetX = targetX;
	record->targetY = targetY;
	record->tileDistance = tileDistance;
	record->lagX = (WORD)(game->enemyPlane.worldX -
		game->enemyPlane.targetWorldX);
	record->lagY = (WORD)(game->enemyPlane.y - game->enemyPlane.targetY);
	record->event = event;
	record->status = game->enemyPlaneRetreating ? 2 : 1;
	record->target = game->enemyMissileTarget;
	record->speed = game->speedLevel;
	record->blocked = blocked;
	record->framesSinceTick = enemyPlaneTraceLastTickFrame ?
		(UBYTE)(frameCounter - enemyPlaneTraceLastTickFrame) : 0;
	if (event != ENEMY_PLANE_TRACE_SPAWN)
		enemyPlaneTraceLastTickFrame = frameCounter;
}
#endif

static WORD radarSurfacePixelYForWorldColumn(LONG worldColumn) {
	/* Town blocks are generated outside the base terrain height map. Find the
	 * highest intact town cell first, then fall back to land/sea. Destroyed
	 * cells deliberately disappear from this query and no longer mask radar. */
	ObjectCell townCell;
	for (WORD tileRow = 0; tileRow < GAME_SEA_TOP_TILE_Y; tileRow++) {
		if (townBlockCellAtWorldColumnRow(worldColumn, tileRow, &townCell))
			return (WORD)(tileRow * GAME_TILE_HEIGHT);
	}
	WORD surfaceRow = landSurfaceYForWorldColumn(worldColumn);
	if (surfaceRow < 0)
		surfaceRow = GAME_SEA_TOP_TILE_Y;
	return (WORD)(surfaceRow * GAME_TILE_HEIGHT);
}

static void updateRadarDetection(GameState* game, UBYTE eligible) {
	/* Detection is intentionally a 12.5 Hz gameplay system. Surface probing
	 * used to run at 50 Hz even though the result could only affect detection
	 * every fourth frame; over towns that meant up to 45 procedural-object
	 * queries whose result was immediately discarded. Keep the last sampled
	 * clearance between ticks and do the expensive work only when state can
	 * actually change. */
	if (frameCounter & (RADAR_DETECTION_TICK_FRAMES - 1)) {
		if (telemetryEnabled)
			telemetryRadarLevel = game->radarDetection;
		return;
	}

	/* Probe the left, centre and right of the Harrier. The highest solid
	 * surface under its footprint governs clearance, including town blocks
	 * which are not part of the base terrain height map. */
	LONG worldPixelX = (LONG)game->scrollX + game->playerX;
	LONG probeColumns[3] = {
		(worldPixelX + 2) >> 3,
		(worldPixelX + PLAYER_SPRITE_WIDTH / 2) >> 3,
		(worldPixelX + PLAYER_SPRITE_WIDTH - 3) >> 3
	};
	WORD surfacePixelY = radarSurfacePixelYForWorldColumn(probeColumns[0]);
	for (UBYTE probe = 1; probe < 3; probe++) {
		WORD probeSurfaceY = radarSurfacePixelYForWorldColumn(probeColumns[probe]);
		if (probeSurfaceY < surfacePixelY)
			surfacePixelY = probeSurfaceY;
	}
	WORD clearance = surfacePixelY -
		(game->playerY + PLAYER_SPRITE_HEIGHT);
	if (clearance < 0)
		clearance = 0;
	if (clearance > 255)
		clearance = 255;
	/* Start from CPC's (3+skill) top-edge boundary, account for the one-row
	 * aircraft body, then apply Enhanced's small extra low-flight demand.
	 * The five belly-clearance limits are now 20/28/36/44/52 pixels. */
	UBYTE threshold = enhancedRadarClearanceThreshold(game);
	game->radarClearance = (UBYTE)clearance;
	game->radarThreshold = threshold;
	if (telemetryEnabled) {
		telemetryRadarClearance = game->radarClearance;
		telemetryRadarThreshold = game->radarThreshold;
	}
	UWORD previousDetection = game->radarDetection;

	if (!eligible) {
		if (game->radarDetection > 0)
			game->radarDetection--;
	} else {
		if (clearance > threshold) {
			/* Make altitude matter immediately: each extra four pixels above
			 * the skill limit adds another detection unit per 4-frame tick.
			 * Slow flight leaves the aircraft exposed for longer; maximum speed
			 * halves this gain while never making high flight radar-safe. */
			UWORD altitudeGain = (UWORD)(2 *
				(1 + (clearance - threshold) / 4));
			UWORD gain = (UWORD)((altitudeGain *
				(30 - game->speedLevel)) / 30);
			if (gain < 1)
				gain = 1;
			gain = (UWORD)(((ULONG)gain * RADAR_GAIN_RESPONSE_PERCENT + 50) /
				100);
			if (gain > 53)
				gain = 53;
			UWORD room = (UWORD)(RADAR_DETECTION_MAX - game->radarDetection);
			if (gain > room)
				gain = room;
			game->radarDetection = (UWORD)(game->radarDetection + gain);
			if (telemetryEnabled) {
				telemetryRadarAboveFrames += RADAR_DETECTION_TICK_FRAMES;
				telemetryRadarGain += gain;
			}
		} else if (clearance < threshold) {
			/* Terrain masking is most effective while moving quickly. Preserve a
			 * very slow decay at low speed, reach the old rate around speed 2,
			 * and scale to as much as 4.25x at maximum speed. */
			UWORD baseDrain = (UWORD)(1 + (threshold - clearance) / 3);
			if (baseDrain > 20)
				baseDrain = 20;
			UWORD speedFactor = (UWORD)(4 + game->speedLevel * 2);
			UWORD drain = (UWORD)((baseDrain * speedFactor) / 8);
			if (drain < 1)
				drain = 1;
			drain = (UWORD)(((ULONG)drain * RADAR_DRAIN_RESPONSE_PERCENT + 50) /
				100);
			if (drain > 86)
				drain = 86;
			if (drain > game->radarDetection)
				drain = game->radarDetection;
			game->radarDetection = (UWORD)(game->radarDetection - drain);
			if (telemetryEnabled) {
				telemetryRadarBelowFrames += RADAR_DETECTION_TICK_FRAMES;
				telemetryRadarDrain += drain;
			}
		}
	}

	if (telemetryEnabled)
		telemetryRadarLevel = game->radarDetection;

	/* Sound only while crossing 70/75/.../100 percent upwards. Holding a
	 * high radar level is silent; falling through a boundary is also silent. */
	if (!eligible || game->radarDetection <= previousDetection ||
		game->radarDetection < RADAR_DETECTION_ALARM_START)
		return;
	UWORD previousStep = previousDetection / 50;
	UWORD currentStep = game->radarDetection / 50;
	UWORD crossedLevel;
	if (previousDetection < RADAR_DETECTION_ALARM_START) {
		crossedLevel = RADAR_DETECTION_ALARM_START;
	} else if (currentStep > previousStep) {
		crossedLevel = (UWORD)(currentStep * 50);
		if (crossedLevel > RADAR_DETECTION_MAX)
			crossedLevel = RADAR_DETECTION_MAX;
	} else {
		return;
	}
	UWORD alarmRange = RADAR_DETECTION_MAX - RADAR_DETECTION_ALARM_START;
	UWORD alarmLevel = crossedLevel - RADAR_DETECTION_ALARM_START;
	UWORD volume = (UWORD)(RADAR_ALARM_MIN_VOLUME +
		((ULONG)alarmLevel * (RADAR_ALARM_MAX_VOLUME -
		RADAR_ALARM_MIN_VOLUME)) / alarmRange);
	UWORD period = (UWORD)(RADAR_ALARM_LOW_PERIOD -
		((ULONG)alarmLevel * (RADAR_ALARM_LOW_PERIOD -
		RADAR_ALARM_HIGH_PERIOD)) / alarmRange);
	playSfxAtTuned(SFX_RADAR_ALARM, SFX_POSITION_CENTER, volume,
		period);
	if (telemetryEnabled && telemetryRadarAlarmPulses < 0xffff)
		telemetryRadarAlarmPulses++;
	if (telemetryEnabled)
		telemetryRadarLevel = game->radarDetection;
}

static void launchEnemyMissile(GameState* game);
static UBYTE enemyRespawnFramesForSkill(UBYTE skillLevel);

static UBYTE spawnEnemyPlane(GameState* game, UWORD decisionColumn) {
	LONG spawnWorldX = (LONG)game->scrollX + ENEMY_CPC_SPAWN_SCREEN_X;
	LONG spawnWorldColumn = spawnWorldX >> 3;
	/* currtime is the same evolving CPC random state used by terrain. The
	 * port's column-derived R state is its deterministic equivalent here. */
	UBYTE lane = (UBYTE)(cpcRStateForWorldColumn(spawnWorldColumn) & 3);
	if (!enemyPlaneRowIsPassable(spawnWorldColumn, lane)) {
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_ENEMY_SPAWN_NO,
			(UBYTE)(lane + 1), decisionColumn, game, lane);
		return 0;
	}

	game->enemyPlane.active = 1;
	game->enemyPlane.timer = 0;
	game->enemyPlane.x = ENEMY_CPC_SPAWN_SCREEN_X;
	game->enemyPlane.y = (WORD)(lane * GAME_TILE_HEIGHT);
	game->enemyPlane.worldX = spawnWorldX;
	game->enemyPlane.targetWorldX = spawnWorldX;
	game->enemyPlane.targetY = game->enemyPlane.y;
	game->enemyPlane.dx = -1;
	game->enemyPlane.dy = 0;
	game->enemyPlaneRetreating = 0;
	game->enemyPlaneDamageState = ENEMY_PLANE_DAMAGE_NORMAL;
	game->enemyPlaneBrokenTimer = 0;
	game->enemyPlaneLogicPhase =
		(UBYTE)(GAME_TILE_WIDTH - HAR_ENEMY_PLANE_INTERPOLATION_PIXELS);
	game->radarDetection = 0;
	/* CPC chooses once when the plane approaches and keeps that identity
	 * until its missile is gone. Only an airborne Wingman is eligible. */
	game->enemyMissileTarget = ENEMY_TARGET_PLAYER;
	if (game->wingman.active &&
		game->wingman.mode != WINGMAN_LANDING_APPROACH &&
		game->wingman.mode != WINGMAN_LANDING_DECK &&
		(cpcRStateForWorldColumn(spawnWorldColumn) & 1))
		game->enemyMissileTarget = ENEMY_TARGET_WINGMAN;
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_ENEMY_SPAWN_OK,
		(UBYTE)(lane + 1), decisionColumn, game,
		game->enemyMissileTarget);
#if HAR_DEBUG_ENEMY_PLANE_LOG
	enemyPlaneTraceLastTickFrame = 0;
	enemyPlaneTraceRecordEvent(game, ENEMY_PLANE_TRACE_SPAWN, -1, 0);
#endif

	maybeStartWingmanIntercept(game);
	return 1;
}

/* One Amiga frame is only one interpolated pixel of the CPC's 8px character
 * step.  Run the complete Classic admission decision once per equivalent
 * logic tick.  CPC's launchenemyplane routine has exactly one ordered path:
 *
 *   gate -> R&15 -> existing drop? enemy : Wingman/drop rotation/enemy
 *
 * Powerups and enemy aircraft must therefore never use independent rolls.
 * Enhanced retains its radar accumulator and column-driven drop path. */
static UBYTE updateClassicAirAdmission(GameState* game,
	UWORD decisionColumn, UBYTE eligible, UBYTE heightGate) {
	game->classicEnemySpawnPhase++;
	if (game->classicEnemySpawnPhase < GAME_TILE_WIDTH)
		return 0;
	game->classicEnemySpawnPhase = 0;

	UWORD value = game->classicEnemySpawnRandomState;
	value = (UWORD)((value >> 1) ^ (-(value & 1) & 0xb400));
	if (!value)
		value = 0x6d2b;
	game->classicEnemySpawnRandomState = value;
	if (telemetryClassicAirAdmissionTicks < 0xffff)
		telemetryClassicAirAdmissionTicks++;

	if (!eligible || !heightGate)
		return 0;

	/* First CPC read: ld a,r / and #0f / ret nz. */
	UBYTE rState = (UBYTE)(value & CPC_R_MASK);
	if ((rState & POWERUP_SPAWN_ROLL_MASK) != 0)
		return 0;

	PowerupState* powerup = &game->powerup;
	UBYTE type = POWERUP_NONE;
	UBYTE startRow = (UBYTE)(cpcRandomStateForWorldColumn(decisionColumn) &
		POWERUP_SPAWN_MAX_ROW);

	/* CPC jumps directly to spawnenemyplane if a drop is already descending. */
	if (!powerup->active) {
		if (game->wingman.destroyed) {
			type = POWERUP_WINGMAN;
		} else {
			/* The second R read occurs after roughly thirteen opcode fetches.
			 * Only values >=100 enter the deterministic 1..6 drop rotation;
			 * slot six deliberately falls through to the enemy branch. */
			UBYTE powerupRoll = (UBYTE)((rState + 13) & CPC_R_MASK);
			if (powerupRoll >= 100) {
				powerup->spawnId = (UBYTE)(powerup->spawnId + 1);
				if (powerup->spawnId > 6)
					powerup->spawnId = 1;
				type = nextPowerupTypeForSpawnId(powerup->spawnId);
			}
		}
	}

	if (type != POWERUP_NONE) {
		/* This counter is expected to stay zero.  Keeping the assertion as
		 * telemetry rather than a runtime abort makes real-hardware audits safe. */
		if (game->enemyPlane.active && telemetryClassicPowerupWhileEnemy < 0xffff)
			telemetryClassicPowerupWhileEnemy++;
		spawnPowerup(game, type, startRow);
		if (telemetryClassicAirPowerupOutcomes < 0xffff)
			telemetryClassicAirPowerupOutcomes++;
		if (type == POWERUP_WINGMAN)
			telemetryLogGameEvent(TELEMETRY_GAME_EVENT_WING_POWERUP, 0,
				decisionColumn, game, rState);
		return 0;
	}

	if (telemetryClassicAirEnemyOutcomes < 0xffff)
		telemetryClassicAirEnemyOutcomes++;
	return spawnEnemyPlane(game, decisionColumn);
}

static UBYTE updateEnemyPlane(GameState* game) {
	UBYTE logicalTick = 0;
	UBYTE traceEvent = ENEMY_PLANE_TRACE_STEP;
	UBYTE traceBlocked = 0;
	WORD traceDistance = -1;
	UWORD rightEdgeColumn = (UWORD)((game->scrollX >> 3) + GAME_MAP_WIDTH);
	const LevelSegmentDef* segment =
		levelSegmentForWorldColumn((LONG)rightEdgeColumn);
	UBYTE stage = stageForWorldColumn((LONG)rightEdgeColumn, segment);
	UBYTE playerAvailable = (UBYTE)(!game->gameOver && !game->crashTimer &&
		game->respawnSafeTimer == 0 &&
		game->takeoffState == TAKEOFF_STATE_AIRBORNE &&
		game->landingState == LANDING_STATE_NONE);
	UBYTE radarStage = (UBYTE)(stage >= HAR_STAGE_ENEMY_SHIP_FIRED_MISSILE &&
		stage < HAR_STAGE_START_PIER);

	if (!game->enemyPlane.active) {
		if (!gameplayUsesRadar(game)) {
			/* CPC launchenemyplane uses the Harrier's absolute character row:
			 * playerTileY < 11-skill, followed by the 1-in-16 R-register roll.
			 * It does not accumulate terrain clearance over time. Keep the Amiga
			 * radar gauge as a visual binary risk indicator only. */
			UBYTE heightGate = playerHighEnoughForPowerup(game);
			UBYTE eligible = (UBYTE)(radarStage && playerAvailable &&
				!game->enemyMissile.active);
			game->radarDetection = (eligible && heightGate) ?
				RADAR_DETECTION_MAX : 0;
			game->radarClearance = (UBYTE)(game->playerY >> 3);
			game->radarThreshold = (UBYTE)(POWERUP_ALTITUDE_FLOOR_BASE -
				game->levelDifficulty);
			if (telemetryEnabled) {
				telemetryRadarLevel = game->radarDetection;
				telemetryRadarClearance = game->radarClearance;
				telemetryRadarThreshold = game->radarThreshold;
			}
			/* The same tick now owns both possible outcomes. Use CPC's actual
			 * spawn column 0x26 (38), not the display fetch's 40-column edge. */
			UWORD decisionColumn = (UWORD)((game->scrollX >> 3) +
				POWERUP_SPAWN_COLUMN);
			return updateClassicAirAdmission(game, decisionColumn,
				eligible, heightGate);
		}

		updateRadarDetection(game, (UBYTE)(radarStage && playerAvailable));
		if (!radarStage || !playerAvailable ||
			game->radarDetection < RADAR_DETECTION_MAX ||
			game->enemyMissile.active)
			return 0;

		return spawnEnemyPlane(game, rightEdgeColumn);
	}
	/* An active interceptor owns the frame. Enhanced radar drains while it is
	 * present as before; Classic has no accumulator to service. */
	if (gameplayUsesRadar(game))
		updateRadarDetection(game, 0);

	/* CPC enemyplanestatus 3 -> 4 -> 0 (asm enemyplanecollided /
	 * enemyplanehit): on the update after a weapon hit, move one complete
	 * character left and switch to the broken aircraft. The following update
	 * removes it. This deliberately does not alter an already launched
	 * heat-seeker's fixed target ownership. */
	if (game->enemyPlaneDamageState == ENEMY_PLANE_DAMAGE_HIT) {
		game->enemyPlaneDamageState = ENEMY_PLANE_DAMAGE_BROKEN;
		game->enemyPlaneBrokenTimer = ENEMY_PLANE_BROKEN_HOLD_FRAMES;
		game->enemyPlane.worldX -= GAME_TILE_WIDTH;
		game->enemyPlane.targetWorldX = game->enemyPlane.worldX;
		game->enemyPlane.x = (WORD)(game->enemyPlane.worldX - game->scrollX);
		return 1;
	}
	if (game->enemyPlaneDamageState == ENEMY_PLANE_DAMAGE_BROKEN) {
		/* Keep the wreck anchored in world space while fine scrolling moves
		 * underneath it.  It is already inert to weapons and collisions. */
		game->enemyPlane.x = (WORD)(game->enemyPlane.worldX - game->scrollX);
		if (game->enemyPlaneBrokenTimer > 0) {
			game->enemyPlaneBrokenTimer--;
			return 1;
		}
		game->enemyPlane.active = 0;
		game->enemyPlaneDamageState = ENEMY_PLANE_DAMAGE_NORMAL;
		game->enemyPlaneBrokenTimer = 0;
		game->enemyPlaneRetreating = 0;
		game->enemyRespawnTimer = enemyRespawnFramesForSkill(game->levelDifficulty);
		if (!game->enemyMissile.active)
			game->enemyMissileTarget = ENEMY_TARGET_NONE;
		return 1;
	}

	/* Keep CPC decisions in 8x8 character coordinates, but interpolate the
	 * resulting target by the selected 1..3 physical Amiga pixels per frame.
	 * The same fixed-point rate drives the decision cadence, so 2x/3x cannot
	 * catch a stationary logical target and visibly pause before the next one. */
	game->enemyPlaneLogicPhase = (UBYTE)(game->enemyPlaneLogicPhase +
		HAR_ENEMY_PLANE_INTERPOLATION_PIXELS);
	if (game->enemyPlaneLogicPhase >= GAME_TILE_WIDTH) {
		game->enemyPlaneLogicPhase -= GAME_TILE_WIDTH;
		logicalTick = 1;
		game->enemyPlane.targetWorldX -= GAME_TILE_WIDTH;
		LONG logicalWorldColumn = game->enemyPlane.targetWorldX >> 3;

		if (!game->enemyPlaneRetreating) {
			WORD targetX = game->playerX;
			WORD targetY = game->playerY;
			if (game->enemyMissileTarget == ENEMY_TARGET_WINGMAN &&
				game->wingman.active) {
				targetX = wingmanScreenX(game);
				targetY = game->wingman.screenY;
			}
			WORD planeTileX = (WORD)((game->enemyPlane.targetWorldX -
				(LONG)game->scrollX) >> 3);
			WORD targetTileX = targetX >> 3;
			WORD tileDistance = (WORD)(planeTileX - targetTileX);
			traceDistance = tileDistance;
			/* CPC changes to retreat status before testing whether its one
			 * shared missile slot is available. There is deliberately no timer
			 * fallback: only the <10-cell test may commit the attack. */
			if (tileDistance >= 0 && tileDistance < ENEMY_CPC_FIRE_RANGE_TILES) {
				traceEvent = ENEMY_PLANE_TRACE_FIRE;
				telemetryLogGameEvent(TELEMETRY_GAME_EVENT_ENEMY_MISSILE, 1,
					(UWORD)logicalWorldColumn, game, (UWORD)tileDistance);
				game->enemyPlaneRetreating = 1;
				if (!game->enemyMissile.active)
					launchEnemyMissile(game);
			} else {
				WORD currentRow = game->enemyPlane.targetY >> 3;
				WORD targetRow = targetY >> 3;
				WORD candidateRow = currentRow;
				if (currentRow < targetRow)
					candidateRow++;
				else if (currentRow > targetRow)
					candidateRow--;
				if (candidateRow != currentRow) {
					if (enemyPlaneRowIsPassable(logicalWorldColumn, candidateRow))
						game->enemyPlane.targetY = (WORD)(candidateRow * GAME_TILE_HEIGHT);
					else {
						traceEvent = ENEMY_PLANE_TRACE_BLOCKED;
						traceBlocked = 1;
						telemetryLogGameEvent(
							TELEMETRY_GAME_EVENT_ENEMY_PLANE_BLOCKED, 1,
							(UWORD)logicalWorldColumn, game, (UWORD)candidateRow);
					}
				}
			}
		} else if (logicalWorldColumn & 1) {
			/* CPC retreat climbs on odd columns only. */
			WORD candidateRow = (WORD)((game->enemyPlane.targetY >> 3) - 1);
			if (candidateRow < 0) {
#if HAR_DEBUG_ENEMY_PLANE_LOG
				enemyPlaneTraceRecordEvent(game, ENEMY_PLANE_TRACE_DESPAWN,
					-1, 0);
#endif
				game->enemyPlane.active = 0;
				game->enemyPlaneRetreating = 0;
				/* CPC missiletargetwingman is fixed until the shared missile is
				 * destroyed.  Do not retarget an in-flight shot merely because
				 * its launching aircraft has completed the retreat. */
				if (!game->enemyMissile.active)
					game->enemyMissileTarget = ENEMY_TARGET_NONE;
				return 1;
			}
			if (enemyPlaneRowIsPassable(logicalWorldColumn, candidateRow))
				game->enemyPlane.targetY = (WORD)(candidateRow * GAME_TILE_HEIGHT);
			else {
				traceEvent = ENEMY_PLANE_TRACE_BLOCKED;
				traceBlocked = 2;
				telemetryLogGameEvent(TELEMETRY_GAME_EVENT_ENEMY_PLANE_BLOCKED,
					2, (UWORD)logicalWorldColumn, game, (UWORD)candidateRow);
			}
		}
	}

	LONG worldDelta = game->enemyPlane.worldX - game->enemyPlane.targetWorldX;
	if (worldDelta > 0) {
		LONG step = worldDelta < HAR_ENEMY_PLANE_INTERPOLATION_PIXELS ?
			worldDelta : HAR_ENEMY_PLANE_INTERPOLATION_PIXELS;
		game->enemyPlane.worldX -= step;
	} else if (worldDelta < 0) {
		LONG step = -worldDelta < HAR_ENEMY_PLANE_INTERPOLATION_PIXELS ?
			-worldDelta : HAR_ENEMY_PLANE_INTERPOLATION_PIXELS;
		game->enemyPlane.worldX += step;
	}
	WORD verticalDelta = (WORD)(game->enemyPlane.targetY - game->enemyPlane.y);
	if (verticalDelta > 0) {
		WORD step = verticalDelta < HAR_ENEMY_PLANE_INTERPOLATION_PIXELS ?
			verticalDelta : HAR_ENEMY_PLANE_INTERPOLATION_PIXELS;
		game->enemyPlane.y += step;
	} else if (verticalDelta < 0) {
		WORD step = -verticalDelta < HAR_ENEMY_PLANE_INTERPOLATION_PIXELS ?
			-verticalDelta : HAR_ENEMY_PLANE_INTERPOLATION_PIXELS;
		game->enemyPlane.y -= step;
	}
	game->enemyPlane.x = (WORD)(game->enemyPlane.worldX - game->scrollX);
	game->enemyPlane.dy = 0;
#if HAR_DEBUG_ENEMY_PLANE_LOG
	if (logicalTick)
		enemyPlaneTraceRecordEvent(game, traceEvent, traceDistance, traceBlocked);
#endif
	if (game->enemyPlane.y < 0) {
		game->enemyPlane.y = 0;
	} else if (game->enemyPlane.y > HUD_TOP - ENEMY_SPRITE_HEIGHT) {
		game->enemyPlane.y = HUD_TOP - ENEMY_SPRITE_HEIGHT;
	}
	game->enemyPlane.timer++;
	if (game->enemyPlane.x < -ENEMY_SPRITE_WIDTH) {
#if HAR_DEBUG_ENEMY_PLANE_LOG
		enemyPlaneTraceRecordEvent(game, ENEMY_PLANE_TRACE_DESPAWN, -1, 0);
#endif
		game->enemyPlane.active = 0;
	}
#if !HAR_DEBUG_ENEMY_PLANE_LOG
	(void)logicalTick;
	(void)traceEvent;
	(void)traceBlocked;
	(void)traceDistance;
#endif
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
	game->enemyMissile.worldX = (LONG)game->scrollX +
		game->enemyMissile.x;
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
	game->enemyMissile.worldX = (LONG)game->scrollX +
		game->enemyMissile.x;
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

/* Register CPC enemyplanestatus=3 without immediately hiding the hardware
 * sprite. updateEnemyPlane() performs the subsequent 3->4->0 broken-aircraft
 * sequence. Returns false if another contact in the same frame already hit
 * this aircraft. */
static UBYTE hitEnemyPlane(GameState* game, UBYTE awardScore) {
	if (!game->enemyPlane.active ||
		game->enemyPlaneDamageState != ENEMY_PLANE_DAMAGE_NORMAL)
		return 0;

	game->enemyPlaneDamageState = ENEMY_PLANE_DAMAGE_HIT;
	game->enemyPlaneBrokenTimer = 0;
	game->enemyPlaneRetreating = 0;
	if (awardScore) {
		game->bonusScore += ENEMY_SCORE_VALUE;
		game->hitsCount++;
		updateHudValues(game);
	}
	/* CPC explosionnoise accompanies the hit, while the broken aircraft is
	 * the visual effect. Do not also create the unrelated tile explosion. */
	playSfxAt(SFX_IMPACT, game->enemyPlane.x);
	return 1;
}

/* Sprint 15.47: bounded Amiga adaptation of CPC findwaypoint() plus
 * checkwingmanradar/foundobstruction. The preferred step is tried first. If
 * its two-column footprint is blocked, all eight directions are considered
 * in an R&7-derived order, but never in an unbounded retry loop. An evasive
 * step deliberately cannot complete a waypoint on the same frame. */
static UBYTE moveWingmanTowardInterceptWaypoint(GameState* game,
	WORD targetX, WORD targetRow, UBYTE* usedEvasion) {
	static const WORD directionX[WINGMAN_EVASION_DIRECTION_COUNT] =
		{ 0, 1, 1, 1, 0, -1, -1, -1 };
	static const WORD directionY[WINGMAN_EVASION_DIRECTION_COUNT] =
		{ -1, -1, 0, 1, 1, 1, 0, -1 };
	WingmanState* wingman = &game->wingman;
	UBYTE verticalStep = 0;
	WORD proposedX = wingman->interceptScreenX;
	WORD proposedRow = wingman->row;

	*usedEvasion = 0;
	wingman->moveTimer++;
	if (wingman->moveTimer >= WINGMAN_MOVE_FRAME_INTERVAL) {
		wingman->moveTimer = 0;
		verticalStep = 1;
	}

	if (proposedX < targetX) {
		proposedX = (WORD)(proposedX + WINGMAN_INTERCEPT_MOVE_PIXELS);
		if (proposedX > targetX)
			proposedX = targetX;
	} else if (proposedX > targetX) {
		proposedX = (WORD)(proposedX - WINGMAN_INTERCEPT_MOVE_PIXELS);
		if (proposedX < targetX)
			proposedX = targetX;
	}
	if (verticalStep && proposedRow != targetRow)
		proposedRow = (WORD)(proposedRow +
			(proposedRow < targetRow ? 1 : -1));

	LONG proposedWorldColumn =
		((LONG)game->scrollX + proposedX) >> 3;
	if (wingmanFormationRowIsSafe(proposedWorldColumn, proposedRow)) {
		wingman->interceptScreenX = proposedX;
		wingman->row = proposedRow;
		return (UBYTE)(proposedX == targetX && proposedRow == targetRow);
	}

	LONG currentWorldColumn =
		((LONG)game->scrollX + wingman->interceptScreenX) >> 3;
	UBYTE firstDirection = (UBYTE)((cpcRStateForWorldColumn(currentWorldColumn) +
		wingman->evasionCursor) & 7);
	wingman->evasionCursor++;
	for (UBYTE attempt = 0; attempt < WINGMAN_EVASION_DIRECTION_COUNT;
		attempt++) {
		UBYTE direction = (UBYTE)((firstDirection + attempt) & 7);
		WORD candidateX = (WORD)(wingman->interceptScreenX +
			directionX[direction] * WINGMAN_INTERCEPT_MOVE_PIXELS);
		WORD candidateRow = wingman->row;
		if (verticalStep)
			candidateRow = (WORD)(candidateRow + directionY[direction]);
		if (candidateX < 0 || candidateX > SCREEN_WIDTH - PLAYER_SPRITE_WIDTH ||
			candidateRow < 1 || candidateRow > WINGMAN_MAX_ROW ||
			(candidateX == wingman->interceptScreenX &&
			 candidateRow == wingman->row))
			continue;
		LONG candidateWorldColumn =
			((LONG)game->scrollX + candidateX) >> 3;
		if (!wingmanFormationRowIsSafe(candidateWorldColumn, candidateRow))
			continue;
		wingman->interceptScreenX = candidateX;
		wingman->row = candidateRow;
		*usedEvasion = 1;
		return 0;
	}

	return 0;
}

/* Sprint 15.72.2: return through the same bounded CPC-direction waypoint
 * mover as intercept instead of maintaining a second split X/Y movement
 * implementation.  This preserves pixel-smooth horizontal presentation,
 * four-frame row timing and the R&7 obstruction fallback all the way back
 * into formation. */
static void updateWingmanReturnToFormation(GameState* game) {
	WingmanState* wingman = &game->wingman;
	WORD targetScreenX = (WORD)(game->playerX - WINGMAN_FORMATION_COLUMNS_BEHIND * GAME_TILE_WIDTH);
	LONG worldColumnLeft = ((LONG)game->scrollX + wingman->interceptScreenX) >> 3;
	WORD targetRow = cachedWingmanFormationTargetRow(game, worldColumnLeft);
	UBYTE usedEvasion = 0;

	if (moveWingmanTowardInterceptWaypoint(game,
		targetScreenX, targetRow, &usedEvasion)) {
		wingman->formationLogicalX = targetScreenX;
		wingman->mode = WINGMAN_FORMATION;
		wingman->returningToFormation = 0;
		wingman->interceptReason = 0;
		wingman->interceptWaypointX = 0;
		wingman->interceptWaypointRow = 0;
	}
}

static void updateWingmanIntercept(GameState* game) {
	WingmanState* wingman = &game->wingman;
	if (wingman->mode != WINGMAN_INTERCEPT_APPROACH &&
		wingman->mode != WINGMAN_INTERCEPT_TRACK &&
		wingman->mode != WINGMAN_INTERCEPT_FIRE)
		return;

	if (wingman->mode == WINGMAN_INTERCEPT_APPROACH &&
		wingman->returningToFormation) {
		updateWingmanReturnToFormation(game);
		return;
	}

	if (wingman->mode == WINGMAN_INTERCEPT_FIRE) {
		if (!wingman->rocket.active) {
			memset(&wingman->rocket, 0, sizeof(wingman->rocket));
			wingman->rocket.active = 1;
			wingman->rocket.x = wingman->interceptScreenX;
			wingman->rocket.y = wingman->screenY;
			wingman->rocket.worldX = (LONG)game->scrollX + wingman->rocket.x;
			wingman->rocket.worldAnchored = 1;
			wingman->rocket.dx = ROCKET_SPEED_PIXELS;
		}
		wingman->mode = WINGMAN_INTERCEPT_APPROACH;
		wingman->returningToFormation = 1;
		wingman->moveTimer = 0;
		return;
	}

	if (!game->enemyPlane.active ||
		game->enemyPlaneDamageState != ENEMY_PLANE_DAMAGE_NORMAL ||
		game->enemyMissileTarget == ENEMY_TARGET_NONE) {
		wingman->mode = WINGMAN_INTERCEPT_APPROACH;
		wingman->returningToFormation = 1;
		wingman->moveTimer = 0;
		return;
	}

	if (wingman->mode == WINGMAN_INTERCEPT_APPROACH) {
		UBYTE usedEvasion;
		if (moveWingmanTowardInterceptWaypoint(game,
			wingman->interceptWaypointX, wingman->interceptWaypointRow,
			&usedEvasion) && !usedEvasion) {
			wingman->mode = WINGMAN_INTERCEPT_TRACK;
			wingman->moveTimer = 0;
		}
		return;
	}

	/* CPC state 6 advances to state 7 at matching height/range. Its separate
	 * `sub 2 / jp c` guard tests the enemy row itself, so a plane in the top
	 * two logical rows also ends tracking instead of trapping the Wingman in
	 * an unreachable waypoint. */
	WORD altitudeGap = (WORD)(wingman->screenY - game->enemyPlane.y);
	if (altitudeGap < 0)
		altitudeGap = (WORD)-altitudeGap;
	WORD closingGap = (WORD)(game->enemyPlane.x - wingman->interceptScreenX);
	if (closingGap < 0)
		closingGap = (WORD)-closingGap;
	WORD enemyRow = (WORD)(game->enemyPlane.y / GAME_TILE_HEIGHT);
	if ((altitudeGap <= WINGMAN_INTERCEPT_ROW_TOLERANCE &&
		 closingGap <= WINGMAN_INTERCEPT_FIRE_RANGE_PIXELS) || enemyRow < 2) {
		wingman->mode = WINGMAN_INTERCEPT_FIRE;
		return;
	}

	UBYTE usedEvasion;
	moveWingmanTowardInterceptWaypoint(game,
		(WORD)(game->enemyPlane.x - WINGMAN_INTERCEPT_LEAD_PIXELS),
		enemyRow, &usedEvasion);
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

	rocket->worldX += rocket->dx;
	rocket->guidanceDistance += (UWORD)rocket->dx;
	rocket->x = (WORD)(rocket->worldX - game->scrollX);
	if (rocket->guidanceDistance >=
		standardRocketRangePixels(game) ||
		rocket->x > SCREEN_WIDTH) {
		rocket->active = 0;
		return;
	}

	if (game->enemyPlane.active &&
		game->enemyPlaneDamageState == ENEMY_PLANE_DAMAGE_NORMAL &&
		rectsOverlap(rocket->x, rocket->y, 16, WEAPON_SPRITE_HEIGHT,
			game->enemyPlane.x, game->enemyPlane.y, ENEMY_SPRITE_WIDTH, ENEMY_SPRITE_HEIGHT)) {
		rocket->active = 0;
		hitEnemyPlane(game, 1);
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
	UBYTE rState = cpcRStateForWorldColumn(rollWorldColumn);
	UBYTE forced = game->enemyMissileTarget == ENEMY_TARGET_WINGMAN;
	if (!forced && !(rState & WINGMAN_INTERCEPT_CHANCE_MASK)) {
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_WING_INTERCEPT_NO,
			3, (UWORD)rollWorldColumn, game, rState);
		return;
	}
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_WING_INTERCEPT_OK,
		forced ? 2 : 1,
		(UWORD)rollWorldColumn, game, rState);

	wingman->mode = WINGMAN_INTERCEPT_APPROACH;
	wingman->interceptWaypointX = (WORD)(wingman->interceptScreenX +
		WINGMAN_INTERCEPT_FIRST_PASS_COLUMNS * GAME_TILE_WIDTH);
	if (wingman->interceptWaypointX > SCREEN_WIDTH - PLAYER_SPRITE_WIDTH)
		wingman->interceptWaypointX = SCREEN_WIDTH - PLAYER_SPRITE_WIDTH;
	wingman->interceptWaypointRow = wingman->row;
	wingman->interceptReason = forced ? 2 : 1;
	wingman->evasionCursor = rState;
	wingman->returningToFormation = 0;
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
	UBYTE rState = cpcRStateForWorldColumn(targetColumn);
	if (rState & WINGMAN_BOMB_CHANCE_MASK) {
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_WING_BOMB_NO,
			(UBYTE)(rState & WINGMAN_BOMB_CHANCE_MASK),
			(UWORD)targetColumn, game, rState);
		return;
	}
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_WING_BOMB_OK, 1,
		(UWORD)targetColumn, game, rState);

	wingman->bombTargetWorldX =
		game->targetLock.worldX + GAME_TILE_WIDTH / 2;
	wingman->bombTargetY =
		(WORD)(game->targetLock.y + GAME_TILE_HEIGHT / 2);
	wingman->mode = WINGMAN_BOMB_APPROACH;
	wingman->moveTimer = 0;
}

static void updateWingmanBombingRun(GameState* game, UBYTE scrollPixels,
	UBYTE** worldBuffers, UBYTE* hudDirty) {
	WingmanState* wingman = &game->wingman;
	if (wingman->mode == WINGMAN_BOMB_APPROACH) {
		/* CPC subtracts five character columns from the shared target lock
		 * before the CPU Wingman releases. That lead is consumed by the same
		 * four-row bomb momentum used by both human players. */
		WORD targetX = (WORD)(wingman->bombTargetWorldX - game->scrollX - 4 -
			WINGMAN_BOMB_LEAD_TILES * GAME_TILE_WIDTH);
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
			wingman->bomb.dy = BOMB_SPEED_Y_PIXELS;
			resetWingmanBombMotion(wingman);
			wingman->mode = WINGMAN_INTERCEPT_APPROACH;
			wingman->returningToFormation = 1;
			wingman->moveTimer = 0;
			playSfxAt(SFX_BOMB, wingmanScreenX(game));
		}
	}

	/* This tail assumes the fixed pre-selected bombTargetY/bombTargetWorldX
	 * only the CPU's own approach logic above ever sets - a Player 2-dropped
	 * bomb (updateWingmanPlayer2Control()) has no such lock-on target and
	 * runs its own independent generic ground/object collision instead. */
	if (!wingman->bomb.active || game->wingmanControl != WINGMAN_CONTROL_CPU)
		return;
	advanceWingmanBombMotion(game, scrollPixels);

	if (wingman->bomb.y + BOMB_SHOT_PIXEL_BOB_HEIGHT >=
		wingman->bombTargetY) {
		LONG targetColumn = wingman->bombTargetWorldX >> 3;
		WORD targetRow = wingman->bombTargetY >> 3;
		retireBombPixelBobBeforeWorldMutation(worldBuffers,
			wingmanBombFootprints);
		markTargetDestroyedAtColumn(targetColumn);
		addCpcHitSmokeAtColumnRow(targetColumn, targetRow);
		dirtyRedrawWorldColumn(worldBuffers, targetColumn);
		clearTargetLockWithTelemetry(game, HAR_OBJ_GROUND_TARGET);
		game->bonusScore += GROUND_TARGET_SCORE_VALUE;
		game->hitsCount++;
		updateHudValues(game);
		*hudDirty = 1;
		/* Always a real locked ground target here - the CPU bombing run
		 * aims precisely at targetLock's own position, it never "misses"
		 * onto plain land the way a freely-aimed Player 2 bomb can. */
		startGroundTargetHitImpact(game, wingman->bomb.x,
			targetColumn, targetRow, HAR_OBJ_GROUND_TARGET);
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

/* CPC-equivalent two-waypoint landing. wingman1stwaypoint (&0518) is the
 * airborne staging point. wingman2ndwaypoint (&0d1d) is the normal deck pad;
 * only when Player 1 occupies it does CPC choose &0d16. The Amiga keeps those
 * carrier-relative coordinates but moves between them pixel by pixel. */
static void updateWingmanLanding(GameState* game) {
	WingmanState* wingman = &game->wingman;
	if (!wingman->active ||
		(wingman->mode != WINGMAN_LANDING_APPROACH &&
		 wingman->mode != WINGMAN_LANDING_DECK))
		return;

	if (wingman->mode == WINGMAN_LANDING_APPROACH) {
		WORD deckLeft = wingmanLandingDeckLeftX(game);
		wingman->landingTargetX = (WORD)(deckLeft +
			WINGMAN_LANDING_APPROACH_DECK_OFFSET);
		wingman->landingTargetSet = 1;
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
			wingman->screenY == WINGMAN_LANDING_WAYPOINT_Y) {
			wingman->mode = WINGMAN_LANDING_DECK;
			wingman->landingTargetSet = 0;
		}
		return;
	}

	{
		if (!wingman->landingTargetSet) {
			WORD deckLeft = wingmanLandingDeckLeftX(game);
			WORD defaultPad = (WORD)(deckLeft +
				WINGMAN_LANDING_DEFAULT_DECK_OFFSET);
			WORD alternatePad = (WORD)(deckLeft +
				WINGMAN_LANDING_ALTERNATE_DECK_OFFSET);
			WORD playerLeft = (WORD)(game->playerX + 2);
			WORD playerRight = (WORD)(game->playerX +
				PLAYER_SPRITE_WIDTH - 2);
			WORD padRight = (WORD)(defaultPad + PLAYER_SPRITE_WIDTH);
			wingman->landingTargetX =
				(playerRight >= defaultPad && playerLeft < padRight) ?
				alternatePad : defaultPad;
			wingman->landingTargetSet = 1;
		}

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
	(void)scrollPixels;

	if (updateEnemyShipMissileTrigger(game))
		changed = 1;

	if (game->enemyMissile.active) {
		/* CPC heatseekposition re-reads the selected aircraft each update, but
		 * only changes missile height while it remains at least five character
		 * cells ahead.  Inside that cutoff (or after passing the target) it
		 * flies level, leaving the authentic late-evasion window.  The CPC
		 * changes a complete 8px row; Amiga retains the same decision but
		 * presents it as the existing smooth one-pixel vertical step. */
		WORD targetX = game->playerX;
		WORD targetY = game->playerY;
		if (game->enemyMissileTarget == ENEMY_TARGET_WINGMAN &&
			game->wingman.active) {
			targetX = wingmanScreenX(game);
			targetY = game->wingman.screenY;
		} else {
			game->enemyMissileTarget = ENEMY_TARGET_PLAYER;
		}
		if ((WORD)(game->enemyMissile.x - targetX) >=
			ENEMY_MISSILE_HOMING_CUTOFF_PIXELS) {
			WORD missileRow = game->enemyMissile.y >> 3;
			WORD targetRow = targetY >> 3;
			if (missileRow < targetRow)
				game->enemyMissile.dy = 1;
			else if (missileRow > targetRow)
				game->enemyMissile.dy = -1;
			else
				game->enemyMissile.dy = 0;
		} else {
			game->enemyMissile.dy = 0;
		}
		/* A ring-buffer BOB needs one authoritative world coordinate.  The
		 * former screen-X += dx-scroll reconstruction could disagree by one
		 * scroll phase at ring/fine-scroll boundaries and made the ship shot
		 * blink. Derive screen X only from worldX and the current camera. */
		game->enemyMissile.worldX += game->enemyMissile.dx;
		game->enemyMissile.x = (WORD)(game->enemyMissile.worldX -
			(LONG)game->scrollX);
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
					powerupScreenX, game->powerup.logicalY,
					POWERUP_COLLISION_WIDTH, POWERUP_SPRITE_HEIGHT)) {
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
	game->aircraftFailureState = AIRCRAFT_FAILURE_NONE;
	stopAircraftFailureAlarm();
	stopAircraftFailureSmokeEmission();
	game->crashTimer = 0;
	game->rocketShot.active = 0;
	game->bombShot.active = 0;
	game->bombLaunchCooldown = 0;
	game->enemyPlane.active = 0;
	game->enemyMissile.active = 0;
	game->enemyMissileFromShip = 0;
	game->enemyRespawnTimer = 0;
	game->powerup.active = 0;
	stopAllSfx();
	startGameOverMusic();
}

static void losePlayerLife(GameState* game);
static void startPlayerCrash(GameState* game, WORD x, WORD y);

static void respawnPlayer(GameState* game) {
	game->playerX = PLAYER_START_X;
	game->playerY = PLAYER_START_Y;
	game->armour = 100;
	game->flakDamageCount = 0;
	game->playerFrigateStatus = PLAYER_FRIGATE_STATUS_CLEAR;
	game->takeoffState = TAKEOFF_STATE_AIRBORNE;
	/* Classic normally owns one aircraft and therefore never reaches this
	 * path. Debug infinite-lives must not silently import Enhanced immunity. */
	game->respawnSafeTimer = gameplayUsesSafeRespawn(game) ?
		PLAYER_RESPAWN_SAFE_FRAMES : 0;
	game->crashTimer = 0;
	game->crashEndsGame = 0;
	game->aircraftFailureState = AIRCRAFT_FAILURE_NONE;
	game->aircraftFailureCause = AIRCRAFT_FAILURE_CAUSE_NONE;
	game->aircraftFailureTimer = 0;
	game->aircraftFailureFallSpeed256 = 0;
	game->aircraftFailureAlarmFrame = 0;
	game->abandonedAircraftActive = 0;
	game->abandonedAircraftCrash = 0;
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
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_PLAYER_RESPAWN,
		game->respawnSafeTimer, (UWORD)(game->scrollX >> 3), game,
		game->lives);
}

static void startAircraftFailure(GameState* game, UBYTE cause) {
#if HAR_HEADLESS_AUTOPLAY
	(void)game;
	(void)cause;
	return;
#endif
	if (game->gameOver || game->crashTimer || game->ejectState ||
		game->aircraftFailureState != AIRCRAFT_FAILURE_NONE ||
		game->respawnSafeTimer > 0 ||
		game->takeoffState != TAKEOFF_STATE_AIRBORNE)
		return;
	LONG failureWorldColumn = ((LONG)game->scrollX + game->playerX) >> 3;
	WORD failureClearance = (WORD)(terrainSurfacePixelYForWorldColumn(
		failureWorldColumn) - (game->playerY + PLAYER_SPRITE_HEIGHT));
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_AIRCRAFT_FAILURE,
		cause, (UWORD)failureWorldColumn, game,
		(UWORD)(failureClearance > 0 ? failureClearance : 0));
	/* CPC has no recoverable failure descent: armour/fuel exhaustion, enemy
	 * missile contact and aircraft contact all break the Harrier into its three
	 * forward-moving parts and end the one-aircraft run. */
	if (!gameplayUsesEnhancedFailure(game)) {
		startPlayerCrash(game, game->playerX, game->playerY);
		return;
	}

	game->aircraftFailureState = AIRCRAFT_FAILURE_DESCENT;
	game->aircraftFailureCause = cause;
	game->aircraftFailureTimer = 0;
	game->aircraftFailureFallSpeed256 = AIRCRAFT_FAILURE_FALL_START_256;
	game->aircraftFailureY256 = (LONG)game->playerY << 8;
	game->aircraftFailureAlarmFrame = 0;
	game->abandonedAircraftActive = 0;
	game->abandonedAircraftCrash = 0;
	if (cause == AIRCRAFT_FAILURE_CAUSE_MISSILE ||
		cause == AIRCRAFT_FAILURE_CAUSE_AIRCRAFT ||
		cause == AIRCRAFT_FAILURE_CAUSE_ARMOUR)
		game->armour = 0;
	game->rocketShot.active = 0;
	game->bombShot.active = 0;
	game->impact.active = 0;
	game->enemyPlane.active = 0;
	game->enemyMissile.active = 0;
	game->enemyMissileFromShip = 0;
	game->enemyMissileTarget = ENEMY_TARGET_NONE;
	game->powerup.active = 0;
	stopSfxChannel(ENGINE_CHANNEL);
	engineActive = 0;
	if (cause == AIRCRAFT_FAILURE_CAUSE_MISSILE ||
		cause == AIRCRAFT_FAILURE_CAUSE_AIRCRAFT)
		playSfxAt(SFX_HIT, game->playerX);
	resetAircraftFailureSmoke();
}

static void startPlayerEject(GameState* game) {
	/* CPC checkejectorseat accepts a live player's eject input in the normal
	 * game loop. Both modes share the seat/parachute presentation; only the
	 * terminal result differs (Classic ends the run, Enhanced can rescue). */
	if (game->gameOver || game->crashTimer || game->ejectState ||
		game->takeoffState != TAKEOFF_STATE_AIRBORNE ||
		game->landingState != LANDING_STATE_NONE || game->missionComplete ||
		game->respawnSafeTimer > 0)
		return;
	if (game->aircraftFailureState != AIRCRAFT_FAILURE_DESCENT) {
		/* Voluntary eject is a valid tactical choice while the Harrier is still
		 * healthy.  Seed the same abandoned-aircraft descent used by a damaged
		 * machine, but do not start the failure alarm or smoke before the pilot
		 * has actually left. */
		game->aircraftFailureCause = AIRCRAFT_FAILURE_CAUSE_VOLUNTARY_EJECT;
		game->aircraftFailureTimer = 0;
		game->aircraftFailureFallSpeed256 = AIRCRAFT_FAILURE_FALL_START_256;
		game->aircraftFailureY256 = (LONG)game->playerY << 8;
		game->aircraftFailureAlarmFrame = 0;
		resetAircraftFailureSmoke();
	}

	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_AIRCRAFT_EJECT,
		game->aircraftFailureCause,
		(UWORD)(((LONG)game->scrollX + game->playerX) >> 3), game,
		game->aircraftFailureTimer);
	game->aircraftFailureState = AIRCRAFT_FAILURE_NONE;
	stopAircraftFailureAlarm();
	stopAircraftFailureSmokeEmission();
	/* The pilot leaves the hardware player channels, but the aircraft keeps
	 * its current falling position and velocity. The now-vacant enemy-plane
	 * attached pair renders it until surface impact. */
	game->abandonedAircraftActive = 1;
	game->abandonedAircraftCrash = 0;
	game->ejectState = 1;
	game->ejectTimer = 0;
	game->ejectX = game->playerX;
	game->ejectY = (WORD)(game->playerY - 4);
	game->rocketShot.active = 0;
	game->bombShot.active = 0;
	game->impact.active = 0;
	game->enemyPlane.active = 0;
	game->enemyMissile.active = 0;
	game->enemyMissileFromShip = 0;
	game->enemyMissileTarget = ENEMY_TARGET_NONE;
	game->powerup.active = 0;
	stopSfxChannel(ENGINE_CHANNEL);
	engineActive = 0;
	playSfxAt(SFX_EJECT, game->playerX);
}

static UBYTE updateAircraftFailure(GameState* game, const InputState* input) {
	if (game->aircraftFailureState != AIRCRAFT_FAILURE_DESCENT)
		return 0;

	game->aircraftFailureTimer++;
	if (input->left && !input->right && game->playerX > PLAYER_MIN_X)
		game->playerX -= AIRCRAFT_FAILURE_HORIZONTAL_PIXELS;
	else if (input->right && !input->left && game->playerX < PLAYER_MAX_X)
		game->playerX += AIRCRAFT_FAILURE_HORIZONTAL_PIXELS;

	/* Keep forward momentum after the engine dies, then bleed one speed level
	 * at a measured cadence. The player can steer the fall but never climb or
	 * arrest it. */
	if ((game->aircraftFailureTimer %
		AIRCRAFT_FAILURE_SPEED_DECAY_FRAMES) == 0 && game->speedLevel > 0)
		game->speedLevel--;
	UBYTE scrollPixels = scrollPixelsForSpeedLevel(game->speedLevel);
	if (game->scrollX < gameScrollMaxPixels()) {
		UWORD nextScrollX = (UWORD)(game->scrollX + scrollPixels);
		game->scrollX = nextScrollX > gameScrollMaxPixels() ?
			gameScrollMaxPixels() : nextScrollX;
	}

	game->aircraftFailureY256 += game->aircraftFailureFallSpeed256;
	game->playerY = (WORD)(game->aircraftFailureY256 >> 8);
	if (game->aircraftFailureFallSpeed256 < AIRCRAFT_FAILURE_FALL_MAX_256) {
		UWORD nextSpeed = (UWORD)(game->aircraftFailureFallSpeed256 +
			AIRCRAFT_FAILURE_FALL_ACCEL_256);
		game->aircraftFailureFallSpeed256 =
			nextSpeed > AIRCRAFT_FAILURE_FALL_MAX_256 ?
			AIRCRAFT_FAILURE_FALL_MAX_256 : nextSpeed;
	}

	updateAircraftFailureSmoke(game);
	updateAircraftFailureAlarm(game);

	LONG hitColumn;
	WORD hitRow;
	if (playerObjectMapCollision(game, &hitColumn, &hitRow) ==
		PLAYER_OBJECT_COLLISION_FATAL ||
		game->playerY + PLAYER_SPRITE_HEIGHT >= HUD_TOP) {
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_AIRCRAFT_IMPACT,
			game->aircraftFailureCause,
			(UWORD)(hitColumn >= 0 ? hitColumn :
			(((LONG)game->scrollX + game->playerX) >> 3)), game,
			game->aircraftFailureTimer);
		game->aircraftFailureState = AIRCRAFT_FAILURE_NONE;
		stopAircraftFailureAlarm();
		stopAircraftFailureSmokeEmission();
		/* A failed aircraft reaching the surface is a full ground impact, not
		 * another in-air damage hit. Keep the established CPC three-fragment
		 * breakup, but use the hard impact bang at the instant it starts. */
		game->crashEndsGame = 1;
		startPlayerCrashWithSfx(game, game->playerX, game->playerY,
			SFX_IMPACT);
	}
	return 1;
}

static UBYTE updateAbandonedAircraft(GameState* game) {
	if (!game->abandonedAircraftActive)
		return 0;

	/* Preserve the failed Harrier's downward velocity after the seat leaves.
	 * The world deliberately remains still for the parachute presentation, so
	 * a small forward drift is applied in screen space instead of scrolling. */
	game->aircraftFailureTimer++;
	if ((game->aircraftFailureTimer & 1) == 0 &&
		game->playerX < PLAYER_MAX_X)
		game->playerX++;
	game->aircraftFailureY256 += game->aircraftFailureFallSpeed256;
	game->playerY = (WORD)(game->aircraftFailureY256 >> 8);
	if (game->aircraftFailureFallSpeed256 < AIRCRAFT_FAILURE_FALL_MAX_256) {
		UWORD nextSpeed = (UWORD)(game->aircraftFailureFallSpeed256 +
			AIRCRAFT_FAILURE_FALL_ACCEL_256);
		game->aircraftFailureFallSpeed256 =
			nextSpeed > AIRCRAFT_FAILURE_FALL_MAX_256 ?
			AIRCRAFT_FAILURE_FALL_MAX_256 : nextSpeed;
	}

	LONG hitColumn;
	WORD hitRow;
	if (playerObjectMapCollision(game, &hitColumn, &hitRow) ==
		PLAYER_OBJECT_COLLISION_FATAL ||
		game->playerY + PLAYER_SPRITE_HEIGHT >= HUD_TOP) {
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_AIRCRAFT_IMPACT,
			game->aircraftFailureCause,
			(UWORD)(hitColumn >= 0 ? hitColumn :
			(((LONG)game->scrollX + game->playerX) >> 3)), game,
			game->aircraftFailureTimer);
		game->abandonedAircraftActive = 0;
		game->abandonedAircraftCrash = 1;
		/* The pilot is already safe in the parachute. This crash owns its
		 * bang/fragments, but must never choose the Game Over outcome itself. */
		game->crashEndsGame = 0;
		startPlayerCrashWithSfx(game, game->playerX, game->playerY,
			SFX_IMPACT);
	}
	return 1;
}

static UBYTE completePlayerEject(GameState* game) {
	game->ejectState = 0;
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_AIRCRAFT_RESCUED,
		game->aircraftFailureCause,
		(UWORD)(((LONG)game->scrollX + game->ejectX) >> 3), game,
		game->lives);
	if (gameplayUsesCpcEjectRules(game)) {
		game->armour = 0;
		triggerGameOver(game);
		return EJECT_UPDATE_CHANGED;
	}
	if (debugInfiniteLives)
		return EJECT_UPDATE_CARRIER_RESTART;
	if (game->lives > 0)
		game->lives--;
	if (game->lives > 0)
		return EJECT_UPDATE_CARRIER_RESTART;
	game->armour = 0;
	triggerGameOver(game);
	return EJECT_UPDATE_CHANGED;
}

static UBYTE updatePlayerEject(GameState* game) {
	if (!game->ejectState)
		return EJECT_UPDATE_NONE;
	if (game->ejectState == 3) {
		if (game->abandonedAircraftActive || game->crashTimer)
			return EJECT_UPDATE_NONE;
		return completePlayerEject(game);
	}

	game->ejectTimer++;
	if (game->ejectState == 1) {
		game->ejectY--;
		if (game->ejectTimer >= 12) {
			game->ejectState = 2;
			game->ejectTimer = 0;
		}
		return EJECT_UPDATE_CHANGED;
	}

	/* CPC descends one character step per update. The Amiga port keeps the
	 * same state transition but moves one pixel every other frame. */
	if ((game->ejectTimer & 1) == 0)
		game->ejectY++;

	LONG worldColumn = ((LONG)game->scrollX + game->ejectX) /
		GAME_TILE_WIDTH;
	WORD surfaceY = terrainSurfacePixelYForWorldColumn(worldColumn);
	if (game->ejectY + HAR_CPC_PARACHUTE_HEIGHT >= surfaceY ||
		game->ejectY >= HUD_TOP - HAR_CPC_PARACHUTE_HEIGHT) {
		game->ejectY = (WORD)(surfaceY - HAR_CPC_PARACHUTE_HEIGHT);
		game->ejectState = 3;
		if (!game->abandonedAircraftActive && !game->crashTimer)
			return completePlayerEject(game);
	}
	return EJECT_UPDATE_CHANGED;
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

#if HAR_HEADLESS_CLASSIC_CONTRACT_TEST
static void writeClassicContractResult(const char* result) {
	BPTR file = Open((CONST_STRPTR)"DH1:classic_contract.txt", MODE_NEWFILE);
	if (!file)
		return;
	Write(file, (APTR)result, (LONG)strlen(result));
	Close(file);
}

static int runClassicGameplayContractTest(void) {
	GameState classic;
	GameState enhanced;
	GameState bombTest;
	GameState wingmanBombTest;
	GameState townCollisionTest;
	WeaponState maverickTest;
	ULONG fuelFrames = 0;
	UWORD bombMomentumFrames = 0;
	UWORD bombDescentFrames = 0;
	UWORD townSmokeCellsTested = 0;
	UWORD failures = 0;
	memset(&classic, 0, sizeof(classic));
	memset(&enhanced, 0, sizeof(enhanced));
	memset(&bombTest, 0, sizeof(bombTest));
	memset(&wingmanBombTest, 0, sizeof(wingmanBombTest));
	memset(&townCollisionTest, 0, sizeof(townCollisionTest));
	memset(&maverickTest, 0, sizeof(maverickTest));
	classic.gameMode = GAME_MODE_CLASSIC;
	enhanced.gameMode = GAME_MODE_ENHANCED;

#define CONTRACT_CHECK(condition, name) do { \
	if (!(condition)) { \
		KPrintF("CLASSIC CONTRACT FAIL: " name "\n"); \
		failures++; \
	} \
} while (0)
	CONTRACT_CHECK(gameplayStartingAircraft(&classic) == PLAYER_CLASSIC_LIVES,
		"Classic starting aircraft");
	CONTRACT_CHECK(gameplayStartingAircraft(&enhanced) == PLAYER_START_LIVES,
		"Enhanced starting aircraft");
	CONTRACT_CHECK(!gameplayUsesRadar(&classic), "Classic radar gameplay disabled");
	CONTRACT_CHECK(gameplayUsesRadar(&enhanced), "Enhanced radar gameplay enabled");
	enhanced.levelDifficulty = 1;
	UBYTE radarSkill1 = enhancedRadarClearanceThreshold(&enhanced);
	enhanced.levelDifficulty = 3;
	UBYTE radarSkill3 = enhancedRadarClearanceThreshold(&enhanced);
	enhanced.levelDifficulty = 5;
	UBYTE radarSkill5 = enhancedRadarClearanceThreshold(&enhanced);
	CONTRACT_CHECK(radarSkill1 > radarSkill3 && radarSkill3 > radarSkill5,
		"Enhanced radar becomes stricter with difficulty");
	CONTRACT_CHECK(!gameplayUsesEnhancedFailure(&classic),
		"Classic Enhanced failure disabled");
	CONTRACT_CHECK(gameplayUsesEnhancedFailure(&enhanced),
		"Enhanced failure enabled");
	CONTRACT_CHECK(!gameplayUsesSafeRespawn(&classic),
		"Classic safe respawn disabled");
	CONTRACT_CHECK(gameplayUsesSafeRespawn(&enhanced),
		"Enhanced safe respawn enabled");
	CONTRACT_CHECK(gameplayUsesCpcCollisionRules(&classic),
		"Classic CPC collision policy");
	CONTRACT_CHECK(gameplayUsesCpcCollisionRules(&enhanced),
		"Enhanced CPC collision policy");
	CONTRACT_CHECK(gameplayUsesCpcEjectRules(&classic),
		"Classic CPC eject policy");
	CONTRACT_CHECK(!gameplayUsesCpcEjectRules(&enhanced),
		"Enhanced rescue eject policy");
	CONTRACT_CHECK(flakDamageThresholdForSkill(1) == 23,
		"Classic flak threshold skill 1");
	CONTRACT_CHECK(flakDamageThresholdForSkill(5) == 15,
		"Classic flak threshold skill 5");
	CONTRACT_CHECK(cpcPlayerCollisionForObjectId(HAR_OBJ_CLOUD) ==
		PLAYER_OBJECT_COLLISION_SAFE &&
		cpcPlayerCollisionForObjectId(HAR_OBJ_SKY) ==
		PLAYER_OBJECT_COLLISION_SAFE,
		"CPC cloud and sky collision are safe");
	CONTRACT_CHECK(cpcPlayerCollisionForObjectId(HAR_OBJ_WINGMAN) ==
		PLAYER_OBJECT_COLLISION_SAFE &&
		cpcPlayerCollisionForObjectId(HAR_OBJ_POWERUP) ==
		PLAYER_OBJECT_COLLISION_SAFE,
		"CPC Wingman and powerup object contact are safe");
	CONTRACT_CHECK(cpcPlayerCollisionForObjectId(HAR_OBJ_FLAK) ==
		PLAYER_OBJECT_COLLISION_FLAK &&
		cpcPlayerCollisionForObjectId(HAR_OBJ_SMOKE) ==
		PLAYER_OBJECT_COLLISION_SMOKE,
		"CPC flak and smoke use accumulated damage path");
	CONTRACT_CHECK(cpcPlayerCollisionForObjectId(HAR_OBJ_LAND) ==
		PLAYER_OBJECT_COLLISION_FATAL &&
		cpcPlayerCollisionForObjectId(HAR_OBJ_GROUND_TARGET) ==
		PLAYER_OBJECT_COLLISION_FATAL &&
		cpcPlayerCollisionForObjectId(HAR_OBJ_ENEMY_SHIP) ==
		PLAYER_OBJECT_COLLISION_FATAL &&
		cpcPlayerCollisionForObjectId(HAR_OBJ_TOWN_BLOCK) ==
		PLAYER_OBJECT_COLLISION_FATAL &&
		cpcPlayerCollisionForObjectId(HAR_OBJ_ENEMY_PLANE) ==
		PLAYER_OBJECT_COLLISION_FATAL &&
		cpcPlayerCollisionForObjectId(HAR_OBJ_OWN_FRIGATE) ==
		PLAYER_OBJECT_COLLISION_FATAL,
		"CPC occupied non-flak objects are fatal");
	CONTRACT_CHECK(CARRIER_TOWER_UPPER_LEFT == 5 * GAME_TILE_WIDTH &&
		CARRIER_TOWER_UPPER_RIGHT == 7 * GAME_TILE_WIDTH &&
		CARRIER_TOWER_LOWER_LEFT == 4 * GAME_TILE_WIDTH &&
		CARRIER_TOWER_LOWER_RIGHT == 8 * GAME_TILE_WIDTH,
		"CPC carrier tower uses stepped 2/4-cell mask");

	/* CPC changes horizontal character position only on every second speed
	 * level. Amiga retains the approved per-level pixel interpolation and
	 * smooth world-scroll bands in both modes, but both mappings are locked
	 * here so later tuning cannot silently masquerade as CPC parity. */
	for (UBYTE speed = 0; speed <= GAME_SPEED_LEVEL_MAX; speed++) {
		CONTRACT_CHECK(cpcPlayerTileXForSpeedLevel(speed) ==
			(UBYTE)(8 + (speed >> 1)), "CPC speed-to-X source mapping");
		CONTRACT_CHECK(playerTargetXForSpeedLevel(speed) ==
			(WORD)(PLAYER_SPEED_ANCHOR_X + speed *
				PLAYER_SPEED_ANCHOR_STEP_PIXELS),
			"Amiga smooth speed-to-X mapping");
		if (speed > 0)
			CONTRACT_CHECK(playerTargetXForSpeedLevel(speed) >
				playerTargetXForSpeedLevel((UBYTE)(speed - 1)),
				"Amiga speed anchor is monotonic");
		CONTRACT_CHECK(playerTargetXForSpeedLevel(speed) >= PLAYER_MIN_X &&
			playerTargetXForSpeedLevel(speed) <= PLAYER_MAX_X,
			"Amiga speed anchor stays inside flight bounds");
	}
	CONTRACT_CHECK(scrollPixelsForSpeedLevel(0) == 1,
		"Amiga minimum flight never stops world progress");
	CONTRACT_CHECK(scrollPixelsForSpeedLevel(1) == 2 &&
		scrollPixelsForSpeedLevel(4) == 2,
		"Amiga low speed scroll band");
	CONTRACT_CHECK(scrollPixelsForSpeedLevel(5) == 3 &&
		scrollPixelsForSpeedLevel(8) == 3,
		"Amiga medium speed scroll band");
	CONTRACT_CHECK(scrollPixelsForSpeedLevel(9) == 4 &&
		scrollPixelsForSpeedLevel(15) == 4,
		"Amiga high speed scroll band");
	CONTRACT_CHECK(ROCKET_RANGE_MIN_TILES == 10 &&
		ROCKET_RANGE_MAX_TILES == 20 &&
		ROCKET_RANGE_DEFAULT_TILES == 10,
		"CPC standard rocket menu range is 10..20, default 10");
	classic.rocketRangeTiles = ROCKET_RANGE_DEFAULT_TILES;
	CONTRACT_CHECK(standardRocketRangePixels(&classic) ==
		10 * GAME_TILE_WIDTH,
		"CPC standard rocket range converts character cells to pixels");

	/* Source-derived CPC Maverick contract: range 2..10 gives eight launch
	 * columns before guidance, getdirectionfromcoords has exact 3x3 signs,
	 * and a lost/arrived lock retains the last non-zero direction. Pixel
	 * interpolation may shorten only its final step to land on an exact axis. */
	CONTRACT_CHECK(MAVERICK_GUIDANCE_DELAY_PIXELS == 8 * GAME_TILE_WIDTH,
		"CPC Maverick has eight-column launch phase");
	classic.targetLock.active = 1;
	classic.targetLock.worldX = 40;
	classic.scrollX = 0;
	classic.playerX = 96;
	CONTRACT_CHECK(targetLockIsAvailable(&classic),
		"CPC Maverick accepts active lock behind player");
	maverickTest.worldX = 100;
	maverickTest.y = 40;
	maverickTest.targetWorldX = 109;
	maverickTest.targetY = 44;
	CONTRACT_CHECK(directionToMaverickTarget(&maverickTest) ==
		MAVERICK_DIRECTION_RIGHT,
		"CPC Maverick steers for one-pixel horizontal residual");
	maverickTest.targetWorldX = 108;
	maverickTest.targetY = 43;
	CONTRACT_CHECK(directionToMaverickTarget(&maverickTest) ==
		MAVERICK_DIRECTION_UP,
		"CPC Maverick steers for one-pixel vertical residual");
	maverickTest.targetWorldX = 111;
	maverickTest.targetY = 42;
	maverickTest.direction = MAVERICK_DIRECTION_RIGHT;
	moveGuidedMaverick(&maverickTest, 1);
	CONTRACT_CHECK(maverickTest.worldX == 103 && maverickTest.y == 38,
		"Amiga Maverick clamps final guided pixel step");
	maverickTest.direction = MAVERICK_DIRECTION_LEFT;
	moveGuidedMaverick(&maverickTest, 0);
	CONTRACT_CHECK(maverickTest.worldX == 99 && maverickTest.y == 38,
		"CPC Maverick retains direction after lock loss");

	/* Source-derived CPC bomb contract: four downward momentum rows keep
	 * screen X fixed against scrolling; subsequent descent keeps world X
	 * fixed and therefore follows scenery left. The 2/3-pixel DDA is the
	 * shared Amiga presentation used by both gameplay modes. */
	bombTest.scrollX = 100;
	bombTest.bombShot.active = 1;
	bombTest.bombShot.x = 50;
	bombTest.bombShot.y = 40;
	bombTest.bombShot.worldX = 150;
	bombTest.bombLogicalWorldX = 150;
	bombTest.bombLogicalY = 40;
	while (bombTest.bombMomentumSteps < BOMB_MOMENTUM_LOGICAL_STEPS &&
		bombMomentumFrames < 32) {
		if (bombMomentumFrames > 0)
			bombTest.scrollX += 4;
		advancePlayerBombMotion(&bombTest, 4);
		bombMomentumFrames++;
	}
	CONTRACT_CHECK(bombTest.bombMomentumSteps == 4,
		"CPC bomb completes four momentum rows");
	CONTRACT_CHECK(bombTest.bombLogicalY == 72,
		"CPC bomb momentum advances down four rows");
	CONTRACT_CHECK(bombTest.bombShot.x == 50,
		"CPC bomb momentum holds screen X against scroll");
	CONTRACT_CHECK(bombMomentumFrames == 13,
		"Amiga bomb momentum interpolation cadence");
	{
		LONG descentWorldX = bombTest.bombShot.worldX;
		WORD descentStartX = bombTest.bombShot.x;
		while (bombTest.bombLogicalY < 80 && bombDescentFrames < 16) {
			bombTest.scrollX += 4;
			advancePlayerBombMotion(&bombTest, 4);
			bombDescentFrames++;
		}
		CONTRACT_CHECK(bombTest.bombLogicalY == 80,
			"CPC bomb descent advances one row");
		CONTRACT_CHECK(bombTest.bombShot.worldX == descentWorldX,
			"CPC bomb descent retains world X");
		CONTRACT_CHECK(bombTest.bombShot.x ==
			(WORD)(descentStartX - bombDescentFrames * 4),
			"CPC bomb descent follows scenery left");
		CONTRACT_CHECK(bombDescentFrames == 3,
			"Amiga bomb descent interpolation cadence");
	}

	/* CPC points all three aircraft at dolaunchbomb. Guard the separate
	 * Wingman interpolation state against drifting back to a vertical-only or
	 * immediately world-anchored implementation. */
	wingmanBombTest.scrollX = 100;
	wingmanBombTest.wingman.bomb.active = 1;
	wingmanBombTest.wingman.bomb.x = 50;
	wingmanBombTest.wingman.bomb.y = 40;
	wingmanBombTest.wingman.bomb.worldX = 150;
	resetWingmanBombMotion(&wingmanBombTest.wingman);
	for (UWORD frame = 0;
		wingmanBombTest.wingman.bombMomentumSteps <
			BOMB_MOMENTUM_LOGICAL_STEPS && frame < 32; frame++) {
		if (frame > 0)
			wingmanBombTest.scrollX += 4;
		advanceWingmanBombMotion(&wingmanBombTest, 4);
	}
	CONTRACT_CHECK(wingmanBombTest.wingman.bombMomentumSteps == 4 &&
		wingmanBombTest.wingman.bomb.x == 50,
		"CPC Wingman bombs share player momentum");
	CONTRACT_CHECK(WINGMAN_BOMB_LEAD_TILES == 5,
		"CPC CPU Wingman bombing lead is five columns");

	resetPlayerFuel(&classic);
	CONTRACT_CHECK(classic.fuel == 999, "Fuel full maps to 999");
	while (classic.fuel > 0 && fuelFrames < 10000) {
		updatePlayerFuel(&classic);
		fuelFrames++;
	}
	CONTRACT_CHECK(classic.fuel == 0, "Fuel reaches zero");
	/* ceil(14 * 16 * 256 * 50 / 300) PAL frames. Keep the literal as
	 * an external contract against accidentally changing the CPC constants. */
	CONTRACT_CHECK(fuelFrames == 9558, "CPC fuel duration is 9558 PAL frames");

	/* Regression: town destruction must not share the old 24-cell ship/target
	 * smoke list.  Exercise more than that limit through the real generated
	 * town lookup and verify every struck facade cell becomes persistent smoke. */
	configureRuntimeLevelRoute(0, 0);
	{
		const LevelSegmentDef* landSegment =
			levelSegmentForWorldColumn(CPC_LAND_PROCEDURAL_WORLD_START);
		const LevelSegmentDef* descendSegment = levelSegmentForWorldColumn(401);
		UBYTE previousHeight = cpcLandProceduralProfile(
			CPC_LAND_PROCEDURAL_BASE_LENGTH - 1);
		UBYTE descentIsMonotonic = 1;
		CONTRACT_CHECK(landSegment &&
			landSegment->terrainKind == HAR_TERRAIN_CPC_RANDOM_LAND &&
			landSegment->startColumn == CPC_LAND_PROCEDURAL_WORLD_START &&
			(UWORD)(landSegment->endColumn - landSegment->startColumn + 1) ==
				CPC_LAND_PROCEDURAL_BASE_LENGTH,
			"CPC random-land table exactly covers route segment");
		if (descendSegment) {
			for (LONG column = descendSegment->startColumn;
				column <= descendSegment->endColumn; column++) {
				UBYTE height = terrainYForWorldColumn(column, descendSegment,
					HAR_TERRAIN_CPC_DESCEND_TO_TOWN);
				if (height < previousHeight || height > previousHeight + 1)
					descentIsMonotonic = 0;
				previousHeight = height;
			}
		}
		CONTRACT_CHECK(descendSegment && descentIsMonotonic &&
			previousHeight == CPC_LAND_PROCEDURAL_BASELINE,
			"CPC pre-town descent is monotonic and reaches town row");
	}
	resetCpcTownBlockTable();
	generateCpcTownBlockTable();
	resetDestroyedShipColumns();
	for (LONG column = 411; column <= 610 && townSmokeCellsTested < 32;
		column++) {
		for (WORD row = 0; row < GAME_OBJECT_MAP_HEIGHT_TILES; row++) {
			ObjectCell townCell;
			if (!townBlockCellAtWorldColumnRow(column, row, &townCell) ||
				townCell.id != HAR_OBJ_TOWN_BLOCK)
				continue;
			if (townSmokeCellsTested == 0) {
				LONG hitColumn;
				WORD hitRow;
				townCollisionTest.takeoffState = TAKEOFF_STATE_AIRBORNE;
				townCollisionTest.scrollX = (UWORD)(column * GAME_TILE_WIDTH -
					PLAYER_MIN_X);
				townCollisionTest.playerX = PLAYER_MIN_X;
				townCollisionTest.playerY = (WORD)(row * GAME_TILE_HEIGHT);
				CONTRACT_CHECK(playerObjectMapCollision(&townCollisionTest,
					&hitColumn, &hitRow) == PLAYER_OBJECT_COLLISION_FATAL &&
					hitColumn == column && hitRow == row,
					"Aircraft collides with generated town facade");
			}
			addCpcTownHitSmokeAtColumnRow(column, row);
			CONTRACT_CHECK(townHitSmokeTileAtColumnRow(column, row) ==
				GAME_SHIP_WRECK_SMOKE_TILE_B,
				"Town hit smoke persists beyond shared-list capacity");
			townSmokeCellsTested++;
			break;
		}
	}
	CONTRACT_CHECK(townSmokeCellsTested > GAME_SHIP_WRECK_SMOKE_MAX,
		"Town smoke capacity exceeds old 24-cell list");
	CONTRACT_CHECK(shipWreckSmokeCount == 0,
		"Town smoke does not consume ship smoke list");

	if (failures) {
		KPrintF("CLASSIC CONTRACT: %ld failure(s)\n", (LONG)failures);
		writeClassicContractResult("FAIL\n");
		return 20;
	}
	KPrintF("CLASSIC CONTRACT PASS: fuel=%ld frames, GameState=%ld bytes\n",
		(LONG)fuelFrames, (LONG)sizeof(GameState));
	writeClassicContractResult(
		"PASS fuelFrames=9558 cpcX=8..15 amigaX=96..186 scroll=1..4 "
		"bombMomentumFrames=13 bombDescentFrames=3 maverick=exact-9dir "
		"collision=cpc-table carrierTower=2x4\n");
	return 0;
#undef CONTRACT_CHECK
}
#endif

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
	if (game->gameOver || game->respawnSafeTimer > 0 || game->crashTimer ||
		game->aircraftFailureState != AIRCRAFT_FAILURE_NONE)
		return;

	UBYTE threshold = flakDamageThresholdForSkill(game->levelDifficulty);
	if (game->flakDamageCount < threshold)
		game->flakDamageCount++;
	game->armour = game->flakDamageCount < threshold ? (UWORD)(100 - (100 * (ULONG)game->flakDamageCount / threshold)) : 0;
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_PLAYER_FLAK_HIT,
		threshold, (UWORD)(((LONG)game->scrollX + game->playerX) >> 3), game,
		game->flakDamageCount);
	playSfxAt(SFX_FLAK_HIT, game->playerX);
	if (game->armour == 0)
		startAircraftFailure(game, AIRCRAFT_FAILURE_CAUSE_ARMOUR);
}

static void startPlayerCrashWithSfx(GameState* game, WORD x, WORD y, UBYTE sfxId) {
#if HAR_HEADLESS_AUTOPLAY
	/* Performance runs must cover the complete map. Collision detection still
	 * executes, but the synthetic pilot is invulnerable so one terrain contact
	 * cannot turn the remaining samples into a stationary game-over screen. */
	(void)game;
	(void)x;
	(void)y;
	(void)sfxId;
	return;
#endif
	if (game->gameOver || game->crashTimer || game->respawnSafeTimer > 0)
		return;
	telemetryLogGameEvent(TELEMETRY_GAME_EVENT_PLAYER_CRASH, sfxId,
		(UWORD)(((LONG)game->scrollX + x) >> 3), game, (UWORD)y);

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
	if (sfxId < SFX_COUNT)
		playSfxAt(sfxId, game->playerX);
}

static void startPlayerCrash(GameState* game, WORD x, WORD y) {
	game->crashEndsGame = 1;
	startPlayerCrashWithSfx(game, x, y, SFX_HIT);
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
		if (game->abandonedAircraftCrash) {
			/* The wreck belongs to an aircraft whose pilot is already under
			 * canopy. Ending these fragments must not respawn or end the run;
			 * updatePlayerEject() resolves the landed pilot independently. */
			game->abandonedAircraftCrash = 0;
			memset(game->crashPart, 0, sizeof(game->crashPart));
		} else if (game->crashEndsGame) {
			game->crashEndsGame = 0;
			triggerGameOver(game);
		} else if (debugInfiniteLives) {
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
	if (objectCollision != PLAYER_OBJECT_COLLISION_SMOKE)
		game->smokeDamageContact = 0;

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
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_PLAYER_COLLISION,
			objectCollision, (UWORD)collisionWorldColumn, game,
			(UWORD)collisionTileY);
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
	if (objectCollision == PLAYER_OBJECT_COLLISION_SMOKE) {
		/* A hardware sprite can overlap the persistent smoke for several
		 * frames. CPC's object-map drawing naturally debounced that contact;
		 * mirror the result explicitly without erasing the wreck smoke. */
		if (!game->smokeDamageContact) {
			telemetryLogRenderEvent(7, objectCollision,
				(UWORD)((game->scrollX + game->playerX) >> 3),
				game->scrollX, (UWORD)game->playerY);
			applyPlayerFlakDamage(game);
			game->smokeDamageContact = 1;
			*hudChanged = 1;
			if (game->crashTimer) {
				*weaponChanged = 1;
				return enemyChanged;
			}
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

	if (game->enemyPlane.active &&
		game->enemyPlaneDamageState == ENEMY_PLANE_DAMAGE_NORMAL &&
		game->rocketShot.active &&
		rectsOverlap(game->rocketShot.x, game->rocketShot.y, 16, WEAPON_SPRITE_HEIGHT,
			game->enemyPlane.x, game->enemyPlane.y, ENEMY_SPRITE_WIDTH, ENEMY_SPRITE_HEIGHT)) {
		game->rocketShot.active = 0;
		hitEnemyPlane(game, 1);
		*hudChanged = 1;
		*weaponChanged = 1;
		enemyChanged = 1;
	}

	if (game->enemyPlane.active &&
		game->enemyPlaneDamageState == ENEMY_PLANE_DAMAGE_NORMAL &&
		game->bombShot.active &&
		rectsOverlap(game->bombShot.x, game->bombShot.y, 16, WEAPON_SPRITE_HEIGHT,
			game->enemyPlane.x, game->enemyPlane.y, ENEMY_SPRITE_WIDTH, ENEMY_SPRITE_HEIGHT)) {
		game->bombShot.active = 0;
		hitEnemyPlane(game, 1);
		*hudChanged = 1;
		*weaponChanged = 1;
		enemyChanged = 1;
	}

	/* CPC checks the two occupied object-map cells after every Wingman move.
	 * Do this before projectile/aircraft contacts so solid world geometry can
	 * no longer be flown through by either CPU or Player 2. */
	if (game->wingman.active && wingmanObjectMapCollision(game)) {
		destroyWingman(game);
		*wingmanChanged = 1;
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
			game->enemyPlaneDamageState == ENEMY_PLANE_DAMAGE_NORMAL &&
			rectsOverlap(wingmanX, wingmanY, PLAYER_SPRITE_WIDTH,
				PLAYER_SPRITE_HEIGHT, game->enemyPlane.x,
				game->enemyPlane.y, ENEMY_SPRITE_WIDTH,
				ENEMY_SPRITE_HEIGHT)) {
			hitEnemyPlane(game, 0);
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

	if (game->enemyPlane.active &&
		game->enemyPlaneDamageState == ENEMY_PLANE_DAMAGE_NORMAL &&
		game->respawnSafeTimer == 0 &&
		rectsOverlap(game->playerX, game->playerY, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT,
			game->enemyPlane.x, game->enemyPlane.y, ENEMY_SPRITE_WIDTH, ENEMY_SPRITE_HEIGHT)) {
		WORD impactX = game->playerX;
		WORD impactY = game->playerY;
		game->enemyPlane.active = 0;
		game->enemyRespawnTimer = enemyRespawnFramesForSkill(game->levelDifficulty);
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_PLAYER_COLLISION,
			AIRCRAFT_FAILURE_CAUSE_AIRCRAFT,
			(UWORD)(((LONG)game->scrollX + game->playerX) >> 3), game,
			(UWORD)game->enemyPlane.y);
		/* Real CPC: every collision object except flak is instant death
		 * (checkplayeragainstobjectmap/planehitbyobject, :7525-7544/:8127-8132)
		 * - no graduated damage points for enemy-plane contact. */
		startAircraftFailure(game, AIRCRAFT_FAILURE_CAUSE_AIRCRAFT);
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
		telemetryLogGameEvent(TELEMETRY_GAME_EVENT_PLAYER_MISSILE_HIT,
			AIRCRAFT_FAILURE_CAUSE_MISSILE,
			(UWORD)(((LONG)game->scrollX + game->playerX) >> 3), game,
			(UWORD)game->enemyMissile.y);
		/* Same as enemy-plane contact above - instant death, matching CPC,
		 * regardless of which source fired the missile. */
		startAircraftFailure(game, AIRCRAFT_FAILURE_CAUSE_MISSILE);
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
		WORD powerupCollisionY = game->powerup.logicalY;
		if (game->rocketShot.active &&
			rectsOverlap(game->rocketShot.x, game->rocketShot.y, 16, WEAPON_SPRITE_HEIGHT,
				powerupScreenX, powerupCollisionY, POWERUP_COLLISION_WIDTH, POWERUP_SPRITE_HEIGHT)) {
			game->rocketShot.active = 0;
			destroyPowerup(game, 1);
			startWorldImpact(game, game->rocketShot.x, game->rocketShot.y);
			*weaponChanged = 1;
		}
		if (game->powerup.active && game->bombShot.active &&
			rectsOverlap(game->bombShot.x, game->bombShot.y, 16, WEAPON_SPRITE_HEIGHT,
				powerupScreenX, powerupCollisionY, POWERUP_COLLISION_WIDTH, POWERUP_SPRITE_HEIGHT)) {
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
	if (game->abandonedAircraftActive) {
		buildPlayerSprite(enemySprite, enemyAttachSprite,
			game->playerX, game->playerY);
		return;
	}
	/* Channel 3 is temporarily borrowed by crash part 2.  Hide the enemy's
	 * even half during a crash, but leave the odd buffer alone so
	 * updateCrashPartSprites() remains its sole owner until the crash ends. */
	if (game->crashTimer) {
		if (!game->abandonedAircraftCrash)
			hideHardwareSprite(enemySprite);
		return;
	}
	if (game->enemyPlane.active && game->enemyPlane.x >= 0 &&
		game->enemyPlane.x <= SCREEN_WIDTH - ENEMY_SPRITE_WIDTH) {
		if (game->enemyPlaneDamageState == ENEMY_PLANE_DAMAGE_BROKEN)
			buildEnemyBrokenPlaneSprite(enemySprite, enemyAttachSprite,
				game->enemyPlane.x, game->enemyPlane.y);
		else
			buildEnemyPlaneSprite(enemySprite, enemyAttachSprite,
				game->enemyPlane.x, game->enemyPlane.y);
	}
	else {
		hideHardwareSprite(enemySprite);
		hideHardwareSprite(enemyAttachSprite);
	}
}

static void updateEnemyMissileSprite(UWORD* enemyMissileSprite, const GameState* game) {
	/* Sprint 15.63: the enemy heatseeker is a black playfield BOB. Keep its
	 * former channel-4 allocation hidden so the established Copper/sprite
	 * channel layout does not change in this visibility-only sprint. */
	(void)game;
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
	UBYTE missionNumber,
	UBYTE skillLevel,
	UBYTE gameModeSetting,
	UBYTE wingmanControl,
	UBYTE preserveVisibleWorld) {
	UBYTE effectiveMission = missionNumber ? missionNumber : 1;
	UBYTE levelDifficulty = cpcDifficultyForMission(skillLevel,
		effectiveMission);
	stopAllSfx();
	/* Must be set before initGameState() below, not after: initGameState()
	 * calls resetCpcRandomSequence(), which now generates the land height/
	 * target table immediately (it reads cpcLandSkillLevel for the climb
	 * ceiling) rather than lazily on first render - setting this afterward
	 * would build the table for the previous session's skill level instead
	 * of the one the player just picked at the menu. */
	cpcLandSkillLevel = levelDifficulty;
	cpcLandRouteExtension = (UWORD)(levelDifficulty *
		CPC_LAND_EXTENSION_PER_DIFFICULTY);
	cpcLandProceduralLength = (UWORD)(CPC_LAND_PROCEDURAL_BASE_LENGTH +
		cpcLandRouteExtension);
	initGameState(game);
	game->missionNumber = effectiveMission;
	/* initGameState() establishes the safe mission-1 default before the
	 * caller's campaign mission is known. Select the actual CPC palette phase
	 * now, before buildGameHudCopper() captures these colours. */
	resetCityFade(game);
	game->takeoffState = TAKEOFF_STATE_ROLLING_IN;
	game->scrollX = TAKEOFF_SCROLL_START_PIXELS;
	setTakeoffDeckPosition(game);
	game->skillLevel = skillLevel;
	game->levelDifficulty = levelDifficulty;
	game->gameMode = gameModeSetting == GAME_MODE_CLASSIC ?
		GAME_MODE_CLASSIC : GAME_MODE_ENHANCED;
	/* Session-time seed mirrors the CPC R register's dependence on how long
	 * the player remained in the menu, without coupling Classic spawns to
	 * terrain columns or to Enhanced's radar accumulator. */
	game->classicEnemySpawnRandomState =
		(UWORD)(frameCounter ^ (frameCounter << 7) ^ 0x6d2b);
	if (!game->classicEnemySpawnRandomState)
		game->classicEnemySpawnRandomState = 0x6d2b;
	game->classicEnemySpawnPhase = 0;
	/* Resolve the mode once at session entry. Classic always owns exactly one
	 * aircraft; Enhanced retains the three-aircraft Amiga campaign. */
	game->lives = gameplayStartingAircraft(game);
	game->rocketHeightLock = menuRocketHeightLock;
	game->rocketRangeTiles = menuRocketRangeTiles;
	/* Same reasoning as lives above - initGameState() set a flat 12/6
	 * regardless of skill; ammoForSkill() gives the real CPC's
	 * skill-scaled starting ammo instead. */
	ammoForSkill(levelDifficulty, &game->bombs, &game->rockets);
	game->wingmanControl = wingmanControl <= WINGMAN_CONTROL_PLAYER2 ?
		wingmanControl : WINGMAN_CONTROL_OFF;
	/* CPC movesecondharrierlandingfrigate scrolls the grey second Harrier
	 * with both the start and end carrier specifically when Wingman control
	 * is OFF. CPU/Player 2 modes also begin with it parked, then remove the
	 * baked copy at takeoff. Thus every new mission starts with the deck
	 * aircraft visible; only an actual Wingman launch clears it. */
	carrierParkedWingmanVisible = 1;
	/* Establish the real deck coordinates immediately. Previously these were
	 * assigned only when Player 1 reached TAKEOFF_STATE_AIRBORNE. Player 2 can
	 * press Up while the carrier is already waiting in READY, so that earlier
	 * input activated the Wingman from initGameState()'s zeroed (0,0)
	 * coordinates and made it appear to be missing from the opening. */
	if (game->wingmanControl != WINGMAN_CONTROL_OFF) {
		game->wingman.mode = WINGMAN_ON_DECK;
		game->wingman.interceptScreenX = WINGMAN_TAKEOFF_DECK_X;
		game->wingman.screenY = WINGMAN_TAKEOFF_DECK_Y;
		game->wingman.row = WINGMAN_TAKEOFF_DECK_Y / GAME_TILE_HEIGHT;
		/* A retained post-landing bitmap contains the carrier without its
		 * parked aircraft: that Wingman landed as a hardware sprite and was
		 * never baked into the world. Keep it live across the mission reset so
		 * it remains visible on deck until its next real takeoff. A fresh game
		 * still uses the cheaper parked carrier composite. */
		if (preserveVisibleWorld) {
			game->wingman.active = 1;
			carrierParkedWingmanVisible = 0;
		}
	}
	*activeWorldBuffer = 0;
	resetBombShotPixelBobFootprints();
	resetRocketShotPixelBobFootprints();
	resetAircraftFailureSmoke();
	resetCarrierAmbienceVisuals();
	bombImpactBobFootprintValid = 0;
	powerupBobFootprintValid = 0;
	if (!preserveVisibleWorld)
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
	updateCrashPartSprites(crashPart1Sprite, enemySprite, enemyAttachSprite,
		game);
	updateEnemySprite(enemySprite, enemyAttachSprite, game);
	updateEnemyMissileSprite(enemyMissileSprite, game);
	updateWingmanSprite(wingmanSprite, unusedSprite7, game);
	hideHardwareSprite(unusedSprite7);
	UWORD displayScrollX = displayScrollXForGameState(game);
	buildGameHudCopper(copper, worldBuffers[*activeWorldBuffer], hudBuffer, (const UWORD*)gamePalette,
		scrollDelayForBplcon1(displayScrollX), displayByteOffsetForGameState(game),
		playerSprite, playerAttachSprite, crashPart1Sprite, enemyAttachSprite, enemySprite, enemyMissileSprite, wingmanSprite, unusedSprite7);
	custom->copjmp1 = 0x7fff;
}

/* Sprint 15.61: the 320x200 loading bitmap is deliberately a DOS asset, not
 * an EMBED_CHIP object. Loading it straight into the final display buffer
 * releases 40,000 bytes of chip RAM for the entire menu/game lifetime and
 * avoids a second staging copy. The relative path is the normal CLI/ADF
 * install; DH1 and DF0 cover the Bartman F5 mount and a bootable floppy when
 * their startup directory is not inherited on Kickstart 1.3. */
static UBYTE loadEarlyLoadingScreen(UBYTE* screenBuffer) {
	static const char* const paths[] = {
		"loading_screen.bpl",
		"DH1:loading_screen.bpl",
		"DF0:loading_screen.bpl"
	};
	const ULONG destinationRowBytes = SCREEN_PLANES * SCREEN_ROW_BYTES;
	const ULONG topRow = (SCREEN_HEIGHT - LOADING_SCREEN_HEIGHT) / 2;
	UBYTE* destination = screenBuffer + topRow * destinationRowBytes;
	APTR oldWindowPtr = suppressDosRequesters();

	memset(screenBuffer, 0, SCREEN_BITMAP_BYTES);
	for (UBYTE pathIndex = 0;
		pathIndex < (UBYTE)(sizeof(paths) / sizeof(paths[0])); pathIndex++) {
		BPTR file = Open((CONST_STRPTR)paths[pathIndex], MODE_OLDFILE);
		if (!file)
			continue;

		LONG bytesRead = Read(file, destination, LOADING_SCREEN_BYTES);
		Close(file);
		if (bytesRead == LOADING_SCREEN_BYTES) {
			restoreDosRequesters(oldWindowPtr);
			KPrintF("Loading screen: %s (%ld bytes)\n", paths[pathIndex],
				bytesRead);
			return 1;
		}
		KPrintF("Loading screen short read: %s (%ld/%ld)\n",
			paths[pathIndex], bytesRead, (LONG)LOADING_SCREEN_BYTES);
		memset(screenBuffer, 0, SCREEN_BITMAP_BYTES);
	}
	restoreDosRequesters(oldWindowPtr);

	/* Missing media must never make a KS1.3 build abort. This uses the
	 * embedded CPC font and the retained loading palette only. */
	drawTextCentered(screenBuffer, (SCREEN_HEIGHT - FONT_HEIGHT) / 2,
		"LOADING...", 11);
	KPrintF("Loading screen missing; using built-in fallback\n");
	return 0;
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

#if HAR_HEADLESS_CLASSIC_CONTRACT_TEST
	{
		int contractResult = runClassicGameplayContractTest();
		CloseLibrary((struct Library*)DOSBase);
		CloseLibrary((struct Library*)GfxBase);
		return contractResult;
	}
#endif

#if HAR_HIGHSCORE_DISK_IO
	loadHighScoreTable();
#else
	resetHighScoreTableToDefaults();
#endif

	KPrintF("Harrier Attack Reloaded Amiga " HAR_BUILD_LABEL "\n");
	Write(Output(), (APTR)"Harrier Amiga flow sprint\n", 26);
#if HAR_DEBUG_PERF_LOG
	perfLogOpen();
#endif

	/* Allocate only what is required to display the loading page before DOS
	 * access ends and the custom chipset is taken over. */
	USHORT* copper = (USHORT*)AllocMem(COPPER_BYTES, MEMF_CHIP | MEMF_CLEAR);
	UBYTE* screenBuffer = (UBYTE*)AllocMem(SCREEN_BITMAP_BYTES,
		MEMF_CHIP | MEMF_CLEAR);
	UWORD* nullSprite = (UWORD*)AllocMem(2 * sizeof(UWORD),
		MEMF_CHIP | MEMF_CLEAR);
	if (!copper || !screenBuffer || !nullSprite) {
		if (copper)
			FreeMem(copper, COPPER_BYTES);
		if (screenBuffer)
			FreeMem(screenBuffer, SCREEN_BITMAP_BYTES);
		if (nullSprite)
			FreeMem(nullSprite, 2 * sizeof(UWORD));
		CloseLibrary((struct Library*)DOSBase);
		CloseLibrary((struct Library*)GfxBase);
		Exit(0);
	}

	loadEarlyLoadingScreen(screenBuffer);
	TakeSystem();
	buildDisplayCopper(copper, screenBuffer, (const UWORD*)loadingPalette,
		nullSprite);
	custom->cop1lc = (ULONG)copper;
	custom->dmacon = DMAF_BLITTER;
	custom->copjmp1 = 0x7fff;
	custom->dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_RASTER | DMAF_COPPER |
		DMAF_BLITTER | DMAF_SPRITE;

	/* The loading bitmap is now visible. Allocate and initialise the larger
	 * runtime resources while Copper/raster DMA keeps that page on screen. */
	menuTickerBitmap = (UBYTE*)AllocMem(MENU_TICKER_BITMAP_BYTES,
		MEMF_CHIP | MEMF_CLEAR);
	UBYTE* worldBuffers[GAME_WORLD_BUFFER_COUNT];
	worldBuffers[0] = (UBYTE*)AllocMem(GAME_WORLD_BITMAP_BYTES,
		MEMF_CHIP | MEMF_CLEAR);
	UBYTE* hudBuffer = (UBYTE*)AllocMem(HUD_BITMAP_BYTES,
		MEMF_CHIP | MEMF_CLEAR);
	UWORD* playerSprite = (UWORD*)AllocMem(PLAYER_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* playerAttachSprite = (UWORD*)AllocMem(PLAYER_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* crashPart1Sprite = (UWORD*)AllocMem(AUXILIARY_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* enemyAttachSprite = (UWORD*)AllocMem(AUXILIARY_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* enemySprite = (UWORD*)AllocMem(ENEMY_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* enemyMissileSprite = (UWORD*)AllocMem(ENEMY_MISSILE_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* wingmanSprite = (UWORD*)AllocMem(PLAYER_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	UWORD* unusedSprite7 = (UWORD*)AllocMem(PLAYER_SPRITE_WORDS * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
	engineBuffer = (UBYTE*)AllocMem(ENGINE_BUFFER_BYTES, MEMF_CHIP | MEMF_CLEAR);
	seaAmbienceBuffer = (UBYTE*)AllocMem(SEA_AMBIENCE_BUFFER_BYTES,
		MEMF_CHIP | MEMF_CLEAR);
	carrierIdleDecodeBuffer = (UBYTE*)AllocMem(
		CARRIER_IDLE_DECODE_BUFFER_BYTES, MEMF_CHIP | MEMF_CLEAR);
	telemetrySamples = (TelemetrySample*)AllocMem(sizeof(TelemetrySample) * TELEMETRY_SAMPLE_COUNT, MEMF_FAST | MEMF_CLEAR);
	if (!telemetrySamples && AvailMem(MEMF_PUBLIC) > sizeof(TelemetrySample) * TELEMETRY_SAMPLE_COUNT + 4096)
		telemetrySamples = (TelemetrySample*)AllocMem(sizeof(TelemetrySample) * TELEMETRY_SAMPLE_COUNT, MEMF_PUBLIC | MEMF_CLEAR);
	telemetryAvailable = telemetrySamples ? 1 : 0;
	telemetryEnabled = 0;
	if (!menuTickerBitmap || !worldBuffers[0] || !hudBuffer || !playerSprite || !playerAttachSprite || !crashPart1Sprite || !enemyAttachSprite || !enemySprite || !enemyMissileSprite || !wingmanSprite || !unusedSprite7 || !engineBuffer || !seaAmbienceBuffer || !carrierIdleDecodeBuffer) {
		FreeSystem();
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
		if (seaAmbienceBuffer)
			FreeMem(seaAmbienceBuffer, SEA_AMBIENCE_BUFFER_BYTES);
		seaAmbienceBuffer = 0;
		if (carrierIdleDecodeBuffer)
			FreeMem(carrierIdleDecodeBuffer,
				CARRIER_IDLE_DECODE_BUFFER_BYTES);
		carrierIdleDecodeBuffer = 0;
		CloseLibrary((struct Library*)DOSBase);
		CloseLibrary((struct Library*)GfxBase);
		Exit(0);
	}
	KPrintF("Chip RAM free after runtime allocation: %ld bytes\n",
		AvailMem(MEMF_CHIP));
	/* Runtime route arrays are writable because the final CPC town block can
	 * extend the route. Do this after the loading page is live; initGameState()
	 * replaces the baseline with the session-specific shifted route. */
	configureRuntimeLevelRoute(0, 0);

	initRingWorldBuffer(worldBuffers[0], 0);
	buildPlayerSprite(playerSprite, playerAttachSprite, PLAYER_START_X, PLAYER_START_Y);
	hideHardwareSprite(crashPart1Sprite);
	hideHardwareSprite(enemyAttachSprite);
	hideHardwareSprite(enemySprite);
	hideHardwareSprite(enemyMissileSprite);

	buildSeaAmbienceBuffer();
	initSfx();

	if (HAR_DEBUG_REGISTER_RESOURCES) {
		debug_register_bitmap(screenBuffer, "screen_buffer.bpl", SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_PLANES, debug_resource_bitmap_interleaved);
		debug_register_bitmap(worldBuffers[0], "world_buffer_0.bpl", GAME_WORLD_BUFFER_WIDTH, GAME_WORLD_HEIGHT, SCREEN_PLANES, debug_resource_bitmap_interleaved);
		debug_register_bitmap(hudBuffer, "hud_buffer.bpl", SCREEN_WIDTH, HUD_HEIGHT, SCREEN_PLANES, debug_resource_bitmap_interleaved);
		debug_register_palette(loadingPalette, "loading_screen.pal", 32, 0);
		debug_register_palette(menuPalette, "menu_palette.pal", 32, 0);
		debug_register_palette(gamePalette, "game_palette.pal", 32, 0);
		debug_register_copperlist(copper, "display_copper", COPPER_BYTES, 0);
	}

	/* VBL timing is deliberately polled by WaitVbl().  Do not enable a CPU
	 * VBL interrupt here: the former handler only incremented frameCounter,
	 * while depending on a valid launcher-provided supervisor stack. */

	InitInput();
	restoreDefaultControlProfiles();
	WaitFramesOrSelect(140);
	WaitForInputRelease();

	short selected = MENU_ITEM_START;
	UBYTE programRunning = 1;
	short skillLevel = 1;
	short gameModeSetting = GAME_MODE_ENHANCED;
	short wingmanControl = WINGMAN_CONTROL_OFF;
	short inGameScene = 0;
	GameState game;
	/* The persisted table is sorted descending; seed the HUD from it instead
	 * of showing zero until the first score event of this process. */
	ULONG highScore = highScoreTable[0].score;
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
	UBYTE debugHubBackArmed = 0;
	UBYTE controlsActive = 0;
	UBYTE controlsPlayer = 0;
	UBYTE controlsSelected = CONTROL_MENU_PLAYER_ROW;
	UBYTE controlsCapture = 0;
	UBYTE controlsMessage = CONTROL_MESSAGE_NONE;
	UBYTE controlsMessageTimer = 0;
	UWORD controlsCaptureSerial = 0;
	UBYTE fieldGuideActive = 0;
	UBYTE menuTickerFinished = 0;
	UBYTE debugHubSelected = DEBUG_ITEM_TELEMETRY;
	UWORD debugGraphicIndex = 0;
	UBYTE debugSoundIndex = 0;
	UBYTE debugMusicIndex = 0;
	initGameState(&game);
	InputState input;
	InputState previousInput;
	Player2InputState input2;
	Player2InputState previousInput2;
	UBYTE lastInputMask = 0xff;

	memset(&input2, 0, sizeof(input2));
	previousInput2 = input2;
	ReadInput(&input, 0);
	previousInput = input;
	drawMenuScreen(screenBuffer, selected, skillLevel, gameModeSetting, wingmanControl, highScore);
	drawTelemetryMenuIndicator(screenBuffer);
	drawInputDebugIfEnabled(screenBuffer, &input, 102, MENU_COLOR_PANEL);
	lastInputMask = InputMask(&input);
	buildMenuCopper(copper, screenBuffer, menuTickerBitmap, menuPalette,
		nullSprite);
	startModMusic();

	while (programRunning) {
		WaitVbl();
		menuTickerFinished = 0;
		serviceModMusicToCurrentVbl();
		updateSfx();
		UBYTE carrierAmbienceEligible =
			inGameScene && !telemetryStatsPaused && !gamePaused &&
			!modPlaying && !game.gameOver &&
			(game.takeoffState == TAKEOFF_STATE_READY ||
			 game.missionComplete);
		updateCarrierIdleSfx(carrierAmbienceEligible);
		updateSeaAmbience(seaAmbienceTargetForGame(&game,
			carrierAmbienceEligible,
			(UBYTE)(inGameScene && !telemetryStatsPaused && !gamePaused)));
		if (inGameScene && !telemetryStatsPaused && !gamePaused)
			updateCarrierGulls(&game, carrierAmbienceEligible, 1);
		else if (!inGameScene)
			resetCarrierAmbienceVisuals();
		if (!inGameScene && !telemetryStatsPaused && !controlsActive &&
			debugHubPage == DEBUG_HUB_CLOSED)
			menuTickerFinished = updateMenuTicker();
		if (inGameScene && !telemetryStatsPaused && !gamePaused) {
			if (pendingGameScrollCopperUpdate) {
				updateGameScrollCopper(worldBuffers[activeWorldBuffer], &game);
				pendingGameScrollCopperUpdate = 0;
			}
		}
		previousInput = input;
		ReadInput(&input, (UBYTE)(inGameScene &&
			game.wingmanControl == WINGMAN_CONTROL_PLAYER2 &&
			(game.gameMode == GAME_MODE_CLASSIC ||
			 controlProfiles[1].joystickPort == CONTROL_JOY_PORT_1)));
		previousInput2 = input2;
		if (inGameScene && game.wingmanControl == WINGMAN_CONTROL_PLAYER2)
			ReadPlayer2Input(&input2, game.gameMode);
		else
			memset(&input2, 0, sizeof(input2));
#if HAR_HEADLESS_AUTOPLAY
		{
			static UBYTE headlessStartSent = 0;
			static UBYTE headlessUpSent = 0;
			static UBYTE headlessTelemetryReady = 0;
			static UWORD headlessFinalCarrierFrame = 0;
#if HAR_HEADLESS_PAUSE_TEST
			static UBYTE headlessPauseStage = 0;
			static UWORD headlessPauseFrame = 0;
#endif
#if HAR_HEADLESS_HIGHSCORE_TEST
			static UBYTE headlessHighScoreTriggered = 0;
			static UBYTE headlessHighScoreRetrySent = 0;
#endif
			debugInfiniteLives = 1;
			debugInfiniteFuel = 1;
#if HAR_HEADLESS_WEAPON_STRESS
			debugInfiniteBombs = 1;
			debugInfiniteRockets = 1;
#endif
			if (!headlessStartSent) {
				skillLevel = HAR_HEADLESS_SKILL_LEVEL;
				gameModeSetting = HAR_HEADLESS_GAME_MODE;
				wingmanControl = HAR_HEADLESS_WINGMAN_CONTROL;
				/* perf_log and parity_log are the headless telemetry. Keep the
				 * optional in-game 100-frame sample ring disabled so the observer
				 * cannot perturb the cycle-exact performance result. */
				telemetryEnabled = 0;
				if (!headlessTelemetryReady) {
					telemetryReset();
					headlessTelemetryReady = 1;
				}
			}
			if (!headlessStartSent && !inGameScene && frameCounter > 100) {
				input.select = 1;
				headlessStartSent = 1;
			} else if (headlessStartSent && !headlessUpSent && inGameScene && game.takeoffState == TAKEOFF_STATE_READY) {
				input.up = 1;
				headlessUpSent = 1;
			}
			if (headlessUpSent && game.takeoffState == TAKEOFF_STATE_AIRBORNE) {
				/* The normal parity run climbs to the ceiling.  The optional
				 * enemy-plane profile instead alternates between two absolute
				 * flight levels while an attacker is active.  This gives the
				 * tracking AI repeatable vertical work without changing normal
				 * gameplay or the reference full-map run. */
#if HAR_HEADLESS_WINGMAN_FORMATION_EXERCISE
				/* Sweep between two safe flight levels on a fixed cadence.  The
				 * player also accelerates normally below, so the Wingman must use
				 * the complete CPC 0..8 direction set rather than only vertical
				 * corrections. */
				{
					const WORD desiredY = ((frameCounter / 96) & 1) ? 24 : 88;
					if (game.playerY > desiredY + 1)
						input.up = 1;
					else if (game.playerY < desiredY - 1)
						input.down = 1;
				}
#elif HAR_HEADLESS_ENEMY_PLANE_EXERCISE
				if (game.enemyPlane.active) {
					const WORD desiredY =
						((game.enemyPlane.timer / 24) & 1) ? 16 : 72;
					if (game.playerY > desiredY + 1)
						input.up = 1;
					else if (game.playerY < desiredY - 1)
						input.down = 1;
				} else {
					input.up = 1;
				}
#else
				input.up = 1;
#endif
				/* Exercise the full-speed renderer and finish a complete map in
				 * practical cycle-exact test time. Stop accelerating before the
				 * carrier's own slowdown state takes control. */
				if (game.scrollX < LANDING_APPROACH_SCROLL_X &&
					game.speedLevel < HAR_HEADLESS_CRUISE_SPEED)
					input.right = 1;
#if HAR_HEADLESS_WEAPON_STRESS
				/* Weapons are release-to-rearm in the real game. Alternate an
				 * asserted and released frame so both players produce a valid
				 * stream of fresh presses whenever their previous shot is free.
				 * P2 also holds Up to leave the deck immediately and remain clear
				 * of the terrain during the full-route renderer stress test. */
				if ((frameCounter & 1) == 0) {
					input.fire = 1;
					input.bomb = 1;
					input2.fire = 1;
					input2.bomb = 1;
				}
				input2.up = 1;
#endif
			}
#if HAR_HEADLESS_PAUSE_TEST
			/* Exercise the real pause entry and resume branches during a Player-2
			 * run. P is asserted for one frame, released while paused, then asserted
			 * again after twenty VBlanks. Reaching the normal route result proves
			 * that HUD overlay drawing, audio shutdown and input re-arming returned. */
			if (!headlessPauseStage && inGameScene &&
				game.takeoffState == TAKEOFF_STATE_AIRBORNE && frameCounter > 350) {
				input.p = 1;
				headlessPauseStage = 1;
				headlessPauseFrame = frameCounter;
			} else if (headlessPauseStage == 1 && gamePaused &&
				(UWORD)(frameCounter - headlessPauseFrame) >= 20) {
				input.p = 1;
				headlessPauseStage = 2;
			}
#endif
#if HAR_HEADLESS_HIGHSCORE_TEST
			/* Test-only, deterministic persistence exercise: create one completed
			 * run and press Fire once. Persistence is deliberately deferred to the
			 * normal, one-way Exit-to-DOS teardown; returning to AmigaOS and taking
			 * the custom display back inside one C call chain proved unsafe on a
			 * stock 68000. Release builds compile this entire block out. */
			if (!headlessHighScoreTriggered && inGameScene &&
				frameCounter > 300) {
				game.bonusScore = 1234;
				game.score = 1234;
				triggerGameOver(&game);
				updateHighScore(&highScore, &game);
				game.highScoreCommitted = 1;
				headlessHighScoreTriggered = 1;
			} else if (headlessHighScoreTriggered && game.gameOver &&
				game.highScoreCommitted && !headlessHighScoreRetrySent) {
				input.select = 1;
				headlessHighScoreRetrySent = 1;
			} else if (headlessHighScoreRetrySent && !game.gameOver) {
				break;
			}
#endif
			if (!headlessFinalCarrierFrame &&
				(game.landingState == LANDING_STATE_HOVER ||
				 game.scrollX >= LANDING_HOVER_SCROLL_X))
				headlessFinalCarrierFrame = frameCounter;
			if ((headlessFinalCarrierFrame &&
				 (UWORD)(frameCounter - headlessFinalCarrierFrame) >= 50) ||
				frameCounter > HAR_HEADLESS_MAX_FRAMES)
				break;
		}
#endif
		UBYTE inputMask = InputMask(&input);

		/* One complete ticker pass now drives the attract-mode transition.
		 * This follows what the player can actually see: main menu -> guide
		 * after the tribute's last glyph leaves, guide -> menu after the
		 * gameplay/rules text leaves. Input still dismisses the guide. */
		if (!inGameScene && !gamePaused && !telemetryStatsPaused && !controlsActive &&
			debugHubPage == DEBUG_HUB_CLOSED) {
			if (fieldGuideActive) {
				if (input.any || menuTickerFinished) {
					fieldGuideActive = 0;
					drawMenuScreen(screenBuffer, selected, skillLevel,
						gameModeSetting, wingmanControl, highScore);
					drawTelemetryMenuIndicator(screenBuffer);
					drawInputDebugIfEnabled(screenBuffer, &input, 102,
						MENU_COLOR_PANEL);
					buildMenuCopper(copper, screenBuffer, menuTickerBitmap,
						menuPalette, nullSprite);
					custom->copjmp1 = 0x7fff;
					lastInputMask = inputMask;
				}
				continue;
			}
			if (menuTickerFinished) {
				fieldGuideActive = 1;
				drawFieldGuideScreen(screenBuffer, gameModeSetting);
				buildMenuCopper(copper, screenBuffer, menuTickerBitmap, menuPalette,
					nullSprite);
				custom->copjmp1 = 0x7fff;
				continue;
			}
		}

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
				/* A CPC high-score row must never be left half-written. Escape
				 * accepts the current text (or PLAYER when still empty) before
				 * performing its normal return-to-menu transition. */
				if (game.highScoreNameEntryActive)
					commitHighScoreNameEntry(&highScore, &game);
				/* A completed run may leave high-score data dirty in RAM. Do not
				 * briefly restore and retake AmigaOS here: that nested ownership
				 * transition can invalidate the 68000 supervisor stack. The hardened
				 * writer runs once during the one-way Exit-to-DOS teardown. */
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
					gameModeSetting, wingmanControl, highScore);
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

		if (!inGameScene && !controlsActive &&
			debugHubPage == DEBUG_HUB_CLOSED &&
			(inputMask != lastInputMask ||
			 input.lastRawKey != previousInput.lastRawKey)) {
			drawInputDebugIfEnabled(screenBuffer, &input, 102, MENU_COLOR_PANEL);
			lastInputMask = inputMask;
		}

		if (telemetryStatsPaused) {
			if (Pressed(input.left, previousInput.left) ||
				Pressed(input.right, previousInput.right)) {
				telemetryStatsPage ^= 1;
				if (telemetryStatsPage)
					drawTelemetryGameEventsScreen(screenBuffer);
				else
					drawTelemetryStatsScreen(screenBuffer);
			} else if (Pressed(input.r, previousInput.r)) {
				telemetryReset();
				if (telemetryStatsPage)
					drawTelemetryGameEventsScreen(screenBuffer);
				else
					drawTelemetryStatsScreen(screenBuffer);
			} else if (Pressed(input.space, previousInput.space)) {
				telemetryStatsPaused = 0;
				UWORD displayScrollX = displayScrollXForGameState(&game);
				buildGameHudCopper(copper, worldBuffers[activeWorldBuffer], hudBuffer, (const UWORD*)gamePalette,
					scrollDelayForBplcon1(displayScrollX), displayByteOffsetForGameState(&game),
					playerSprite, playerAttachSprite, crashPart1Sprite, enemyAttachSprite, enemySprite, enemyMissileSprite, wingmanSprite, unusedSprite7);
				custom->copjmp1 = 0x7fff;
				if (!game.gameOver && game.takeoffState == TAKEOFF_STATE_AIRBORNE &&
					!game.crashTimer && !game.ejectState &&
					game.aircraftFailureState == AIRCRAFT_FAILURE_NONE)
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
					!game.crashTimer && !game.ejectState &&
					game.aircraftFailureState == AIRCRAFT_FAILURE_NONE)
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
			if (controlsActive) {
				if (controlsMessageTimer) {
					controlsMessageTimer--;
					if (!controlsMessageTimer) {
						controlsMessage = CONTROL_MESSAGE_NONE;
						drawControlsRow(screenBuffer, controlsPlayer,
							controlsSelected, 1, controlsCapture,
							controlsMessage);
					}
				}
				if (Pressed(input.cancel, previousInput.cancel)) {
					if (controlsCapture) {
						controlsCapture = 0;
						controlsMessage = CONTROL_MESSAGE_NONE;
						drawControlsRow(screenBuffer, controlsPlayer,
							controlsSelected, 1, 0, controlsMessage);
					} else {
						controlsActive = 0;
						drawMenuScreen(screenBuffer, selected, skillLevel,
							gameModeSetting, wingmanControl, highScore);
						drawTelemetryMenuIndicator(screenBuffer);
						buildMenuCopper(copper, screenBuffer,
							menuTickerBitmap, menuPalette, nullSprite);
						custom->copjmp1 = 0x7fff;
					}
				} else if (controlsCapture) {
					if (keyboardMakeSerial != controlsCaptureSerial) {
						UBYTE action = (UBYTE)(controlsSelected -
							CONTROL_MENU_ACTION_FIRST);
						UBYTE rawKey = lastKeyboardMakeKey;
						controlsCaptureSerial = keyboardMakeSerial;
						controlsCapture = 0;
						if (rawKey == RAWKEY_ESCAPE) {
							controlsMessage = CONTROL_MESSAGE_NONE;
						} else if (controlKeyIsDuplicate(controlsPlayer,
							action, rawKey)) {
							controlsMessage = CONTROL_MESSAGE_DUPLICATE;
							controlsMessageTimer = 50;
						} else {
							controlProfiles[controlsPlayer].key[action] = rawKey;
							controlsMessage = CONTROL_MESSAGE_NONE;
						}
						drawControlsRow(screenBuffer, controlsPlayer,
							controlsSelected, 1, controlsCapture,
							controlsMessage);
					}
				} else {
					if (Pressed(input.menuPrev, previousInput.menuPrev) ||
						Pressed(input.menuNext, previousInput.menuNext)) {
						UBYTE oldControlsSelected = controlsSelected;
						if (Pressed(input.menuPrev, previousInput.menuPrev))
							controlsSelected = (UBYTE)((controlsSelected +
								CONTROL_MENU_ROW_COUNT - 1) %
								CONTROL_MENU_ROW_COUNT);
						else
							controlsSelected = (UBYTE)((controlsSelected + 1) %
								CONTROL_MENU_ROW_COUNT);
						if (controlsMessage != CONTROL_MESSAGE_NONE) {
							controlsMessage = CONTROL_MESSAGE_NONE;
							controlsMessageTimer = 0;
							drawControlsRow(screenBuffer, controlsPlayer,
								oldControlsSelected, 0, 0,
								CONTROL_MESSAGE_NONE);
							drawControlsRow(screenBuffer, controlsPlayer,
								controlsSelected, 1, 0,
								CONTROL_MESSAGE_NONE);
						} else {
							drawControlsCursor(screenBuffer,
								oldControlsSelected, 0);
							drawControlsCursor(screenBuffer,
								controlsSelected, 1);
						}
					}

					if (Pressed(input.left, previousInput.left) ||
						Pressed(input.right, previousInput.right)) {
						short direction = Pressed(input.left,
							previousInput.left) ? -1 : 1;
						if (controlsSelected == CONTROL_MENU_PLAYER_ROW &&
							gameModeSetting != GAME_MODE_CLASSIC) {
							controlsPlayer ^= 1;
							controlsMessage = CONTROL_MESSAGE_NONE;
						} else {
							adjustControlOption(controlsPlayer,
								controlsSelected, direction, &controlsMessage);
							if (controlsMessage)
								controlsMessageTimer = 50;
						}
						if (controlsSelected == CONTROL_MENU_PLAYER_ROW)
							drawControlsScreen(screenBuffer, controlsPlayer,
								controlsSelected, 0, controlsMessage);
						else
							drawControlsRow(screenBuffer, controlsPlayer,
								controlsSelected, 1, 0, controlsMessage);
					}

					if (Pressed(input.select, previousInput.select)) {
						if (controlsSelected == CONTROL_MENU_PLAYER_ROW &&
							gameModeSetting != GAME_MODE_CLASSIC) {
							controlsPlayer ^= 1;
							drawControlsScreen(screenBuffer, controlsPlayer,
								controlsSelected, 0, CONTROL_MESSAGE_NONE);
						} else if (controlsSelected >=
							CONTROL_MENU_ACTION_FIRST &&
							controlsSelected < CONTROL_MENU_JOYSTICK_ROW) {
							controlsCapture = 1;
							controlsCaptureSerial = keyboardMakeSerial;
							controlsMessage = CONTROL_MESSAGE_NONE;
							drawControlsRow(screenBuffer, controlsPlayer,
								controlsSelected, 1, 1, controlsMessage);
						} else if (controlsSelected ==
							CONTROL_MENU_DEFAULTS_ROW) {
							restoreDefaultControlProfiles();
							controlsMessage = CONTROL_MESSAGE_NONE;
							/* All displayed bindings may have changed. */
							drawControlsScreen(screenBuffer, controlsPlayer,
								controlsSelected, 0, controlsMessage);
						} else if (controlsSelected == CONTROL_MENU_BACK_ROW) {
							controlsActive = 0;
							drawMenuScreen(screenBuffer, selected, skillLevel,
								gameModeSetting, wingmanControl, highScore);
							drawTelemetryMenuIndicator(screenBuffer);
							buildMenuCopper(copper, screenBuffer,
								menuTickerBitmap, menuPalette, nullSprite);
							custom->copjmp1 = 0x7fff;
						} else {
							adjustControlOption(controlsPlayer,
								controlsSelected, 1, &controlsMessage);
							if (controlsMessage)
								controlsMessageTimer = 50;
							drawControlsRow(screenBuffer, controlsPlayer,
								controlsSelected, 1, 0, controlsMessage);
						}
					}
				}
			} else if (debugHubPage != DEBUG_HUB_CLOSED) {
				/* Enter and exit share Shift+D. Require a complete release after
				 * opening so the entry make-code (or a stale Escape level) cannot
				 * close the modal hub on its first displayed frame. */
				if (!debugHubBackArmed && !input.cancel &&
					!(input.shift && input.d))
					debugHubBackArmed = 1;
				UBYTE debugBack = debugHubBackArmed && (input.cancel ||
					(input.shift && Pressed(input.d, previousInput.d)));
				if (debugBack) {
					if (debugHubPage == DEBUG_HUB_OPTIONS) {
						debugHubPage = DEBUG_HUB_CLOSED;
						drawMenuScreen(screenBuffer, selected, skillLevel,
							gameModeSetting, wingmanControl, highScore);
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
									skillLevel, gameModeSetting,
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
						stopAllSfx();
						debugSoundIndex = debugSoundIndex == 0
							? SFX_COUNT - 1
							: (UBYTE)(debugSoundIndex - 1);
						drawDebugSoundBrowser(screenBuffer,
							debugSoundIndex, 0);
					} else if (Pressed(input.right, previousInput.right)) {
						stopAllSfx();
						debugSoundIndex =
							(UBYTE)((debugSoundIndex + 1) % SFX_COUNT);
						drawDebugSoundBrowser(screenBuffer,
							debugSoundIndex, 0);
					} else if (Pressed(input.select, previousInput.select)) {
						/* Audition one exact master at a time. Without this reset,
						 * repeated Fire presses allocated the same sample to another
						 * Paula voice while the first was still playing. The phase-
						 * shifted overlap changed its tone on every press and made a
						 * valid conversion sound corrupt. stopAllSfx also clears the
						 * retrigger guard, so this is a deterministic restart. */
						stopAllSfx();
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
				debugHubBackArmed = 0;
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
						skillLevel, gameModeSetting, wingmanControl);
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
						selected, direction, &skillLevel, &gameModeSetting,
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
							highScore, 1, (UBYTE)skillLevel,
							(UBYTE)gameModeSetting,
							(UBYTE)wingmanControl, 0);
						if (telemetryEnabled)
							telemetryReset();
						lastInputMask = inputMask;
						inGameScene = 1;
						gameCancelArmed = 0;
						gamePaused = 0;
					} else if (selected == MENU_ITEM_CONTROLS) {
						controlsActive = 1;
						/* Classic P2 uses CPC's fixed keypad/joystick-port-1
						 * controls; only Enhanced exposes P2 rebinding. */
						if (gameModeSetting == GAME_MODE_CLASSIC)
							controlsPlayer = 0;
						controlsSelected = CONTROL_MENU_PLAYER_ROW;
						controlsCapture = 0;
						controlsMessage = CONTROL_MESSAGE_NONE;
						controlsMessageTimer = 0;
						drawControlsScreen(screenBuffer, controlsPlayer,
							controlsSelected, 0, controlsMessage);
						buildDisplayCopper(copper, screenBuffer,
							menuPalette, nullSprite);
						custom->copjmp1 = 0x7fff;
					} else if (selected == MENU_ITEM_EXIT_DOS) {
						/* Leave through main()'s single cleanup path so AmigaOS
						 * receives its Copper, interrupts, audio and memory back. */
						stopModMusic();
						programRunning = 0;
					} else if (adjustSelectedMenuOption(screenBuffer,
							selected, 1, &skillLevel, &gameModeSetting,
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
				telemetryStatsPage = 0;
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
					/* Player 2 must be able to leave the deck during the opening
					 * READY phase, not only after Player 1 has completed the whole
					 * lift. This also swaps the parked carrier composite for the
					 * live Wingman sprite at the moment P2 presses Up. */
					if (game.wingmanControl == WINGMAN_CONTROL_PLAYER2) {
						updateWingmanPlayer2Control(&game, worldBuffers,
							&input2, &previousInput2, &hudDirty);
						if (game.wingman.active)
							pendingWingmanSpriteUpdate = 1;
					}
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
				if (game.aircraftFailureState == AIRCRAFT_FAILURE_DESCENT) {
					UWORD oldFailureScrollX = game.scrollX;
					WORD oldFailurePlayerX = game.playerX;
					WORD oldFailurePlayerY = game.playerY;
					/* Failure is itself the one-shot gate. Accept E even when it was
					 * pressed on the fatal-hit frame; requiring a new key edge here
					 * made the useful pre-impact eject window appear unresponsive. */
					if (input.eject && !input.cancel)
						startPlayerEject(&game);
					if (game.aircraftFailureState == AIRCRAFT_FAILURE_DESCENT)
						updateAircraftFailure(&game, &input);
					if (game.scrollX != oldFailureScrollX) {
						pendingGameScrollCopperUpdate = 1;
						if (updateHudValues(&game))
							hudDirty = 1;
					}
					if (game.playerX != oldFailurePlayerX ||
						game.playerY != oldFailurePlayerY)
						pendingPlayerSpriteUpdate = 1;
					if (game.ejectState || game.crashTimer) {
						pendingPlayerSpriteUpdate = 1;
						pendingCrashSpriteUpdate = 1;
						pendingEnemySpriteUpdate = 1;
					}
					if (hudDirty) {
						drawHudValues(hudBuffer, &game, highScore, 0);
						hudDirty = 0;
					}
				} else if (game.ejectState) {
					if (updateAbandonedAircraft(&game)) {
						pendingEnemySpriteUpdate = 1;
						if (game.crashTimer)
							pendingCrashSpriteUpdate = 1;
					}
					/* The abandoned aircraft's wreck animation runs alongside the
					 * independent seat/parachute state. It has no authority to consume
					 * an aircraft or enter Game Over. */
					if (game.crashTimer && updatePlayerCrash(&game)) {
						hudDirty = 1;
						pendingCrashSpriteUpdate = 1;
						pendingEnemySpriteUpdate = 1;
						pendingEnemyMissileSpriteUpdate = 1;
					}
					UBYTE ejectUpdate = updatePlayerEject(&game);
					if (ejectUpdate) {
						hudDirty = 1;
						pendingPlayerSpriteUpdate = 1;
						pendingWingmanSpriteUpdate = 1;
					}
					if (ejectUpdate == EJECT_UPDATE_CARRIER_RESTART) {
						/* Rescue restarts the carrier sequence without resetting the
						 * sortie's score or damage map.  startGameSession gives us one
						 * canonical actor/audio/display reset; the run-persistent CPC
						 * state is restored before rebuilding the ring buffer. */
						ULONG rescuedScore = game.score;
						ULONG rescuedBonusScore = game.bonusScore;
						ULONG rescuedMissionStartScore = game.missionStartScore;
						UWORD rescuedHits = game.hitsCount;
						UBYTE rescuedLives = game.lives;
						UBYTE rescuedGameMode = game.gameMode;
						UBYTE rescuedMissionNumber = game.missionNumber;
						UBYTE rescuedExtraAircraftBonusSpawned =
							game.extraAircraftBonusSpawned;
						UBYTE rescuedWingmanDestroyed = game.wingman.destroyed;
						UBYTE rescuedTargetCount = destroyedTargetCount;
						UBYTE rescuedShipCellCount = destroyedShipCellCount;
						UBYTE rescuedCraterCount = landCraterCount;
						UWORD rescuedTargetColumns[GAME_DESTROYED_TARGET_MAX];
						UWORD rescuedShipColumns[GAME_DESTROYED_SHIP_CELL_MAX];
						UBYTE rescuedShipRows[GAME_DESTROYED_SHIP_CELL_MAX];
						UWORD rescuedCraterColumns[GAME_LAND_CRATER_MAX];
						UBYTE rescuedCraterRows[GAME_LAND_CRATER_MAX];
						memcpy(rescuedTargetColumns, destroyedTargetColumns,
							sizeof(rescuedTargetColumns));
						memcpy(rescuedShipColumns, destroyedShipCellColumns,
							sizeof(rescuedShipColumns));
						memcpy(rescuedShipRows, destroyedShipCellRows,
							sizeof(rescuedShipRows));
						memcpy(rescuedCraterColumns, landCraterColumns,
							sizeof(rescuedCraterColumns));
						memcpy(rescuedCraterRows, landCraterRows,
							sizeof(rescuedCraterRows));

						startGameSession(&game, copper, worldBuffers,
							&activeWorldBuffer, hudBuffer, playerSprite,
							playerAttachSprite, crashPart1Sprite, enemyAttachSprite,
							enemySprite, enemyMissileSprite, wingmanSprite,
							unusedSprite7, &pendingGameScrollCopperUpdate,
							&pendingPlayerSpriteUpdate, &pendingCrashSpriteUpdate,
							&pendingEnemySpriteUpdate,
							&pendingEnemyMissileSpriteUpdate,
							&pendingWingmanSpriteUpdate, &hudDirty, highScore,
							rescuedMissionNumber, game.skillLevel, rescuedGameMode,
							game.wingmanControl, 0);
						game.score = rescuedScore;
						game.bonusScore = rescuedBonusScore;
						game.missionStartScore = rescuedMissionStartScore;
						game.hitsCount = rescuedHits;
						game.lives = rescuedLives;
						game.missionNumber = rescuedMissionNumber;
						game.extraAircraftBonusSpawned =
							rescuedExtraAircraftBonusSpawned;
						game.wingman.destroyed = rescuedWingmanDestroyed;
						if (rescuedWingmanDestroyed)
							game.wingman.mode = WINGMAN_DESTROYED;
						destroyedTargetCount = rescuedTargetCount;
						destroyedShipCellCount = rescuedShipCellCount;
						landCraterCount = rescuedCraterCount;
						memcpy(destroyedTargetColumns, rescuedTargetColumns,
							sizeof(rescuedTargetColumns));
						memcpy(destroyedShipCellColumns, rescuedShipColumns,
							sizeof(rescuedShipColumns));
						memcpy(destroyedShipCellRows, rescuedShipRows,
							sizeof(rescuedShipRows));
						memcpy(landCraterColumns, rescuedCraterColumns,
							sizeof(rescuedCraterColumns));
						memcpy(landCraterRows, rescuedCraterRows,
							sizeof(rescuedCraterRows));
						initRingWorldBuffer(worldBuffers[0], 0);
						drawHudValues(hudBuffer, &game, highScore, 0);
						pendingGameScrollCopperUpdate = 1;
						lastInputMask = inputMask;
						gameCancelArmed = 0;
						hudDirty = 0;
					}
					if (hudDirty) {
						drawHudValues(hudBuffer, &game, highScore, 0);
						hudDirty = 0;
					}
				} else if (game.takeoffState == TAKEOFF_STATE_AIRBORNE &&
					game.landingState == LANDING_STATE_NONE &&
					Pressed(input.eject, previousInput.eject) && !input.cancel) {
					/* Healthy voluntary eject is edge-triggered so holding E cannot
					 * consume another aircraft after the carrier restart. */
					startPlayerEject(&game);
					pendingPlayerSpriteUpdate = 1;
					pendingCrashSpriteUpdate = 1;
					pendingEnemySpriteUpdate = 1;
					pendingEnemyMissileSpriteUpdate = 1;
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
					} else {
						/* CPC scrollrightfortakeoffloop keeps the current
						 * landing screen and scrolls the carrier back with
						 * both landed aircraft attached to its deck. The old
						 * port instead slid Player 1 alone across a stationary
						 * mirrored ship. Move the world and both aircraft by
						 * the same pixel delta, ending at the opening carrier's
						 * screen X before swapping in the next sortie. */
						if (!game.postLandingSlide)
							game.postLandingSlide = 1;
						if (game.postLandingSlide <=
							LANDING_RESTART_SCROLL_PIXELS) {
							game.scrollX = (UWORD)(game.scrollX +
								LANDING_RESTART_SLIDE_PIXELS);
							game.playerX = (WORD)(game.playerX -
								LANDING_RESTART_SLIDE_PIXELS);
							if (game.wingman.active &&
								game.wingman.mode == WINGMAN_LANDING_DECK) {
								game.wingman.interceptScreenX = (WORD)(
									game.wingman.interceptScreenX -
									LANDING_RESTART_SLIDE_PIXELS);
								/* updateWingmanLanding() still runs below. Move
								 * its settled target too, otherwise it would undo
								 * this carrier-relative motion every frame. */
								game.wingman.landingTargetX = (WORD)(
									game.wingman.landingTargetX -
									LANDING_RESTART_SLIDE_PIXELS);
								pendingWingmanSpriteUpdate = 1;
							}
							game.postLandingSlide++;
							pendingGameScrollCopperUpdate = 1;
							pendingPlayerSpriteUpdate = 1;
						} else if (!modPlaying) {
						/* CPC beginlandingapproach: after four delays, raise
						 * difficulty (up to 5), replenish, reset level
						 * progress and return to newlevelloop/checkliftoff.
						 * The Amiga landing fanfare also gets to finish
						 * before this transition starts the next sortie.
						 * startGameSession supplies the same complete world/
						 * actor reset; restore run-persistent values afterward
						 * and skip the initial carrier-entry animation. */
						/* Remove retained ambience before capturing the authoritative
						 * carrier scene. Resetting their footprints first would bake a
						 * gull or wave phase permanently into the rebased world. */
						eraseCarrierGulls(worldBuffers[activeWorldBuffer],
							activeWorldBuffer);
						eraseSeaWaves(worldBuffers[activeWorldBuffer],
							activeWorldBuffer);
						UBYTE preservedLandingWorld =
							rebaseLandedWorldForNextMission(
								worldBuffers[activeWorldBuffer], &game);

						ULONG nextBonusScore = game.bonusScore;
						UWORD nextHitsCount = game.hitsCount;
						UBYTE nextLives = game.lives;
						UBYTE nextGameMode = game.gameMode;
						/* CPC increments leveldifficulty, not the player's menu
						 * selection. startGameSession derives the next board's
						 * capped difficulty from this unchanged starting skill. */
						UBYTE nextSkill = game.skillLevel;
						UBYTE nextMissionNumber = game.missionNumber < 99
							? (UBYTE)(game.missionNumber + 1) : 99;
						UBYTE nextWingmanControl = game.wingmanControl;
						/* A wingman lost earlier in the run must stay lost
						 * into the next mission too - only a Wingman powerup
						 * pickup should bring it back, not a free respawn
						 * every time the carrier resets for the next sortie. */
						UBYTE nextWingmanDestroyed = game.wingman.destroyed;

						startGameSession(&game, copper, worldBuffers,
							&activeWorldBuffer, hudBuffer, playerSprite,
							playerAttachSprite, crashPart1Sprite, enemyAttachSprite,
							enemySprite, enemyMissileSprite, wingmanSprite,
							unusedSprite7, &pendingGameScrollCopperUpdate,
							&pendingPlayerSpriteUpdate, &pendingCrashSpriteUpdate,
							&pendingEnemySpriteUpdate,
							&pendingEnemyMissileSpriteUpdate,
							&pendingWingmanSpriteUpdate, &hudDirty, highScore,
							nextMissionNumber, nextSkill, nextGameMode, nextWingmanControl,
							preservedLandingWorld);
						game.bonusScore = nextBonusScore;
						game.score = nextBonusScore;
						game.missionStartScore = nextBonusScore;
						game.extraAircraftBonusSpawned = 0;
						game.hitsCount = nextHitsCount;
						game.lives = nextLives;
						game.missionNumber = nextMissionNumber;
						game.wingman.destroyed = nextWingmanDestroyed;
						if (nextWingmanDestroyed) {
							game.wingman.active = 0;
							game.wingman.mode = WINGMAN_DESTROYED;
							pendingWingmanSpriteUpdate = 1;
						}
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
					game.scrollX < gameScrollMaxPixels()) {
					UWORD nextScrollX = (UWORD)(game.scrollX + scrollPixels);
					if (game.landingState == LANDING_STATE_SLOWING &&
						nextScrollX > LANDING_HOVER_SCROLL_X)
						nextScrollX = LANDING_HOVER_SCROLL_X;
					game.scrollX = nextScrollX > gameScrollMaxPixels() ? gameScrollMaxPixels() : nextScrollX;
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
						 * over subsequent frames. !destroyed guards both
						 * branches below - a Wingman lost earlier in the run
						 * must stay grounded at the start of the next mission
						 * too (destroyed survives the session reset, see the
						 * post-landing transition below), not respawn for
						 * free just because a new takeoff began. */
						if (game.wingmanControl == WINGMAN_CONTROL_CPU &&
							!game.wingman.destroyed &&
							(!game.wingman.active ||
							 game.wingman.mode == WINGMAN_ON_DECK)) {
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
							UBYTE carrierHadBakedWingman =
								carrierParkedWingmanVisible;
							carrierParkedWingmanVisible = 0;
							/* Only the start carrier changed from the parked-
							 * Wingman composite. Redrawing every carrier here
							 * mapped the far end carrier into the same ring
							 * slots and made it appear behind the first enemy
							 * frigate until those columns streamed again. */
							if (carrierHadBakedWingman)
								dirtyRedrawNativeCarrierAt(worldBuffers, 8);
						} else if (game.wingmanControl == WINGMAN_CONTROL_PLAYER2 &&
							!game.wingman.active && !game.wingman.destroyed) {
							/* Not airborne yet - CPC's checkwingmankeys/
							 * liftoff rule requires Player 2 to press Up
							 * before the carrier scrolls out of reach (see
							 * updateWingmanPlayer2Control()), unlike CPU's
							 * automatic launch above. Only record the deck
							 * position here; that function performs the
							 * actual activation and carrier-composite swap
							 * once Up is pressed. */
							game.wingman.mode = WINGMAN_ON_DECK;
							game.wingman.interceptScreenX =
								WINGMAN_TAKEOFF_DECK_X;
							game.wingman.screenY = WINGMAN_TAKEOFF_DECK_Y;
							game.wingman.row =
								WINGMAN_TAKEOFF_DECK_Y / GAME_TILE_HEIGHT;
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
				/* Boundary telemetry is edge-triggered so holding a direction at a
				 * limit never fills the event ring every frame. */
				if ((Pressed(input.up, previousInput.up) &&
						game.playerY <= PLAYER_MIN_Y) ||
					(Pressed(input.down, previousInput.down) &&
						game.playerY >= PLAYER_MAX_Y) ||
					(Pressed(input.left, previousInput.left) &&
						game.playerX <= PLAYER_MIN_X) ||
					(Pressed(input.right, previousInput.right) &&
						game.playerX >= PLAYER_MAX_X))
					telemetryLogGameEvent(
						TELEMETRY_GAME_EVENT_PLAYER_MOVE_LIMIT, inputMask,
						(UWORD)(((LONG)game.scrollX + game.playerX) >> 3), &game,
						(UWORD)game.playerY);
				if (game.scrollX > gameScrollMaxPixels())
					game.scrollX = gameScrollMaxPixels();
				/* CPC fuel is elapsed-time based, not distance based. Start the
				 * clock when takeoff begins and keep it running through the landing
				 * approach; a completed landing/refuel stops and resets it. */
				if (!game.missionComplete &&
					game.takeoffState >= TAKEOFF_STATE_LIFTING &&
					updatePlayerFuel(&game))
					hudDirty = 1;

				updateEngineSound(scrollPixels);

				if (game.scrollX != oldScrollX) {
					pendingGameScrollCopperUpdate = 1;
					if (updateHudValues(&game))
						hudDirty = 1;
				}
				if (game.playerX != oldPlayerX || game.playerY != oldPlayerY)
					pendingPlayerSpriteUpdate = 1;
				/* Player weapons are deliberately release-to-rearm. Holding
				 * Fire must not launch another rocket as soon as the previous
				 * one disappears; a fresh press is required, matching the
				 * bomb path below and preventing accidental ammunition drain. */
				if (game.landingState != LANDING_STATE_HOVER &&
					Pressed(input.fire, previousInput.fire)) {
					if (launchRocket(&game, input.left)) {
						hudDirty = 1;
						pendingCrashSpriteUpdate = 1;
					}
				}
				if (game.landingState != LANDING_STATE_HOVER &&
					BombPressed(&input, &previousInput)) {
					if (launchBomb(&game)) {
						hudDirty = 1;
						pendingCrashSpriteUpdate = 1;
					}
				}
			if (updateWeapons(&game, scrollPixels, worldBuffers))
				pendingCrashSpriteUpdate = 1;
			trySpawnFlak(&game, worldBuffers);
			telemetryTrackGameplayStage(&game);
			updateCityFade(&game);
			updateTargetLock(&game);
			maybeStartWingmanBombingRun(&game);
			updatePowerup(&game);
				{
					UWORD radarBeforeEnemyUpdate = game.radarDetection;
					if (updateEnemyPlane(&game))
					pendingEnemySpriteUpdate = 1;
					if (game.radarDetection != radarBeforeEnemyUpdate)
						hudDirty = 1;
				}
			/* Enhanced resolves a radar-qualified aircraft before either drop
			 * source. If an aircraft was admitted (or remains active), both drop
			 * functions reject the frame. Existing falling drops are untouched. */
			trySpawnExtraAircraftBonus(&game);
			trySpawnPowerup(&game);
				if (updateEnemyMissile(&game, scrollPixels))
					pendingEnemyMissileSpriteUpdate = 1;
				updateWingmanFormationRow(&game);
				updateWingmanTakeoff(&game);
				updateWingmanPlayer2Control(&game, worldBuffers, &input2,
					&previousInput2, &hudDirty);
				updateWingmanPlayer2Bomb(&game, scrollPixels, worldBuffers,
					&hudDirty);
				updateWingmanIntercept(&game);
				updateWingmanBombingRun(&game, scrollPixels, worldBuffers,
					&hudDirty);
				updateWingmanLanding(&game);
				updateWingmanVisualY(&game);
				if (game.wingman.active ||
					game.wingmanControl == WINGMAN_CONTROL_PLAYER2)
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
				/* Fuel exhaustion is recoverable only through a timely ejection.
				 * Defer this until after the frame's collision work so direct terrain/
				 * water impacts keep their stricter immediate-crash precedence. */
				if (!game.gameOver && !game.crashTimer && game.fuel == 0 &&
					game.aircraftFailureState == AIRCRAFT_FAILURE_NONE)
					startAircraftFailure(&game, AIRCRAFT_FAILURE_CAUSE_FUEL);
				if (game.gameOver) {
					if (!game.highScoreCommitted &&
						!game.highScoreNameEntryActive) {
						beginHighScoreNameEntry(&highScore, &game);
						hudDirty = 1;
					}
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
				if (game.highScoreNameEntryActive) {
					UBYTE nameEntryUpdate = updateHighScoreNameEntry(
						&highScore, &game, &input, &previousInput);
					if (nameEntryUpdate)
						drawHudBuffer(hudBuffer, &game, highScore, 0);
				} else if (Pressed(input.select, previousInput.select)) {
					stopModMusic();
					stopAllSfx();
					startGameSession(&game, copper, worldBuffers, &activeWorldBuffer, hudBuffer, playerSprite, playerAttachSprite, crashPart1Sprite, enemyAttachSprite,
						enemySprite, enemyMissileSprite, wingmanSprite, unusedSprite7,
						&pendingGameScrollCopperUpdate, &pendingPlayerSpriteUpdate,
						&pendingCrashSpriteUpdate, &pendingEnemySpriteUpdate, &pendingEnemyMissileSpriteUpdate, &pendingWingmanSpriteUpdate,
							&hudDirty, highScore, 1, (UBYTE)skillLevel, (UBYTE)gameModeSetting, (UBYTE)wingmanControl, 0);
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
				updateCrashPartSprites(crashPart1Sprite, enemySprite,
					enemyAttachSprite, &game);
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
			/* Bombs still retire before streaming because their tiny moving BOBs
			 * may restore saved bytes. Missiles use a narrower rule below: erase
			 * early only if the streamer can actually touch their old columns,
			 * otherwise keep them visible until the late redraw group. */
			eraseBombPixelBobFootprint(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, bombShotFootprints);
			eraseBombPixelBobFootprint(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, wingmanBombFootprints);
			if (rocketPixelBobNeedsPreStreamErase(&game, activeWorldBuffer,
				rocketShotFootprints))
				eraseRocketPixelBobFootprint(worldBuffers[activeWorldBuffer],
					activeWorldBuffer, rocketShotFootprints);
			if (rocketPixelBobNeedsPreStreamErase(&game, activeWorldBuffer,
				wingmanRocketFootprints))
				eraseRocketPixelBobFootprint(worldBuffers[activeWorldBuffer],
					activeWorldBuffer, wingmanRocketFootprints);
			if (rocketPixelBobNeedsPreStreamErase(&game, activeWorldBuffer,
				enemyMissileFootprints))
				eraseRocketPixelBobFootprint(worldBuffers[activeWorldBuffer],
					activeWorldBuffer, enemyMissileFootprints);
			/* Failure smoke is restored from world truth, not from saved bytes:
			 * the ring buffer may have recycled those columns since the previous
			 * frame.  Erase before streaming, then composite the new plume after
			 * persistent impacts/powerups and before the foreground projectiles. */
			UBYTE redrawFailureSmoke = aircraftFailureSmokeNeedsRedraw(&game);
			if (redrawFailureSmoke)
				eraseAircraftFailureSmokeFootprint(
					worldBuffers[activeWorldBuffer], activeWorldBuffer);
			UBYTE redrawCarrierGulls = carrierGullsNeedRedraw(&game);
			UBYTE seaWaveUpdate = seaWavesUpdateKind(&game);
			if (redrawCarrierGulls) {
				eraseCarrierGulls(worldBuffers[activeWorldBuffer],
					activeWorldBuffer);
				bobGullRedraws++;
			} else {
				bobGullUnchangedSkips++;
			}
			if (seaWaveUpdate == SEA_WAVE_UPDATE_FULL) {
				eraseSeaWaves(worldBuffers[activeWorldBuffer],
					activeWorldBuffer);
				bobWaveRedraws++;
			} else {
				bobWaveUnchangedSkips++;
			}
			serviceRingWorldStream(worldBuffers[0], &game);
			if (seaWaveUpdate == SEA_WAVE_UPDATE_FULL)
				drawSeaWaves(worldBuffers[activeWorldBuffer], activeWorldBuffer,
					&game);
			else if (seaWaveUpdate == SEA_WAVE_UPDATE_PHASE)
				updateSeaWavePhasesInPlace(worldBuffers[activeWorldBuffer],
					activeWorldBuffer);
			if (redrawCarrierGulls)
				drawCarrierGulls(worldBuffers[activeWorldBuffer],
					activeWorldBuffer, &game);
			updateBombImpactBob(worldBuffers[0], &game);
			updatePowerupBob(worldBuffers[0], &game);
			if (redrawFailureSmoke)
				drawAircraftFailureSmoke(worldBuffers[activeWorldBuffer],
					activeWorldBuffer);
			drawBombPixelBob(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, &game.bombShot, bombShotFootprints,
				game.scrollX);
			drawBombPixelBob(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, &game.wingman.bomb,
				wingmanBombFootprints, game.scrollX);
			/* Keep the single-buffer erase interval extremely short. All three
			 * old missile footprints are removed together immediately before the
			 * new silhouettes are composited, preserving correct overlap order. */
			eraseRocketPixelBobFootprint(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, rocketShotFootprints);
			eraseRocketPixelBobFootprint(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, wingmanRocketFootprints);
			eraseRocketPixelBobFootprint(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, enemyMissileFootprints);
			drawRocketPixelBob(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, &game.rocketShot,
				rocketShotFootprints, game.crashTimer != 0);
			drawRocketPixelBob(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, &game.wingman.rocket,
				wingmanRocketFootprints, game.crashTimer != 0);
			drawEnemyMissilePixelBob(worldBuffers[activeWorldBuffer],
				activeWorldBuffer, &game);
			telemetryUpdate(&game, activeWorldBuffer);
#if HAR_DEBUG_PERF_LOG
			perfLogFrame(&game, activeWorldBuffer);
#endif
		}
	}

	stopAllSfx();
	stopModMusic();
	/* Restore AmigaOS while every Copper, bitplane, sprite and audio buffer it
	 * may still reference is valid. The former order freed CHIP memory first
	 * and only then waited/restored the OS display, creating a teardown-only
	 * use-after-free on real hardware and strict WinUAE configurations. */
	FreeSystem();
	/* Filesystem access is now safe and happens only on this one-way path. A
	 * failed/read-only save leaves the built-in table intact and cannot block
	 * returning to DOS. */
	if (HAR_HIGHSCORE_DISK_IO && !flushHighScoreTable() && highScoreSaveDirty)
		KPrintF("High-score save skipped: media unavailable or read-only\n");
#if HAR_DEBUG_PERF_LOG
	perfLogFlushToDisk();
	parityLogFlushToDisk(&game);
#endif
#if HAR_DEBUG_LAND_LOG
	landLogFlushToDisk();
#endif
#if HAR_DEBUG_ENEMY_PLANE_LOG
	enemyPlaneTraceFlushToDisk(&game);
#endif

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
	FreeMem(seaAmbienceBuffer, SEA_AMBIENCE_BUFFER_BYTES);
	seaAmbienceBuffer = 0;
	FreeMem(carrierIdleDecodeBuffer, CARRIER_IDLE_DECODE_BUFFER_BYTES);
	carrierIdleDecodeBuffer = 0;
	if (telemetrySamples)
		FreeMem(telemetrySamples, sizeof(TelemetrySample) * TELEMETRY_SAMPLE_COUNT);
	telemetrySamples = 0;
	FreeMem(menuTickerBitmap, MENU_TICKER_BITMAP_BYTES);
	menuTickerBitmap = 0;
	FreeMem(screenBuffer, SCREEN_BITMAP_BYTES);
	FreeMem(copper, COPPER_BYTES);

	CloseLibrary((struct Library*)DOSBase);
	CloseLibrary((struct Library*)GfxBase);
	return 0;
}
