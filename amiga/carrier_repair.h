/* Compatibility landing-pad query; terrain VTOL/loading logic was retired. */
static DEFENCE_SMALL LONG repairLandingColumn(const GameState* g, WORD x, WORD y) {
    if (g->gameMode != GAME_MODE_ENHANCED || !g->defence.fullVtol) return -1;
    LONG center = ((LONG)g->scrollX + x + 8) >> 3;
    for (LONG col = center - 1; col <= center; col++) {
        if (repairDepotLocalColumn(col) < 0 || isTargetDestroyedAtColumn(col)) continue;
        LONG left = col * 8 - g->scrollX;
        const LevelSegmentDef* seg = levelSegmentForWorldColumn(col);
        WORD top = (terrainYForWorldColumn(col, seg, HAR_TERRAIN_CPC_RANDOM_LAND) - 1) * 8;
        if (x >= left - 2 && x <= left + 2 && y + PLAYER_SPRITE_HEIGHT == top) return col;
    }
    return -1;
}
