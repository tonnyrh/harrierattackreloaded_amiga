/* Stationary Enhanced carrier defence. No dynamic allocations or sample synthesis. */
#define CARRIER_BOMB_HULL_DAMAGE 8
#define CARRIER_HEAVY_BOMB_HULL_DAMAGE 16

#define DEFENCE_SMALL __attribute__((noinline, optimize("Os")))

static DEFENCE_SMALL void carrierDeliverRepair(GameState* g, UBYTE hull, UBYTE cargo) {
    g->defence.hull = hull + cargo > 100 ? 100 : hull + cargo;
    g->defence.cargo = 0;
    if (cargo >= 20) g->defence.gunHealth[0] = g->defence.gunHealth[1] = 2;
}

static DEFENCE_SMALL UBYTE carrierHullOverlap(WORD x, WORD y, UBYTE width, UBYTE height);

static DEFENCE_SMALL UBYTE carrierDefenceOnDeck(WORD x, WORD y) {
    return y == TAKEOFF_PLAYER_DECK_Y &&
        x >= 64 && x <= 144 && !carrierHullOverlap(x + 2, y, 12, 7);
}

static DEFENCE_SMALL void carrierServiceAircraft(GameState* g) {
    UWORD q = g->fuelGaugeLevel ? (g->fuelGaugeLevel - 1) * CPC_FUEL_SUBCOUNT_FULL + g->fuelSubCounter : 0;
    q += 4;
    if (q >= CPC_FUEL_TOTAL_QUANTA) resetPlayerFuel(g);
    else {
        g->fuelGaugeLevel = (q - 1) / CPC_FUEL_SUBCOUNT_FULL + 1;
        g->fuelSubCounter = (q - 1) % CPC_FUEL_SUBCOUNT_FULL + 1;
        g->fuel = cpcFuelHudValue(g);
    }
    if (g->armour < 100) g->armour = g->armour > 98 ? 100 : g->armour + 2;
    g->flakDamageCount = 0; g->missileDamageThirds = (100 - g->armour) * 3;
    UBYTE bombs, rockets; ammoForSkill(g->levelDifficulty, &bombs, &rockets);
    if (g->rockets < rockets) g->rockets++;
    if (g->bombs < bombs) g->bombs++;
}

static DEFENCE_SMALL UBYTE carrierBombsActive(const GameState* g) {
    for (UBYTE i = 0; i < CARRIER_DEFENCE_BOMBS; i++) if (g->defence.bombs[i].active) return 1;
    return 0;
}

static DEFENCE_SMALL void carrierDropBomb(GameState* g, WORD x, WORD y, UBYTE heavy) {
    for (UBYTE i = 0; i < CARRIER_DEFENCE_BOMBS; i++) {
        WeaponState* b = &g->defence.bombs[i];
        if (b->active) continue;
        memset(b, 0, sizeof(*b)); b->active = 1; b->type = heavy;
        b->x = x; b->worldX = (LONG)g->scrollX + x; b->y = y; b->dy = 1;
        playSfxAtTuned(SFX_BOMB, x, 24, SFX_PAULA_PERIOD); return;
    }
}

static DEFENCE_SMALL void carrierDefenceImpact(GameState* g, WORD x, UBYTE damage) {
    CarrierDefenceState* d = &g->defence;
    if (d->phase == DEFENCE_SINKING || !d->hull) return;
    d->hull = d->hull > damage ? d->hull - damage : 0;
    startWorldImpact(g, x, 108);
    if (!d->hull) {
        d->phase = DEFENCE_SINKING; d->phaseTicks = 0;
        g->enemyPlane.active = g->enemyMissile.active = g->helicopter.active = 0;
        memset(d->bombs, 0, sizeof(d->bombs));
        memset(g->helicopterBullets, 0, sizeof(g->helicopterBullets));
        stopAllSfx(); playSfxAt(SFX_HIT, 112);
    }
}

/* Two persistent two-hit turrets. Height is physical and matches the tile.
 * Approach hysteresis avoids cycling when hovering near the safety boundary. */
/* Left / up / right, relative to each muzzle. Integer comparisons only.
 * A small angular dead band prevents flicker at a sector boundary. */
static DEFENCE_SMALL UBYTE carrierGunAimPose(WORD dx, WORD rise, UBYTE previous) {
    if (rise < 1) rise = 1;
    WORD threshold = rise / 2;
    if (previous == 0 && dx < -threshold + 3) return 0;
    if (previous == 2 && dx > threshold - 3) return 2;
    if (dx < -threshold - 3) return 0;
    if (dx > threshold + 3) return 2;
    return 1;
}

