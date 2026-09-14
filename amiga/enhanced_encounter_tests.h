#if HAR_HEADLESS_CLASSIC_CONTRACT_TEST
/* Actual generator and fuel clock: no pickup is required for route feasibility.
 * Conservative budget: whole maximum viewport route (longer than approach),
 * 10 seconds takeoff/acceleration plus 30 seconds at 3x landing consumption. */
static ULONG fuelSupplyClockRemaining(const GameState* g) {
    if (!g->fuelGaugeLevel) return 0;
    return ((ULONG)(g->fuelGaugeLevel - 1) * CPC_FUEL_SUBCOUNT_FULL +
        g->fuelSubCounter) * playerFuelClockLimit(g) - g->fuelClockAccumulator;
}
static UBYTE referenceFuelSupplyMatch(void) {
    static GameState g;
    UBYTE oldInfinite = debugInfiniteFuel, oldMod = modPlaying;
    debugInfiniteFuel = 0; modPlaying = 1;
    for (UBYTE skill = 1; skill <= 5; skill++) {
        cpcLandSkillLevel = skill;
        cpcLandRouteExtension = skill * CPC_LAND_EXTENSION_PER_DIFFICULTY;
        cpcLandProceduralLength = CPC_LAND_PROCEDURAL_BASE_LENGTH + cpcLandRouteExtension;
        for (UWORD seed = 1; seed <= 16; seed++) {
            initGameState(&g, seed, seed, 1);
            g.gameMode = GAME_MODE_ENHANCED; g.levelDifficulty = skill;
            LONG previous = -1000, first = -1;
            UWORD count = 0;
            for (UWORD i = 0; i < cpcLandProceduralLength; i++) {
                LONG column = CPC_LAND_PROCEDURAL_WORLD_START + i;
                if (!isFuelDepotColumn(column)) continue;
                UBYTE target = cpcLandGameplayTable[i].target;
                if (target < 1 || target > 3 || isMissileTankColumn(column) ||
                    missileSiloLocalColumn(column) >= 0 || column - previous < 192) return 1;
                previous = column; if (first < 0) first = column; count++;
            }
            if (!count || first < 0) return 2;
            /* Preserve the fractional clock: each refill is exactly 20%,
             * not a rounded number of gauge cells or a reset of elapsed time. */
            g.fuelGaugeLevel = 5; g.fuelSubCounter = 7; g.fuelClockAccumulator = 1234;
            g.fuel = cpcFuelHudValue(&g);
            ULONG before = fuelSupplyClockRemaining(&g);
            refillFuelDepot(&g, first);
            if (fuelSupplyClockRemaining(&g) - before !=
                (ULONG)CPC_FUEL_TOTAL_QUANTA * playerFuelClockLimit(&g) / 5) return 3;
            markTargetDestroyedAtColumn(first);
            before = fuelSupplyClockRemaining(&g); refillFuelDepot(&g, first);
            if (fuelSupplyClockRemaining(&g) != before) return 4;
            resetDestroyedTargets(); resetPlayerFuel(&g);
            updatePlayerFuel(&g); refillFuelDepot(&g, first);
            if (g.fuel != 999 || g.fuelClockAccumulator) return 5;
            g.gameMode = GAME_MODE_CLASSIC;
            for (UWORD t = 0; t < 2000; t++) updatePlayerFuel(&g);
            before = fuelSupplyClockRemaining(&g); refillFuelDepot(&g, first);
            if (fuelSupplyClockRemaining(&g) != before) return 6;
            g.gameMode = GAME_MODE_ENHANCED; resetPlayerFuel(&g);
            UWORD cruiseSteps = (gameScrollMaxPixels() + 2) / scrollPixelsForSpeedLevel(5);
            for (UWORD t = 0; t < cruiseSteps + 500; t++) updatePlayerFuel(&g);
            g.lowSpeedLanding = 1;
            for (UWORD t = 0; t < 1500; t++) updatePlayerFuel(&g);
            if (g.fuel == 0 || g.fuel < 100) return 7;
        }
    }
    g.gameMode = GAME_MODE_CLASSIC; g.lowSpeedLanding = 0; resetPlayerFuel(&g);
    UWORD ticks = 0;
    while (g.fuel && ticks < 10000) { updatePlayerFuel(&g); ticks++; }
    if (ticks != 9558) return 8;
    if (HUD_COLOR_FUEL == HUD_COLOR_VALUE || HUD_COLOR_FUEL >= (1 << HUD_PLANES)) return 9;
    debugInfiniteFuel = oldInfinite; modPlaying = oldMod;
    return 0;
}

