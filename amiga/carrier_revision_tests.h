#if HAR_HEADLESS_CLASSIC_CONTRACT_TEST
static UBYTE carrierTestPixel(const UBYTE* pixels,UWORD x,UWORD y) {
    x+=GAME_WORLD_BUFFER_MARGIN_PIXELS;
    UBYTE pen=0, bit=0x80>>(x&7);
    for(UBYTE p=0;p<4;p++) if(pixels[(ULONG)y*SCREEN_PLANES*GAME_WORLD_ROW_BYTES+p*GAME_WORLD_ROW_BYTES+(x>>3)]&bit) pen|=1<<p;
    return pen;
}
static UBYTE referenceCarrierSceneStatusMatches(void) {
    UBYTE* pixels=AllocMem(GAME_WORLD_BITMAP_BYTES,MEMF_PUBLIC|MEMF_CLEAR);
    if(!pixels) return 210;
    UBYTE* buffers[GAME_WORLD_BUFFER_COUNT]={pixels};
    static GameState g; initGameState(&g,12040,12040,1); g.gameMode=GAME_MODE_ENHANCED;
    UBYTE oldMode=currentWorldPresentationMode; currentWorldPresentationMode=GAME_MODE_ENHANCED;
    resetRocketShotPixelBobFootprints(); resetBombShotPixelBobFootprints();
    carrierSceneHull=255; carrierDefenceSinkPixels=0;
    for(UBYTE c=0;c<40;c++) renderRingWorldColumn(pixels,c);
    g.defence.phase=DEFENCE_WAVE; g.defence.hull=60; g.defence.cargo=40;
    carrierSyncSceneStatus(&g,buffers);
    UBYTE result=0;
    if(carrierTestPixel(pixels,80,129)!=GAME_COLOR_WHITE ||
       carrierTestPixel(pixels,80,131)!=GAME_COLOR_POWERUP_GREEN ||
       carrierTestPixel(pixels,150,131)!=GAME_COLOR_BLACK) result=211;
    renderRingWorldColumn(pixels,10); /* Full-column streaming must preserve the frame. */
    if(carrierTestPixel(pixels,80,129)!=GAME_COLOR_WHITE) result=215;
    /* Visual fixture from the actual compositor, not a separate mock-up. */
    BPTR f=Open((CONST_STRPTR)"DH1:carrier-status-preview.bpl",MODE_NEWFILE);
    if(f) {
        UBYTE header[6]={GAME_WORLD_BUFFER_WIDTH>>8,GAME_WORLD_BUFFER_WIDTH&255,GAME_WORLD_HEIGHT>>8,GAME_WORLD_HEIGHT&255,SCREEN_PLANES,GAME_WORLD_BUFFER_MARGIN_PIXELS};
        Write(f,header,6); Write(f,pixels,GAME_WORLD_BITMAP_BYTES); Close(f);
    }
    g.defence.hull=20; g.defence.cargo=20; carrierSyncSceneStatus(&g,buffers);
    if(carrierTestPixel(pixels,80,131)!=GAME_COLOR_RED || carrierTestPixel(pixels,100,131)!=GAME_COLOR_BLACK) result=212;
    ensureSeaWaveCandidates(&g);
    for(UBYTE n=0;n<seaWaveCandidateCount;n++) if(seaWaveCandidates[n].column>=7 && seaWaveCandidates[n].column<20 && seaWaveCandidates[n].y>=128 && seaWaveCandidates[n].y<136) result=213;
    g.defence.phase=0; carrierSyncSceneStatus(&g,buffers);
    if(carrierSceneHull!=255 || carrierTestPixel(pixels,80,129)==GAME_COLOR_WHITE) result=214;
    currentWorldPresentationMode=oldMode; FreeMem(pixels,GAME_WORLD_BITMAP_BYTES); return result;
}

/* Exercise real OCS DMA against the independent CPU tile renderer, including
 * shifter carry, every pose, the fifth plane, fallback and padded overlaps. */
static UBYTE referenceCarrierBlitterMatches(void) {
    const ULONG testBytes=42UL*SCREEN_PLANES*GAME_WORLD_ROW_BYTES;
    UBYTE* actual=AllocMem(testBytes,MEMF_CHIP);
    UBYTE* expected=AllocMem(testBytes,MEMF_PUBLIC);
    CarrierBlitMemory* memory=AllocMem(sizeof(CarrierBlitMemory),MEMF_CHIP|MEMF_CLEAR);
    UBYTE result=0;
    if(!actual || !expected || !memory) result=225;
    if(!result) {
        OwnBlitter(); WaitBlit();
        UWORD oldDma=custom->dmaconr;
        custom->dmacon=DMAF_SETCLR|DMAF_MASTER|DMAF_BLITTER;
        for(UBYTE pose=0;pose<11 && !result;pose++) for(UBYTE shift=0;shift<16 && !result;shift++) {
            UBYTE pattern=shift&1 ? 0xa5 : 0x5a;
            for(UBYTE pass=0;pass<2;pass++) {
                UBYTE* pixels=pass ? actual : expected;
                memset(pixels,pattern,testBytes);
                resetRocketShotPixelBobFootprints();
                carrierBlitMemory=pass ? memory : 0; carrierBlitPose=255;
                static GameState state;
                memset(&state,0,sizeof(state)); state.gameMode=GAME_MODE_ENHANCED;
                state.defence.phase=DEFENCE_WAVE; state.defence.jetType=2;
                state.enemyPlane.active=1; state.enemyPlane.direction=pose;
                state.enemyPlane.x=state.enemyPlane.worldX=shift<16 ? 80+shift : (shift==16 ? -9 : SCREEN_WIDTH-17);
                state.enemyPlane.y=24;
                if(pose>=5) {
                    state.defence.jetHits=1+(pose-5)/2; state.enemyPlane.direction=(pose-5)&1;
                } else if(pose>=2) {
                    state.defence.bomberBlastTicks=24-(pose-2)*8;
                    state.defence.bomberBlastX=state.enemyPlane.x; state.defence.bomberBlastY=24;
                }
                drawEnhancedEncounterBobs(pixels,0,&state);
                if(pass && shift<16 && !bomberBlitValid[0]) result=226;
                /* An overlay outside the silhouette but inside the saved
                 * 48-pixel rectangle must be retired before bomber motion. */
                state.siloMissile.active=1;
                state.siloMissile.x=state.siloMissile.worldX=state.enemyPlane.x+33;
                state.siloMissile.y=28;
                drawEnhancedEncounterBobs(pixels,0,&state);
                state.enemyPlane.x+=3; state.enemyPlane.worldX+=3;
                state.defence.bomberBlastX+=3;
                updateCarrierBomberBob(pixels,0,&state);
                retireEncounterTransientBobs(pixels,0);
                drawEnhancedEncounterBobs(pixels,0,&state);
            }
            if(!referenceBuffersEqual(actual,expected,testBytes)) result=227;
            ULONG changedByte=testBytes;
            if(bomberBlitValid[0]) {
                /* Persistent terrain/effect edit in padding must invalidate
                 * the saved rectangle even though it misses the silhouette. */
                WORD edge=bomberBlitByteX[0]*8-GAME_WORLD_BUFFER_MARGIN_PIXELS+47;
                changedByte=24UL*SCREEN_PLANES*GAME_WORLD_ROW_BYTES+bomberBlitByteX[0]+5;
                retireEncounterRegion(actual,0,edge,24,1,1);
                if(bomberBlitValid[0]) result=229;
                actual[changedByte]^=0xff;
            }
            retireEncounterBobs(actual,0);
            memset(expected,pattern,testBytes);
            if(changedByte<testBytes) expected[changedByte]^=0xff;
            if(!referenceBuffersEqual(actual,expected,testBytes)) result=228;
        }
        WaitBlit();
        custom->dmacon=(DMAF_MASTER|DMAF_BLITTER)&~oldDma;
        DisownBlitter();
    }
    carrierBlitMemory=0; carrierBlitPose=255;
    resetRocketShotPixelBobFootprints();
    if(memory) FreeMem(memory,sizeof(CarrierBlitMemory));
    if(expected) FreeMem(expected,testBytes);
    if(actual) FreeMem(actual,testBytes);
    return result;
}