static DEFENCE_SMALL void carrierSyncGuns(GameState* g, UBYTE** buffers) {
    for (UBYTE i = 0; i < 2; i++) {
        UBYTE height = g->defence.phase == DEFENCE_SINKING ? 0 : g->defence.gunHeight[i];
        UBYTE pose = carrierGunAimPose(g->defence.aimX - (i ? 148 : 76),
            104 - g->defence.aimY, carrierDefenceGunPose[i]);
        UBYTE changed = carrierDefenceGunHeight[i] != height ||
            (height && carrierDefenceGunPose[i] != pose);
        carrierDefenceGunPose[i] = pose;
        if (!changed) continue;
        carrierDefenceGunHeight[i] = height;
        if (height) carrierDefenceGunMask |= 1 << i;
        else carrierDefenceGunMask &= ~(1 << i);
        if (buffers[0]) bobCompositorErase(buffers[0], i ? 18 : 9, 13, 1);
    }
}

static DEFENCE_SMALL void carrierUpdateGuns(GameState* g) {
    CarrierDefenceState* d = &g->defence;
    UBYTE approach = g->playerX + 16 > 40 && g->playerX < 184;
    if (d->landed || (approach && g->playerY + 8 >= 80)) d->gunsRetract = 1;
    else if (!approach || g->playerY + 8 < 72) d->gunsRetract = 0;
    for (UBYTE i = 0; i < 2; i++) {
        WORD x = i ? 144 : 72;
        UBYTE height = d->gunHeight[i];
        if (d->gunHealth[i] && height && !g->crashTimer && !g->ejectState &&
            !g->respawnSafeTimer && !g->aircraftFailureState &&
            rectsOverlap(g->playerX + 2, g->playerY, 12, 8, x, 112 - height, 8, height)) {
            d->gunHealth[i] = 0;
            startWorldImpact(g, x, 104); applyPlayerMissileDamage(g, 0);
        }
        if (!d->gunHealth[i]) { d->gunHeight[i] = d->gunFlash[i] = 0; continue; }
        if (d->gunsRetract || d->phase == DEFENCE_DEPART || d->phase == DEFENCE_SINKING ||
            (i == 1 && (carrierWingmanLiftDepth < 8 || d->phase == DEFENCE_SECURE))) {
            if (height) d->gunHeight[i]--;
        } else if (height < 8 && !(d->clock & 1)) d->gunHeight[i]++;
    }
}

/* A retracted turret is protected by the deck. Each direct ordnance hit
 * removes one hit; the normal hull damage is still applied by caller. */
static DEFENCE_SMALL UBYTE carrierWeaponHitsGun(GameState* g, WORD x, WORD y, UBYTE size) {
    for (UBYTE i = 0; i < 2; i++) {
        WORD gunX = i ? 144 : 72;
        UBYTE height = g->defence.gunHeight[i];
        if (g->defence.gunHealth[i] && height &&
            rectsOverlap(x, y, size, size, gunX, 112 - height, 8, height)) {
            if (!--g->defence.gunHealth[i]) {
                g->defence.gunHeight[i] = g->defence.gunFlash[i] = 0;
            }
            startWorldImpact(g, gunX, 104); return 1;
        }
    }
    return 0;
}

static DEFENCE_SMALL void carrierDefenceKillJet(GameState* g) {
    startWorldImpact(g, g->enemyPlane.x, g->enemyPlane.y);
    if (g->defence.jetType == 2 && ++g->defence.jetHits < 4) { playSfxAt(SFX_HIT, g->enemyPlane.x); return; }
    g->enemyPlane.active = 0; awardGameScore(g, ENEMY_SCORE_VALUE); g->hitsCount++;
}

static DEFENCE_SMALL void carrierDefenceKillHeli(GameState* g) {
    if (g->helicopter.type == 2) {
        startWorldImpact(g, g->helicopter.x, g->helicopter.y);
        g->helicopter.active = 0; playSfxAt(SFX_HIT, g->helicopter.x); return;
    }
    if (++g->helicopterHits >= 2) {
        g->helicopter.type = 2; g->helicopter.dy = 1;
        dropCarrierRepair(g);
        awardGameScore(g, 750); g->hitsCount++;
    }
    playSfxAt(SFX_IMPACT, g->helicopter.x);
}

/* Test the actual ship silhouette, so shots above the deck or beside the
 * island remain clear. Transparent source pixels are not solid hull. */