static UBYTE encounterBytesDiffer(const UBYTE* a, const UBYTE* b, ULONG length) {
    while (length--) if (*a++ != *b++) return 1;
    return 0;
}
static UBYTE referenceMissileDamageEjectMatch(void) {
    static GameState g;
    memset(&g, 0, sizeof(g));
    g.gameMode = GAME_MODE_ENHANCED; g.takeoffState = TAKEOFF_STATE_AIRBORNE;
    g.armour = 100; g.fuel = 100; g.playerY = 40; g.playerX = 100;
    g.levelDifficulty = 1;
    UBYTE oldMod = modPlaying; modPlaying = 1;
    applyPlayerMissileDamage(&g, 0);
    if (g.armour != 67 || g.aircraftFailureState) return 1;
    applyPlayerMissileDamage(&g, 0);
    if (g.armour != 34 || g.aircraftFailureState) return 2;
    applyPlayerMissileDamage(&g, 0);
    if (g.armour || g.aircraftFailureState != AIRCRAFT_FAILURE_DESCENT) return 3;
    g.aircraftFailureState = 0; g.missileDamageThirds = 0; g.armour = 100;
    applyPlayerMissileDamage(&g, 1);
    if (g.armour != 50 || g.aircraftFailureState) return 4;
    applyPlayerMissileDamage(&g, 1);
    if (g.armour || g.aircraftFailureState != AIRCRAFT_FAILURE_DESCENT) return 5;
    g.aircraftFailureState = 0; g.missileDamageThirds = 0; g.armour = 100;
    applyPlayerMissileDamage(&g, 0); applyPlayerFlakDamage(&g);
    if (g.armour >= 67 || g.armour == 0) return 6;
    activatePowerup(&g, POWERUP_HEALTH);
    if (g.armour != 100 || g.missileDamageThirds || g.flakDamageCount) return 7;
    UBYTE hud = 0, weapon = 0, wing = 0;
    UBYTE* buffers[GAME_WORLD_BUFFER_COUNT] = {0};
    g.siloMissile.active = 1; g.siloMissile.x = g.playerX; g.siloMissile.y = g.playerY;
    collideEnhancedEncounters(&g, buffers, &hud, &weapon, &wing);
    if (g.armour != 67 || g.siloMissile.active || !hud) return 16;
    collideEnhancedEncounters(&g, buffers, &hud, &weapon, &wing);
    if (g.armour != 67) return 17; /* consumed missile cannot hit twice */
    activatePowerup(&g, POWERUP_HEALTH);
    g.wingman.active = 1; g.wingman.mode = WINGMAN_FORMATION;
    g.wingman.screenY = 80; g.wingman.interceptScreenX = 60;
    g.siloMissile.active = 1; g.siloMissile.x = wingmanScreenX(&g); g.siloMissile.y = 80;
    collideEnhancedEncounters(&g, buffers, &hud, &weapon, &wing);
    if (g.wingman.active || !g.wingman.destroyed || g.siloMissile.active || !wing) return 18;
    g.respawnSafeTimer = 1; applyPlayerMissileDamage(&g, 1);
    if (g.armour != 100) return 8;
    g.respawnSafeTimer = 0;
    InputState in = {0}; in.fire = in.bomb = 1;
    for (UBYTE i = 0; i < 20; i++) if (updateEjectChord(&g, &in)) return 9;
    g.aircraftFailureState = AIRCRAFT_FAILURE_DESCENT;
    for (UBYTE i = 0; i < 11; i++) if (updateEjectChord(&g, &in)) return 10;
    if (!updateEjectChord(&g, &in) || updateEjectChord(&g, &in)) return 11;
    in.bomb = 0; updateEjectChord(&g, &in);
    if (g.ejectChordTicks) return 12;
    in.bomb = 1; in.cancel = 1;
    if (updateEjectChord(&g, &in) || g.ejectChordTicks) return 13;
    in.cancel = 0; g.gameMode = GAME_MODE_CLASSIC;
    for (UBYTE i = 0; i < 20; i++) if (updateEjectChord(&g, &in)) return 14;
    g.gameMode = GAME_MODE_ENHANCED;
    for (UBYTE i = 0; i < 12; i++) if (updateEjectChord(&g, &in)) startPlayerEject(&g);
    if (!g.ejectState || g.aircraftFailureState) return 15;
    modPlaying = oldMod;
    return 0;
}