/* Identical stick input must produce identical VTOL motion and art during
 * the raid and the final approach; entry and touchdown reset momentum. */
static UBYTE referenceLandingVtolMatches(void) {
    static GameState landing,raid;
    initGameState(&landing,12040,12040,1);
    landing.gameMode=GAME_MODE_ENHANCED;
    landing.takeoffState=TAKEOFF_STATE_AIRBORNE;
    landing.playerFrigateStatus=PLAYER_FRIGATE_STATUS_CLEAR;
    landing.landingState=LANDING_STATE_SLOWING;
    landing.scrollX=LANDING_HOVER_SCROLL_X;
    landing.speedLevel=landing.throttleRepeatTimer=0;
    landing.defence.vtolVX=landing.defence.vtolVY=384;
    landing.defence.vtolAX=24; landing.defence.vtolSubY=128;
    updateLandingApproach(&landing);
    if(landing.landingState!=LANDING_STATE_HOVER || landing.defence.vtolVX ||
        landing.defence.vtolVY || landing.defence.vtolAX || landing.defence.vtolSubY) return 231;
    landing.playerX=80; landing.playerY=40;
    raid=landing; raid.scrollX=0; raid.defence.phase=DEFENCE_WAVE;
    InputState in={0};
    static UWORD a[PLAYER_SPRITE_WORDS],aa[PLAYER_SPRITE_WORDS];
    static UWORD b[PLAYER_SPRITE_WORDS],ba[PLAYER_SPRITE_WORDS];
    for(UBYTE tick=0;tick<60;tick++) {
        memset(&in,0,sizeof(in));
        if(tick<12) in.right=in.up=1;
        else if(tick>=24 && tick<48) in.left=in.down=1;
        carrierMoveVtol(&raid,&in); carrierUpdateHeading(&raid.defence,&in);
        updateEnhancedLandingVtol(&landing,&in);
        if(landing.playerX!=raid.playerX || landing.playerY!=raid.playerY ||
            landing.defence.vtolVX!=raid.defence.vtolVX || landing.defence.vtolVY!=raid.defence.vtolVY ||
            landing.defence.vtolPose!=raid.defence.vtolPose) return 232;
        updatePlayerSprite(a,aa,&raid); updatePlayerSprite(b,ba,&landing);
        if(!referenceBuffersEqual((UBYTE*)a,(UBYTE*)b,sizeof(a)) ||
            !referenceBuffersEqual((UBYTE*)aa,(UBYTE*)ba,sizeof(aa))) return 233;
    }
    carrierParkedWingmanVisible=0;
    landing.playerX=wingmanLandingDeckLeftX(&landing)+16;
    landing.playerY=playerCarrierDeckY(&landing)-1;
    carrierResetVtol(&landing.defence); landing.defence.vtolVY=384;
    memset(&in,0,sizeof(in)); updateEnhancedLandingVtol(&landing,&in);
    if(!landing.defence.landed || landing.defence.vtolVY ||
        landing.playerY!=playerCarrierDeckY(&landing)) return 234;
    WORD parkedX=landing.playerX;
    for(UBYTE tick=0;tick<10;tick++) updateEnhancedLandingVtol(&landing,&in);
    if(landing.playerX!=parkedX || landing.playerY!=playerCarrierDeckY(&landing)) return 235;
    in.up=1; updateEnhancedLandingVtol(&landing,&in);
    if(landing.defence.landed || landing.playerY!=playerCarrierDeckY(&landing)-3) return 236;
    return 0;
}

static UBYTE referenceCarrierPickupTowerMatches(void) {
    static GameState g; static UBYTE before[960];
    UBYTE* bitmap=AllocMem(GAME_WORLD_BITMAP_BYTES,MEMF_PUBLIC|MEMF_CLEAR);
    if(!bitmap) return 241;
    initGameState(&g,12040,12040,1); g.gameMode=GAME_MODE_ENHANCED;
    currentWorldPresentationMode=GAME_MODE_ENHANCED;
    carrierDefenceSinkPixels=carrierParkedWingmanVisible=carrierMissileHeight=carrierSubHeight=0;
    carrierDefenceGunMask=0;
    resetRocketShotPixelBobFootprints(); resetBombShotPixelBobFootprints();
    powerupBobFootprintValid=0; powerupBackgroundCacheDirty=1;
    for(LONG col=13;col<=15;col++) renderRingWorldColumn(bitmap,col);
    UWORD n=0;
    for(WORD y=88;y<128;y++) for(UBYTE col=13;col<=15;col++) for(UBYTE plane=0;plane<4;plane++) {
        UWORD x=ringWorldTileXForColumn(col);
        for(UBYTE mirror=0;mirror<2;mirror++)
            before[n++]=bitmap[(ULONG)y*SCREEN_PLANES*GAME_WORLD_ROW_BYTES+plane*GAME_WORLD_ROW_BYTES+x+mirror*GAME_WORLD_SCROLL_PAGE_BYTES];
    }
    g.powerup.active=1; g.powerup.type=POWERUP_CARRIER_REPAIR; g.powerup.worldX=111;
    for(WORD y=88;y<=120;y++) { g.powerup.y=y; updatePowerupBob(bitmap,&g); }
    g.powerup.active=0; updatePowerupBob(bitmap,&g);
    UBYTE good=1; n=0;
    for(WORD y=88;y<128;y++) for(UBYTE col=13;col<=15;col++) for(UBYTE plane=0;plane<4;plane++) {
        UWORD x=ringWorldTileXForColumn(col);
        for(UBYTE mirror=0;mirror<2;mirror++)
            if(before[n++]!=bitmap[(ULONG)y*SCREEN_PLANES*GAME_WORLD_ROW_BYTES+plane*GAME_WORLD_ROW_BYTES+x+mirror*GAME_WORLD_SCROLL_PAGE_BYTES]) good=0;
    }
    FreeMem(bitmap,GAME_WORLD_BITMAP_BYTES); return good ? 0 : 242;
}

