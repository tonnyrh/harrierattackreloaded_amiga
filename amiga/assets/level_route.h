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
	{ 100, 105, HAR_STAGE_DO_LAND, HAR_TERRAIN_COAST_RISE },
	{ 106, 400, HAR_STAGE_DO_LAND, HAR_TERRAIN_CPC_RANDOM_LAND },
	{ 401, 410, HAR_STAGE_DESCEND_MOUNTAINS, HAR_TERRAIN_CPC_DESCEND_TO_TOWN },
	{ 411, 610, HAR_STAGE_FLAT_TOWNLAND, HAR_TERRAIN_TOWN },
	{ 611, 616, HAR_STAGE_START_PIER, HAR_TERRAIN_COAST_FALL },
	{ 617, 628, HAR_STAGE_START_PIER, HAR_TERRAIN_SEA },
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
	{ 50, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 20, 0, 3 },
	{ 51, 13, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 18, 0, 3 },
	{ 51, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 21, 0, 3 },
	{ 52, 13, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 19, 0, 3 },
	{ 52, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 22, 0, 3 },
	{ 53, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_ENEMY_SHIP, 23, 0, 3 },

	// Deterministic stand-ins for CPC's randomized enemylandsprites during the
	// long land run. The tile ids come directly from the CPC table:
	// radar=2a, missile launcher=2b, gun=2c, tank=2d/2e.
	// Sprint 14.9 increases density so the run reads closer to the original
	// generated pressure. Keep these as data for the future route editor.
	// Sprint 14.19 adds an early playtest belt so destruction/collision can be
	// verified shortly after land begins instead of far into the route.
	{ 104, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 42, 0, 1 },
	{ 110, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 43, 0, 1 },
	{ 116, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 42, 0, 1 },
	{ 123, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 44, 0, 1 },
	{ 132, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 43, 0, 1 },
	{ 141, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 45, 0, 2 },
	{ 142, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 46, 0, 2 },
	{ 150, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 44, 0, 1 },
	{ 158, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 42, 0, 1 },
	{ 169, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 45, 0, 2 },
	{ 170, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 46, 0, 2 },
	{ 190, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 42, 0, 1 },
	{ 197, -5, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_FLAK, 57, 0, 1 },
	{ 209, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 43, 0, 1 },
	{ 228, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 44, 0, 1 },
	{ 236, -6, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_FLAK, 58, 0, 1 },
	{ 247, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 45, 0, 2 },
	{ 248, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 46, 0, 2 },
	{ 268, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 42, 0, 1 },
	{ 276, -5, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_FLAK, 57, 0, 1 },
	{ 286, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 43, 0, 1 },
	{ 305, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 44, 0, 1 },
	{ 314, -6, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_FLAK, 58, 0, 1 },
	{ 324, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 45, 0, 2 },
	{ 325, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 46, 0, 2 },
	{ 344, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 42, 0, 1 },
	{ 352, -5, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_FLAK, 57, 0, 1 },
	{ 362, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 43, 0, 1 },
	{ 380, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 44, 0, 1 },
	{ 388, -6, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_FLAK, 58, 0, 1 },
	{ 396, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 45, 0, 2 },
	{ 397, -1, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_GROUND_TARGET, 46, 0, 2 },

	// Sprint 14.95 Part 5: town blocks were hand-placed here (17 entries,
	// 8 block variants at ~half the columns actually covered) - replaced by
	// generateCpcTownBlockTable()'s continuous procedural generator in
	// main.c, which covers the whole town segment (411-610) densely instead
	// of leaving large gaps between isolated blocks. The hand-placed flak
	// entries that used to sit between them stay as-is (harmless extra flak
	// alongside trySpawnFlak()'s live town-stage spawning).
	{ 456, -6, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_FLAK, 57, 0, 1 },
	{ 482, -6, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_FLAK, 58, 0, 1 },
	{ 510, -6, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_FLAK, 57, 0, 1 },
	{ 538, -6, HAR_ROW_TERRAIN_RELATIVE, HAR_OBJ_FLAK, 58, 0, 1 },

	// CPC setcloudcolourtosea emits a small solid slope before pendata. Keep it
	// as clean land for now; raw slope tiles can be promoted once the compositor
	// handles CPC Mode 0 edge cases cleanly.
	{ 611, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_LAND, 1, 0, 0 },
	{ 612, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_LAND, 1, 0, 0 },
	{ 613, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_LAND, 1, 0, 0 },

	// CPC pendata "JHIJHIJHIJHI" at row 0x0e. These are raw CPC tile ids
	// 74,72,73 repeated; they give us a first direct pier comparison.
	{ 617, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 74, 0, 0 },
	{ 618, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 72, 0, 0 },
	{ 619, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 73, 0, 0 },
	{ 620, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 74, 0, 0 },
	{ 621, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 72, 0, 0 },
	{ 622, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 73, 0, 0 },
	{ 623, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 74, 0, 0 },
	{ 624, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 72, 0, 0 },
	{ 625, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 73, 0, 0 },
	{ 626, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 74, 0, 0 },
	{ 627, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 72, 0, 0 },
	{ 628, 14, HAR_ROW_ABSOLUTE, HAR_OBJ_OWN_FRIGATE, 73, 0, 0 },

	// CPC scrolls the gunship on around the pier/second-ship section.
	{ 621, 9, HAR_ROW_ABSOLUTE, HAR_OBJ_GUNSHIP, 0, HAR_OBJECT_FLAG_CPC_GUNSHIP, 2 },

	// Second CPC enemy ship after the pier, using the same stream.
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