static UBYTE referenceHardwareFeedbackMatch(void) {
    static GameState g;
    initGameState(&g, 12040, 12040, 2);
    g.gameMode = GAME_MODE_ENHANCED; g.takeoffState = TAKEOFF_STATE_AIRBORNE;
    g.playerX = 80; g.playerY = 40; g.scrollX = 1200;
    g.wingman.active = 1; g.wingman.mode = WINGMAN_FORMATION;
    g.wingman.interceptScreenX = 60; g.wingman.screenY = 40;
    UWORD live[PLAYER_SPRITE_WORDS], reference[PLAYER_SPRITE_WORDS];
    for (UBYTE mode = 0; mode < 2; mode++) {
        g.wingman.mode = mode ? WINGMAN_ON_DECK : WINGMAN_FORMATION;
        for (UWORD i = 0; i < PLAYER_SPRITE_WORDS; i++) live[i] = 0x5aa5;
        stageWingmanSprite(live, &g);
        for (UWORD i = 0; i < PLAYER_SPRITE_WORDS; i++) if (live[i] != 0x5aa5) return 1;
        buildSpriteFromCpcPlusHalves(reference, PLAYER_SPRITE_HEIGHT, wingmanScreenX(&g), g.wingman.screenY,
            mode ? harCpcWingmanLandedLeftPixels : harCpcWingmanFlyingLeftPixels,
            mode ? harCpcWingmanLandedRightPixels : harCpcWingmanFlyingRightPixels,
            cpcPlusPenToWingmanHardwareColor);
        commitWingmanSprite();
        if (encounterBytesDiffer((UBYTE*)live, (UBYTE*)reference, sizeof(live))) return 2;
    }
    g.wingman.active = 0; stageWingmanSprite(live, &g); commitWingmanSprite();
    if (live[0] || live[1]) return 3;
    g.helicopter.active = 1; g.helicopter.worldX = g.scrollX + 180;
    g.helicopter.x = 180; g.helicopter.y = 45;
    for (UBYTE i = 0; i < 9; i++) fireHelicopterBullet(&g, i % HELICOPTER_BULLET_MAX);
    for (UBYTE i = 0; i < HELICOPTER_BULLET_MAX; i++)
        if (!g.helicopterBullets[i].active || g.helicopterBullets[i].vx16 >= 0) return 4;
    if (g.helicopterBullets[0].vy16 == g.helicopterBullets[HELICOPTER_BULLET_MAX - 1].vy16) return 5;
    for (UBYTE i = 0; i < HELICOPTER_BULLET_MAX; i++) {
        g.helicopterBullets[i].y = terrainSurfacePixelYForWorldColumn(g.helicopterBullets[i].worldX >> 3);
        g.helicopterBullets[i].vx16 = g.helicopterBullets[i].vy16 = 0;
    }
    updateHelicopterBullets(&g);
    for (UBYTE i = 0; i < HELICOPTER_BULLET_MAX; i++) if (g.helicopterBullets[i].active) return 6;
    UBYTE oldMod = modPlaying; modPlaying = 1;
    memset(&g.enemyMissile, 0, sizeof(g.enemyMissile));
    g.enemyMissile.active = 1; g.enemyMissile.type = ENEMY_MISSILE_TRUCK_TYPE;
    g.enemyMissile.worldX = g.scrollX + 160; g.enemyMissile.x = 160;
    g.enemyMissile.y = terrainSurfacePixelYForWorldColumn(g.enemyMissile.worldX >> 3);
    if (!updateEnemyMissile(&g, 0) || g.enemyMissile.active || !g.impact.active) return 7;
    modPlaying = oldMod;
    return 0;
}

