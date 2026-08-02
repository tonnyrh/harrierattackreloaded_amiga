// Harrier Attack Reloaded - Amiga level route data.
//
// This file is intentionally plain table data, not gameplay code. A future
// editor should be able to export these two tables: broad terrain/stage
// segments and explicit object placements. Columns are world tile columns,
// not pixels. These are immutable source coordinates: Sprint 15.54 copies
// them at runtime, completes the final CPC town block and shifts every
// post-town segment/object/trigger by the same 0..4 columns.
//
// Sprint 14.7 mirrors the CPC procedural level generator more closely. The CPC
// does not store one static map; it emits one right-edge column at a time from
// gamelevelprogress states and countdowns. The Amiga build below bakes that
// flow into an explicit route so we can compare the whole mission visually:
//
//   sea -> enemy ship -> sea/missile -> land -> long hill/target run -> town
//   -> pier -> second enemy ship -> sea -> final frigate -> landing approach.

#ifndef HAR_LEVEL_ROUTE_H
#define HAR_LEVEL_ROUTE_H

static const LevelSegmentDef harLevelRouteSource[] = {
	// start, end, stage, terrain kind
	{ 0, 49, HAR_STAGE_START, HAR_TERRAIN_SEA },
	{ 50, 53, HAR_STAGE_START_ENEMY_SHIP, HAR_TERRAIN_SEA },
	{ 54, 99, HAR_STAGE_ENEMY_SHIP_FIRED_MISSILE, HAR_TERRAIN_SEA },
	// Sprint 14.97 PRI 8: COAST_RISE reduced from 6 columns (100-105) to 2
	// (100-101), matching CPC's 1+1 tile coast transition (one solid tile
	// at row 15, one hill-up at row 14). Procedural land starts immediately
	// after at column 102. CPC_LAND_PROCEDURAL_WORLD_START in main.c still
	// references column 106 — the 4 extra columns of procedural land that
	// replace the old coast-rise fill generate identically to any other
	// land column, so no separate adjustment is needed there.
	{ 100, 101, HAR_STAGE_DO_LAND, HAR_TERRAIN_COAST_RISE },
	{ 102, 400, HAR_STAGE_DO_LAND, HAR_TERRAIN_CPC_RANDOM_LAND },
	{ 401, 410, HAR_STAGE_DESCEND_MOUNTAINS, HAR_TERRAIN_CPC_DESCEND_TO_TOWN },
	{ 411, 610, HAR_STAGE_FLAT_TOWNLAND, HAR_TERRAIN_TOWN },
	// Sprint 15.46: setcloudcolourtosea passes C=3 as the LAND object ID,
	// not a width. drawspriteblock3 uses B=2 and therefore emits one column
	// with two solid tiles. The 12 JHI pier columns follow immediately; one
	// sea/terminator tick at 624 then starts the four-column enemy ship.
	{ 611, 611, HAR_STAGE_START_PIER, HAR_TERRAIN_COAST_FALL },
	{ 612, 624, HAR_STAGE_START_PIER, HAR_TERRAIN_SEA },
	{ 625, 628, HAR_STAGE_END_PIER, HAR_TERRAIN_SEA },
	{ 629, 662, HAR_STAGE_SECOND_SHIP_MISSILE, HAR_TERRAIN_SEA },
	{ 663, 676, HAR_STAGE_START_FRIGATE, HAR_TERRAIN_SEA },
	{ 677, 686, HAR_STAGE_END_FRIGATE, HAR_TERRAIN_SEA },
	{ 687, 703, HAR_STAGE_LANDING_ON_FRIGATE, HAR_TERRAIN_SEA }
};

static const LevelObjectDef harLevelObjectsSource[] = {
	// column, row/offset, row mode, object id, tile id, flags, hp

	// Friendly start carrier.
	{ 8, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_CARRIER, 0 },

	// CPC enemyshipsprite stream at state 1:
	// 00,00,14, 00,12,15, 00,13,16, 00,00,17, ff
	// Each comma-separated group is one 3-tile vertical world column.
	// drawnewgunship's left half reaches the same right-edge column as the
	// first group, so the Plus and Mode 1 layers share column 50 as origin.
	{ 50, 13, HAR_ROW_ABSOLUTE, HAR_OBJ_GUNSHIP, 0, HAR_OBJECT_FLAG_CPC_GUNSHIP, 2 },
	{ 50, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 20, 0, 3 },
	{ 51, 13, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 18, 0, 3 },
	{ 51, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 21, 0, 3 },
	{ 52, 13, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 19, 0, 3 },
	{ 52, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 22, 0, 3 },
	{ 53, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 23, 0, 3 },

	// Sprint 14.97 PRI 1: all hand-placed GROUND_TARGET entries in the
	// procedural land area (columns 104-400) have been removed.
	// generateCpcLandHeightTable() already produces radar/launcher/gun/tank
	// procedurally, and the hand-placed stand-ins here were overriding or
	// duplicating that generator's output, producing higher target density
	// than CPC and placing targets on columns where CPC's mode dispatcher
	// never selected one. Likewise removed all hand-placed FLAK entries in
	// land (197-388) and town (456-538) - trySpawnFlak() handles all flak
	// generation at the right screen edge, and the fixed entries added
	// guaranteed flak at positions CPC never places them. The old LAND cells
	// at the city/pier seam were also removed - that
	// transition is handled by the segment/terrain system, not explicit
	// objects (see PRI 6 for the exact by-pier block sequence).

	// CPC pendata "JHIJHIJHIJHI" at row 0x0e. These are raw CPC tile ids
	// 74,72,73 repeated; they give us a first direct pier comparison.
	// Exact 12-byte pendata stream after the single transition column.
	// Column 624 is the CPC terminator tick and remains ordinary sea.
	{ 612, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 74, 0, 0 },
	{ 613, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 72, 0, 0 },
	{ 614, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 73, 0, 0 },
	{ 615, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 74, 0, 0 },
	{ 616, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 72, 0, 0 },
	{ 617, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 73, 0, 0 },
	{ 618, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 74, 0, 0 },
	{ 619, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 72, 0, 0 },
	{ 620, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 73, 0, 0 },
	{ 621, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 74, 0, 0 },
	{ 622, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 72, 0, 0 },
	{ 623, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_PIER, 73, 0, 0 },

	// Second CPC enemy ship after the pier, using the same stream.
	{ 625, 13, HAR_ROW_ABSOLUTE, HAR_OBJ_GUNSHIP, 0, HAR_OBJECT_FLAG_CPC_GUNSHIP, 2 },
	{ 625, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 20, 0, 3 },
	{ 626, 13, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 18, 0, 3 },
	{ 626, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 21, 0, 3 },
	{ 627, 13, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 19, 0, 3 },
	{ 627, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 22, 0, 3 },
	{ 628, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 23, 0, 3 },

	// Final friendly frigate/carrier area. CPC inserts this stream from the
	// opposite screen edge, but its fully assembled carrier retains the
	// opening carrier's orientation: bow and parked Wingman remain forward.
	{ 663, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_CARRIER, 0 },
	{ 663, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 664, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 665, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 666, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 667, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 668, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 669, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 670, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 671, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 672, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 673, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 674, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 675, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 676, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 }
};

static const UWORD harEnemyShipMissileTriggersSource[] = {
	54,
	629
};

#endif