static DEFENCE_SMALL UBYTE carrierHullOverlap(WORD x, WORD y, UBYTE width, UBYTE height) {
    WORD left = x < 64 ? 64 : x, right = x + width > 160 ? 160 : x + width;
    WORD top = y < 96 ? 96 : y, bottom = y + height > 120 ? 120 : y + height;
    for (WORD py = top; py < bottom; py++) for (WORD px = left; px < right; px++) {
        UWORD tile = ((py - 96) >> 3) * HAR_CARRIER_TILES_WIDE + ((px - 64) >> 3);
        if (harCarrierWithoutWingmanTileData[tile * HAR_CARRIER_TILE_BYTES + (py & 7) * 5 + 4] & (0x80 >> (px & 7)))
            return 1;
    }
    return 0;
}

static DEFENCE_SMALL void carrierDefenceShot(GameState* g, WeaponState* shot) {
    if (!shot->active) return;
    shot->x += shot->dx; shot->y += shot->dy; shot->worldX = g->scrollX + shot->x;
    shot->timer++;
    if (shot->x < -8 || shot->x > SCREEN_WIDTH || shot->y < 0 || shot->y > 112) { shot->active = 0; return; }
    if (shot != &g->wingman.rocket && (carrierWeaponHitsGun(g, shot->x, shot->y, 8) || carrierHullOverlap(shot->x, shot->y, 8, 8))) {
        shot->active = 0; carrierDefenceImpact(g, shot->x, CARRIER_BOMB_HULL_DAMAGE); return;
    }
    for (UBYTE i = 0; i < CARRIER_DEFENCE_BOMBS; i++) {
        WeaponState* b = &g->defence.bombs[i];
        if (b->active && rectsOverlap(shot->x, shot->y, 8, 8, b->x - 2, b->y - 2, 10, 10)) {
            b->active = shot->active = 0; awardGameScore(g, 25); playSfxAtTuned(SFX_IMPACT, b->x, 24, SFX_PAULA_PERIOD); return;
        }
    }
    if (g->enemyMissile.active && rectsOverlap(shot->x, shot->y, 8, 8, g->enemyMissile.x, g->enemyMissile.y, 8, 8)) {
        g->enemyMissile.active = shot->active = 0; awardGameScore(g, 25); return;
    }
    if (g->enemyPlane.active && rectsOverlap(shot->x, shot->y, 8, 8, g->enemyPlane.x, g->enemyPlane.y, 16, 8)) {
        shot->active = 0; carrierDefenceKillJet(g); return;
    }
    if (g->helicopter.active && rectsOverlap(shot->x, shot->y, 8, 8, g->helicopter.x, g->helicopter.y, 16, 8)) {
        shot->active = 0; carrierDefenceKillHeli(g);
    }
}

static DEFENCE_SMALL void carrierDefenceLaunch(GameState* g, WeaponState* shot, WORD x, WORD y, UBYTE direction) {
    memset(shot, 0, sizeof(*shot)); shot->active = 1;
    shot->x = x; shot->worldX = x; shot->y = y;
    shot->type = ROCKET_SHOT_MAVERICK_GUIDED; shot->direction = direction;
    shot->dx = direction == MAVERICK_DIRECTION_LEFT || direction == MAVERICK_DIRECTION_UP_LEFT ? -4 :
        (direction == MAVERICK_DIRECTION_UP || direction == MAVERICK_DIRECTION_DOWN ? 0 : 4);
    shot->dy = direction == MAVERICK_DIRECTION_UP || direction == MAVERICK_DIRECTION_UP_LEFT ||
        direction == MAVERICK_DIRECTION_UP_RIGHT ? -4 : (direction == MAVERICK_DIRECTION_DOWN ? 4 : 0);
    playSfxAtTuned(SFX_FIRE, x, 36, SFX_PAULA_PERIOD);
}

