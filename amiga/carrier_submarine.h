/* One surfaced boat and one independent ballistic missile: bounded A500 cost. */

static DEFENCE_SMALL void carrierSyncSubmarine(GameState* g, UBYTE** buffers) {
    UBYTE height=g->defence.phase==DEFENCE_SINKING ? 0 : g->defence.subHeight;
    UBYTE bubbles=g->defence.phase!=DEFENCE_SINKING &&
        (g->defence.subState==CARRIER_SUB_RISING || g->defence.subState==CARRIER_SUB_SINKING) ?
        1+((g->defence.subClock/6)&7) : 0;
    if(height==carrierSubHeight && bubbles==carrierSubBubbleFrame && g->defence.subX==carrierSubX) return;
    WORD oldX=carrierSubX; UBYTE oldVisible=carrierSubHeight || carrierSubBubbleFrame;
    carrierSubHeight=height; carrierSubX=g->defence.subX; carrierSubBubbleFrame=bubbles;
    if(!buffers[0]) return;
    if(oldVisible && oldX!=carrierSubX) {
        for(UBYTE row=14;row<=15;row++) {
            for(UBYTE c=0;c<4;c++) retireImpactProjectiles(buffers[0],oldX/8+c,row);
            bobCompositorErase(buffers[0],oldX/8,row,4);
        }
    }
    for(UBYTE row=14;row<=15;row++) {
        for(UBYTE c=0;c<4;c++) retireImpactProjectiles(buffers[0],carrierSubX/8+c,row);
        bobCompositorErase(buffers[0],carrierSubX/8,row,4);
    }
}

static DEFENCE_SMALL UBYTE carrierBombHitsSubmarine(GameState* g, WeaponState* bomb) {
    CarrierDefenceState* d=&g->defence;
    if(!bomb->active || !d->subHeight || !d->subState || d->subState==CARRIER_SUB_SINKING ||
        !rectsOverlap(bomb->x,bomb->y,4,3,d->subX+8,SEA_SURFACE_Y-d->subHeight,16,d->subHeight)) return 0;
    bomb->active=0; d->subHits++;
#if HAR_DEBUG_PERF_LOG
    carrierSubStats[5]++;
#endif
    startWorldImpact(g,d->subX+12,SEA_SURFACE_Y-8); playSfxAt(SFX_IMPACT,d->subX+16);
    if(d->subHits>=2) {
        d->subState=CARRIER_SUB_SINKING; d->subClock=0;
        awardGameScore(g,1000); g->hitsCount++;
    }
    return 1;
}

static DEFENCE_SMALL UBYTE carrierInterceptBallistic(GameState* g, WeaponState* shot) {
    WeaponState* m=&g->defence.ballistic;
    if(!shot->active || !m->active ||
        !rectsOverlap(shot->x,shot->y,8,8,m->x,m->y,8,16)) return 0;
    shot->active=0;
    /* Launch clears this otherwise unused field; damage survives the
     * offscreen wait and parachute deployment. */
    if(++m->guidanceDistance<3) {
        playSfxAt(SFX_IMPACT,m->x); return 1;
    }
    m->active=0; g->defence.ballisticPhase=0;
#if HAR_DEBUG_PERF_LOG
    carrierSubStats[4]++;
#endif
    startWorldImpact(g,m->x,m->y<0 ? 0 : m->y);
    playSfxAt(SFX_IMPACT,m->x); awardGameScore(g,100); return 1;
}

