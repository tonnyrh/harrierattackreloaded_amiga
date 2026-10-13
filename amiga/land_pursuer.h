/* Enhanced land interceptor. Reuses the existing attached aircraft sprite,
 * one enemy missile slot and the ordinary pickup BOB. No new DMA channels. */
static UBYTE pursuerFootprintClear(LONG x, WORD y, WORD width) {
    if (y < 0 || y + ENEMY_SPRITE_HEIGHT >= HUD_TOP) return 0;
    for (LONG col = x >> 3; col <= (x + width - 1) >> 3; col++) {
        if (y + ENEMY_SPRITE_HEIGHT > radarSurfacePixelYForWorldColumn(col)) return 0;
    }
    return 1;
}

static UBYTE trySpawnLandPursuer(GameState* game, UWORD decisionColumn) {
    if (game->gameMode != GAME_MODE_ENHANCED || game->defence.phase ||
        !game->ordinaryPlaneSeen || game->pursuerUsed || game->enemyPlane.active || game->enemyMissile.active ||
        game->playerX < 64 || game->ejectState || game->radarDetection < RADAR_DETECTION_MAX) return 0;
    const LevelSegmentDef* segment = levelSegmentForWorldColumn(decisionColumn);
    UBYTE stage = stageForWorldColumn(decisionColumn, segment);
    if (stage < HAR_STAGE_DO_LAND || stage >= HAR_STAGE_START_PIER) return 0;
    /* One roll per normal radar admission, rising from 1/16 to certainty
     * in the last third. Never bypass radar or a blocked approach corridor. */
    ULONG end = POWERUP_EXTRA_AIRCRAFT_SCROLL_X;
    UWORD chance = end ? (UWORD)((ULONG)game->scrollX * 24 / end) : 16;
    if (chance < 1) chance = 1;
    UWORD roll = (UWORD)(game->campaignSeed ^ decisionColumn ^ (game->missionNumber * 73));
    roll = (UWORD)(roll * 1509u + 41u);
    if (chance < 16 && ((roll >> 8) & 15) >= chance) return 0;
    WORD y = game->playerY;
    /* A living Wingman may occupy the trailing formation lane. Try a higher
     * clear approach before deferring; never spawn on top of another plane. */
    if (game->wingman.active && wingmanScreenX(game) < game->playerX &&
        game->wingman.screenY + ENEMY_SPRITE_HEIGHT > y - 8 &&
        game->wingman.screenY < y + ENEMY_SPRITE_HEIGHT + 8)
        y = game->wingman.screenY - 24;
    if (!pursuerFootprintClear(game->scrollX, y, game->playerX - 24)) return 0;
    if (game->helicopter.active && game->helicopter.x < game->playerX &&
        game->helicopter.y + 16 > y - 8 && game->helicopter.y < y + 16) return 0;
    WeaponState* p = &game->enemyPlane;
    memset(p, 0, sizeof(*p));
    p->active = 1; p->x = 0; p->y = y; p->direction = 1;
    p->worldX = p->targetWorldX = game->scrollX;
    p->targetY = y; p->dx = 1;
    game->pursuerUsed = game->pursuerActive = 1;
    game->pursuerFireCooldown = 100;
    game->enemyPlaneDamageState = ENEMY_PLANE_DAMAGE_NORMAL;
    game->enemyPlaneBrokenTimer = 0;
    game->enemyPlaneRetreating = 0;
    game->enemyMissileTarget = ENEMY_TARGET_PLAYER;
    game->radarDetection = 0;
    game->radarAlarmTickCountdown = 0;
    maybeStartWingmanIntercept(game);
    return 1;
}

static UBYTE updateLandPursuer(GameState* game) {
    WeaponState* p = &game->enemyPlane;
    /* Braking makes it overshoot. Once ahead it continues out of the screen,
     * leaving a window for Harrier's forward rockets. Never teleports back. */
    if (p->x >= game->playerX - 8) game->enemyPlaneRetreating = 1;
    if (game->enemyPlaneRetreating || game->speedLevel < 5) p->x += 2;
    else {
        WORD holdX = game->playerX - 64;
        if (p->x < holdX) p->x++;
        else if (p->x > holdX) p->x--;
    }
    p->timer++;
    if (!game->enemyPlaneRetreating && !(p->timer & 3)) {
        if (p->y < game->playerY) p->y++;
        else if (p->y > game->playerY) p->y--;
    }
    p->worldX = p->targetWorldX = (LONG)game->scrollX + p->x;
    p->targetY = p->y;
    if (p->x >= SCREEN_WIDTH) { p->active = 0; game->pursuerActive = 0; return 1; }
    if (!pursuerFootprintClear(p->worldX, p->y, ENEMY_SPRITE_WIDTH)) {
        startWorldImpact(game, p->x, p->y);
        p->active = 0; game->pursuerActive = 0;
        return 1; /* Terrain crashes do not qualify for a rescue pilot. */
    }
    if (game->pursuerFireCooldown) game->pursuerFireCooldown--;
    if (!game->enemyPlaneRetreating && p->x + ENEMY_SPRITE_WIDTH < game->playerX &&
        !game->pursuerFireCooldown && !game->enemyMissile.active &&
        !game->gameOver && !game->crashTimer && !game->ejectState) {
        WeaponState* m = &game->enemyMissile;
        memset(m, 0, sizeof(*m));
        m->active = 1; m->type = ENEMY_MISSILE_PURSUER_TYPE;
        m->x = p->x + ENEMY_SPRITE_WIDTH; m->y = p->y + 3;
        m->worldX = (LONG)game->scrollX + m->x; m->dx = 6;
        game->enemyMissileTarget = ENEMY_TARGET_PLAYER;
        game->enemyMissileFromShip = 0;
        game->pursuerFireCooldown = 125;
        playSfxAt(SFX_FIRE, p->x);
    }
    return 1;
}

static void deliverRescuedPilot(GameState* game) {
    if (!game->rescuedPilotPending) return;
    if (game->rescuedPilotWorldX + 16 < (LONG)game->scrollX || game->defence.phase) {
        game->rescuedPilotPending = 0; return;
    }
    /* Do not overwrite an existing repair kit or pickup. */
    if (game->powerup.active || game->defence.repairDropPending) return;
    spawnPowerup(game, POWERUP_RESCUED_PILOT, 0);
    game->powerup.worldX = game->rescuedPilotWorldX;
    game->powerup.y = game->powerup.logicalY = game->rescuedPilotY;
    game->powerup.fallRate = 6;
    game->rescuedPilotPending = 0;
}

static void landPursuerWeaponKill(GameState* game, UBYTE awardScore) {
    if (game->gameMode == GAME_MODE_ENHANCED && !game->defence.phase) {
        if (game->pursuerActive) {
            if (awardScore && game->ordinaryPlaneKilled) {
                game->rescuedPilotPending = 1;
                game->rescuedPilotWorldX = game->enemyPlane.worldX;
                game->rescuedPilotY = game->enemyPlane.y;
            }
        } else if (awardScore) game->ordinaryPlaneKilled = 1;
    }
}

/* Plain rocket: the release height is fixed, with no seeker or target updates. */
static void advanceLandPursuerMissile(GameState* game) {
    WeaponState* missile=&game->enemyMissile;
    missile->dy=0;
    missile->worldX+=missile->dx;
    missile->x=(WORD)(missile->worldX-game->scrollX);
}
