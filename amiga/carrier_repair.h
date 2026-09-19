/* Optional terrain stop: full VTOL, a vulnerable loading pad and repair cargo. */
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

static DEFENCE_SMALL UBYTE updateTerrainVtol(GameState* g, const InputState* in) {
    CarrierDefenceState* d = &g->defence;
    if (g->gameMode != GAME_MODE_ENHANCED || g->takeoffState != TAKEOFF_STATE_AIRBORNE ||
        g->landingState || g->missionComplete || g->crashTimer || g->ejectState || g->aircraftFailureState) {
        d->leftHold = d->rightHold = d->fullVtol = 0; return 0;
    }
    if (!d->fullVtol) {
        if (in->left && !in->right && g->speedLevel == 0) {
            if (++d->leftHold >= 35) {
                d->fullVtol = 1; d->centerVtol = 1; d->leftHold = 0;
                d->repairLoad = 0; d->repairPad = -1;
                playSfxAtTuned(SFX_RADAR_ALARM, g->playerX, 20, 700); return 1;
            }
        } else d->leftHold = 0;
    } else {
        if (in->right && !in->left) {
            if (++d->rightHold >= 40) {
                d->fullVtol = d->rightHold = d->centerVtol = 0;
                d->repairLoad = 0; d->repairPad = -1; g->speedLevel = 5;
                return 1;
            }
        } else d->rightHold = 0;
    }
    return 0;
}

static DEFENCE_SMALL void moveTerrainVtol(GameState* g, const InputState* in) {
    CarrierDefenceState* d = &g->defence;
    d->vtolPose = in->left && !in->right ? 2 : (in->right && !in->left ? 1 : 0);
    WORD x = g->playerX, y = g->playerY;
    if (d->centerVtol) {
        if (x < 152) x += 2; else if (x > 152) x -= 2;
        if (x >= 151 && x <= 153) { x = 152; d->centerVtol = 0; }
    } else x += (in->right ? 2 : 0) - (in->left ? 2 : 0);
    y += (in->down ? 2 : 0) - (in->up ? 2 : 0);
    if (x < 8) x = 8; if (x > 296) x = 296;
    if (y < PLAYER_MIN_Y) y = PLAYER_MIN_Y; if (y > PLAYER_MAX_Y) y = PLAYER_MAX_Y;
    /* Catch a descending aircraft crossing the pad height, not just an
     * exact coordinate. This is essential with odd starting Y positions. */
    for (UBYTE offset = 0; offset < 3; offset++) {
        WORD candidateY = y - offset;
        if (g->playerY <= candidateY && repairLandingColumn(g, x, candidateY) >= 0) { y = candidateY; break; }
    }
    g->playerX = x; g->playerY = y;
}

