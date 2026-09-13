/* Enhanced-only bounded encounters. Included after the shared gameplay helpers. */
static __attribute__((noinline, optimize("Os"))) WORD encounterClampVelocity(WORD value) {
    return value < -4 ? -4 : (value > 4 ? 4 : value);
}

/* Integrated acceleration, calculated offline. Six comparisons replace a
 * 64-step trajectory simulation on every candidate frame. */
static const UWORD siloRiseByStep[65] = {0,1,2,3,5,7,9,11,14,17,20,23,27,31,35,39,44,49,54,59,65,71,77,83,89,95,101,107,113,119,125,131,137,143,149,155,161,167,173,179,185,191,197,203,209,215,221,227,233,239,245,251,257,263,269,275,281,287,293,299,305,311,317,323,329};
static __attribute__((noinline, optimize("Os"))) UBYTE siloWouldIntercept(const GameState* game, LONG launchX, WORD launchY,
    WORD targetX, WORD targetY, WORD velocityX, WORD velocityY) {
    WORD distance = launchY - targetY - 7;
    UBYTE lo = 1, hi = 64;
    while (lo < hi) {
        UBYTE mid = (lo + hi) >> 1;
        WORD closed = siloRiseByStep[mid] + velocityY * mid;
        if (closed >= distance) hi = mid; else lo = mid + 1;
    }
    /* Cover the complete small overlap window, not just its first scanline. */
    for (UBYTE t = lo; t <= 64; t++) {
        WORD y = targetY + velocityY * t;
        WORD missileY = launchY - siloRiseByStep[t];
        WORD previousY = launchY - siloRiseByStep[t - 1];
        if (previousY + 8 < y) break;
        LONG dx = (LONG)game->scrollX + targetX + 8 + velocityX * t - (launchX + 4);
        if (y >= PLAYER_MIN_Y && y <= PLAYER_MAX_Y && missileY <= y + 7 &&
            previousY + 8 >= y && dx >= -7 && dx <= 7) return 1;
    }
    return 0;
}

static __attribute__((noinline, optimize("Os"))) void updateMissileSilo(GameState* game, UBYTE scrollPixels, UBYTE* bitmap) {
    WeaponState* shot = &game->siloMissile;
    if (shot->active) {
        shot->timer++;
        if (!(shot->timer & 3) && shot->dy > -6) shot->dy--;
        shot->y += shot->dy;
        shot->x = (WORD)(shot->worldX - game->scrollX);
        if (shot->y < 0 || shot->x <= -8 || shot->x >= SCREEN_WIDTH) shot->active = 0;
        return;
    }
    if (!game->encounterPoseValid) return;
    LONG first = game->scrollX >> 3;
    /* Search only newly exposed columns after the initial viewport scan.
     * The spacing rule guarantees one candidate, so a retained visible
     * anchor needs no terrain lookups even after it has fired. */
    if (game->siloScanColumn != first) {
        LONG oldFirst = game->siloScanColumn;
        LONG scanFrom = first, scanTo = first + GAME_MAP_WIDTH;
        if (oldFirst != 0xffff && first > oldFirst && first - oldFirst <= GAME_MAP_WIDTH)
            scanFrom = oldFirst + GAME_MAP_WIDTH + 1;
        else if (oldFirst != 0xffff && first < oldFirst && oldFirst - first <= GAME_MAP_WIDTH)
            scanTo = oldFirst - 1;
        game->siloScanColumn = first;
        if (game->siloCandidateColumn < first || game->siloCandidateColumn > first + GAME_MAP_WIDTH)
            game->siloCandidateColumn = 0xffff;
        if (game->siloCandidateColumn == 0xffff)
            for (LONG column = scanFrom; column <= scanTo; column++)
                if (missileSiloLocalColumn(column) >= 0) {
                    game->siloCandidateColumn = column; break;
                }
    }
    if (game->siloCandidateColumn == 0xffff) return;
    /* A 42-column placement gap guarantees at most one 16px silo visible. */
    for (LONG column = game->siloCandidateColumn; column <= game->siloCandidateColumn; column++) {
        LONG local = missileSiloLocalColumn(column);
        if (local < 0 || (missileSiloFired[local >> 3] & (1 << (local & 7))) ||
            isTargetDestroyedAtColumn(column) || !groundTargetIsTwoColumnTank(column)) continue;
        LONG x = column * 8 + 4;
        WORD y = terrainSurfacePixelYForWorldColumn(column) - 16;
        WORD vx = scrollPixels + encounterClampVelocity(game->playerX - game->encounterPlayerX);
        WORD vy = encounterClampVelocity(game->playerY - game->encounterPlayerY);
        UBYTE fire = siloWouldIntercept(game, x, y, game->playerX, game->playerY, vx, vy);
        if (!fire && game->wingman.active) {
            WORD wingX = wingmanScreenX(game);
            vx = scrollPixels + encounterClampVelocity(wingX - game->encounterWingX);
            vy = encounterClampVelocity(game->wingman.screenY - game->encounterWingY);
            fire = siloWouldIntercept(game, x, y, wingX, game->wingman.screenY, vx, vy);
        }
        if (!fire) continue;
        missileSiloFired[local >> 3] |= 1 << (local & 7);
        ENCOUNTER_STAT(0);
        memset(shot, 0, sizeof(*shot));
        shot->active = 1; shot->worldX = x;
        shot->x = (WORD)(x - game->scrollX); shot->y = y; shot->dy = -1;
        shot->type = ROCKET_SHOT_MAVERICK_LAUNCH;
        shot->direction = MAVERICK_DIRECTION_UP;
        invalidateBobEraseColumns(column, column + 1);
        bobCompositorErase(bitmap, column, (y + 8) >> 3, 2);
        playSfxAt(SFX_FIRE, shot->x);
        break;
    }
}

