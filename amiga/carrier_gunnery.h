/* Carrier defence reuses two light-pixel slots and Wingman's rocket slot. */
static DEFENCE_SMALL WORD carrierAbs(WORD n) { return n < 0 ? -n : n; }

static DEFENCE_SMALL UBYTE carrierChooseAim(GameState* g) {
    CarrierDefenceState* d = &g->defence;
    WeaponState* target = 0;
    for (UBYTE i = 0; i < CARRIER_DEFENCE_BOMBS; i++) {
        WeaponState* b = &d->bombs[i];
        if (b->active && (!target || b->y > target->y)) target = b;
    }
    if (!target && g->enemyMissile.active) target = &g->enemyMissile;
    if (!target && g->enemyPlane.active) target = &g->enemyPlane;
    if (!target && g->helicopter.active && g->helicopter.type != 2) target = &g->helicopter;
    if (!target) return 0;
    d->aimX = target->x + 4; d->aimY = target->y + 3;
    if (target == &g->enemyPlane) d->aimX += (g->enemyPlane.direction ? 1 : -1) * (100 - target->y) / 4;
    return 1;
}

static DEFENCE_SMALL void carrierGunBullet(GameState* g, UBYTE gun) {
    HelicopterBullet* b = &g->helicopterBullets[gun];
    CarrierDefenceState* d = &g->defence;
    if (b->active || !d->gunHealth[gun] || d->gunHeight[gun] != 8) return;
    WORD x = gun ? 148 : 76, y = 104;
    WORD dx = d->aimX + ((d->clock + gun * 3) & 7) - 3 - x, dy = d->aimY - y;
    WORD distance = carrierAbs(dx); if (carrierAbs(dy) > distance) distance = carrierAbs(dy);
    if (!distance) return;
    memset(b, 0, sizeof(*b)); b->active = 1; b->worldX = x; b->y = y;
    b->vx16 = (LONG)dx * 64 / distance; b->vy16 = (LONG)dy * 64 / distance;
    d->gunFlash[gun] = 5;
    playSfxAtTuned(SFX_FLAK_GUN_1, x, 22, SFX_PAULA_PERIOD);
}

static DEFENCE_SMALL void carrierAdvanceBullet(GameState* g, HelicopterBullet* b) {
    if (!b->active) return;
    WORD x = b->subX + b->vx16, y = b->subY + b->vy16;
    b->worldX += x / 16; b->y += y / 16; b->subX = x % 16; b->subY = y % 16;
    if (++b->age > 90 || b->worldX < 0 || b->worldX >= SCREEN_WIDTH || b->y < 0 || b->y >= 112) { b->active = 0; return; }
    x = b->worldX - 1; y = b->y - 1;
    for (UBYTE i = 0; i < CARRIER_DEFENCE_BOMBS; i++) {
        WeaponState* bomb = &g->defence.bombs[i];
        if (bomb->active && rectsOverlap(x, y, 3, 3, bomb->x, bomb->y, 6, 6)) {
            bomb->active = b->active = 0; return;
        }
    }
    if (g->enemyMissile.active && rectsOverlap(x, y, 3, 3, g->enemyMissile.x, g->enemyMissile.y, 8, 8)) {
        g->enemyMissile.active = b->active = 0; return;
    }
    if (g->enemyPlane.active && rectsOverlap(x, y, 3, 3, g->enemyPlane.x, g->enemyPlane.y, 16, 8)) {
        b->active = 0;
        if (++g->defence.jetBulletHits >= 4) { g->defence.jetBulletHits = 0; carrierDefenceKillJet(g); }
    } else if (g->helicopter.active &&
        rectsOverlap(x, y, 3, 3, g->helicopter.x, g->helicopter.y, 16, 8)) {
        b->active = 0;
        if (g->helicopter.type == 2 || ++g->defence.heliBulletHits >= 4) { g->defence.heliBulletHits = 0; carrierDefenceKillHeli(g); }
    }
    /* Deliberately no friendly-aircraft collision for bullets. */
}

