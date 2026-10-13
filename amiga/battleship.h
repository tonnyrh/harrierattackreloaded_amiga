static LONG battleshipAbs(LONG n) {return n<0 ? -n : n;}
/* Resident tile boss: bounded ordnance and no full-scene redraw during combat. */
static UBYTE battleshipDestroyed(void) {
    UBYTE n=0; for(UBYTE i=0;i<BATTLESHIP_TARGETS;i++) if(!battleship.hp[i]) n++;
    return n;
}
static void battleshipBegin(GameState* g) {
    if(g->gameMode!=GAME_MODE_ENHANCED) { battleship.active=0; return; }
    if(battleship.finished==1) { battleship.finished=2; battleship.active=0; return; }
    if(!battleshipTileCache) battleshipTileCache=AllocMem(16*sizeof(BattleshipTileCache),MEMF_FAST|MEMF_CLEAR);
    memset(&battleship,0,sizeof(battleship)); battleship.active=1; battleship.pass=1; battleship.hudHealth=255;
    for(UBYTE i=0;i<BATTLESHIP_TARGETS;i++) {
        battleship.hp[i]=i==8 ? 9 : i==9 ? 4 : (i==10 || i==11) ? 1 : 2; battleship.pose[i]=1; battleship.lift[i]=i>=12 ? 0 : 16;
        /* Ignore thin aerials: mount each weapon on a roof wide enough for it. */
        UBYTE top=104;
        for(UBYTE y=56;y<112;y++) {
            UBYTE solid=0;
            for(UBYTE x=0;x<16;x++) {
                UWORD px=battleshipTargetX[i]+x;
                UWORD tile=battleshipTileMap[((y-56)/8)*120+px/8-BATTLESHIP_START_COLUMN];
                if(battleshipTiles[tile*40UL+(y&7)*5+4] & (128>>(px&7))) solid++;
            }
            if(solid>=12) {top=y;break;}
        }
        battleship.targetY[i]=((top+7)&~7)-battleshipHeight(i);
    }
    /* Reuse the existing depot fire, reduced once to three tiny 8x8 tiles. */
    memset(battleshipFireTiles,0,sizeof(battleshipFireTiles));
    for(UBYTE f=0;f<3;f++)for(UBYTE y=0;y<8;y++)for(UBYTE x=0;x<8;x++) {
        UBYTE sx=x*2,sy=y*2,part=(sy/8)*2+sx/8;
        const UBYTE* src=ammunitionDepotBlast+f*160+part*40+(sy&7)*5;
        UBYTE bit=128>>(sx&7),out=128>>x;
        for(UBYTE plane=0;plane<5;plane++)if(src[plane]&bit)battleshipFireTiles[f][y*5+plane]|=out;
    }
    for(UBYTE i=0;i<5;i++) {
        battleship.fireRow[i]=(battleshipSurface[battleshipFireColumn[i]-BATTLESHIP_START_COLUMN]-4)/8;
        battleship.fireFrame[i]=i%3;
    }
    battleship.savedWingman=g->wingmanControl;
    g->wingman.active=g->wingmanControl!=WINGMAN_CONTROL_OFF && !g->wingman.destroyed;
    g->wingman.destroyed=!g->wingman.active;
    g->wingman.mode=g->wingmanControl==WINGMAN_CONTROL_PLAYER2 ? WINGMAN_PLAYER2_FLIGHT : WINGMAN_FORMATION;
    g->wingman.interceptScreenX=64;g->wingman.screenY=24;
    g->defence.phase=0; g->scrollX=0; g->takeoffState=TAKEOFF_STATE_AIRBORNE;
    g->playerX=100; g->playerY=32; g->speedLevel=2; g->respawnSafeTimer=100;
    g->defence.fullVtol=0; battleship.supplyClock=160; battleship.heliClock=220;
    carrierSceneHull=255; carrierParkedWingmanVisible=0;
}
static UBYTE battleshipHealth(void) {
    UBYTE hp=battleship.hp[8]+battleship.hp[9];
    for(UBYTE i=0;i<BATTLESHIP_TARGETS;i++) if(battleshipIsGun(i)) hp+=battleship.hp[i];
    return hp;
}
static void battleshipDrawHud(UBYTE* hud) {
    UBYTE health=battleshipHealth();
    /* Static label is not cleared/repainted during the visible raster. */
    if(battleship.hudHealth==255) {
        fillRect(hud,272,16,48,29,HUD_COLOR_BACKGROUND);
        fillRect(hud,0,58,320,17,HUD_COLOR_BACKGROUND);
        drawTextCentered(hud,58,"BATTLESHIP",HUD_COLOR_LABEL);
        fillRect(hud,64,68,192,6,2);
    }
    if(battleship.hudHealth!=health) {
        UWORD width=(190UL*health)/BATTLESHIP_MAX_HEALTH;
        fillRect(hud,65,69,width,4,HUD_COLOR_FUEL);
        fillRect(hud,65+width,69,190-width,4,HUD_COLOR_BACKGROUND);
        battleship.hudHealth=health;
    }
}
static void battleshipCommitDamage(UBYTE* bitmap,LONG scrollX) {
    if(!battleship.active) return;
    for(UBYTE i=0;i<5;i++)if(battleship.fireDirty&(1<<i)) {
        LONG col=battleshipFireColumn[i];WORD row=battleship.fireRow[i];
        if(col*8+8<scrollX || col*8>scrollX+320)continue;
        if(battleshipResident[(UWORD)col&127]==col) {
            invalidatePowerupBackgroundColumn(col);
            UWORD x=ringWorldTileXForColumn(col);
            drawGameScrollTile(bitmap,x,row,0);battleshipDrawRow(bitmap,x,col,row);
            if(x<GAME_WORLD_BUFFER_MARGIN_TILES+GAME_FETCH_BYTES) {
                drawGameScrollTile(bitmap,x+GAME_WORLD_SCROLL_PAGE_BYTES,row,0);
                battleshipDrawRow(bitmap,x+GAME_WORLD_SCROLL_PAGE_BYTES,col,row);
            }
        }
    }
    battleship.fireDirty=0;
    for(UBYTE i=0;i<BATTLESHIP_TARGETS;i++) if(battleship.dirty[i]) {
        /* Row/column reconstruction shares exactly the BOB restore overlay. */
        for(UBYTE c=0;c<battleshipWidth(i)/8;c++) for(UBYTE r=0;r<battleshipHeight(i)/8;r++) {
            LONG col=battleshipTargetX[i]/8+c; WORD row=battleship.targetY[i]/8+r;
            invalidatePowerupBackgroundColumn(col);
            UWORD physical=ringWorldTileXForColumn(col);
            if(battleshipResident[(UWORD)col&127]!=col) continue;
            drawGameScrollTile(bitmap,physical,row,0);
            battleshipDrawRow(bitmap,physical,col,row);
            if(physical<GAME_WORLD_BUFFER_MARGIN_TILES+GAME_FETCH_BYTES) {
                drawGameScrollTile(bitmap,physical+GAME_WORLD_SCROLL_PAGE_BYTES,row,0);
                battleshipDrawRow(bitmap,physical+GAME_WORLD_SCROLL_PAGE_BYTES,col,row);
            }
        }
        battleship.dirty[i]=0;
    }
}
static void battleshipFuel(GameState* g) {
    if(!g->fuelGaugeLevel || g->aircraftFailureState) return;
    UWORD limit=playerFuelClockLimit(g);
    ULONG capacity=(ULONG)CPC_FUEL_TOTAL_QUANTA*limit;
    ULONG remaining=((ULONG)(g->fuelGaugeLevel-1)*CPC_FUEL_SUBCOUNT_FULL+g->fuelSubCounter)*limit-g->fuelClockAccumulator;
    remaining+=capacity/5;
    if(remaining>=capacity) resetPlayerFuel(g);
    else {
        UWORD q=(remaining+limit-1)/limit;
        g->fuelClockAccumulator=(ULONG)q*limit-remaining;
        g->fuelGaugeLevel=(q-1)/CPC_FUEL_SUBCOUNT_FULL+1;
        g->fuelSubCounter=(q-1)%CPC_FUEL_SUBCOUNT_FULL+1;
        g->fuel=cpcFuelHudValue(g);
    }
    playSfxAt(SFX_PICKUP_POWERUP,g->playerX);
}
static void battleshipUpdateFires(void) {
    if(!battleship.burning && (UWORD)battleshipHealth()*10<=BATTLESHIP_MAX_HEALTH) {
        battleship.burning=1;
        /* Light fires gradually; authoritative compositor handles streamed ones. */
    }
    if(battleship.burning && !(battleship.clock&7)) {
        UBYTE i=(battleship.clock/8)%5;
        battleship.fireFrame[i]=(battleship.fireFrame[i]+1)%3;
        battleship.fireDirty|=1<<i;
    }
}
static void battleshipHitTarget(GameState* g,UBYTE i,UBYTE bomb) {
    if(!battleship.hp[i] || (battleshipIsGun(i) && battleship.lift[i]<16)) return;
    UBYTE damage=bomb ? 2 : 1;
    battleship.hp[i]=battleship.hp[i]>damage ? battleship.hp[i]-damage : 0;
    battleship.dirty[i]=1; battleship.flash[i]=8;
    startImpact(g,battleshipTargetX[i]+battleshipWidth(i)/2-g->scrollX-4,battleship.targetY[i]+4);
    g->impact.worldAnchored=1;
    if(i==8) battleship.stage=battleship.hp[8]<=3 ? 2 : battleship.hp[8]<=6 ? 1 : 0;
    if(!battleship.hp[i]) {
        awardGameScore(g,500); g->hitsCount++;
        if(i==10 || i==11) battleshipFuel(g);
    }
    if(!battleshipHealth()) {
        battleship.phase=2; battleship.wait=224;
        g->enemyMissile.active=g->helicopter.active=0;
        memset(battleship.flak,0,sizeof(battleship.flak));
        memset(g->helicopterBullets,0,sizeof(g->helicopterBullets));
        awardGameScore(g,3000);
    }
}
static UBYTE battleshipWeaponHit(GameState* g,WeaponState* w,UBYTE bomb) {
    if(g->helicopter.active && battleshipAbs(w->x-g->helicopter.x)<14 && battleshipAbs(w->y-g->helicopter.y)<9) {
        w->active=0; startImpact(g,w->x,w->y);
        if(bomb || ++g->helicopterHits>=2) {g->helicopter.active=0;awardGameScore(g,400);}
        return 1;
    }
    for(UBYTE i=0;i<BATTLESHIP_TARGETS;i++) if(battleship.hp[i] && (!battleshipIsGun(i) || battleship.lift[i]==16) &&
        w->worldX+6>=battleshipTargetX[i] && w->worldX<battleshipTargetX[i]+battleshipWidth(i) &&
        w->y+5>=battleship.targetY[i] && w->y<battleship.targetY[i]+battleshipHeight(i)) {
        battleshipHitTarget(g,i,bomb); w->active=0; return 1;
    }
    LONG col=(w->worldX>>3)-BATTLESHIP_START_COLUMN;
    if(col>=0 && col<120 && w->y+3>=battleshipSurface[col]) {
        /* Cosmetic hull impacts must not replace the positive target-hit burst. */
        if(!g->impact.active) startImpact(g,w->x,w->y);
        w->active=0; return 1;
    }
    return 0;
}
static void battleshipMoveCamera(GameState* g) {
    WORD target=battleship.phase==1 ? -40 : 16;
    if(battleship.velocity16<target) battleship.velocity16++;
    if(battleship.velocity16>target) battleship.velocity16--;
    battleship.camera16+=battleship.velocity16;
    if(battleship.camera16>=BATTLESHIP_LAST_CAMERA*16) {
        battleship.camera16=BATTLESHIP_LAST_CAMERA*16; battleship.phase=1;
    }
    if(battleship.phase==1 && battleship.camera16<BATTLESHIP_FIRST_RETURN*16) {
        battleship.camera16=BATTLESHIP_FIRST_RETURN*16; battleship.velocity16=0; battleship.phase=0; battleship.pass++;
    }
    g->scrollX=battleship.camera16/16;
}
static UBYTE battleshipFlakRoom(const GameState* g) {
    UBYTE used=0;
    for(UBYTE i=0;i<4;i++)used+=battleship.flak[i].active!=0;
    for(UBYTE i=0;i<HELICOPTER_BULLET_MAX;i++)used+=g->helicopterBullets[i].active!=0;
    return used<4;
}
static UBYTE battleshipMissileSector(WORD dx,WORD dy) {
    return battleshipAbs(dx)>=48 && battleshipAbs(dx)>=battleshipAbs(dy);
}
static void battleshipLaunchMissile(GameState* g,UBYTE i) {
    WeaponState* w=&g->enemyMissile;if(w->active)return;
    LONG x=battleshipTargetX[i]+8;WORD y=battleship.targetY[i]+4;
    WORD dx=(LONG)g->scrollX+g->playerX+8-x,dy=g->playerY+4-y;
    if(!battleshipMissileSector(dx,dy)) {
        if(!g->wingman.active)return;
        dx=(LONG)g->scrollX+wingmanScreenX(g)+8-x;dy=g->wingman.screenY+4-y;
        if(!battleshipMissileSector(dx,dy))return;
    }
    WORD extent=battleshipAbs(dx);
    memset(w,0,sizeof(*w));w->active=1;w->type=ENEMY_MISSILE_CARRIER_TYPE;
    w->worldX=x;w->x=x-g->scrollX;w->y=y;
    w->dx=dx*48/extent;w->dy=dy*48/extent;
    battleship.missileSubX=battleship.missileSubY=0;
    battleship.pose[i]=dx<0 ? 0 : 2;battleship.dirty[i]=1;
    playSfxAt(SFX_FIRE,w->x);
}
static void battleshipMoveMissile(GameState* g) {
    WeaponState* w=&g->enemyMissile;if(!w->active)return;
    battleship.missileSubX+=w->dx;battleship.missileSubY+=w->dy;
    WORD dx=battleship.missileSubX/16,dy=battleship.missileSubY/16;
    battleship.missileSubX-=dx*16;battleship.missileSubY-=dy*16;
    w->worldX+=dx;w->y+=dy;w->x=w->worldX-g->scrollX;
    if(w->x<-8 || w->x>319 || w->y<0 || w->y>=119 || ++w->timer>180) {w->active=0;return;}
    if(!g->respawnSafeTimer && rectsOverlap(w->x,w->y,8,8,g->playerX,g->playerY,16,8)) {
        w->active=0;applyPlayerMissileDamage(g,0);return;
    }
    if(g->wingman.active && rectsOverlap(w->x,w->y,8,8,wingmanScreenX(g),g->wingman.screenY,16,8)) {
        w->active=0;g->wingman.active=0;g->wingman.destroyed=1;startImpact(g,w->x,w->y);return;
    }
    WeaponState* shots[2]={&g->rocketShot,&g->wingman.rocket};
    for(UBYTE n=0;n<2;n++)if(shots[n]->active && rectsOverlap(w->x,w->y,8,8,shots[n]->x,shots[n]->y,8,8)) {
        w->active=shots[n]->active=0;startImpact(g,w->x,w->y);return;
    }
    LONG col=(w->worldX>>3)-BATTLESHIP_START_COLUMN;
    if(col>=0 && col<120 && w->y>=battleshipSurface[col]) {w->active=0;startImpact(g,w->x,w->y);}
}
static void battleshipSupport(GameState* g,WORD delta,const Player2InputState* in2) {
    if(battleship.supplyClock) battleship.supplyClock--;
    else if(battleship.phase==1 && battleship.supplyPass!=battleship.pass && !g->powerup.active) {
        UBYTE type=(battleship.supplyId++&1) ? POWERUP_BOMBS : POWERUP_ROCKETS;
        if(g->rockets<g->bombs) type=POWERUP_ROCKETS;
        else if(g->bombs<g->rockets) type=POWERUP_BOMBS;
        if(battleship.savedWingman!=WINGMAN_CONTROL_OFF && !g->wingman.active)
            type=POWERUP_WINGMAN;
        battleship.supplyPass=battleship.pass;
        spawnPowerup(g,type,1);
        g->powerup.worldX=g->scrollX+g->playerX+24;
        battleship.supplyClock=300+(battleship.clock&127);
    }
    if(g->powerup.active) {
        PowerupState* p=&g->powerup;
        /* Drops descend in screen space while the ship overtakes below. */
        p->worldX+=delta; moveEnhancedPowerup(p);
        WORD x=p->worldX-g->scrollX;
        if(battleshipAbs(x-g->playerX)<16 && battleshipAbs(p->y-g->playerY)<12) {
            activatePowerup(g,p->type); p->active=0;
        } else if(x<0 || x>312 || p->y>=116) p->active=0;
    }
    if(battleship.heliClock) battleship.heliClock--;
    else if(battleship.hp[9] && !g->helicopter.active) {
        WORD sx=battleshipTargetX[9]-g->scrollX;
        if(sx>8 && sx<296) {
            WeaponState* h=&g->helicopter;memset(h,0,sizeof(*h));
            h->active=1;h->worldX=battleshipTargetX[9]+4;h->x=sx+4;h->y=battleship.targetY[9]+2;
            g->helicopterAge=g->helicopterHits=0;battleship.heliClock=600;
            updateHelicopterHeading(g);
        }
    }
    if(g->helicopter.active) {
        WeaponState* h=&g->helicopter;g->helicopterAge++;
        WORD previous=h->x;
        if(g->helicopterAge<80) {if(!(g->helicopterAge&1)) h->y--;h->x=h->worldX-g->scrollX;}
        else {
            WORD goal=g->playerX+72;if(goal>288)goal=288;
            if(!(g->helicopterAge&1)) {if(h->x<goal)h->x++;if(h->x>goal)h->x--;}
            WORD altitude=g->playerY-12;if(altitude<12)altitude=12;
            if(!(g->helicopterAge&3)) {if(h->y<altitude)h->y++;else if(h->y>altitude)h->y--;}
            h->worldX=g->scrollX+h->x;
        }
        if(h->x!=previous) h->direction=h->x>previous;
        updateHelicopterHeading(g);
        if(h->x<-16 || h->x>=320 || g->helicopterAge>650) h->active=0;
        else if(g->helicopterAge>80 && !(g->helicopterAge%100) && battleshipFlakRoom(g)) {
            for(UBYTE n=0;n<HELICOPTER_BULLET_MAX;n++) if(!g->helicopterBullets[n].active) {
                HelicopterBullet* b=&g->helicopterBullets[n];memset(b,0,sizeof(*b));
                b->active=1;b->worldX=h->worldX;b->y=h->y+6;
                b->vx16=(g->playerX+48-h->x)*16/12;b->vy16=(g->playerY-h->y)*16/12;break;
            }
        }
        if(!(g->helicopterAge&7)) playSfxAt(SFX_HELICOPTER,h->x);
    }
    WingmanState* wing=&g->wingman;
    if(wing->active) {
        UBYTE human=g->wingmanControl==WINGMAN_CONTROL_PLAYER2;
        wing->mode=human ? WINGMAN_PLAYER2_FLIGHT : WINGMAN_FORMATION;
        if(human) {wing->interceptScreenX+=(in2->right-in2->left)*2;wing->screenY+=(in2->down-in2->up)*2;}
        else {
            WORD x=g->playerX-32,y=g->playerY-12;
            if(wing->interceptScreenX<x)wing->interceptScreenX++;
            else if(wing->interceptScreenX>x)wing->interceptScreenX--;
            if(wing->screenY<y)wing->screenY++;
            else if(wing->screenY>y)wing->screenY--;
        }
        if(wing->interceptScreenX<8)wing->interceptScreenX=8;
        if(wing->interceptScreenX>296)wing->interceptScreenX=296;
        if(wing->screenY<8)wing->screenY=8;
        if(wing->screenY>108)wing->screenY=108;
        wing->row=wing->screenY/8;
        if(!wing->rocket.active && (human ? in2->fire && g->rockets : !(battleship.clock%80))) {
            WeaponState* w=&wing->rocket;memset(w,0,sizeof(*w));w->active=1;
            w->worldX=g->scrollX+wing->interceptScreenX+8;w->x=wing->interceptScreenX+8;w->y=wing->screenY+2;w->dx=5;
            if(human)g->rockets--;playSfxAt(SFX_FIRE,w->x);
        }
        if(human && in2->bomb && g->bombs && !wing->bomb.active) {
            WeaponState* w=&wing->bomb;memset(w,0,sizeof(*w));w->active=1;
            w->worldX=g->scrollX+wing->interceptScreenX+6;w->x=wing->interceptScreenX+6;w->y=wing->screenY+7;
            w->dx=12;w->dy=16;g->bombs--;
        }
        LONG col=(g->scrollX+wing->interceptScreenX+8)/8-BATTLESHIP_START_COLUMN;
        if(col>=0 && col<120 && wing->screenY+7>=battleshipSurface[col]) {
            startImpact(g,wing->interceptScreenX,wing->screenY);wing->active=0;wing->destroyed=1;
        }
    }
    WeaponState* w=&wing->rocket;
    if(w->active) {w->worldX+=5+delta;w->x=w->worldX-g->scrollX;if(!battleshipWeaponHit(g,w,0) && (w->x>=320 || ++w->timer>70))w->active=0;}
    w=&wing->bomb;
    if(w->active) {
        w->targetWorldX+=w->dx;w->targetY+=w->dy;
        w->worldX+=delta+w->targetWorldX/16;w->y+=w->targetY/16;w->targetWorldX%=16;w->targetY%=16;
        if(w->dy<40)w->dy+=2;w->x=w->worldX-g->scrollX;
        if(!battleshipWeaponHit(g,w,1) && (w->y>=119 || w->x<0 || w->x>=320))w->active=0;
    }
}
/* Four existing effect layers bound both CPU work and the flak wall. */
static void battleshipBurst(LONG x,WORD y) {
    for(UBYTE n=0;n<4;n++) if(!battleship.flak[n].active) {
        WeaponState* f=&battleship.flak[n];memset(f,0,sizeof(*f));
        f->active=1;f->worldX=x;f->y=y;f->type=(x>>3)&1;
        if(f->y<8)f->y=8;if(f->y>100)f->y=100;
        break;
    }
}
static void battleshipMoveFlak(GameState* g,WORD delta) {
    for(UBYTE n=0;n<4;n++) {
        WeaponState* f=&battleship.flak[n];if(!f->active)continue;
        f->worldX+=delta-2;f->x=f->worldX-g->scrollX;
        if(f->timer<255)f->timer++;
        if(f->x<=-8) {f->active=0;continue;}
        if(!battleship.hitSafe && !g->respawnSafeTimer && battleshipAbs(f->x-g->playerX)<10 && battleshipAbs(f->y-g->playerY)<8) {
            applyPlayerFlakDamage(g);battleship.hitSafe=20;
        }
        if(g->wingman.active && battleshipAbs(f->x-wingmanScreenX(g))<10 && battleshipAbs(f->y-g->wingman.screenY)<8) {
            startImpact(g,wingmanScreenX(g),g->wingman.screenY);g->wingman.active=0;g->wingman.destroyed=1;
        }
    }
}
static void battleshipUpdate(GameState* g,const InputState* in,const InputState* old,const Player2InputState* in2,UBYTE** buffers) {
    (void)buffers; (void)old;
    for(UBYTE i=0;i<BATTLESHIP_TARGETS;i++) {
        if(battleship.flash[i] && !--battleship.flash[i]) battleship.dirty[i]=1;
        if(i>=12 && battleship.stage>=(i-12)/2+1 && battleship.lift[i]<16 && !(battleship.clock&3)) {
            battleship.lift[i]++;battleship.dirty[i]=1;
        }
    }
    battleship.clock++;
#if HAR_HEADLESS_BATTLESHIP_FINALE
    if(battleship.clock==600) {
        memset(battleship.hp,0,sizeof(battleship.hp));battleship.hp[9]=3;
        for(UBYTE i=0;i<BATTLESHIP_TARGETS;i++){battleship.lift[i]=16;battleship.dirty[i]=1;}
    }
    if(battleship.clock==900)
        for(UBYTE i=0;i<BATTLESHIP_TARGETS;i++) {battleship.lift[i]=16;while(battleship.hp[i])battleshipHitTarget(g,i,1);}
#endif

    if(battleship.pass==1 && battleship.clock>=90 && battleship.clock<140)carrierTryAlarm(&g->defence);
    /* Shoreless opening: offscreen batteries warn the player before the stern arrives. */
    if(battleship.pass==1 && (battleship.clock==140 || battleship.clock==180) && battleshipFlakRoom(g)) {
        for(UBYTE n=0;n<HELICOPTER_BULLET_MAX;n++)if(!g->helicopterBullets[n].active) {
            HelicopterBullet* b=&g->helicopterBullets[n];memset(b,0,sizeof(*b));
            b->active=1;b->worldX=(LONG)g->scrollX+312;b->y=92;
            b->vx16=(g->playerX+64-312)*16/12;
            b->vy16=(g->playerY+(battleship.clock==140 ? -16 : 16)-92)*16/12;
            playSfxAtTuned(SFX_FLAK_GUN_1,319,56,SFX_PAULA_PERIOD*3/2);break;
        }
    }
    battleshipUpdateFires();
    battleship.water=(battleship.water+2)&255;
    if(g->respawnSafeTimer) g->respawnSafeTimer--;
    if(battleship.hitSafe) battleship.hitSafe--;
    if(g->impact.active && g->impact.timer && !--g->impact.timer) g->impact.active=0;
    if(battleship.phase==2) {
        /* One compact 16x16 fireball plus one small secondary: fixed BOB budget. */
        if(g->ammoBlast.active && g->ammoBlast.timer && !--g->ammoBlast.timer) g->ammoBlast.active=0;
        if(g->defence.bomberBlastTicks)g->defence.bomberBlastTicks--;
        UWORD age=224-battleship.wait;
        if(age==180) {
            g->ammoBlast.active=0;
            g->defence.bomberBlastTicks=24;
            g->defence.bomberBlastX=144;g->defence.bomberBlastY=72;
            playSfxAt(SFX_DEPOT_BOOM,160);
        }
        if(!(age%40) && battleship.wait>24) {
            static const UWORD positions[5]={48,216,120,264,80};
            WORD x=positions[(age/40)%5];
            memset(&g->ammoBlast,0,sizeof(g->ammoBlast));
            g->ammoBlast.active=1;g->ammoBlast.timer=36;
            g->ammoBlast.worldX=((g->scrollX+x)>>3)*8;
            g->ammoBlast.x=g->ammoBlast.worldX-g->scrollX;
            g->ammoBlast.y=72+(age/40%3)*12;
            playSfxAt(SFX_DEPOT_BOOM,x);
        }
        if((age&15)==8) startWorldImpactQuiet(g,32+(age*7)%256,64+(age&31));
        if(battleship.wait) battleship.wait--; else {g->ammoBlast.active=0;g->defence.bomberBlastTicks=0;battleship.finished=1;}
        return;
    }
    WORD oldScroll=g->scrollX; battleshipMoveCamera(g);
    WORD delta=(WORD)g->scrollX-oldScroll;
    g->lowSpeedLanding=battleship.phase==1;
    WORD oldX=g->playerX,oldY=g->playerY;
    if(in->up && g->playerY>8) g->playerY-=2;
    if(in->down && g->playerY<108) g->playerY+=2;
    if(in->left && g->playerX>24) g->playerX-=2;
    if(in->right && g->playerX<280) g->playerX+=2;
    battleship.vx16=(g->playerX-oldX)*16;
    battleship.vy16=(g->playerY-oldY)*16;
    if(g->bombLaunchCooldown) g->bombLaunchCooldown--;
    if(in->fire && !g->rocketShot.active) {
        launchRocket(g,0);g->rocketShot.dx=5;
    }
    if(in->bomb && !g->bombShot.active && launchBomb(g)) {
        g->bombShot.dx=12;
        g->bombShot.dy=16;
        g->bombShot.targetWorldX=g->bombShot.targetY=0;
    }
    WeaponState* w=&g->rocketShot;
    if(w->active) {
        w->worldX+=w->dx+delta; w->x=w->worldX-g->scrollX;
        if(g->rocketHeightLock) w->y=g->playerY+2;
        if(!battleshipWeaponHit(g,w,0) && (w->x<0 || w->x>=320 || ++w->timer>70)) w->active=0;
    }
    w=&g->bombShot;
    if(w->active) {
        w->targetWorldX+=w->dx;w->targetY+=w->dy;
        w->worldX+=delta+w->targetWorldX/16;w->y+=w->targetY/16;
        w->targetWorldX%=16;w->targetY%=16;
        if(w->dy<40) w->dy+=2;
        w->x=w->worldX-g->scrollX;w->timer++;
        if(!battleshipWeaponHit(g,w,1) && (w->y>=119 || w->y<0 || w->x<0 || w->x>=320)) w->active=0;
    }
    if(g->impact.active) g->impact.x=g->impact.worldX-g->scrollX;
    battleshipSupport(g,delta,in2);
    LONG playerWorld=(LONG)g->scrollX+g->playerX;
    /* Only visible live turrets fire, with two timed shells and four clouds max. */
    for(UBYTE i=0;i<BATTLESHIP_TARGETS;i++) {
        WORD sx=battleshipTargetX[i]-g->scrollX;
        if(!battleshipIsGun(i) || battleship.lift[i]<16 || !battleship.hp[i] || sx<12 || sx>304) continue;
        WORD dx=playerWorld+8-battleshipTargetX[i];
        UBYTE pose=dx < -24 ? 0 : dx>24 ? 2 : 1;
        if(pose!=battleship.pose[i]) {battleship.pose[i]=pose;battleship.dirty[i]=1;}
        if(battleshipIsMissile(i)) {
            if(!((battleship.clock+i*19)%100))battleshipLaunchMissile(g,i);
            continue;
        }
        if((battleship.clock+i*19)%(60-battleship.stage*9) || !battleshipFlakRoom(g)) continue;
        /* Burst ahead rather than directly on the aircraft; alternate heights. */
        static const BYTE heights[5]={-24,12,-12,24,0};
        WORD aimY=g->playerY+heights[(i+battleship.clock/35)%5];
        if(aimY<12)aimY=12;if(aimY>88)aimY=88;
        WORD ahead=g->playerX+64+(i%3)*8;
        if(ahead>300)ahead=300;
        LONG aimX=(LONG)g->scrollX+ahead+(battleship.velocity16*12)/16;
        for(UBYTE n=0;n<HELICOPTER_BULLET_MAX;n++) if(!g->helicopterBullets[n].active) {
            HelicopterBullet* b=&g->helicopterBullets[n];memset(b,0,sizeof(*b));
            b->active=1;b->worldX=battleshipTargetX[i]+8;b->y=battleship.targetY[i];
            b->vx16=(aimX-b->worldX)*16/12;b->vy16=(aimY-b->y)*16/12;
            playSfxAtTuned(SFX_FLAK_GUN_1,sx,56,SFX_PAULA_PERIOD*3/2);break;
        }
    }
    battleshipMoveMissile(g);
    battleshipMoveFlak(g,delta);
    for(UBYTE n=0;n<HELICOPTER_BULLET_MAX;n++) {
        HelicopterBullet* b=&g->helicopterBullets[n]; if(!b->active) continue;
        WORD px=b->subX+b->vx16,py=b->subY+b->vy16;
        b->worldX+=px/16;b->y+=py/16;b->subX=px%16;b->subY=py%16;
        WORD sx=b->worldX-g->scrollX;
        if(sx<0 || sx>319 || b->y<1 || b->y>118) {b->active=0;continue;}
        if(++b->age>=12) {
            battleshipBurst(b->worldX,b->y);b->active=0;
            playFlakGunSfx(battleship.clock^(n<<8),sx,b->y,g->playerY);
        }
    }
    if(g->helicopter.active && !g->respawnSafeTimer &&
        rectsOverlap(g->playerX,g->playerY,16,8,g->helicopter.x,g->helicopter.y,16,8)) {
        g->helicopter.active=0;startWorldImpact(g,g->playerX,g->playerY);
        applyPlayerAircraftCollision(g);
    }
    LONG col=((playerWorld+8)>>3)-BATTLESHIP_START_COLUMN;
    if(col>=0 && col<120 && g->playerY+7>=battleshipSurface[col]) {
        g->playerY=battleshipSurface[col]-12;battleship.vy16=-16;
        if(!battleship.hitSafe && !g->respawnSafeTimer) {
            applyPlayerMissileDamage(g,1);battleship.hitSafe=30;
            startImpact(g,g->playerX,g->playerY+8);
        }
    }
    if(!(battleship.clock&7)) updatePlayerFuel(g);
    if(!g->armour || !g->fuel || g->aircraftFailureState) {
        if(g->lives>1) {
            g->lives--;g->armour=100;g->missileDamageThirds=0;g->flakDamageCount=0;
            g->aircraftFailureState=0;g->crashTimer=0;g->playerY=20;battleship.vx16=battleship.vy16=0;g->respawnSafeTimer=120;
            resetPlayerFuel(g);ammoForGame(g,&g->bombs,&g->rockets);
        } else {g->gameOver=1;g->lives=0;}
    }
    updateEngineSound(2,0);
}