static DEFENCE_SMALL void carrierDefenceAircraft(GameState* g) {
    CarrierDefenceState* d = &g->defence;
    if (g->enemyPlane.active) {
        WeaponState* p = &g->enemyPlane; d->jetAge++;
        p->x += (p->direction ? 1 : -1) * (d->jetType == 1 ? 2 : 1); p->worldX = p->targetWorldX = p->x;
        if (d->jetType == 1) {
            if (!(d->jetAge & 3) && (p->direction ? p->x < 50 : p->x > 170)) p->y += p->y < g->playerY ? 1 : (p->y > g->playerY ? -1 : 0);
            if (d->jetAge == 48 && !g->enemyMissile.active) {
                WeaponState* m = &g->enemyMissile; memset(m, 0, sizeof(*m));
                m->active = 1; m->type = ENEMY_MISSILE_CARRIER_TYPE;
                m->x = p->x + (p->direction ? 12 : -4); m->y = p->y + 8; m->worldX = m->x;
                WORD dx = g->playerX + 4 - m->x, dy = g->playerY - m->y;
                WORD distance = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
                if (ay > distance) distance = ay; if (!distance) distance = 1;
                m->dx = (LONG)dx * 1024 / distance; m->dy = (LONG)dy * 1024 / distance;
                m->targetWorldX = (LONG)m->x * 256; m->targetY = m->y * 256;
                g->enemyMissileTarget = ENEMY_TARGET_PLAYER;
                playSfxAt(SFX_FIRE, p->x);
            }
        }
        if (p->x >= 70 && p->x <= 150 && !(d->jetAge % (g->missionNumber >= 4 ? 20 : 28)))
            carrierDropBomb(g, p->x + 4, p->y + 8, d->jetType == 2);
        if (p->x < -16 || p->x > 320) {
            if (d->jetType == 2) {
                /* Heavy bomber keeps making high passes until shot down.
                 * Turn entirely offscreen, preserving accumulated damage. */
                p->direction = !p->direction;
                p->x = p->direction ? -16 : 320;
                p->worldX = p->targetWorldX = p->x;
            } else p->active = 0;
        }
    }
    if (g->helicopter.active) {
        WeaponState* h = &g->helicopter; d->heliAge++; g->helicopterAge++;
        if (h->type == 2) {
            if (!(d->heliAge & 7) && h->dy < 4) h->dy++;
            h->y += h->dy;
            if (h->y >= 112) { startWorldImpact(g, h->x, 112); h->active = 0; }
        } else {
            if (h->type == 0 && (h->direction ? h->x < 104 : h->x > 120)) h->x += h->direction ? 1 : -1;
            else if (h->type == 0) { h->type = 1; d->heliAge = 0; }
            else if (d->heliAge > 180 + g->levelDifficulty * 20) {
                h->x += h->direction ? 1 : -1; if (!(d->heliAge & 1)) h->y--;
                if (h->x < -16 || h->x >= 320 || h->y < 0) h->active = 0;
            } else if (!(d->heliAge % 45)) carrierDropBomb(g, h->x + 4, h->y + 8, 0);
            if (!(d->heliAge % (g->helicopterHits ? 14 : 10)))
                playSfxAtTuned(SFX_HELICOPTER, h->x, 20, g->helicopterHits ? 560 : 443);
        }
        h->worldX = h->x;
    }
    if (d->phase != DEFENCE_WAVE || d->spawned >= d->quota) return;
    if (d->spawnDelay) { d->spawnDelay--; return; }
    UBYTE kind = (d->spawned + d->wave - 1) % 3;
    UBYTE fromLeft = (d->spawned + d->wave + g->missionNumber) & 1;
    if (kind == 2) {
        if (g->helicopter.active) return;
        memset(&g->helicopter, 0, sizeof(g->helicopter));
        g->helicopter.active = 1; g->helicopter.direction = fromLeft;
        g->helicopter.x = g->helicopter.worldX = fromLeft ? 0 : 300;
        g->helicopter.y = 24 + (d->wave & 1) * 12;
        g->helicopterHits = d->heliBulletHits = 0; g->helicopterAge = d->heliAge = 0;
    } else {
        if (g->enemyPlane.active) return;
        memset(&g->enemyPlane, 0, sizeof(g->enemyPlane));
        g->enemyPlane.active = 1; g->enemyPlane.direction = fromLeft;
        g->enemyPlane.x = g->enemyPlane.worldX = g->enemyPlane.targetWorldX = fromLeft ? 0 : 312;
        g->enemyPlane.y = 24 + ((d->spawned + d->wave) & 3) * 12;
        g->enemyPlaneDamageState = ENEMY_PLANE_DAMAGE_NORMAL;
        d->jetType = kind == 0 && ((g->missionNumber + d->spawned + d->wave) & 1) ? 2 : kind;
        if (d->jetType == 2) g->enemyPlane.y = 20 + (d->wave & 1) * 8;
        d->jetAge = 0; d->jetBulletHits = d->jetHits = 0;
    }
    d->spawned++; d->spawnDelay = 130 - g->levelDifficulty * 10;
}