static DEFENCE_SMALL void carrierLaunchMissile(GameState* g) {
    WeaponState* m = &g->wingman.rocket;
    WORD dx = g->defence.aimX - 110, dy = g->defence.aimY - 100;
    WORD distance = carrierAbs(dx); if (carrierAbs(dy) > distance) distance = carrierAbs(dy);
    if (!distance || m->active) return;
    memset(m, 0, sizeof(*m)); m->active = 1; m->x = 106; m->y = 96; m->worldX = 106;
    m->targetWorldX = (LONG)m->x * 256; m->targetY = m->y * 256;
    m->dx = (LONG)dx * 768 / distance; m->dy = (LONG)dy * 768 / distance;
    m->type = ROCKET_SHOT_MAVERICK_GUIDED;
    m->direction = carrierAbs(dx) < carrierAbs(dy) / 2 ? MAVERICK_DIRECTION_UP :
        (dx < 0 ? (carrierAbs(dy) < carrierAbs(dx) / 2 ? MAVERICK_DIRECTION_LEFT : MAVERICK_DIRECTION_UP_LEFT) :
        (carrierAbs(dy) < carrierAbs(dx) / 2 ? MAVERICK_DIRECTION_RIGHT : MAVERICK_DIRECTION_UP_RIGHT));
    g->defence.missileCooldown = 100;
    playSfxAt(SFX_FIRE, 112);
}

static DEFENCE_SMALL void carrierAdvanceMissile(GameState* g) {
    WeaponState* m = &g->wingman.rocket;
    if (!m->active) return;
    m->targetWorldX += m->dx; m->targetY += m->dy;
    m->x = m->targetWorldX >> 8; m->y = m->targetY >> 8; m->worldX = m->x;
    if (m->x < -8 || m->x > SCREEN_WIDTH || m->y < 0 || m->y > 112 || ++m->timer > 150) { m->active = 0; return; }
    if (!g->crashTimer && !g->ejectState && !g->respawnSafeTimer &&
        rectsOverlap(m->x, m->y, 8, 8, g->playerX, g->playerY, 16, 8)) {
        m->active = 0; applyPlayerMissileDamage(g, 0); return;
    }
    /* Reuse Harrier's hit rules without applying its horizontal motion twice. */
    WORD dx = m->dx, dy = m->dy;
    m->dx = m->dy = 0; carrierDefenceShot(g, m); m->timer--; m->dx = dx; m->dy = dy;
}

static DEFENCE_SMALL void carrierGunnery(GameState* g, const Player2InputState* in) {
    CarrierDefenceState* d = &g->defence;
    UBYTE shoot = 0, missile = 0;
    if (!d->aimX) { d->aimX = 160; d->aimY = 48; }
    if (g->wingmanControl == WINGMAN_CONTROL_PLAYER2) {
        d->aimX += (in->right ? 2 : 0) - (in->left ? 2 : 0);
        d->aimY += (in->down ? 2 : 0) - (in->up ? 2 : 0);
        shoot = in->fire; missile = in->bomb;
    } else if (d->phase == DEFENCE_WAVE && carrierChooseAim(g)) {
        shoot = 1;
        /* CPU does not fire its dangerous rocket directly through Harrier. */
        WORD dx = d->aimX - 110, dy = d->aimY - 100;
        LONG cross = (LONG)(g->playerX + 8 - 110) * dy - (LONG)(g->playerY + 4 - 100) * dx;
        if (cross < 0) cross = -cross;
        missile = cross > (LONG)(carrierAbs(dx) + carrierAbs(dy)) * 14 || g->playerY > 96;
    }
    if (d->aimX < 8) d->aimX = 8; if (d->aimX > 308) d->aimX = 308;
    if (d->aimY < 8) d->aimY = 8; if (d->aimY > 88) d->aimY = 88;
    if (d->gunCooldown) d->gunCooldown--;
    if (d->missileCooldown) d->missileCooldown--;
    for (UBYTE i = 0; i < 2; i++) {
        carrierAdvanceBullet(g, &g->helicopterBullets[i]);
        if (d->gunFlash[i]) d->gunFlash[i]--;
    }
    carrierAdvanceMissile(g);
    if (shoot && !d->gunCooldown) {
        carrierGunBullet(g, 0); carrierGunBullet(g, 1); d->gunCooldown = 8;
    }
    if (missile && !d->missileCooldown && !g->wingman.rocket.active) carrierLaunchMissile(g);
}