static DEFENCE_SMALL void carrierUpdateSubmarine(GameState* g, UBYTE** buffers) {
    CarrierDefenceState* d=&g->defence;
    if(g->gameMode!=GAME_MODE_ENHANCED || !d->phase) return;
    if(d->phase==DEFENCE_SINKING) {
        d->subState=d->subHeight=d->ballisticPhase=d->ballistic.active=0;
        carrierSyncSubmarine(g,buffers); return;
    }
    if(g->missionNumber>=3 && d->phase==DEFENCE_WAVE && d->waves && d->wave==d->waves && d->spawned && !d->subUsed) {
        d->subUsed=1; d->subState=CARRIER_SUB_RISING; d->subHits=d->subHeight=0;
        d->subX=216+((g->missionNumber&1)*32); d->subClock=0;
        playSfxAt(SFX_BOMB,d->subX);
    }
#if HAR_DEBUG_PERF_LOG
    if(d->subState) carrierSubStats[0]++;
    if(d->ballistic.active) carrierSubStats[1]++;
#endif
    if(d->subState) {
        d->subClock++;
        if(d->subState==CARRIER_SUB_RISING) {
            /* Bubbles warn before the periscope, then pause its first two art
             * rows before the tower rises; anchor its bottom to the water. */
            UWORD rise=d->subClock>CARRIER_SUB_BUBBLE_LEAD ? d->subClock-CARRIER_SUB_BUBBLE_LEAD : 0;
            d->subHeight=rise<=24 ? rise/12 : rise<=69 ? 2 : 2+(rise-69)/12;
            if(d->subHeight>=8) { d->subHeight=8; d->subState=CARRIER_SUB_SURFACED; d->subClock=0; }
        } else if(d->subState==CARRIER_SUB_SINKING && !(d->subClock%6)) {
            if(d->subHeight) d->subHeight--;
            if(!d->subHeight) d->subState=0;
        } else if(d->subState==CARRIER_SUB_SURFACED && d->subClock>=100 && !d->ballisticPhase) {
            WeaponState* m=&d->ballistic; memset(m,0,sizeof(*m));
            m->active=1; m->x=m->worldX=d->subX+12; m->y=SEA_SURFACE_Y-16;
            m->targetWorldX=80+((d->clock>>3)&3)*16;
            d->ballisticPhase=CARRIER_BALLISTIC_ASCENT; d->ballisticClock=0; d->subClock=0;
            playSfxAt(SFX_FIRE,m->x);
#if HAR_DEBUG_PERF_LOG
            carrierSubStats[2]++;
#endif
        }
    }
    WeaponState* m=&d->ballistic;
    if(d->ballisticPhase==CARRIER_BALLISTIC_WAIT) {
        if(d->ballisticClock && !--d->ballisticClock) {
            d->ballisticPhase=CARRIER_BALLISTIC_DESCENT; m->active=1;
            m->x=m->worldX=m->targetWorldX; m->y=-16; d->ballisticClock=0;
            playSfxAt(SFX_BOMB,m->x);
#if HAR_DEBUG_PERF_LOG
            carrierSubStats[3]++;
#endif
        }
    } else if(m->active) {
        UWORD age=++d->ballisticClock;
        WORD speed=d->ballisticPhase==CARRIER_BALLISTIC_DESCENT ? !(age&1) :
            age>=24 ? 4 : age>=12 ? 3 : 2;
        m->y+=d->ballisticPhase==CARRIER_BALLISTIC_ASCENT ? -speed : speed;
        if(d->ballisticPhase==CARRIER_BALLISTIC_ASCENT && m->y<=-16) {
            m->active=0; d->ballisticPhase=CARRIER_BALLISTIC_WAIT;
            d->ballisticClock=CARRIER_BALLISTIC_DELAY;
        } else if(!g->crashTimer && !g->ejectState && !g->respawnSafeTimer &&
            rectsOverlap(m->x,m->y,8,16,g->playerX,g->playerY,16,8)) {
            m->active=0; d->ballisticPhase=0; applyPlayerMissileDamage(g,0);
        } else if(d->ballisticPhase==CARRIER_BALLISTIC_DESCENT &&
            (carrierHullOverlap(m->x,m->y,8,16) || m->y+16>=112)) {
            m->active=0; d->ballisticPhase=0;
            carrierWeaponHitsGun(g,m->x,m->y+8,8);
            carrierDefenceImpact(g,m->x,CARRIER_HEAVY_BOMB_HULL_DAMAGE);
        }
    }
    carrierSyncSubmarine(g,buffers);
}
