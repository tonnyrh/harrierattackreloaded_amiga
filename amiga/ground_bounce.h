/* Enhanced ground recoil: no new sprites, allocation or runtime synthesis. */
static void updatePlayerGroundBounce(GameState* g) {
    if (!g->groundBounceTicks) return;
    if (g->gameMode != GAME_MODE_ENHANCED || g->crashTimer || g->ejectState ||
        g->aircraftFailureState || g->defence.phase) { g->groundBounceTicks = 0; return; }
    WORD rise = g->groundBounceTicks > 6 ? 3 : (g->groundBounceTicks > 2 ? 2 : 1);
    g->playerY -= rise;
    if (g->playerY < PLAYER_MIN_Y) g->playerY = PLAYER_MIN_Y;
    g->groundBounceTicks--;
}

static UBYTE handleEnhancedGroundBounce(GameState* g, UBYTE** buffers,
    LONG column, WORD row) {
    if (g->gameMode != GAME_MODE_ENHANCED || g->defence.phase ||
        g->takeoffState != TAKEOFF_STATE_AIRBORNE || g->crashTimer ||
        g->ejectState || g->aircraftFailureState || g->gameOver || g->respawnSafeTimer) return 0;
    ObjectCell cell;
    if (!aircraftObjectCell(column, row, &cell) ||
        (cell.id != HAR_OBJ_LAND && cell.id != HAR_OBJ_GROUND_TARGET &&
         cell.id != HAR_OBJ_TOWN_BLOCK)) return 0;
    /* Clear the contacted cell and the steepest surface under the full body.
     * Repeated geometry contact during recoil separates without extra damage. */
    WORD top = row * GAME_TILE_HEIGHT;
    LONG left = ((LONG)g->scrollX + g->playerX + 2) >> 3;
    LONG right = ((LONG)g->scrollX + g->playerX + PLAYER_SPRITE_WIDTH - 3) >> 3;
    for (LONG c=left; c<=right; c++) {
        WORD surface = terrainSurfacePixelYForWorldColumn(c);
        if (surface < top) top = surface;
    }
    WORD clearY = top - PLAYER_SPRITE_HEIGHT - 1;
    if (clearY < PLAYER_MIN_Y) clearY = PLAYER_MIN_Y;
    if (g->playerY > clearY) g->playerY = clearY;
    if (g->groundBounceTicks) return 1;
    UBYTE target = cell.id == HAR_OBJ_GROUND_TARGET;
    if (target) {
        LONG anchor = groundTargetAnchorColumn(column);
        startAmmoDepotBlast(g, anchor);
        markTargetDestroyedAtColumn(anchor);
        if (g->targetLock.active && groundTargetAnchorColumn(g->targetLock.worldX / GAME_TILE_WIDTH) == anchor)
            clearTargetLockWithTelemetry(g, cell.id);
        addCpcHitSmokeAtColumnRow(column, row);
        if (buffers && buffers[0]) dirtyRedrawGroundTarget(buffers, anchor);
        awardGameScore(g, GROUND_TARGET_SCORE_VALUE); g->hitsCount++;
        startWorldImpactQuiet(g, (WORD)(column * 8 - g->scrollX), row * 8);
    }
    /* Reuse the existing single smoke BOB; ground impact takes priority over
     * a distant helicopter puff without adding any draw slots on A500. */
    memset(&g->helicopterSmoke, 0, sizeof(g->helicopterSmoke));
    g->helicopterSmoke.active = 1;
    g->helicopterSmoke.worldX = column * GAME_TILE_WIDTH;
    g->helicopterSmoke.x = g->helicopterSmoke.worldX - g->scrollX;
    g->helicopterSmoke.y = top >= 8 ? top - 8 : 0;
    g->missileDamageThirds += target ? 120 : 150;
    if (g->missileDamageThirds > 300) g->missileDamageThirds = 300;
    updatePlayerDamageArmour(g);
    playSfxAt(SFX_GROUND_BOUNCE, g->playerX);
    if (!g->armour) startAircraftFailure(g, AIRCRAFT_FAILURE_CAUSE_ARMOUR);
    else g->groundBounceTicks = 8;
    return 1;
}