static __attribute__((noinline, optimize("Os"))) void stopHelicopterAudio(void) {
    for (UBYTE channel = 0; channel < SFX_CHANNEL_COUNT; channel++)
        if (sfxChannelCurrentId[channel] == SFX_HELICOPTER ||
            sfxChannelPendingId[channel] == SFX_HELICOPTER) stopSfxChannel(channel);
}

static __attribute__((noinline, optimize("Os"))) WORD helicopterTerrainY(GameState* game, LONG worldX) {
    UWORD column = worldX >> 3;
    if (game->helicopterTerrainColumn == column) return game->helicopterTerrainTarget;
    game->helicopterTerrainColumn = column;
    WORD surface = GAME_WORLD_HEIGHT;
    /* Look ahead over slopes before the fuselage reaches their collision cell. */
    for (UBYTE i = 0; i < 6; i++) {
        WORD y = terrainSurfacePixelYForWorldColumn((worldX >> 3) + i);
        if (y < surface) surface = y;
    }
    WORD y = surface - 32;
    game->helicopterTerrainTarget = y < PLAYER_MIN_Y ? PLAYER_MIN_Y : y;
    return game->helicopterTerrainTarget;
}

static __attribute__((noinline, optimize("Os"))) void updateHelicopter(GameState* game, UBYTE scrollPixels, UBYTE* bitmap) {
    WeaponState* heli = &game->helicopter;
    WeaponState* smoke = &game->helicopterSmoke;
    if (smoke->active) {
        smoke->timer++;
        if (!(smoke->timer & 3)) smoke->y--;
        smoke->x = (WORD)(smoke->worldX - game->scrollX);
        if (smoke->timer >= 22 || smoke->x < -8) smoke->active = 0;
    }
    if (!heli->active) {
        if (game->helicopterCooldown) { game->helicopterCooldown--; return; }
        if (game->missionNumber < 2) return;
        LONG spawnX = (LONG)game->scrollX + SCREEN_WIDTH - 24;
        const LevelSegmentDef* segment = levelSegmentForWorldColumn(spawnX >> 3);
        if (!segment || segment->terrainKind != HAR_TERRAIN_CPC_RANDOM_LAND) return;
        memset(heli, 0, sizeof(*heli));
        heli->active = 1; heli->worldX = spawnX; ENCOUNTER_STAT(1);
        heli->x = SCREEN_WIDTH - 24; heli->y = helicopterTerrainY(game, spawnX);
        game->helicopterAge = 0; game->helicopterHits = 0; game->helicopterStopped = 0;
        UWORD extra = game->missionNumber > 12 ? 10 : game->missionNumber - 2;
        game->helicopterHold = 500 + extra * 100;
    }
    game->helicopterAge++;
    if (game->helicopterHits >= 2) {
        if (!(game->helicopterAge & 3) && heli->dy < 5) heli->dy++;
        heli->y += heli->dy;
        if (heli->y + 8 >= terrainSurfacePixelYForWorldColumn(heli->worldX >> 3)) {
            startWorldImpact(game, (WORD)(heli->worldX - game->scrollX), heli->y);
            heli->active = 0; game->helicopterCooldown = 750;
        }
    } else {
        const LevelSegmentDef* ahead = levelSegmentForWorldColumn((heli->worldX + 40) >> 3);
        if (!ahead || ahead->terrainKind == HAR_TERRAIN_TOWN) game->helicopterStopped = 1;
        if (game->helicopterHold) game->helicopterHold--;
        else game->helicopterStopped = 1;
        if (!game->helicopterStopped) {
            WORD desired = game->playerX + 96;
            if (desired > SCREEN_WIDTH - 32) desired = SCREEN_WIDTH - 32;
            heli->worldX += scrollPixels;
            WORD x = (WORD)(heli->worldX - game->scrollX);
            if (x > desired) heli->worldX--;
            else if (x < desired) heli->worldX++;
        }
        WORD targetY = helicopterTerrainY(game, heli->worldX);
        WORD dy = targetY - heli->y;
        heli->y += encounterClampVelocity(dy);
        heli->x = (WORD)(heli->worldX - game->scrollX);
        /* Fixed simulation cadence, sparse shots, and no firing behind player. */
        if (!(game->helicopterAge % 65) && heli->x > game->playerX + 16 &&
            heli->x > 0 && heli->x < SCREEN_WIDTH) {
            LONG column = ((LONG)game->scrollX + game->playerX + 32) >> 3;
            WORD row = (game->playerY + 4) >> 3;
            ObjectCell cell;
            if (aircraftObjectCell(column, row, &cell) &&
                (cell.id == HAR_OBJ_SKY || cell.id == HAR_OBJ_CLOUD) &&
                addRuntimeFlak(column, row, 57)) {
                ENCOUNTER_STAT(2);
                bobCompositorErase(bitmap, column, row, 1);
                playSfxAt(SFX_FLAK_GUN_1, heli->x);
            }
        }
        UBYTE pulse = game->helicopterHits ? 10 : 7;
        if (!(game->helicopterAge % pulse) && heli->x >= 0 && heli->x < SCREEN_WIDTH)
            playSfxAtTuned(SFX_HELICOPTER, heli->x, 24, game->helicopterHits ? 560 : 443);
        /* One 22-step cosmetic puff every two seconds; never an object-map hazard. */
        if (game->helicopterHits == 1 && !(game->helicopterAge % 100)) {
            memset(smoke, 0, sizeof(*smoke)); smoke->active = 1; ENCOUNTER_STAT(5);
            smoke->worldX = heli->worldX + 11; smoke->y = heli->y + 3;
            smoke->x = (WORD)(smoke->worldX - game->scrollX);
        }
    }
    heli->x = (WORD)(heli->worldX - game->scrollX);
    if (heli->x < -16) {
        heli->active = 0; game->helicopterCooldown = 750; stopHelicopterAudio();
    }
}

