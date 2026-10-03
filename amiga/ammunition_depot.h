/* At most two candidate columns and one victim per tick; two victims total.
 * The large retained BOB uses the otherwise idle land-side bomber slots. */
static __attribute__((noinline, optimize("Os"))) UBYTE updateAmmoDepotBlast(GameState* game, UBYTE** buffers) {
#if HAR_HEADLESS_AUTOPLAY && HAR_HEADLESS_AMMO_EXERCISE
    LONG testColumn = ((LONG)game->scrollX + 200) >> 3;
    if (!game->ammoBlast.active && isAmmoDepotColumn(testColumn) && !isTargetDestroyedAtColumn(testColumn)) {
        startAmmoDepotBlast(game, testColumn);
        markTargetDestroyedAtColumn(testColumn);
        if (buffers && buffers[0]) dirtyRedrawGroundTarget(buffers, testColumn);
    }
#endif
    WeaponState* blast = &game->ammoBlast;
    if (!blast->active) return 0;
    if (game->gameMode != GAME_MODE_ENHANCED || game->defence.phase || game->gameOver) {
        blast->active = 0; return 0;
    }
    blast->x = blast->worldX - game->scrollX;
    if (!--blast->timer || blast->x < -40) { blast->active = 0; return 0; }
    if (blast->dy) { blast->dy--; return 0; }
    if (blast->guidanceDistance >= 2) return 0;
    for (UBYTE probe = 0; probe < 2 && blast->dx <= 8; probe++, blast->dx++) {
        LONG column = blast->targetWorldX + blast->dx;
        const LevelSegmentDef* segment = levelSegmentForWorldColumn(column);
        if (!segment || segment->terrainKind != HAR_TERRAIN_CPC_RANDOM_LAND ||
            column == blast->targetWorldX || isTargetDestroyedAtColumn(column) || isFuelDepotColumn(column)) continue;
        LONG local = column - segment->startColumn;
        if (local < 0 || local >= cpcLandProceduralLength) continue;
        UBYTE target = cpcLandProceduralTarget((UWORD)local);
        if (target == CPC_LAND_TARGET_NONE || target == CPC_LAND_TARGET_TANK_REAR) continue;
        WORD row = terrainYForWorldColumn(column, segment, segment->terrainKind) - 1;
        WORD delta = row * 8 - (blast->y + 8);
        if (delta < -24 || delta > 24) continue;
        markTargetDestroyedAtColumn(column);
        if (!isTargetDestroyedAtColumn(column)) continue;
        blast->dx++; blast->guidanceDistance++; blast->dy = 4;
#if HAR_DEBUG_PERF_LOG
        ammoPerfVictims++;
#endif
        awardGameScore(game, GROUND_TARGET_SCORE_VALUE); game->hitsCount++;
        if (game->targetLock.active && groundTargetAnchorColumn(game->targetLock.worldX / 8) == column)
            clearTargetLockWithTelemetry(game, HAR_OBJ_GROUND_TARGET);
        addCpcHitSmokeAtColumnRow(column, row);
        if (buffers && buffers[0]) dirtyRedrawGroundTarget(buffers, column);
        startGroundTargetHitImpact(game, column * 8 - game->scrollX, column, row, HAR_OBJ_GROUND_TARGET);
        updateHudValues(game);
        return 1;
    }
    return 0;
}