static DEFENCE_SMALL UBYTE updateCarrierRepair(GameState* g, UBYTE** buffers) {
    if (g->gameMode != GAME_MODE_ENHANCED || g->defence.phase) return 0;
    CarrierDefenceState* d = &g->defence; d->clock++;
    LONG pad = repairLandingColumn(g, g->playerX, g->playerY);
    UBYTE changed = 0;
    if (pad >= 0) {
        LONG local = repairDepotLocalColumn(pad);
        if (pad != d->repairPad) { d->repairPad = pad; d->repairLoad = 0; }
        if (!(repairDepotCollected[local >> 3] & (1 << (local & 7))) && d->cargo < 60) {
            if (!d->repairLoad && !d->repairAmbush) {
                d->repairAmbush = 1; d->phaseTicks = 60;
                playSfxAtTuned(SFX_RADAR_ALARM, g->playerX, 40, 420);
            }
            if (++d->repairLoad >= 150) {
                repairDepotCollected[local >> 3] |= 1 << (local & 7);
                d->cargo += 20; d->repairLoad = 150;
                playSfxAt(SFX_PICKUP_POWERUP, g->playerX); changed = 1;
            }
        }
    } else { d->repairPad = -1; d->repairLoad = 0; }
    if (d->repairAmbush == 1) {
        if (d->phaseTicks) d->phaseTicks--;
        else if (!g->enemyPlane.active && !g->enemyMissile.active && spawnEnemyPlane(g, (g->scrollX >> 3) + 38)) {
            d->repairAmbush = 2; d->phaseTicks = 0;
            g->enemyPlane.x = g->playerX + 140;
            if (g->enemyPlane.x > 304) g->enemyPlane.x = 304;
            g->enemyPlane.worldX = g->enemyPlane.targetWorldX = g->scrollX + g->enemyPlane.x;
            g->enemyPlane.y = g->playerY > 48 ? g->playerY - 40 : 8;
            g->enemyPlane.targetY = g->enemyPlane.y;
            /* Freeze a bombing coordinate. Escaping does not drag the bombs after you. */
            d->jetAge = (UWORD)(((LONG)g->scrollX + g->playerX + 8) >> 3);
        }
    } else if (d->repairAmbush == 2) {
        d->phaseTicks++;
        if (g->enemyPlane.active && !g->enemyPlaneDamageState) {
            g->enemyPlane.worldX -= 2;
            g->enemyPlane.targetWorldX = g->enemyPlane.worldX;
            g->enemyPlane.x = g->enemyPlane.worldX - g->scrollX;
            LONG dx = g->enemyPlane.worldX - (LONG)d->jetAge * 8;
            if (dx >= -8 && dx <= 40 && !(d->phaseTicks % 12))
                carrierDropBomb(g, g->enemyPlane.x + 4, g->enemyPlane.y + 8, 0);
        }
        if (!g->enemyPlane.active || g->enemyPlane.x < -16 || d->phaseTicks > 240) {
            g->enemyPlane.active = 0; d->repairAmbush = 3; d->phaseTicks = 150;
        }
    } else if (d->repairAmbush == 3 && !--d->phaseTicks) d->repairAmbush = 0;
    for (UBYTE i = 0; i < CARRIER_DEFENCE_BOMBS; i++) {
        WeaponState* b = &d->bombs[i]; if (!b->active) continue;
        b->x = b->worldX - g->scrollX;
        if (b->x < -8 || b->x > 328) { b->active = 0; continue; }
        b->timer++; if (!(b->timer & 15) && b->dy < 3) b->dy++;
        if (!(d->clock & 1)) b->y += b->dy;
        if (g->rocketShot.active && rectsOverlap(g->rocketShot.x, g->rocketShot.y, 8, 8, b->x, b->y, 8, 8)) {
            b->active = g->rocketShot.active = 0; continue;
        }
        if (!g->respawnSafeTimer && rectsOverlap(g->playerX, g->playerY, 16, 8, b->x, b->y, 6, 6)) {
            b->active = 0; applyPlayerMissileDamage(g, 1); changed = 1; continue;
        }
        LONG col = b->worldX >> 3;
        LONG anchor = groundTargetAnchorColumn(col);
        if (repairDepotLocalColumn(anchor) >= 0 && !isTargetDestroyedAtColumn(anchor)) {
            const LevelSegmentDef* seg = levelSegmentForWorldColumn(anchor);
            WORD roof = (terrainYForWorldColumn(anchor, seg, HAR_TERRAIN_CPC_RANDOM_LAND) - 1) * 8;
            if (b->y + 6 >= roof) {
                b->active = 0; markTargetDestroyedAtColumn(anchor);
                dirtyRedrawGroundTarget(buffers, anchor); startWorldImpact(g, b->x, roof); changed = 1;
            }
        }
        if (b->active && b->y + 6 >= terrainSurfacePixelYForWorldColumn(col)) {
            b->active = 0; startWorldImpact(g, b->x, b->y);
        }
    }
    return changed || (d->fullVtol && !(d->clock & 7));
}