#if HAR_HEADLESS_CLASSIC_CONTRACT_TEST
static UBYTE referenceBattleship(void) {
    static GameState g;
    InputState in={0};Player2InputState in2={0};
    memset(&g,0,sizeof(g));g.gameMode=GAME_MODE_ENHANCED;g.missionNumber=1;g.levelDifficulty=1;
    g.wingmanControl=WINGMAN_CONTROL_CPU;
    battleship.finished=0;battleshipBegin(&g);g.armour=100;g.lives=3;resetPlayerFuel(&g);ammoForGame(&g,&g.bombs,&g.rockets);
    if(!battleship.active || g.scrollX || battleship.hp[8]!=9 || battleshipHealth()!=37 || !g.wingman.active || g.defence.fullVtol) return 1;
    memset(battleship.hp,0,sizeof(battleship.hp));battleship.hp[8]=4;
    battleshipUpdateFires();if(battleship.burning)return 43;
    battleship.hp[8]=3;battleshipUpdateFires();if(!battleship.burning)return 44;
    for(UBYTE tick=0;tick<40;tick++) {
        battleship.clock=tick;battleship.fireDirty=0;battleshipUpdateFires();
        UBYTE mask=battleship.fireDirty;
        if(mask && (mask&(mask-1)))return 45;
        for(UBYTE i=0;i<5;i++)if(battleship.fireFrame[i]>=3 || battleship.fireRow[i]>=15)return 46;
    }
    battleshipBegin(&g);
    UBYTE rockets=g.rockets;WORD wingX=g.wingman.interceptScreenX;
    g.wingmanControl=WINGMAN_CONTROL_PLAYER2;in2.right=in2.fire=1;
    battleshipSupport(&g,0,&in2);
    if(g.wingman.interceptScreenX!=wingX+2 || g.rockets!=rockets-1 || !g.wingman.rocket.active)return 20;
    in2.right=in2.fire=0;g.wingmanControl=WINGMAN_CONTROL_CPU;g.wingman.rocket.active=0;
    battleshipHitTarget(&g,12,1);if(battleship.hp[12]!=2) return 2;
    battleshipHitTarget(&g,0,1);if(battleship.hp[0] || !g.impact.active || !battleship.flash[0]) return 3;
    UBYTE sawReverse=0;g.bombs=g.rockets=0;
    for(UWORD t=0;t<2600;t++) {
        WORD old=g.scrollX;battleshipMoveCamera(&g);
        if(battleshipAbs((WORD)g.scrollX-old)>3 || g.scrollX>BATTLESHIP_LAST_CAMERA) return 4;
        if((WORD)g.scrollX<old) sawReverse=1;
    }
    if(!sawReverse || battleship.pass<2 || battleship.hp[0] || g.bombs || g.rockets) return 5;
    for(UBYTE h=0;h<3;h++)battleshipHitTarget(&g,8,0);
    if(battleship.stage!=1) return 6;
    for(UWORD t=0;t<70;t++)battleshipUpdate(&g,&in,&in,&in2,0);
    if(battleship.lift[12]!=16 || battleship.lift[13]!=16 || battleship.lift[14])return 7;
    for(UBYTE h=0;h<3;h++)battleshipHitTarget(&g,8,0);
    for(UWORD t=0;t<70;t++)battleshipUpdate(&g,&in,&in,&in2,0);
    if(battleship.stage!=2 || battleship.lift[14]!=16 || battleship.lift[15]!=16)return 8;
    /* Newly dropped bomb visibly descends and no longer runs flat forward. */
    g.bombs=4;g.bombLaunchCooldown=0;g.bombShot.active=0;g.playerY=8;g.playerX=100;
    in.bomb=1;battleshipUpdate(&g,&in,&in,&in2,0);in.bomb=0;
    WORD bombY=g.bombShot.y;
    for(UBYTE t=0;t<8;t++)battleshipUpdate(&g,&in,&in,&in2,0);
    if(!g.bombShot.active || g.bombShot.y<=bombY+8 || g.bombs!=3)return 9;
    g.powerup.active=0;battleship.supplyClock=0;battleship.supplyPass=0;battleship.phase=0;
    battleshipSupport(&g,0,&in2);if(g.powerup.active)return 25;
    battleship.phase=1;g.rockets=0;g.bombs=10;battleship.savedWingman=WINGMAN_CONTROL_OFF;
    battleshipSupport(&g,0,&in2);if(!g.powerup.active || g.powerup.type!=POWERUP_ROCKETS)return 10;
    g.powerup.worldX=g.scrollX+g.playerX;g.powerup.y=g.powerup.logicalY=g.playerY;
    battleshipSupport(&g,0,&in2);if(g.powerup.active || !g.rockets)return 11;
    battleship.supplyClock=0;battleshipSupport(&g,0,&in2);if(g.powerup.active)return 26;
    battleship.pass++;g.bombs=0;g.rockets=10;
    battleshipSupport(&g,0,&in2);if(!g.powerup.active || g.powerup.type!=POWERUP_BOMBS)return 35;
    g.powerup.active=0;battleship.supplyClock=0;battleship.pass++;
    battleship.savedWingman=g.wingmanControl=WINGMAN_CONTROL_CPU;g.wingman.active=0;g.wingman.destroyed=1;
    battleshipSupport(&g,0,&in2);if(!g.powerup.active || g.powerup.type!=POWERUP_WINGMAN)return 39;
    g.powerup.worldX=g.scrollX+g.playerX;g.powerup.y=g.powerup.logicalY=g.playerY;
    battleshipSupport(&g,0,&in2);if(!g.wingman.active)return 40;
    /* Camera reversals cannot change a released bomb's screen-space drift. */
    g.powerup.active=0;g.playerY=8;g.bombShot.active=0;g.bombs=4;g.bombLaunchCooldown=0;
    battleship.camera16=800*16;g.scrollX=800;battleship.velocity16=-40;
    in.bomb=1;battleshipUpdate(&g,&in,&in,&in2,0);in.bomb=0;
    WORD lastBombX=g.bombShot.x;
    if(!g.lowSpeedLanding || playerLandingLampVisual(&g)!=COCKPIT_INDICATOR_YELLOW)return 36;
    for(UBYTE t=0;t<8;t++) {
        if(t==4)battleship.phase=0;
        battleshipUpdate(&g,&in,&in,&in2,0);
        if(g.bombShot.x<lastBombX || g.bombShot.x>lastBombX+1)return 37;
        lastBombX=g.bombShot.x;
    }
    if(g.lowSpeedLanding)return 38;
    memset(battleship.flak,0,sizeof(battleship.flak));
    battleshipBurst(g.scrollX+180,40);battleshipBurst(g.scrollX+220,60);
    UBYTE clouds=0;for(UBYTE n=0;n<4;n++)clouds+=battleship.flak[n].active;
    if(clouds!=2 || battleship.flak[0].y==battleship.flak[1].y)return 27;
    LONG flakOld=battleship.flak[0].worldX;g.scrollX+=2;battleshipMoveFlak(&g,2);
    if(battleship.flak[0].worldX!=flakOld)return 28;
    for(UBYTE tick=0;tick<42;tick++)battleshipMoveFlak(&g,0);
    if(!battleship.flak[0].active || !battleship.flak[1].active)return 29;
    for(UWORD tick=0;tick<200;tick++)battleshipMoveFlak(&g,0);
    for(UBYTE n=0;n<4;n++)if(battleship.flak[n].active)return 31;
    if(battleshipMissileSector(0,-60) || battleshipMissileSector(47,-20) ||
        !battleshipMissileSector(-64,-30) || !battleshipMissileSector(64,-30))return 32;
    g.wingman.active=0;g.enemyMissile.active=0;g.scrollX=600;
    g.playerX=battleshipTargetX[1]+8-g.scrollX;g.playerY=battleship.targetY[1]-40;
    battleshipLaunchMissile(&g,1);if(g.enemyMissile.active)return 33;
    g.playerX+=80;battleshipLaunchMissile(&g,1);
    if(!g.enemyMissile.active || g.enemyMissile.dx<=0 || g.enemyMissile.type!=ENEMY_MISSILE_CARRIER_TYPE)return 34;
    g.enemyMissile.active=0;g.playerX-=160;battleshipLaunchMissile(&g,1);
    if(!g.enemyMissile.active || g.enemyMissile.dx>=0)return 35;
    g.enemyMissile.active=0;
    UWORD fuel=g.fuel;g.fuelGaugeLevel=8;g.fuelSubCounter=1;g.fuelClockAccumulator=0;g.fuel=cpcFuelHudValue(&g);fuel=g.fuel;
    battleshipHitTarget(&g,10,0);if(g.fuel<=fuel)return 12;
    g.scrollX=812;g.helicopter.active=0;battleship.heliClock=0;
    battleshipSupport(&g,0,&in2);if(!g.helicopter.active)return 13;
    battleshipHitTarget(&g,9,1);battleshipHitTarget(&g,9,1);
    g.helicopter.active=0;battleship.heliClock=0;battleshipSupport(&g,0,&in2);if(g.helicopter.active)return 14;
    for(UBYTE i=0;i<BATTLESHIP_TARGETS;i++)while(battleship.hp[i])battleshipHitTarget(&g,i,1);
    if(battleship.phase!=2 || battleshipHealth())return 15;
    UBYTE fireballs=0,finalBlast=0;
    for(UWORD tick=0;tick<225;tick++) {
        battleshipUpdate(&g,&in,&in,&in2,0);
        if(g.ammoBlast.active && g.ammoBlast.timer==36)fireballs++;
        if(g.defence.bomberBlastTicks==24)finalBlast++;
        if(g.ammoBlast.active && g.defence.bomberBlastTicks)return 47;
        if(g.ammoBlast.active && (g.ammoBlast.timer>36 || (g.ammoBlast.worldX&7)))return 41;
    }
    if(fireballs!=5 || finalBlast!=1 || !battleship.finished || g.ammoBlast.active || g.defence.bomberBlastTicks)return 42;

    /* Normal terrain must never leak through the boss, including cache-off builds. */
    for(UWORD col=0;col<120;col++) {
        RenderColumn base;buildWorldTileColumn(col,&base);
        for(UBYTE row=0;row<15;row++) if(base.tile[row]) return 21;
        for(UBYTE row=6;row<8;row++) {
            UWORD tile=battleshipTileMap[row*120+col];
            if(col>0 && col<119) for(UBYTE y=0;y<8;y++)
                if(battleshipTiles[tile*40UL+y*5+4]!=255) return 22;
        }
    }
    UBYTE* bitmap=AllocMem(GAME_WORLD_BITMAP_BYTES,MEMF_PUBLIC|MEMF_CLEAR);
    if(!bitmap)return 16;
    initRingWorldBuffer(bitmap,0);
    ULONG before=0,after=0;
    for(ULONG i=0;i<GAME_WORLD_BITMAP_BYTES;i++)before=(before<<1|before>>31)^bitmap[i];
    for(UWORD col=0;col<120;col++)renderRingWorldColumn(bitmap,col);
    for(ULONG i=0;i<GAME_WORLD_BITMAP_BYTES;i++)after=(after<<1|after>>31)^bitmap[i];
    if(before!=after){FreeMem(bitmap,GAME_WORLD_BITMAP_BYTES);return 17;}
    /* Real pickup compositor: both sub-tile shifts and vertical fast path,
     * live overlays, collection/removal, and the duplicated ring margin. */
    battleship.hp[0]=2;battleship.hp[8]=5;
    initRingWorldBuffer(bitmap,0);before=0;
    for(ULONG i=0;i<GAME_WORLD_BITMAP_BYTES;i++)before=(before<<1|before>>31)^bitmap[i];
    for(UBYTE run=0;run<9;run++) {
        static const UWORD starts[3]={512,984,1440};
        LONG first=starts[run/3]/8;
        for(LONG c=first;c<first+128;c++){renderRingWorldColumn(bitmap,c);battleshipResident[(UWORD)c&127]=c;}
        before=0;
        for(ULONG i=0;i<GAME_WORLD_BITMAP_BYTES;i++)before=(before<<1|before>>31)^bitmap[i];
        powerupBobFootprintValid=0;powerupBackgroundCacheDirty=1;
        g.gameOver=0;g.powerup.active=1;g.powerup.type=POWERUP_ROCKETS;
        for(WORD y=24;y<120;y++) {
            g.powerup.y=y;g.powerup.worldX=starts[run/3]+(run%3)*3+(y-24)/8;
            updatePowerupBob(bitmap,&g);
        }
        g.powerup.active=0;updatePowerupBob(bitmap,&g);after=0;
        for(ULONG i=0;i<GAME_WORLD_BITMAP_BYTES;i++)after=(after<<1|after>>31)^bitmap[i];
        if(before!=after){FreeMem(bitmap,GAME_WORLD_BITMAP_BYTES);return 30;}
    }
    initRingWorldBuffer(bitmap,0);battleship.camera16=battleship.velocity16=0;battleship.phase=0;g.scrollX=0;
    for(UWORD tick=0;tick<2800;tick++) {
        battleshipMoveCamera(&g);serviceBattleshipStream(bitmap,&g);
        LONG first=g.scrollX/8;
        for(LONG c=first;c<first+42;c++)if(battleshipResident[(UWORD)c&127]!=c){FreeMem(bitmap,GAME_WORLD_BITMAP_BYTES);return 36;}
    }
    FreeMem(bitmap,GAME_WORLD_BITMAP_BYTES);
    UBYTE* hud=AllocMem(HUD_BITMAP_BYTES,MEMF_PUBLIC|MEMF_CLEAR);
    if(!hud)return 23;
    battleship.hudHealth=255;battleshipDrawHud(hud);
    ULONG hudBefore=0,hudAfter=0;
    for(ULONG i=0;i<76UL*SCREEN_PLANES*SCREEN_ROW_BYTES;i++)hudBefore=(hudBefore<<1|hudBefore>>31)^hud[i];
    drawPauseHudOverlay(hud,1);drawPauseHudOverlay(hud,0);
    for(ULONG i=0;i<76UL*SCREEN_PLANES*SCREEN_ROW_BYTES;i++)hudAfter=(hudAfter<<1|hudAfter>>31)^hud[i];
    FreeMem(hud,HUD_BITMAP_BYTES);if(hudBefore!=hudAfter)return 24;
    battleship.finished=1;battleshipBegin(&g);if(battleship.active)return 18;
    battleship.finished=0;g.gameMode=GAME_MODE_CLASSIC;battleshipBegin(&g);if(battleship.active)return 19;
    return 0;
}
#endif