static DEFENCE_SMALL void carrierDefenceBombs(GameState* g) {
    CarrierDefenceState* d = &g->defence;
    for (UBYTE i = 0; i < CARRIER_DEFENCE_BOMBS; i++) {
        WeaponState* b = &d->bombs[i]; if (!b->active) continue;
        b->timer++; b->x = b->worldX - g->scrollX;
        if (!(b->timer & 15) && b->dy < 3) b->dy++;
        if (!(d->clock & 1)) b->y += b->dy;
        if (!g->crashTimer && !g->respawnSafeTimer && rectsOverlap(b->x, b->y, 6, 6, g->playerX, g->playerY, 16, 8)) {
            b->active = 0;
            if (d->landed && b->x >= 64 && b->x < 160) carrierDefenceImpact(g, b->x, b->type ? CARRIER_HEAVY_BOMB_HULL_DAMAGE : CARRIER_BOMB_HULL_DAMAGE);
            applyPlayerMissileDamage(g, 1); continue;
        }
        if (g->wingman.active && rectsOverlap(b->x, b->y, 6, 6, wingmanScreenX(g), g->wingman.screenY, 16, 8)) {
            b->active = 0; g->wingman.active = 0; g->wingman.destroyed = 1; continue;
        }
        if (carrierWeaponHitsGun(g, b->x, b->y, 6)) {
            b->active = 0; carrierDefenceImpact(g, b->x, b->type ? CARRIER_HEAVY_BOMB_HULL_DAMAGE : CARRIER_BOMB_HULL_DAMAGE);
        } else if (b->y + 6 >= CARRIER_DECK_PIXEL_Y && b->x >= 64 && b->x < 160) {
            b->active = 0; carrierDefenceImpact(g, b->x, b->type ? CARRIER_HEAVY_BOMB_HULL_DAMAGE : CARRIER_BOMB_HULL_DAMAGE);
        } else if (b->y >= SEA_SURFACE_Y) { b->active = 0; startWaterSplash(g, b->x); }
    }
    WeaponState* m = &g->enemyMissile;
    if (m->active) {
        m->targetWorldX += m->dx; m->x = m->targetWorldX >> 8; m->worldX = m->x; m->targetY += m->dy; m->y = m->targetY >> 8;
        if (m->x < -8 || m->x > 320 || m->y < 0 || m->y >= 113) m->active = 0;
        else if (!g->crashTimer && !g->respawnSafeTimer && rectsOverlap(m->x, m->y, 8, 8, g->playerX, g->playerY, 16, 8)) {
            m->active = 0; applyPlayerMissileDamage(g, 0);
        } else if (g->wingman.active && rectsOverlap(m->x, m->y, 8, 8, wingmanScreenX(g), g->wingman.screenY, 16, 8)) {
            m->active = 0; g->wingman.active = 0; g->wingman.destroyed = 1;
        }
    }
}

/* Match the visible superstructure, excluding deck contact and transparent
 * wingtip margins. Solid tower contact is fatal, including Land Now. */
static DEFENCE_SMALL void carrierMoveVtol(GameState* g, const InputState* in) {
    WORD x = g->playerX + (in->right ? 2 : 0) - (in->left ? 2 : 0);
    WORD y = g->playerY + (in->down ? 2 : 0) - (in->up ? 2 : 0);
    if (x < 8) x = 8; if (x > 296) x = 296; if (y < 8) y = 8;
    if (carrierHullOverlap(x + 2, y, 12, y < 112 ? (112 - y < 7 ? 112 - y : 7) : 0)) {
        /* Preserve the existing short respawn grace without passing through. */
        if (!g->respawnSafeTimer) {
            g->playerX = x; g->playerY = y; g->defence.landed = 0;
            startPlayerCrash(g, x, y);
        }
        return;
    }
    if (y >= TAKEOFF_PLAYER_DECK_Y && g->playerY <= TAKEOFF_PLAYER_DECK_Y && carrierDefenceOnDeck(x, TAKEOFF_PLAYER_DECK_Y)) {
        y = TAKEOFF_PLAYER_DECK_Y; g->defence.landed = 1;
    }
    g->playerX = x; g->playerY = y;
}

/* A wider front-facing turn window. Release horizontal input during the
 * turn to hold a stable hover; altitude input does not end that hover. */
static DEFENCE_SMALL void carrierUpdatePose(UBYTE* facing, UBYTE* ticks,
    UBYTE* target, UBYTE* pose, UBYTE left, UBYTE right) {
    if (*facing != MAVERICK_DIRECTION_LEFT && *facing != MAVERICK_DIRECTION_RIGHT)
        *facing = MAVERICK_DIRECTION_RIGHT;
    UBYTE lateral = left != right;
    UBYTE requested = left ? MAVERICK_DIRECTION_LEFT : MAVERICK_DIRECTION_RIGHT;
    if (*ticks) {
        if (lateral) {
            if (requested == *facing) *ticks = 0;
            else if (!--*ticks) *facing = *target;
        }
    } else if (lateral && requested != *facing) { *target = requested; *ticks = 12; }
    *pose = *ticks ? 0 : (*facing == MAVERICK_DIRECTION_LEFT ? 2 : 1);
}

