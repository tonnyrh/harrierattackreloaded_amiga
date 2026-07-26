// Harrier Attack Reloaded - Amiga level route data.
//
// This file is intentionally plain table data, not gameplay code. A future
// editor should be able to export these two tables: broad terrain/stage
// segments and explicit object placements. Columns are world tile columns, not
// pixels.
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

static const LevelSegmentDef harLevelRoute[] = {
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
	// Sprint 14.97 PRI 6: COAST_FALL reduced from 6 columns (611-616) to 3
	// (611-613), matching CPC's setcloudcolourtosea 3x2 solid land block.
	// Pier data (pendata "JHIJHIJHIJHI") starts immediately after at 614.
	{ 611, 613, HAR_STAGE_START_PIER, HAR_TERRAIN_COAST_FALL },
	{ 614, 628, HAR_STAGE_START_PIER, HAR_TERRAIN_SEA },
	{ 629, 632, HAR_STAGE_END_PIER, HAR_TERRAIN_SEA },
	{ 633, 666, HAR_STAGE_SECOND_SHIP_MISSILE, HAR_TERRAIN_SEA },
	{ 667, 680, HAR_STAGE_START_FRIGATE, HAR_TERRAIN_SEA },
	{ 681, 690, HAR_STAGE_END_FRIGATE, HAR_TERRAIN_SEA },
	{ 691, 703, HAR_STAGE_LANDING_ON_FRIGATE, HAR_TERRAIN_SEA }
};

static const LevelObjectDef harLevelObjects[] = {
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
	// guaranteed flak at positions CPC never places them. The 3 LAND cells
	// at 611-613 (setcloudcolourtosea stand-ins) were also removed - that
	// transition is handled by the segment/terrain system, not explicit
	// objects (see PRI 6 for the exact by-pier block sequence).

	// CPC pendata "JHIJHIJHIJHI" at row 0x0e. These are raw CPC tile ids
	// 74,72,73 repeated; they give us a first direct pier comparison.
	// Sprint 14.97 PRI 6: shifted from 617-628 to 614-625 to follow the
	// 3-column solid land block immediately (no sea gap between).
	{ 614, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 74, 0, 0 },
	{ 615, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 72, 0, 0 },
	{ 616, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 73, 0, 0 },
	{ 617, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 74, 0, 0 },
	{ 618, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 72, 0, 0 },
	{ 619, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 73, 0, 0 },
	{ 620, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 74, 0, 0 },
	{ 621, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 72, 0, 0 },
	{ 622, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 73, 0, 0 },
	{ 623, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 74, 0, 0 },
	{ 624, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 72, 0, 0 },
	{ 625, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 73, 0, 0 },

	// Second CPC enemy ship after the pier, using the same stream.
	{ 629, 13, HAR_ROW_ABSOLUTE, HAR_OBJ_GUNSHIP, 0, HAR_OBJECT_FLAG_CPC_GUNSHIP, 2 },
	{ 629, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 20, 0, 3 },
	{ 630, 13, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 18, 0, 3 },
	{ 630, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 21, 0, 3 },
	{ 631, 13, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 19, 0, 3 },
	{ 631, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 22, 0, 3 },
	{ 632, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 23, 0, 3 },

	// Final friendly frigate/carrier area. The CPC endfrigatesprite is a
	// 14-column stream, but the promoted Plus carrier is more legible today; the
	// deck span matches the CPC final approach width.
	// End carrier: CPC's endfrigatesprite is horizontally reversed from the
	// start carrier ("FRIGATE REVERSED, SO IT CAN COME IN SCREEN FROM
	// OPPOSITE SIDE") - flags OR in the mirrored-sprite variant.
	{ 667, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_CARRIER | HAR_OBJECT_FLAG_NATIVE_CARRIER_REVERSED, 0 },
	{ 667, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 668, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 669, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 670, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 671, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 672, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 673, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 674, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 675, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 676, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 677, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 678, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 679, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 },
	{ 680, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 0, HAR_OBJECT_FLAG_NATIVE_DECK, 0 }
};

static const UWORD harEnemyPlaneTriggers[] = {
	132,
	246,
	360,
	482,
	628
};

static const UWORD harEnemyShipMissileTriggers[] = {
	54,
	633
};

#endif