static __attribute__((noinline, optimize("Os"))) void updateEnhancedEncounters(GameState* game, UBYTE scrollPixels, UBYTE* bitmap) {
    if (game->gameMode != GAME_MODE_ENHANCED || game->takeoffState != TAKEOFF_STATE_AIRBORNE ||
        game->landingState != LANDING_STATE_NONE || game->missionComplete ||
        game->gameOver || game->crashTimer || game->ejectState) return;
#if HAR_HEADLESS_AUTOPLAY && HAR_HEADLESS_OMIT_ENCOUNTERS
    return;
#endif
#if HAR_DEBUG_PERF_LOG
    UWORD begin = currentRasterY();
#endif
    updateMissileSilo(game, scrollPixels, bitmap);
    updateHelicopter(game, scrollPixels, bitmap);
    game->encounterPlayerX = game->playerX; game->encounterPlayerY = game->playerY;
    game->encounterWingX = wingmanScreenX(game); game->encounterWingY = game->wingman.screenY;
    game->encounterPoseValid = 1;
#if HAR_DEBUG_PERF_LOG
    UWORD end = currentRasterY();
    ULONG cost = end >= begin ? end - begin : end + 312 - begin;
    encounterCosts[0] += cost; encounterCosts[3]++;
    if (cost > encounterCosts[4]) encounterCosts[4] = cost;
#endif
}