static DEFENCE_SMALL void carrierUpdateHeading(CarrierDefenceState* d, const InputState* in) {
    carrierUpdatePose(&d->facing, &d->turnTicks, &d->turnTarget, &d->vtolPose, in->left, in->right);
}

static DEFENCE_SMALL UBYTE carrierBombHitsAircraft(GameState* g, WeaponState* bomb) {
    if (!bomb->active) return 0;
    if (g->enemyPlane.active && rectsOverlap(bomb->x, bomb->y, 4, 3,
        g->enemyPlane.x, g->enemyPlane.y, 16, 8)) {
        /* A direct bomb hit defeats even a fresh four-hit bomber. */
        if (g->defence.jetType == 2) g->defence.jetHits = 3;
        bomb->active = 0; carrierDefenceKillJet(g); return 1;
    }
    if (g->helicopter.active && g->helicopter.type != 2 &&
        rectsOverlap(bomb->x, bomb->y, 4, 3, g->helicopter.x, g->helicopter.y, 16, 8)) {
        g->helicopterHits = 1; /* Bombs deliver the fatal hit immediately. */
        bomb->active = 0; carrierDefenceKillHeli(g); return 1;
    }
    return 0;
}

static DEFENCE_SMALL void carrierTryAlarm(CarrierDefenceState* d) {
    if (d->alarmPlayed) return;
    playSfxAt(SFX_CARRIER_KLAXON, SFX_POSITION_CENTER);
    /* Retry if MOD ownership or busy high-priority channels rejected it. */
    for (UBYTE i = 0; i < SFX_CHANNEL_COUNT; i++)
        if (sfxChannelCurrentId[i] == SFX_CARRIER_KLAXON &&
            (sfxPendingSample[i] || sfxChannelFrames[i])) d->alarmPlayed = 1;
}

#include "carrier_gunnery.h"

static DEFENCE_SMALL void carrierUpdateDeckLift(GameState* g, UBYTE** buffers) {
    UBYTE oldDepth = carrierWingmanLiftDepth, oldVisible = carrierParkedWingmanVisible;
    carrierParkedWingmanVisible = !g->wingman.destroyed;
    UBYTE raising = g->defence.phase == DEFENCE_SECURE || g->defence.phase == DEFENCE_DEPART;
    if (g->wingman.destroyed) carrierWingmanLiftDepth = 8;
    else if (!(g->defence.clock % 6)) {
        if (!raising && carrierWingmanLiftDepth < 8) carrierWingmanLiftDepth++;
        else if (raising && carrierWingmanLiftDepth && !g->defence.gunHeight[1]) carrierWingmanLiftDepth--;
    }
    if (buffers[0] && (oldDepth != carrierWingmanLiftDepth || oldVisible != carrierParkedWingmanVisible))
        for (LONG column = 17; column <= 19; column++) dirtyRedrawWorldColumn(buffers, column);
}