static UBYTE referenceCarrierDeckReadyMatches(void) {
    static GameState g;
    UBYTE* buffers[GAME_WORLD_BUFFER_COUNT]={0};
    for(UBYTE control=0;control<=WINGMAN_CONTROL_PLAYER2;control++) {
        for(UBYTE dead=0;dead<2;dead++) {
            initGameState(&g,12040,12040,1);
            g.gameMode=GAME_MODE_ENHANCED; g.wingmanControl=control;
            g.wingman.destroyed=dead; g.defence.phase=DEFENCE_SECURE;
            carrierWingmanLiftDepth=8; carrierParkedWingmanVisible=0;
            for(UBYTE t=0;t<48;t++) { g.defence.clock++; carrierUpdateDeckLift(&g,buffers); }
            UBYTE ready=control!=WINGMAN_CONTROL_OFF && !dead;
            if(carrierParkedWingmanVisible!=ready || carrierWingmanLiftDepth!=(ready ? 0 : 8)) return 237;
            g.defence.phase=DEFENCE_DEPART; carrierUpdateDeckLift(&g,buffers);
            if(carrierParkedWingmanVisible!=ready) return 238;
            g.defence.phase=DEFENCE_WAVE;
            for(UBYTE t=0;t<48;t++) { g.defence.clock++; carrierUpdateDeckLift(&g,buffers); }
            if(carrierParkedWingmanVisible || carrierWingmanLiftDepth!=8) return 239;
        }
    }
    /* The aft gun must retract before the aircraft rises into its space. */
    g.wingmanControl=WINGMAN_CONTROL_CPU; g.wingman.destroyed=0;
    g.defence.phase=DEFENCE_SECURE; g.defence.gunHeight[1]=8;
    for(UBYTE t=0;t<48;t++) { g.defence.clock++; carrierUpdateDeckLift(&g,buffers); }
    if(carrierWingmanLiftDepth!=8) return 240;
    carrierParkedWingmanVisible=0; carrierWingmanLiftDepth=0;
    return 0;
}