static __attribute__((noinline, optimize("Os"))) void collideEnhancedEncounters(GameState* game, UBYTE** buffers,
    UBYTE* hudDirty, UBYTE* weaponDirty, UBYTE* wingDirty) {
    if (game->gameMode != GAME_MODE_ENHANCED || game->gameOver || game->crashTimer) return;
    WeaponState* missile = &game->siloMissile;
    WeaponState* heli = &game->helicopter;
    WeaponState* rockets[2] = { &game->rocketShot, &game->wingman.rocket };
    for (UBYTE i = 0; i < 2; i++) {
        WeaponState* shot = rockets[i];
        if (!shot->active) continue;
        if (missile->active && rectsOverlap(shot->x, shot->y, 8, 8, missile->x, missile->y, 8, 8)) {
            shot->active = missile->active = 0;
            awardGameScore(game, ENEMY_MISSILE_SCORE_VALUE); game->hitsCount++;
            startWorldImpact(game, missile->x, missile->y); *hudDirty = *weaponDirty = 1;
        } else if (heli->active && game->helicopterHits < 2 &&
            rectsOverlap(shot->x, shot->y, 8, 8, heli->x, heli->y, 16, 8)) {
            shot->active = 0; game->helicopterHits++; ENCOUNTER_STAT(3);
            playSfxAt(SFX_HIT, heli->x); *weaponDirty = 1;
            if (game->helicopterHits == 2) {
                stopHelicopterAudio(); heli->dy = 1; ENCOUNTER_STAT(4);
                awardGameScore(game, ENEMY_SCORE_VALUE); game->hitsCount++;
                *hudDirty = 1;
            }
        }
    }
    if (missile->active && game->respawnSafeTimer == 0 &&
        rectsOverlap(game->playerX, game->playerY, 16, 8, missile->x, missile->y, 8, 8)) {
        missile->active = 0; applyPlayerMissileDamage(game, 0);
        *hudDirty = *weaponDirty = 1;
    }
    if (game->wingman.active && missile->active &&
        rectsOverlap(wingmanScreenX(game), game->wingman.screenY, 16, 8, missile->x, missile->y, 8, 8)) {
        missile->active = 0; destroyWingman(game, buffers); *wingDirty = 1;
    }
    if (heli->active && game->helicopterHits < 2 && game->respawnSafeTimer == 0 &&
        rectsOverlap(game->playerX, game->playerY, 16, 8, heli->x, heli->y, 16, 8)) {
        game->helicopterHits = 2; heli->dy = 1; stopHelicopterAudio();
        startAircraftFailure(game, AIRCRAFT_FAILURE_CAUSE_AIRCRAFT); *hudDirty = *weaponDirty = 1;
    }
}