static DEFENCE_SMALL void updateCarrierDefence(GameState* g, const InputState* in,
    const InputState* previous, const Player2InputState* in2, UBYTE** buffers) {
    CarrierDefenceState* d = &g->defence; d->clock++;
    g->wingman.active = 0;
    if (g->bombLaunchCooldown) g->bombLaunchCooldown--;
    carrierUpdateHeading(d, in);
    if (d->phase == DEFENCE_SINKING) {
        carrierSyncGuns(g, buffers);
        updateEncounterSmoke(g);
        if (g->impact.active && g->impact.timer && !--g->impact.timer) g->impact.active = 0;
        if (!(d->phaseTicks++ & 7) && carrierDefenceSinkPixels < SEA_SURFACE_Y - 96) {
            carrierDefenceSinkPixels++;
            retireEncounterBobs(buffers[0], 0);
            for (LONG col = 8; col < 20; col++) dirtyRedrawWorldColumn(buffers, col);
            if (!(carrierDefenceSinkPixels & 7)) startWaterSplash(g, 68 + carrierDefenceSinkPixels * 3);
        }
        if (d->phaseTicks >= 260) triggerGameOver(g);
        return;
    }
    g->scrollX = 0; g->speedLevel = 0; g->lowSpeedLanding = 1;
    g->landingState = LANDING_STATE_NONE;
    carrierUpdateDeckLift(g, buffers);
    if (g->impact.active && g->impact.timer && !--g->impact.timer) g->impact.active = 0;
    if (g->ejectState) {
        if (g->ejectState == 2) {
            g->ejectX += (in->right && !in->left) - (in->left && !in->right);
            if (g->ejectX < 0) g->ejectX = 0;
            if (g->ejectX > SCREEN_WIDTH - HAR_CPC_PARACHUTE_WIDTH) g->ejectX = SCREEN_WIDTH - HAR_CPC_PARACHUTE_WIDTH;
        }
        updateAbandonedAircraft(g);
        if (g->crashTimer) updatePlayerCrash(g);
        UBYTE rescue = updatePlayerEject(g);
        if (rescue == EJECT_UPDATE_CARRIER_RESTART && !g->gameOver) {
            respawnPlayer(g); g->scrollX = 0; g->playerX = 80; g->playerY = TAKEOFF_PLAYER_DECK_Y;
            g->takeoffState = TAKEOFF_STATE_AIRBORNE;
            d->landed = 1; resetPlayerFuel(g);
        }
    } else if (g->crashTimer) {
        updatePlayerCrash(g);
        if (!g->crashTimer && !g->gameOver) {
            g->playerX = 80; g->playerY = 72; resetPlayerFuel(g); d->landed = 0;
        }
    } else if (g->aircraftFailureState) {
        if (in->eject) startPlayerEject(g);
        else {
            updateAircraftFailure(g, in); g->scrollX = 0;
            if (!g->crashTimer && g->playerY >= 105) {
                d->landed = 0; startPlayerCrash(g, g->playerX, g->playerY);
                g->aircraftFailureState = 0;
            }
        }
    } else if (!g->fuel || !g->armour) {
        startAircraftFailure(g, !g->fuel ? AIRCRAFT_FAILURE_CAUSE_FUEL : AIRCRAFT_FAILURE_CAUSE_ARMOUR);
    } else {
        g->takeoffState = TAKEOFF_STATE_AIRBORNE;
        if (g->respawnSafeTimer) g->respawnSafeTimer--;
        if (d->landed) {
            if (d->cargo) carrierDeliverRepair(g, d->hull, d->cargo);
            g->playerY = TAKEOFF_PLAYER_DECK_Y;
            if (!(d->clock & 7)) carrierServiceAircraft(g);
            if (in->up) { d->landed = 0; g->playerY -= 3; startEngineSound(1); }
        } else {
            InputState movement = *in;
            if (d->turnTicks) movement.left = movement.right = 0;
            carrierMoveVtol(g, &movement);
            if (g->crashTimer) return;
            WORD x = g->playerX, y = g->playerY;
            if (y >= SEA_SURFACE_Y - 8) {
                /* A pilot who has not ejected cannot survive a sea impact. */
                d->landed = 0; startPlayerCrash(g, x, y);
            }
            /* Carrier hovering uses Enhanced VTOL's 3x fuel rate. Stop burning
             * immediately on touchdown; deck service refuels every eight ticks. */
            if (!d->landed) updatePlayerFuel(g);
            if (d->fireCooldown) d->fireCooldown--;
            /* Match terrain flight: one missile per fresh press, never repeat
             * a held/stuck input when the previous missile leaves the screen. */
            if (Pressed(in->fire, previous->fire) && !d->turnTicks && !d->fireCooldown && !g->rocketShot.active && g->rockets) {
                carrierDefenceLaunch(g, &g->rocketShot, x + 4, y, d->facing);
                if (!debugInfiniteRockets) g->rockets--; d->fireCooldown = 10;
            }
            if (BombPressed(in, previous) && !g->bombShot.active && g->bombs) launchBomb(g);
        }
        if (d->landed) {
            /* Stop only our engine voice, once: leave deck ambience and alarms alone. */
            if (engineActive) stopSfxChannel(ENGINE_CHANNEL);
        } else if (!g->crashTimer && !g->aircraftFailureState) updateEngineSound(1, 1);
    }
    carrierUpdateGuns(g);
    if (g->bombShot.active) {
        g->bombShot.y++; g->bombShot.worldX = g->bombShot.x;
        if (carrierBombHitsAircraft(g, &g->bombShot)) {
            /* A direct hit consumes the bomb before deck collision. */
        } else if (carrierWeaponHitsGun(g, g->bombShot.x, g->bombShot.y, 8)) {
            carrierDefenceImpact(g, g->bombShot.x, CARRIER_BOMB_HULL_DAMAGE); g->bombShot.active = 0;
        } else if (g->bombShot.y >= 110) {
            if (g->bombShot.x >= 64 && g->bombShot.x < 160) carrierDefenceImpact(g, g->bombShot.x, CARRIER_BOMB_HULL_DAMAGE);
            g->bombShot.active = 0;
        }
    }
    g->wingman.active = 0;
    serviceCarrierRepairDrop(g); updatePowerup(g);
    updateEncounterSmoke(g);
    if (!g->helicopterSmoke.active && g->helicopter.active && g->helicopter.type != 2 &&
        g->helicopterHits == 1 && !(d->clock & 63)) {
        memset(&g->helicopterSmoke, 0, sizeof(g->helicopterSmoke));
        g->helicopterSmoke.active = 1;
        g->helicopterSmoke.x = g->helicopterSmoke.worldX = g->helicopter.x + 6;
        g->helicopterSmoke.y = g->helicopter.y;
    }
    if (!g->helicopterSmoke.active && d->hull < 50 && !(d->clock % 48)) {
        memset(&g->helicopterSmoke, 0, sizeof(g->helicopterSmoke));
        g->helicopterSmoke.active = 1; g->helicopterSmoke.x = 116; g->helicopterSmoke.worldX = 116; g->helicopterSmoke.y = 98;
    }
    carrierDefenceAircraft(g);
    /* Resolve enemy ordnance reaching Harrier/deck before interception.
     * Otherwise a missile can erase a bomb already overlapping the aircraft,
     * making a visible direct hit harmless (especially under CPU defence). */
    carrierDefenceBombs(g);
    carrierDefenceShot(g, &g->rocketShot);
    carrierGunnery(g, in2);
    carrierSyncGuns(g, buffers);
    if (!g->crashTimer && !g->ejectState && !g->respawnSafeTimer) {
        UBYTE contact = 0;
        if (g->enemyPlane.active && rectsOverlap(g->playerX, g->playerY, 16, 8, g->enemyPlane.x, g->enemyPlane.y, 16, 8)) {
            g->enemyPlane.active = 0; contact = 1;
        }
        if (g->helicopter.active && rectsOverlap(g->playerX, g->playerY, 16, 8, g->helicopter.x, g->helicopter.y, 16, 8)) {
            if (g->helicopter.type != 2) {
                g->helicopterHits = 1; carrierDefenceKillHeli(g);
            }
            contact = 1;
        }
        if (contact) { startWorldImpact(g, g->playerX, g->playerY); applyPlayerMissileDamage(g, 1); }
    }
    if (d->phase == DEFENCE_SINKING || g->gameOver) return;
    if (d->phase == DEFENCE_ALARM || d->phase == DEFENCE_LULL) {
        carrierTryAlarm(d);
        if (d->phaseTicks) d->phaseTicks--;
        else { d->phase = DEFENCE_WAVE; d->wave++; d->spawned = 0; d->quota = 3 + (g->levelDifficulty - 1) / 2; d->spawnDelay = 15; }
    } else if (d->phase == DEFENCE_WAVE && d->spawned >= d->quota && !g->enemyPlane.active && !g->helicopter.active && !g->enemyMissile.active && !carrierBombsActive(g)) {
        d->phase = d->wave < d->waves ? DEFENCE_LULL : DEFENCE_SECURE; d->phaseTicks = 150; d->alarmPlayed = 0;
        playSfxAt(SFX_PICKUP_POWERUP, 112);
    } else if (d->phase == DEFENCE_SECURE && d->landed) { d->phase = DEFENCE_DEPART; d->phaseTicks = 0; }
    else if (d->phase == DEFENCE_DEPART) {
        if (d->phaseTicks) d->phaseTicks--;
        else if (in->up && (!carrierWingmanLiftDepth || g->wingman.destroyed)) {
            d->phase = 0; d->landed = 0; carrierDefenceGunMask = 0;
            d->gunHeight[0] = d->gunHeight[1] = 0;
            carrierDefenceGunHeight[0] = carrierDefenceGunHeight[1] = 0;
            if (buffers[0]) { dirtyRedrawWorldColumn(buffers, 9); dirtyRedrawWorldColumn(buffers, 18); }
            g->rocketShot.active = g->wingman.rocket.active = g->bombShot.active = 0;
            memset(g->helicopterBullets, 0, sizeof(g->helicopterBullets));
            g->playerX = TAKEOFF_PLAYER_DECK_X; g->playerY = TAKEOFF_PLAYER_DECK_Y - 2;
            g->wingman.active = 0; g->wingman.mode = WINGMAN_ON_DECK;
            g->wingman.interceptScreenX = WINGMAN_TAKEOFF_DECK_X;
            g->wingman.screenY = WINGMAN_TAKEOFF_DECK_Y;
            g->takeoffState = TAKEOFF_STATE_LIFTING; g->lowSpeedLanding = 0; g->speedLevel = GAME_SPEED_LEVEL_DEFAULT;
            hudRenderState[0].valid = 0;
        }
    }
}