static UBYTE referenceCarrierRevisionMatches(void) {
    static GameState g;
    InputState in = {0};
    /* A parked Harrier keeps its side profile, including the first alarm. */
    memset(&g,0,sizeof(g)); g.defence.phase=DEFENCE_ALARM; g.defence.landed=1;
    g.defence.facing=MAVERICK_DIRECTION_RIGHT;
    carrierUpdateHeading(&g.defence,&in);
    if(g.defence.vtolPose!=1) return 217;
    g.defence.facing=MAVERICK_DIRECTION_LEFT;
    carrierUpdateHeading(&g.defence,&in);
    if(g.defence.vtolPose!=2) return 218;
    for(UBYTE side=0;side<2;side++) {
        g.defence.landed=1;
        g.defence.facing=side ? MAVERICK_DIRECTION_LEFT : MAVERICK_DIRECTION_RIGHT;
        carrierUpdateHeading(&g.defence,&in);
        g.defence.landed=0; in.up=1;
        for(UBYTE tick=0;tick<20;tick++) {
            carrierVtolAxis(&g.defence.vtolVY,&g.defence.vtolAY,&g.defence.vtolSubY,-1);
            carrierUpdateHeading(&g.defence,&in);
            if(g.defence.vtolPose!=(side ? 2 : 1)) return 230;
        }
    }
    memset(&in,0,sizeof(in));
    UBYTE* emptyBuffers[GAME_WORLD_BUFFER_COUNT]={0};
    for(UBYTE dead=0;dead<2;dead++) {
        g.wingman.destroyed=dead; carrierParkedWingmanVisible=1;
        carrierUpdateDeckLift(&g,emptyBuffers);
        if(carrierParkedWingmanVisible) return 219;
    }
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_SECURE; g.playerX = 83; g.playerY = 88;
    in.down = 1; carrierMoveVtol(&g,&in);
    if (g.playerY != 88 || g.defence.vtolVY<=0) return 130; /* Exact 17.09.2026.uss position. */
    for (UBYTE i=0;i<40 && !g.defence.landed;i++) carrierMoveVtol(&g,&in);
    if (!g.defence.landed || g.playerY != TAKEOFF_PLAYER_DECK_Y || !carrierDefenceOnDeck(83,TAKEOFF_PLAYER_DECK_Y)) return 131;
    g.playerX = 112; g.playerY = 88; g.defence.landed = 0;
    for(UBYTE t=0;t<24 && !g.crashTimer;t++) carrierMoveVtol(&g,&in);
    if (!g.crashTimer || !g.crashEndsGame || g.armour) return 132; /* Mast contact crashes, not clamps. */
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_WAVE; g.defence.quota = 0; g.defence.jetType = 1;
    g.enemyPlane.active = 1; g.enemyPlane.x = 280; g.enemyPlane.y = 40;
    g.defence.jetAge = 47; g.playerX = 80; g.playerY = 60; g.respawnSafeTimer = 20;
    carrierDefenceAircraft(&g);
    if (!g.enemyMissile.active || g.enemyMissile.type != ENEMY_MISSILE_CARRIER_TYPE) return 133;
    WORD separation = g.enemyPlane.x - g.enemyMissile.x;
    for (UBYTE i=0;i<8;i++) { carrierDefenceAircraft(&g); carrierDefenceBombs(&g); }
    if (!g.enemyMissile.active || g.enemyPlane.x - g.enemyMissile.x <= separation + 8) return 134;
    WORD dx = g.enemyMissile.dx, dy = g.enemyMissile.dy; g.playerY = 10;
    carrierDefenceBombs(&g);
    if (g.enemyMissile.dx != dx || g.enemyMissile.dy != dy) return 135;
    memset(g.defence.bombs,0,sizeof(g.defence.bombs));
    g.enemyPlane.x = 120; g.defence.jetAge = 27;
    carrierDefenceAircraft(&g);
    if (!carrierBombsActive(&g)) return 136; /* Fighter bombs too. */
    for (UBYTE side=0;side<2;side++) {
        initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
        g.defence.phase = DEFENCE_WAVE; g.defence.wave = 1; g.defence.quota = 3;
        g.defence.spawned = side; carrierDefenceAircraft(&g);
        if (!g.enemyPlane.active || g.enemyPlane.direction != side) return 137;
        WORD oldX = g.enemyPlane.x; carrierDefenceAircraft(&g);
        if (side ? g.enemyPlane.x <= oldX : g.enemyPlane.x >= oldX) return 138;
        g.enemyPlane.active = 0; g.defence.spawnDelay = 0;
        g.defence.wave = side ? 2 : 1; g.defence.spawned = side ? 1 : 2;
        g.missionNumber = side ? 2 : 1; carrierDefenceAircraft(&g);
        if (!g.helicopter.active || g.helicopter.direction != side) return 139;
        oldX = g.helicopter.x; carrierDefenceAircraft(&g);
        if (side ? g.helicopter.x <= oldX : g.helicopter.x >= oldX) return 140;
    }
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_WAVE; g.defence.jetType = 2;
    g.enemyPlane.active = 1; g.enemyPlane.x = 180; g.enemyPlane.y = 40;
    for (UBYTE hit=0; hit<3; hit++) {
        carrierDefenceKillJet(&g);
        if (!g.enemyPlane.active || g.hitsCount || g.defence.jetHits != hit + 1) return 141;
    }
    carrierDefenceKillJet(&g);
    if (g.enemyPlane.active || g.hitsCount != 1) return 142;
    g.helicopter.active = 1; g.helicopter.x = g.helicopter.worldX = 180; g.helicopter.y = 40;
    carrierDefenceKillHeli(&g); carrierDefenceKillHeli(&g);
    if (g.helicopter.type != 2 || !g.powerup.active) return 143;
    HelicopterBullet* bullet = &g.helicopterBullets[0];
    memset(bullet,0,sizeof(*bullet)); bullet->active = 1; bullet->worldX = 184; bullet->y = 43;
    carrierAdvanceBullet(&g,bullet);
    if (g.helicopter.active || bullet->active || g.defence.repairDropPending || g.hitsCount != 2) return 144;
    g.helicopter.active = 1; g.helicopter.type = 2;
    memset(&g.rocketShot,0,sizeof(g.rocketShot));
    g.rocketHeightLock=0; /* Fixed trajectory fixture intersects the falling helicopter. */
    g.rocketShot.active = 1; g.rocketShot.x = 182; g.rocketShot.y = 41;
    carrierDefenceShot(&g,&g.rocketShot);
    if (g.helicopter.active || g.rocketShot.active || g.defence.repairDropPending) return 145;
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_WAVE; g.defence.wave = 1; g.defence.quota = 3;
    carrierDefenceAircraft(&g);
    if (g.defence.jetType == 2) return 146;
    g.enemyPlane.active = 0; g.defence.wave = g.defence.waves = 2; g.defence.spawned = 2; g.defence.spawnDelay = 0;
    carrierDefenceAircraft(&g);
    if (g.defence.jetType != 2 || g.enemyPlane.y > 28) return 147;
    /* High bomber reverses at both edges without healing or ending its wave. */
    g.defence.quota = g.defence.spawned; g.defence.jetHits = 2;
    WORD bomberY = g.enemyPlane.y;
    for (UBYTE side=0; side<2; side++) {
        g.enemyPlane.direction = side; g.enemyPlane.x = side ? 320 : -32; g.defence.jetTurnTicks=0;
        carrierDefenceAircraft(&g);
        if (!g.enemyPlane.active || g.enemyPlane.direction == side ||
            g.enemyPlane.y != bomberY || g.defence.jetHits != 2) return 165;
        if(g.defence.jetTurnTicks!=125) return 237;
        WORD turnX=g.enemyPlane.x;
        for(UBYTE tick=0;tick<125;tick++) {
            carrierDefenceAircraft(&g);
            if(g.enemyPlane.x!=turnX || g.defence.jetHits!=2) return 238;
        }
        memset(g.defence.bombs,0,sizeof(g.defence.bombs));
        g.enemyPlane.x = 110; g.defence.jetAge = 27;
        carrierDefenceAircraft(&g);
        if (!carrierBombsActive(&g) || !g.defence.bombs[0].type) return 166;
    }
    UBYTE* pixels = AllocMem(GAME_WORLD_BITMAP_BYTES, MEMF_PUBLIC | MEMF_CLEAR);
    if (!pixels) return 148;
    UBYTE mode = currentWorldPresentationMode; currentWorldPresentationMode = GAME_MODE_ENHANCED;
    carrierDefenceSinkPixels = carrierParkedWingmanVisible = 0;
    carrierMissileHeight=8; carrierMissilePose=1;
    drawDirectColumnRangeObjectRow(pixels,0,14,12);
    UBYTE tip=pixels[96UL*SCREEN_PLANES*GAME_WORLD_ROW_BYTES+GAME_WORLD_ROW_BYTES]&0x18;
    carrierMissileHeight=0;
    FreeMem(pixels,GAME_WORLD_BITMAP_BYTES); currentWorldPresentationMode = mode;
    if (!tip) return 149;
    memset(g.defence.bombs,0,sizeof(g.defence.bombs));
    g.enemyMissile.active = g.helicopter.active = 0;
    g.enemyPlane.active = 1; g.enemyPlane.x = 100; g.enemyPlane.y = 40;
    g.enemyPlane.direction = 1; carrierChooseAim(&g);
    if (g.defence.aimX <= 104) return 150;
    g.enemyPlane.direction = 0; carrierChooseAim(&g);
    if (g.defence.aimX >= 104) return 151;
    g.defence.jetType = 2; g.defence.jetHits = 2; g.defence.jetBulletHits = 2;
    carrierDefenceKillJet(&g);
    if (!g.enemyPlane.active || g.defence.jetBulletHits != 2) return 152;
    for (UBYTE i=0;i<2;i++) {
        memset(bullet,0,sizeof(*bullet)); bullet->active = 1;
        bullet->worldX = g.enemyPlane.x + 4; bullet->y = g.enemyPlane.y + 3;
        carrierAdvanceBullet(&g,bullet);
    }
    if (g.enemyPlane.active) return 153;
    /* Both muzzles track the same CPU/P2 aim independently, at full height. */
    UBYTE* noBuffers[2] = {0,0};
    g.defence.phase = DEFENCE_WAVE;
    g.defence.gunHeight[0] = g.defence.gunHeight[1] = 8;
    g.defence.aimX = 112; g.defence.aimY = 64;
    carrierSyncGuns(&g,noBuffers);
    if (carrierDefenceGunPose[0] != 2 || carrierDefenceGunPose[1] != 0) return 154;
    g.defence.aimX = 76; carrierSyncGuns(&g,noBuffers);
    if (carrierDefenceGunPose[0] != 1 || carrierDefenceGunHeight[0] != 8) return 155;
    g.defence.aimX = 148; carrierSyncGuns(&g,noBuffers);
    if (carrierDefenceGunPose[1] != 1 || carrierDefenceGunHeight[1] != 8) return 156;
    g.defence.aimX = 8; carrierSyncGuns(&g,noBuffers);
    if (carrierDefenceGunPose[0] || carrierDefenceGunPose[1]) return 157;
    g.defence.aimX = 308; carrierSyncGuns(&g,noBuffers);
    if (carrierDefenceGunPose[0] != 2 || carrierDefenceGunPose[1] != 2) return 158;
    if (carrierGunAimPose(21,40,1) != 1 || carrierGunAimPose(21,40,2) != 2 ||
        carrierGunAimPose(16,40,2) != 1) return 159;
    /* Force software fallback: three overlapping missiles save layered backgrounds.
     * Erasing a lower layer must first retire every layer saved above it. */
    UBYTE* overlap = AllocMem(2UL * GAME_WORLD_BITMAP_BYTES, MEMF_PUBLIC);
    if (!overlap) return 160;
    UBYTE* clean = overlap + GAME_WORLD_BITMAP_BYTES;
    memset(clean, 0x55, GAME_WORLD_BITMAP_BYTES);
    UBYTE missileClean = 1;
    for (UBYTE shift = 0; shift < 8; shift++) {
        memcpy(overlap, clean, GAME_WORLD_BITMAP_BYTES);
        resetRocketShotPixelBobFootprints();
        WeaponState shot; memset(&shot,0,sizeof(shot));
        shot.active = 1; shot.type = ROCKET_SHOT_MAVERICK_GUIDED;
        shot.direction = MAVERICK_DIRECTION_RIGHT; shot.y = 40;
        shot.x = shot.worldX = 80 + shift;
        drawRocketPixelBob(overlap,0,&shot,rocketShotFootprints,0);
        shot.x++; shot.worldX++;
        drawRocketPixelBob(overlap,0,&shot,wingmanRocketFootprints,0);
        shot.x++; shot.worldX++;
        drawRocketPixelBob(overlap,0,&shot,enemyMissileFootprints,0);
        eraseRocketPixelBobFootprint(overlap,0,rocketShotFootprints);
        eraseRocketPixelBobFootprint(overlap,0,wingmanRocketFootprints);
        eraseRocketPixelBobFootprint(overlap,0,enemyMissileFootprints);
        missileClean &= referenceBuffersEqual(overlap,clean,GAME_WORLD_BITMAP_BYTES);
    }
    /* Main loop erases falling bombs early and retained missiles late.
     * Shared saved bytes must unwind in reverse draw order, including
     * adjacent silhouettes sharing a byte rather than touching pixels. */
    for(UBYTE shift=0;shift<8;shift++) {
        memcpy(overlap,clean,GAME_WORLD_BITMAP_BYTES);
        resetRocketShotPixelBobFootprints(); resetBombShotPixelBobFootprints();
        WeaponState bomb={0}, shot={0};
        bomb.active=1; bomb.x=bomb.worldX=180+shift; bomb.y=56;
        drawBombPixelBob(overlap,0,&bomb,bombShotFootprints,0);
        bomb.x++; bomb.worldX++;
        drawBombPixelBob(overlap,0,&bomb,wingmanBombFootprints,0);
        shot.active=1; shot.x=shot.worldX=184+shift; shot.y=52;
        shot.type=ROCKET_SHOT_MAVERICK_GUIDED; shot.direction=MAVERICK_DIRECTION_UP;
        drawRocketPixelBob(overlap,0,&shot,wingmanRocketFootprints,0);
        eraseBombPixelBobFootprint(overlap,0,bombShotFootprints);
        eraseBombPixelBobFootprint(overlap,0,wingmanBombFootprints);
        eraseRocketPixelBobFootprint(overlap,0,wingmanRocketFootprints);
        if(!referenceBuffersEqual(overlap,clean,GAME_WORLD_BITMAP_BYTES)) missileClean=0;
    }
    FreeMem(overlap,2UL * GAME_WORLD_BITMAP_BYTES);
    if (!missileClean) return 161;
    /* Ten seconds of carrier hover consumes ~16% at skill 1, independent
     * of rendering. No spawning in SECURE: this exercises the real update. */
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.levelDifficulty = 1; g.defence.phase = DEFENCE_SECURE;
    g.playerX = 240; g.playerY = 40; g.defence.landed = 0;
    resetPlayerFuel(&g);
    InputState idle; memset(&idle,0,sizeof(idle));
    Player2InputState idle2; memset(&idle2,0,sizeof(idle2));
    for (UWORD tick = 0; tick < 500; tick++) updateCarrierDefence(&g,&idle,&idle,&idle2,noBuffers);
    if (g.fuel < 840 || g.fuel > 850 || !g.lowSpeedLanding) return 162;
    /* Touch down with the last quantum due to expire: no final airborne burn. */
    g.playerX = 81; g.playerY = 104; g.fuelGaugeLevel = g.fuelSubCounter = 1;
    g.fuelClockAccumulator = playerFuelClockLimit(&g) - 1; g.fuel = cpcFuelHudValue(&g);
    InputState down = idle; down.down = 1;
    updateCarrierDefence(&g,&down,&idle,&idle2,noBuffers);
    if (!g.defence.landed || !g.fuel || g.aircraftFailureState) return 163;
    /* Refill through the same timed deck path used during an active raid. */
    g.defence.phase = DEFENCE_WAVE; g.defence.spawned = 0; g.defence.quota = 0;
    for (UWORD tick = 0; tick < 448; tick++) updateCarrierDefence(&g,&idle,&idle,&idle2,noBuffers);
    if (g.fuel != 999 || !g.defence.landed || g.aircraftFailureState) return 164;
    /* Every sinking depth preserves all underwater bytes exactly. */
    UBYTE* sea = AllocMem(GAME_WORLD_BITMAP_BYTES,MEMF_PUBLIC);
    if (!sea) return 167;
    UBYTE seaClean = 1;
    carrierParkedWingmanVisible = 0; carrierDefenceGunMask = 0;
    for (UBYTE depth=1; depth<=SEA_SURFACE_Y-96; depth++) {
        memset(sea,0x55,GAME_WORLD_BITMAP_BYTES); carrierDefenceSinkPixels = depth;
        for (UBYTE col=0; col<HAR_CARRIER_TILES_WIDE; col++)
            drawPromotedCpcCarrierRangeAt(sea,col,col);
        ULONG first = (ULONG)SEA_SURFACE_Y * SCREEN_PLANES * GAME_WORLD_ROW_BYTES;
        for (ULONG at=first; at<GAME_WORLD_BITMAP_BYTES; at++)
            if (sea[at] != 0x55) seaClean = 0;
    }
    /* Fully submerged means no remaining ship pixels anywhere. */
    for (ULONG at=0; at<GAME_WORLD_BITMAP_BYTES; at++)
        if (sea[at] != 0x55) seaClean = 0;
    FreeMem(sea,GAME_WORLD_BITMAP_BYTES); carrierDefenceSinkPixels = 0;
    if (!seaClean) return 168;
    memset(&g.helicopterSmoke,0,sizeof(g.helicopterSmoke));
    g.scrollX = 0; g.helicopterSmoke.active = 1;
    g.helicopterSmoke.x = g.helicopterSmoke.worldX = 100; g.helicopterSmoke.y = 60;
    for (UBYTE tick=0; tick<32; tick++) updateEncounterSmoke(&g);
    if (!g.helicopterSmoke.active || g.helicopterSmoke.y != 52 || g.helicopterSmoke.x != 104) return 169;
    for (UBYTE tick=0; tick<8; tick++) updateEncounterSmoke(&g);
    if (g.helicopterSmoke.active) return 170;
    /* Deck elevator takes 48 logical ticks in either direction, never a pop. */
    g.wingmanControl = WINGMAN_CONTROL_CPU;
    g.wingman.destroyed = 0; g.defence.phase = DEFENCE_ALARM;
    carrierWingmanLiftDepth = 0; carrierParkedWingmanVisible = 1;
    for (UBYTE tick=1; tick<=48; tick++) {
        g.defence.clock = tick; carrierUpdateDeckLift(&g,noBuffers);
        if (carrierWingmanLiftDepth != tick / 6) return 171;
    }
    g.defence.phase = DEFENCE_SECURE; g.defence.gunHeight[1] = 0;
    for (UBYTE tick=1; tick<=48; tick++) {
        g.defence.clock = tick; carrierUpdateDeckLift(&g,noBuffers);
        if (carrierWingmanLiftDepth != 8 - tick / 6) return 172;
    }
    UBYTE* lift = AllocMem(2UL * GAME_WORLD_BITMAP_BYTES,MEMF_PUBLIC | MEMF_CLEAR);
    if (!lift) return 173;
    UBYTE* bare = lift + GAME_WORLD_BITMAP_BYTES;
    carrierParkedWingmanVisible = carrierDefenceGunMask = carrierDefenceSinkPixels = 0;
    for (UBYTE col=0; col<12; col++) drawPromotedCpcCarrierRangeAt(bare,col,col);
    UBYTE liftGood = 1; UWORD previousPixels = 128;
    for (UBYTE depth=0; depth<=8; depth++) {
        memset(lift,0,GAME_WORLD_BITMAP_BYTES);
        carrierParkedWingmanVisible = 1; carrierWingmanLiftDepth = depth;
        for (UBYTE col=0; col<12; col++) drawPromotedCpcCarrierRangeAt(lift,col,col);
        UWORD pixels = 0;
        for (UWORD y=0; y<GAME_WORLD_HEIGHT; y++) for (UBYTE col=0; col<12; col++) {
            UBYTE diff = 0;
            for (UBYTE plane=0; plane<4; plane++) {
                ULONG at = (ULONG)y * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + plane * GAME_WORLD_ROW_BYTES + col;
                diff |= lift[at] ^ bare[at];
            }
            if ((y < 104 || y >= 112) && diff) liftGood = 0;
            for (UBYTE bit=0; bit<8; bit++) pixels += (diff >> bit) & 1;
        }
        if (pixels > previousPixels || (!depth && !pixels) || (depth == 8 && pixels)) liftGood = 0;
        previousPixels = pixels;
    }
    FreeMem(lift,2UL * GAME_WORLD_BITMAP_BYTES);
    carrierParkedWingmanVisible = carrierWingmanLiftDepth = 0;
    if (!liftGood) return 174;
    g.gameMode = GAME_MODE_ENHANCED;
    if (playerCarrierDeckY(&g) != TAKEOFF_PLAYER_DECK_Y || !carrierDefenceOnDeck(81,104)) return 175;
    g.gameMode = GAME_MODE_CLASSIC;
    if (playerCarrierDeckY(&g) != 105) return 176;
    /* Holding Up during the last lift step must depart without another press. */
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_DEPART; g.defence.landed = 1; g.defence.clock = 5;
    g.playerX = 81; g.playerY = TAKEOFF_PLAYER_DECK_Y; carrierWingmanLiftDepth = 1;
    InputState heldUp = {0}; heldUp.up = 1;
    updateCarrierDefence(&g,&heldUp,&heldUp,&idle2,noBuffers);
    if (g.defence.phase || carrierWingmanLiftDepth || g.takeoffState != TAKEOFF_STATE_LIFTING) return 177;
    /* Real Land Now update must crash before service/fire can proceed. */
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_SECURE; g.playerX = 112; g.playerY = 88;
    g.respawnSafeTimer = 0; g.defence.landed = 0;
    InputState hitTower = {0}; hitTower.down = 1;
    for(UBYTE t=0;t<24 && !g.crashTimer;t++) updateCarrierDefence(&g,&hitTower,&idle,&idle2,noBuffers);
    if (!g.crashTimer || g.armour || g.defence.landed || !g.crashEndsGame) return 178;
    /* Normal terrain-return landing still reports the tower as an obstruction. */
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.takeoffState = TAKEOFF_STATE_AIRBORNE; g.scrollX = 0;
    g.playerX = 112; g.playerY = 96; g.respawnSafeTimer = 0;
    LONG towerColumn; WORD towerRow;
    if (!playerHitsNativeCarrierObstruction(&g,&towerColumn,&towerRow)) return 179;
    /* Expiring a hit flash underneath a retained missile/heli must not
     * resurrect that flash when their saved backgrounds are restored. */
    UBYTE* flash = AllocMem(2UL * GAME_WORLD_BITMAP_BYTES,MEMF_PUBLIC | MEMF_CLEAR);
    if (!flash) return 180;
    UBYTE* flashClean = flash + GAME_WORLD_BITMAP_BYTES;
    bombImpactBobFootprintValid = 0; resetRocketShotPixelBobFootprints();
    bobCompositorErase(flashClean,10,5,3);
    UBYTE flashGone = 1;
    for (UBYTE shift=0; shift<8; shift++) {
        memcpy(flash,flashClean,GAME_WORLD_BITMAP_BYTES);
        resetRocketShotPixelBobFootprints(); bombImpactBobFootprintValid = 0;
        drawBombImpactBobAt(flash,10,5,BOMB_IMPACT_BOB_KIND_IMPACT_LARGE);
        WeaponState retained; memset(&retained,0,sizeof(retained));
        retained.active = 1; retained.type = ROCKET_SHOT_MAVERICK_GUIDED;
        retained.direction = MAVERICK_DIRECTION_RIGHT;
        retained.x = retained.worldX = 80 + shift; retained.y = 40;
        drawRocketPixelBob(flash,0,&retained,wingmanRocketFootprints,0);
        g.helicopter = retained; g.helicopter.direction = 1; g.helicopterHits = 0;
        drawHelicopterBob(flash,0,&g);
        g.impact.active = 0; updateBombImpactBob(flash,&g);
        retireEncounterBobs(flash,0);
        eraseRocketPixelBobFootprint(flash,0,wingmanRocketFootprints);
        flashGone &= referenceBuffersEqual(flash,flashClean,GAME_WORLD_BITMAP_BYTES);
    }
    FreeMem(flash,2UL * GAME_WORLD_BITMAP_BYTES);
    if (!flashGone) return 181;
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.takeoffState = TAKEOFF_STATE_AIRBORNE; g.respawnSafeTimer = 0;
    g.aircraftFailureState = AIRCRAFT_FAILURE_DESCENT;
    InputState trigger={0}, released={0}; trigger.fire=1;
    if(!fireButtonEjectRequested(&g,&trigger,&released)) return 182;
    if(fireButtonEjectRequested(&g,&trigger,&trigger)) return 183;
    trigger.fire=0; trigger.bomb=1;
    if(fireButtonEjectRequested(&g,&trigger,&released)) return 184;
    g.aircraftFailureState=AIRCRAFT_FAILURE_NONE; trigger.fire=1;
    if(fireButtonEjectRequested(&g,&trigger,&released)) return 185;
    /* Lamp may light from zero fuel one step before failure descent starts. */
    g.defence.phase=DEFENCE_WAVE; g.fuel=0; g.playerX=220; g.playerY=40;
    if(!fireButtonEjectRequested(&g,&trigger,&released)) return 217;
    trigger.eject=1;
    updateCarrierDefence(&g,&trigger,&released,&idle2,noBuffers);
    if(!g.ejectState) return 218;
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_SECURE; g.ejectState = 2;
    g.ejectX = 180; g.ejectY = 60; g.ejectTimer = 0;
    InputState steer = {0}; steer.left = 1;
    for (UBYTE tick=0; tick<30; tick++) updateCarrierDefence(&g,&steer,&steer,&idle2,noBuffers);
    if (g.ejectState != 2 || g.ejectX != 150 || g.ejectY != 70) return 186;
    /* Launcher raises even after both AA guns are destroyed; all three aim
     * sectors and the launch interlock share the actual deployed geometry. */
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_WAVE; g.defence.hull = 100;
    g.defence.gunHealth[0] = g.defence.gunHealth[1] = 0;
    g.playerX = 240; g.playerY = 20;
    g.defence.aimX = 160; g.defence.aimY = 40;
    carrierLaunchMissile(&g); if (g.wingman.rocket.active) return 203;
    for (UBYTE tick=0; tick<16; tick++) { g.defence.clock++; carrierUpdateGuns(&g); }
    if (g.defence.missileHeight != 8) return 204;
    const WORD launcherAim[3] = {8,108,308};
    for (UBYTE pose=0; pose<3; pose++) {
        g.defence.aimX=launcherAim[pose]; carrierSyncGuns(&g,noBuffers);
        if (carrierMissileHeight != 8 || carrierMissilePose != pose) return 205;
    }
    carrierDefenceImpact(&g,108,8);
    if (g.defence.missileHeight != 8 || g.defence.hull != 92) return 206;
    carrierLaunchMissile(&g); if (!g.wingman.rocket.active) return 207;
    g.defence.landed=1;
    for (UBYTE tick=0; tick<8; tick++) carrierUpdateGuns(&g);
    carrierSyncGuns(&g,noBuffers);
    if (g.defence.missileHeight || carrierMissileHeight) return 208;
    /* 32x16 bomber, both directions and every pixel shift, including ring
     * boundaries. Reverse saved-background restore must recover every byte. */
    UBYTE* bomberPixels=AllocMem(2UL*GAME_WORLD_BITMAP_BYTES,MEMF_PUBLIC);
    if (!bomberPixels) return 209;
    UBYTE* bomberClean=bomberPixels+GAME_WORLD_BITMAP_BYTES;
    memset(bomberClean,0x55,GAME_WORLD_BITMAP_BYTES);
    UBYTE bomberRestored=1;
    initGameState(&g,12040,12040,1); g.gameMode=GAME_MODE_ENHANCED;
    g.defence.phase=DEFENCE_WAVE; g.defence.jetType=2; g.enemyPlane.active=1;
    g.enemyPlane.y=24;
    for (UBYTE direction=0; direction<2; direction++) for (UBYTE seam=0; seam<2; seam++) for (UBYTE shift=0; shift<8; shift++) {
        memcpy(bomberPixels,bomberClean,GAME_WORLD_BITMAP_BYTES);
        resetRocketShotPixelBobFootprints();
        g.enemyPlane.direction=direction; g.enemyPlane.x=80+shift;
        g.enemyPlane.worldX=seam ? GAME_WORLD_SCROLL_PAGE_BYTES*8-24+shift : 80+shift;
        drawEnhancedEncounterBobs(bomberPixels,0,&g);
        if (referenceBuffersEqual(bomberPixels,bomberClean,GAME_WORLD_BITMAP_BYTES) || !encounterFootprints[13][0].valid) bomberRestored=0;
        UWORD redraws=bomberDrawCount;
        for(UBYTE repeat=0;repeat<8;repeat++) {
            retireEncounterTransientBobs(bomberPixels,0);
            drawEnhancedEncounterBobs(bomberPixels,0,&g);
        }
        if(bomberDrawCount!=redraws) bomberRestored=0;
        g.enemyPlane.x++; g.enemyPlane.worldX++;
        retireEncounterTransientBobs(bomberPixels,0);
        updateCarrierBomberBob(bomberPixels,0,&g);
        if(bomberDrawCount!=redraws+1) bomberRestored=0;
        retireEncounterBobs(bomberPixels,0);
        if (!referenceBuffersEqual(bomberPixels,bomberClean,GAME_WORLD_BITMAP_BYTES)) bomberRestored=0;
    }
    /* Moving an upper BOB over a stationary bomber must leave its cached
     * image intact; moving the bomber must unwind intersecting snapshots. */
    resetRocketShotPixelBobFootprints();
    memcpy(bomberPixels,bomberClean,GAME_WORLD_BITMAP_BYTES);
    g.enemyPlane.active=1; g.enemyPlane.x=g.enemyPlane.worldX=80; g.enemyPlane.y=24;
    g.siloMissile.active=1; g.siloMissile.x=g.siloMissile.worldX=84; g.siloMissile.y=28;
    drawEnhancedEncounterBobs(bomberPixels,0,&g);
    UWORD retainedDraws=bomberDrawCount;
    retireEncounterTransientBobs(bomberPixels,0);
    drawEnhancedEncounterBobs(bomberPixels,0,&g);
    if(bomberDrawCount!=retainedDraws) bomberRestored=0;
    g.enemyPlane.x++; g.enemyPlane.worldX++;
    updateCarrierBomberBob(bomberPixels,0,&g);
    drawEnhancedEncounterBobs(bomberPixels,0,&g);
    if(bomberDrawCount!=retainedDraws+1) bomberRestored=0;
    retireEncounterBobs(bomberPixels,0);
    g.siloMissile.active=0;
    if(!referenceBuffersEqual(bomberPixels,bomberClean,GAME_WORLD_BITMAP_BYTES)) bomberRestored=0;
    /* Blast uses the same footprint, with only three visual updates. */
    g.enemyPlane.active=0; g.defence.bomberBlastX=80; g.defence.bomberBlastY=24;
    UWORD beforeBlast=bomberDrawCount;
    for(UBYTE t=24;t;t--) {
        g.defence.bomberBlastTicks=t;
        updateCarrierBomberBob(bomberPixels,0,&g);
    }
    if(bomberDrawCount!=beforeBlast+3) bomberRestored=0;
    g.defence.bomberBlastTicks=0; updateCarrierBomberBob(bomberPixels,0,&g);
    if(!referenceBuffersEqual(bomberPixels,bomberClean,GAME_WORLD_BITMAP_BYTES)) bomberRestored=0;
    FreeMem(bomberPixels,2UL*GAME_WORLD_BITMAP_BYTES);
    if (!bomberRestored) return 210;
    g.enemyPlane.active=1; g.enemyPlane.x=g.enemyPlane.worldX=100; g.enemyPlane.y=40;
    g.bombShot.active=1; g.bombShot.x=128; g.bombShot.y=52;
    if (!carrierBombHitsAircraft(&g,&g.bombShot) || g.enemyPlane.active) return 211;

    /* Carrier-only acceleration, explicit braking and neutral momentum retention. */
    initGameState(&g,12040,12040,1); g.gameMode=GAME_MODE_ENHANCED;
    g.defence.phase=DEFENCE_WAVE; g.playerX=200; g.playerY=40;
    InputState analog={0}; analog.right=analog.up=1;
    carrierMoveVtol(&g,&analog);
    if(g.playerX!=200 || g.playerY!=40 || g.defence.vtolVX<=0 || g.defence.vtolVY>=0) return 212;
    for(UBYTE t=0;t<24;t++) carrierMoveVtol(&g,&analog);
    if(g.playerX<=200 || g.playerY>=40) return 213;
    WORD speed=g.defence.vtolVX;
    analog.right=0; analog.left=1; analog.up=0;
    for(UBYTE t=0;t<18;t++) carrierMoveVtol(&g,&analog);
    if(g.defence.vtolVX>=speed) return 214;
    carrierResetVtol(&g.defence);
    g.playerX=120; g.playerY=48;
    g.defence.vtolVX=128; g.defence.vtolVY=64;
    g.defence.vtolAX=18; g.defence.vtolAY=-12;
    memset(&analog,0,sizeof(analog));
    for(UBYTE t=0;t<20;t++) carrierMoveVtol(&g,&analog);
    if(g.defence.vtolVX!=128 || g.defence.vtolVY!=64 || g.defence.vtolAX || g.defence.vtolAY ||
        g.playerX!=130 || g.playerY!=53) return 215;
    carrierUpdateHeading(&g.defence,&analog);
    if(!g.defence.vtolPose) return 216; /* Drift must not snap back to front/hover. */
    analog.left=analog.up=1;
    carrierMoveVtol(&g,&analog);
    if(g.defence.vtolVX>=128 || g.defence.vtolVX<=0 || g.defence.vtolVY>=64 || g.defence.vtolVY<=0) return 219;
    for(UBYTE t=0;t<16;t++) carrierMoveVtol(&g,&analog);
    if(g.defence.vtolVX>=0 || g.defence.vtolVY>=0) return 220;
    carrierResetVtol(&g.defence); memset(&analog,0,sizeof(analog));
    WORD stoppedX=g.playerX,stoppedY=g.playerY;
    for(UBYTE t=0;t<20;t++) carrierMoveVtol(&g,&analog);
    if(g.playerX!=stoppedX || g.playerY!=stoppedY) return 221;

    /* Enhanced ground recoil: actual terrain/target cells, fixed damage,
     * single contact, decaying lift, fatal exhaustion and Classic exclusion. */
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.takeoffState = TAKEOFF_STATE_AIRBORNE;
    LONG bounceLand = -1, bounceTarget = -1; WORD landRow = 0, targetRow = 0;
    for (LONG col=CPC_LAND_PROCEDURAL_WORLD_START;
         col<CPC_LAND_PROCEDURAL_WORLD_START+cpcLandProceduralLength && (bounceLand<0 || bounceTarget<0); col++) {
        for (WORD row=0; row<24; row++) {
            ObjectCell cell;
            if (!aircraftObjectCell(col,row,&cell)) continue;
            if (cell.id == HAR_OBJ_LAND && bounceLand<0) { bounceLand=col; landRow=row; }
            if (cell.id == HAR_OBJ_GROUND_TARGET && bounceTarget<0) { bounceTarget=col; targetRow=row; }
        }
    }
    if (bounceLand<0 || bounceTarget<0) return 194;
    g.playerX = 80; g.scrollX = bounceLand*8-80; g.playerY = landRow*8-4;
    g.gameMode = GAME_MODE_CLASSIC;
    if (handleEnhancedGroundBounce(&g,0,bounceLand,landRow) || g.armour != 100) return 195;
    g.gameMode = GAME_MODE_ENHANCED;
    if (!handleEnhancedGroundBounce(&g,0,bounceLand,landRow) || g.armour != 50 || g.groundBounceTicks != 8 || g.crashTimer) return 196;
    if (!handleEnhancedGroundBounce(&g,0,bounceLand,landRow) || g.armour != 50) return 197;
    if (!g.helicopterSmoke.active || g.helicopterSmoke.worldX != bounceLand*8 ||
        g.helicopterSmoke.timer || playerGroundShakeOffset(&g) != -2) return 201;
    WORD shakeX = g.playerX, smokeY = g.helicopterSmoke.y;
    for (UBYTE tick=0; tick<40; tick++) updateEncounterSmoke(&g);
    if (g.helicopterSmoke.active || g.helicopterSmoke.y >= smokeY || g.playerX != shakeX) return 202;

    WORD bounceStartY = g.playerY;
    for (UBYTE tick=0; tick<8; tick++) updatePlayerGroundBounce(&g);
    WORD bounceEndY = bounceStartY-16; if (bounceEndY < PLAYER_MIN_Y) bounceEndY=PLAYER_MIN_Y;
    if (g.playerY != bounceEndY || g.groundBounceTicks || playerGroundShakeOffset(&g)) return 198;
    if (!handleEnhancedGroundBounce(&g,0,bounceLand,landRow) || g.armour || !g.aircraftFailureState) return 199;
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.takeoffState = TAKEOFF_STATE_AIRBORNE; g.playerX=80;
    g.scrollX=bounceTarget*8-80; g.playerY=targetRow*8-4;
    if (!handleEnhancedGroundBounce(&g,0,bounceTarget,targetRow) || g.armour != 60 ||
        !isTargetDestroyedAtColumn(bounceTarget) || !g.impact.active || g.hitsCount != 1) return 200;

    /* Saved 17.09.2026II.uss has fire=1 in both input snapshots.
     * Holding fire across multiple missile lifetimes must spend only one. */
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_WAVE; g.defence.quota = 0;
    g.playerX = 200; g.playerY = 40; g.rockets = 10;
    InputState firing = {0}; firing.fire = 1;
    updateCarrierDefence(&g,&firing,&idle,&idle2,noBuffers);
    if (g.rockets != 9 || !g.rocketShot.active) return 187;
    for (UWORD tick=0; tick<300; tick++) updateCarrierDefence(&g,&firing,&firing,&idle2,noBuffers);
    if (g.rockets != 9 || g.rocketShot.active) return 188;
    updateCarrierDefence(&g,&idle,&firing,&idle2,noBuffers);
    updateCarrierDefence(&g,&firing,&idle,&idle2,noBuffers);
    if (g.rockets != 8 || !g.rocketShot.active) return 189;
    /* A bomb already touching Harrier cannot be rescued by a same-tick
     * interception from a player/carrier missile. Resolve contact first. */
    for (UBYTE friendly=0; friendly<2; friendly++)
    for (UBYTE carrierShot=0; carrierShot<3; carrierShot++) {

        initGameState(&g,12040,12040,2); g.gameMode = GAME_MODE_ENHANCED;
        g.missionNumber = g.levelDifficulty = 2;
        g.defence.phase = DEFENCE_WAVE; g.defence.hull = 100;
        g.defence.spawnDelay = 200; g.defence.quota = 3;
        g.playerX = 118; g.playerY = 71; g.defence.clock = 1;
        carrierDropBomb(&g,122,69,0);
        WeaponState* coveringShot = carrierShot ? &g.wingman.rocket : &g.rocketShot;
        memset(coveringShot,0,sizeof(*coveringShot)); coveringShot->active = 1;
        coveringShot->x = coveringShot->worldX = 122; coveringShot->y = 69;
        coveringShot->targetWorldX = 122L << 8; coveringShot->targetY = 69 << 8;
        if (carrierShot == 2) {
            coveringShot->active = 0;
            memset(g.helicopterBullets,0,sizeof(g.helicopterBullets));
            g.helicopterBullets[0].active = 1;
            g.helicopterBullets[0].worldX = 123; g.helicopterBullets[0].y = 70;
        }
        /* Enemy bomb contact always hurts; carrier missile damage is opt-in. */
        g.friendlyFire = friendly;
        updateCarrierDefence(&g,&idle,&idle,&idle2,noBuffers);
        if (g.armour != (friendly && carrierShot == 1 ? 17 : 50) || g.defence.bombs[0].active) return 192;
    }
    /* Level-two enemy bombs: full tick, including CPU interception/service. */
    for (UBYTE landed=0; landed<2; landed++) {
        initGameState(&g,12040,12040,2); g.gameMode = GAME_MODE_ENHANCED;
        g.missionNumber = g.levelDifficulty = 2;
        g.defence.phase = DEFENCE_WAVE; g.defence.hull = 100;
        g.defence.spawnDelay = 200; g.defence.quota = 3;
        g.defence.landed = landed; g.playerX = 80;
        g.playerY = landed ? TAKEOFF_PLAYER_DECK_Y : 71;
        g.defence.clock = 1;
        carrierDropBomb(&g, g.playerX+4, g.playerY-2, 0);
        updateCarrierDefence(&g,&idle,&idle,&idle2,noBuffers);
        if (g.armour != 50 || g.missileDamageThirds != 150 || g.defence.bombs[0].active) return 190;
        carrierDropBomb(&g, g.playerX+4, g.playerY-2, 1);
        updateCarrierDefence(&g,&idle,&idle,&idle2,noBuffers);
        if (g.armour || !g.aircraftFailureState || g.defence.bombs[0].active) return 191;
    }


    carrierParkedWingmanVisible = carrierWingmanLiftDepth = 0;



    carrierParkedWingmanVisible = 0;

    return 0;
}
#endif