static UBYTE referenceEnhancedEncountersMatch(void) {
    static GameState game;
    memset(&game, 0xa5, sizeof(game));
    initGameState(&game, 12040, 12040, 2);
    game.skillLevel = game.levelDifficulty = 1;
    if (game.lowSpeedLanding || game.siloMissile.active || game.helicopter.active ||
        game.helicopterSmoke.active || game.encounterPoseValid) return 1;
    game.speedLevel = 2;
    if (!updateLowSpeedLanding(&game) || !game.lowSpeedLanding) return 2;
    game.speedLevel = 3; updateLowSpeedLanding(&game);
    if (!game.lowSpeedLanding) return 3;
    game.speedLevel = 4; updateLowSpeedLanding(&game);
    if (game.lowSpeedLanding) return 4;
    game.landingState = LANDING_STATE_HOVER; updateLowSpeedLanding(&game);
    if (!game.lowSpeedLanding) return 5;
    UBYTE oldInfinite = debugInfiniteFuel; debugInfiniteFuel = 0;
    resetPlayerFuel(&game);
    for (UWORD t = 0; t < 1000; t++) updatePlayerFuel(&game);
    UWORD hover = (CPC_FUEL_GAUGE_LEVELS - game.fuelGaugeLevel) * CPC_FUEL_SUBCOUNT_FULL +
        CPC_FUEL_SUBCOUNT_FULL - game.fuelSubCounter;
    resetPlayerFuel(&game); game.lowSpeedLanding = 0;
    for (UWORD t = 0; t < 3000; t++) updatePlayerFuel(&game);
    UWORD cruise = (CPC_FUEL_GAUGE_LEVELS - game.fuelGaugeLevel) * CPC_FUEL_SUBCOUNT_FULL +
        CPC_FUEL_SUBCOUNT_FULL - game.fuelSubCounter;
    debugInfiniteFuel = oldInfinite;
    if (hover != cruise || !hover) return 6;
    game.gameMode = GAME_MODE_CLASSIC; game.speedLevel = 0;
    updateLowSpeedLanding(&game); if (game.lowSpeedLanding) return 7;
    game.gameMode = GAME_MODE_ENHANCED; game.landingState = LANDING_STATE_NONE;
    game.scrollX = 0;
    if (!siloWouldIntercept(&game, 104, 140, 100, 60, 0, 0) ||
        siloWouldIntercept(&game, 104, 140, 100, 60, 4, 0)) return 8;
    /* Placement must never alias a truck or put two silos on one screen. */
    LONG previous = -1000; UWORD count = 0;
    for (UWORD i = 0; i < cpcLandProceduralLength; i++) {
        UBYTE bit = 1 << (i & 7);
        if (!(missileSiloColumns[i >> 3] & bit)) continue;
        if ((missileTankColumns[i >> 3] & bit) || i - previous < GAME_MAP_WIDTH + 2 ||
            cpcLandGameplayTable[i].target != CPC_LAND_TARGET_TANK_FRONT) return 9;
        previous = i; count++;
    }
    if (!count) return 10;
    /* Matching missile trajectories cannot depend on the camera speed. */
    game.siloMissile.active = 1; game.siloMissile.worldX = 150;
    game.siloMissile.x = 150; game.siloMissile.y = 150; game.siloMissile.dy = -1;
    for (UBYTE t = 0; t < 16; t++) updateMissileSilo(&game, 0, 0);
    if (game.siloMissile.worldX != 150 || game.siloMissile.dy != -5 || game.siloMissile.y >= 120) return 11;
    UBYTE savedMod = modPlaying; modPlaying = 1;
    game.siloMissile.active = 0;
    game.helicopter.active = 1; game.helicopter.x = 180; game.helicopter.y = 40;
    game.helicopter.worldX = 180;
    game.playerX = 40; game.playerY = 40;
    game.rocketShot.active = 1; game.rocketShot.x = 180; game.rocketShot.y = 40;
    UBYTE hud = 0, weapon = 0, wing = 0;
    collideEnhancedEncounters(&game, 0, &hud, &weapon, &wing);
    if (game.helicopterHits != 1 || !game.helicopter.active || game.rocketShot.active) return 12;
    game.rocketShot.active = 1;
    collideEnhancedEncounters(&game, 0, &hud, &weapon, &wing);
    if (game.helicopterHits != 2 || !game.helicopter.active || game.helicopter.dy != 1 || !hud) return 13;
    modPlaying = savedMod;
    /* A non-grid formation target must settle from either side. */
    game.scrollX = 0; game.playerX = 77; game.playerY = 48;
    game.wingman.active = 1; game.wingman.mode = WINGMAN_FORMATION;
    game.wingman.row = 3; game.wingman.formationBelow = 0;
    game.wingman.formationSafetyValid = 0;
    for (UBYTE side = 0; side < 2; side++) {
        game.wingman.formationLogicalX = 53 + (side ? 3 : -3);
        game.wingman.interceptScreenX = game.wingman.formationLogicalX;
        for (UBYTE t = 0; t < 32; t++) updateWingmanFormationRow(&game);
        if (game.wingman.formationLogicalX != 53 || game.wingman.interceptScreenX != 53) return 16;
    }
    game.wingman.active = 0;
    /* Truck rise is precisely 3 pixels in four steps, horizontal drift 2. */
    memset(&game.enemyMissile, 0, sizeof(game.enemyMissile));
    game.enemyMissile.y = 120; game.playerY = 16;
    for (UBYTE t = 0; t < 4; t++) advanceMissileTankShot(&game, 0);
    if (game.enemyMissile.y != 117 || game.enemyMissile.worldX != 2) return 17;
    /* Overlapping halves + missile + smoke, all bit alignments and ring seam:
     * reverse restore must recover every original byte, including plane five. */
    UBYTE* pixels = AllocMem(2UL * GAME_WORLD_BITMAP_BYTES, MEMF_PUBLIC);
    if (!pixels) return 14;
    UBYTE* baseline = pixels + GAME_WORLD_BITMAP_BYTES;
    UBYTE savedMode = currentWorldPresentationMode; currentWorldPresentationMode = GAME_MODE_ENHANCED;
    UBYTE ok = 1; UBYTE failure = 15;
    game.siloMissile.active = game.helicopter.active = game.helicopterSmoke.active = 1;
    game.siloMissile.y = game.helicopter.y = game.helicopterSmoke.y = 48;
    game.siloMissile.x = game.helicopter.x = game.helicopterSmoke.x = 80;
    for (UBYTE shift = 0; shift < 16 && ok; shift++) {
        for (ULONG b = 0; b < GAME_WORLD_BITMAP_BYTES; b++) pixels[b] = baseline[b] = (UBYTE)(b * 37 + 11);
        LONG x = shift < 8 ? 80 + shift : GAME_WORLD_SCROLL_PAGE_BYTES * 8 - 8 + (shift & 7);
        game.siloMissile.worldX = game.helicopter.worldX = game.helicopterSmoke.worldX = x;
        game.scrollX = x - 80;
        for (UBYTE i = 0; i < HELICOPTER_BULLET_MAX; i++) {
            game.helicopterBullets[i].active = 1;
            game.helicopterBullets[i].worldX = x + ((shift & 1) ? i : 0);
            game.helicopterBullets[i].y = 48;
        }
        memset(encounterFootprints, 0, sizeof(encounterFootprints));
        drawEnhancedEncounterBobs(pixels, 0, &game);
        if (!encounterBytesDiffer(pixels, baseline, GAME_WORLD_BITMAP_BYTES)) ok = 0;
        retireEncounterBobs(pixels, 0);
        if (encounterBytesDiffer(pixels, baseline, GAME_WORLD_BITMAP_BYTES)) { ok = 0; failure = 30 + shift; }
    }
    memset(game.helicopterBullets, 0, sizeof(game.helicopterBullets));
    /* Independent renderer reference: compare the combined preshifted path
     * against the original two masked tile operations, for both rotor poses. */
    game.siloMissile.active = game.helicopterSmoke.active = 0;
    game.helicopterHits = 0;
    for (UBYTE frame = 0; frame < 2 && ok; frame++) {
        game.helicopterAge = frame * 4;
        for (UBYTE shift = 0; shift < 16 && ok; shift++) {
            for (ULONG i = 0; i < GAME_WORLD_BITMAP_BYTES; i++) pixels[i] = baseline[i] = (UBYTE)(i * 37 + 11);
            game.helicopter.worldX = shift < 8 ? 80 + shift : GAME_WORLD_SCROLL_PAGE_BYTES * 8 - 8 + (shift & 7);
            memset(encounterFootprints, 0, sizeof(encounterFootprints));
            memset(helicopterFootprints, 0, sizeof(helicopterFootprints));
            drawHelicopterBob(pixels, 0, &game);
            const UBYTE* source = enhancedEncounterTiles + 160 + frame * 80;
            drawEncounterTile(baseline, 0, &game.helicopter, 0, 0, source);
            drawEncounterTile(baseline, 0, &game.helicopter, 1, 8, source + 40);
            if (encounterBytesDiffer(pixels, baseline, GAME_WORLD_BITMAP_BYTES)) { ok = 0; failure = 50 + frame * 16 + shift; }
            memset(encounterFootprints, 0, sizeof(encounterFootprints));
            eraseHelicopter(pixels, 0);
            for (ULONG i = 0; i < GAME_WORLD_BITMAP_BYTES; i++)
                if (pixels[i] != (UBYTE)(i * 37 + 11)) { ok = 0; failure = 90; break; }
        }
    }
    /* The real silo launch path remains independent of both enemy slots. */
    modPlaying = 1;
    LONG start = -1;
    for (UWORD i = 0; i < HAR_LEVEL_SEGMENT_COUNT; i++)
        if (harLevelRoute[i].terrainKind == HAR_TERRAIN_CPC_RANDOM_LAND) { start = harLevelRoute[i].startColumn; break; }
    LONG siloColumn = start + previous;
    game.scrollX = siloColumn * 8 - 100; game.playerX = 100;
    game.playerY = terrainSurfacePixelYForWorldColumn(siloColumn) - 80;
    if (game.playerY < PLAYER_MIN_Y) game.playerY = PLAYER_MIN_Y;
    game.encounterPlayerX = game.playerX; game.encounterPlayerY = game.playerY;
    game.encounterPoseValid = 1; game.siloScanColumn = 0xffff;
    game.siloMissile.active = 0; game.helicopter.active = game.helicopterSmoke.active = 0;
    game.enemyPlane.active = game.enemyMissile.active = 1;
    updateMissileSilo(&game, 0, pixels);
    if (!game.siloMissile.active || !game.enemyPlane.active || !game.enemyMissile.active) ok = 0;
    game.siloMissile.active = 0;
    updateMissileSilo(&game, 0, pixels);
    if (game.siloMissile.active) ok = 0; /* one shot per intact silo */
    /* Incremental cache: entry at the right edge, backward removal and
     * re-entry must find the same already-fired silo without relaunching. */
    game.scrollX = (siloColumn - GAME_MAP_WIDTH - 1) * 8;
    game.siloScanColumn = game.siloCandidateColumn = 0xffff;
    updateMissileSilo(&game, 0, pixels);
    if (game.siloCandidateColumn != 0xffff) { ok = 0; failure = 91; }
    game.scrollX += 8; updateMissileSilo(&game, 0, pixels);
    if (game.siloCandidateColumn != siloColumn || game.siloMissile.active) { ok = 0; failure = 92; }
    game.scrollX -= 8; updateMissileSilo(&game, 0, pixels);
    if (game.siloCandidateColumn != 0xffff) { ok = 0; failure = 93; }
    game.scrollX += 8; updateMissileSilo(&game, 0, pixels);
    if (game.siloCandidateColumn != siloColumn || game.siloMissile.active) { ok = 0; failure = 94; }

    /* No level-one helicopter, then one admission on level two. */
    game.missionNumber = 1; game.helicopterCooldown = 0;
    game.helicopter.active = 0; game.helicopterSmoke.active = 0;
    updateHelicopter(&game, 0, pixels);
    if (game.helicopter.active) ok = 0;
    game.missionNumber = 2;
    game.scrollX = start * 8;
    updateHelicopter(&game, 0, pixels);
    if (!game.helicopter.active || game.helicopterHold != 499) ok = 0;
    /* The chase deadline stops world motion; it does not prolong itself. */
    game.helicopterHold = 0; game.helicopterAge = 1;
    LONG heldX = game.helicopter.worldX;
    updateHelicopter(&game, 4, pixels);
    if (!game.helicopterStopped || game.helicopter.worldX != heldX) ok = 0;
    /* It must stop a safe distance before entering a town segment. */
    for (UWORD i = 0; i < HAR_LEVEL_SEGMENT_COUNT; i++) {
        if (harLevelRoute[i].terrainKind != HAR_TERRAIN_TOWN) continue;
        game.helicopter.worldX = harLevelRoute[i].startColumn * 8 - 40;
        game.scrollX = game.helicopter.worldX - 180;
        game.helicopterStopped = 0; game.helicopterHold = 100;
        heldX = game.helicopter.worldX;
        updateHelicopter(&game, 4, pixels);
        if (!game.helicopterStopped || game.helicopter.worldX != heldX) ok = 0;
        break;
    }
    /* A passed player cannot receive another flak burst. */
    game.helicopterAge = 89; game.helicopter.x = 40;
    game.helicopter.worldX = game.scrollX + 40; game.playerX = 100;
    memset(game.helicopterBullets, 0, sizeof(game.helicopterBullets));
    updateHelicopter(&game, 0, pixels);
    for (UBYTE i = 0; i < HELICOPTER_BULLET_MAX; i++) if (game.helicopterBullets[i].active) ok = 0;
    modPlaying = savedMod;
    currentWorldPresentationMode = savedMode;
    FreeMem(pixels, 2UL * GAME_WORLD_BITMAP_BYTES);
    return ok ? 0 : failure;
}
#endif
