#if HAR_HEADLESS_CLASSIC_CONTRACT_TEST
static UBYTE referenceCarrierSubmarineMatches(void) {
    static GameState g;
    UBYTE* buffers[GAME_WORLD_BUFFER_COUNT]={0};
    initGameState(&g,12040,12040,1); g.defence.phase=DEFENCE_WAVE;
    g.defence.waves=g.defence.wave=2; g.defence.spawned=1;
    g.gameMode=GAME_MODE_CLASSIC; carrierUpdateSubmarine(&g,buffers);
    if(g.defence.subState) return 180;
    g.gameMode=GAME_MODE_ENHANCED; g.respawnSafeTimer=255;
    for(UBYTE mission=1;mission<3;mission++) {
        g.missionNumber=mission; carrierUpdateSubmarine(&g,buffers);
        if(g.defence.subState || g.defence.subUsed) return 178;
    }
    g.missionNumber=3;
    g.playerX=8; g.playerY=8;
    for(UBYTE t=0;t<CARRIER_SUB_BUBBLE_LEAD;t++) carrierUpdateSubmarine(&g,buffers);
    if(g.defence.subHeight || !carrierSubBubbleFrame || g.defence.subState!=CARRIER_SUB_RISING) return 179;
    for(UBYTE t=0;t<24;t++) carrierUpdateSubmarine(&g,buffers);
    if(g.defence.subHeight!=2 || g.defence.subState!=CARRIER_SUB_RISING) return 181;
    for(UBYTE t=0;t<45;t++) carrierUpdateSubmarine(&g,buffers);
    if(g.defence.subHeight!=2 || g.defence.ballistic.active) return 181;
    for(UBYTE t=0;t<72;t++) carrierUpdateSubmarine(&g,buffers);
    if(carrierSubBubbleFrame || g.defence.subState!=CARRIER_SUB_SURFACED || g.defence.subHeight!=8 || !g.defence.subUsed) return 181;
    WeaponState shot={0}; shot.active=1; shot.x=g.defence.subX+8; shot.y=SEA_SURFACE_Y-8;
    carrierDefenceShot(&g,&shot);
    if(g.defence.subHits || g.defence.subState!=CARRIER_SUB_SURFACED) return 182;
    HelicopterBullet bullet={0}; bullet.active=1; bullet.worldX=g.defence.subX+8; bullet.y=SEA_SURFACE_Y-7;
    carrierAdvanceBullet(&g,&bullet);
    if(g.defence.subHits || g.defence.subState!=CARRIER_SUB_SURFACED) return 182;
    for(UBYTE t=0;t<100;t++) carrierUpdateSubmarine(&g,buffers);
    if(g.defence.ballisticPhase!=CARRIER_BALLISTIC_ASCENT || !g.defence.ballistic.active) return 183;
    WORD launchX=g.defence.ballistic.x;
    for(UBYTE t=0;t<80 && g.defence.ballisticPhase==CARRIER_BALLISTIC_ASCENT;t++) {
        carrierUpdateSubmarine(&g,buffers);
        if(g.defence.ballistic.x!=launchX) return 184;
    }
    if(g.defence.ballisticPhase!=CARRIER_BALLISTIC_WAIT || g.defence.ballistic.active) return 185;
    for(UWORD t=0;t<CARRIER_BALLISTIC_DELAY-1;t++) carrierUpdateSubmarine(&g,buffers);
    if(g.defence.ballistic.active) return 186;
    carrierUpdateSubmarine(&g,buffers);
    if(g.defence.ballisticPhase!=CARRIER_BALLISTIC_DESCENT || !g.defence.ballistic.active) return 187;
    WORD targetX=g.defence.ballistic.x; g.defence.hull=100;
    WORD returnY=g.defence.ballistic.y;
    for(UBYTE t=0;t<20;t++) carrierUpdateSubmarine(&g,buffers);
    if(g.defence.ballistic.y!=returnY+10) return 188;
    for(UWORD t=0;t<300 && g.defence.ballistic.active;t++) {
        carrierUpdateSubmarine(&g,buffers);
        if(g.defence.ballistic.x!=targetX) return 188;
    }
    if(g.defence.ballisticPhase || g.defence.hull!=76) return 189;
    g.rocketHeightLock=0; /* Fixed interception coordinates, independent of Harrier altitude. */
    for(UBYTE pass=0;pass<2;pass++) {
        g.defence.ballistic.active=1; g.defence.ballistic.x=200; g.defence.ballistic.y=52;
        g.defence.ballisticPhase=pass ? CARRIER_BALLISTIC_DESCENT : CARRIER_BALLISTIC_ASCENT;
        g.defence.ballistic.guidanceDistance=0;
        WeaponState* missile=pass ? &g.wingman.rocket : &g.rocketShot;
        memset(missile,0,sizeof(*missile)); missile->active=1; missile->x=200; missile->y=60;
        carrierDefenceShot(&g,missile);
        if(missile->active || !g.defence.ballistic.active || g.defence.ballistic.guidanceDistance!=1) return 190;
        missile=pass ? &g.rocketShot : &g.wingman.rocket;
        memset(missile,0,sizeof(*missile)); missile->active=1; missile->x=200; missile->y=60;
        carrierDefenceShot(&g,missile);
        if(missile->active || !g.defence.ballistic.active || g.defence.ballistic.guidanceDistance!=2) return 190;
        memset(missile,0,sizeof(*missile)); missile->active=1; missile->x=200; missile->y=60;
        carrierDefenceShot(&g,missile);
        if(missile->active || g.defence.ballistic.active || g.defence.ballisticPhase) return 190;
    }
    WeaponState bomb={0}; bomb.active=1; bomb.x=g.defence.subX+10; bomb.y=SEA_SURFACE_Y-3;
    if(!carrierBombHitsSubmarine(&g,&bomb) || bomb.active || g.defence.subHits!=1 || g.defence.subState!=CARRIER_SUB_SURFACED) return 191;
    g.defence.ballisticPhase=CARRIER_BALLISTIC_WAIT; g.defence.ballisticClock=150;
    g.defence.ballistic.active=0; bomb.active=1;
    if(!carrierBombHitsSubmarine(&g,&bomb) || g.defence.subState!=CARRIER_SUB_SINKING) return 192;
    for(UBYTE t=0;t<48;t++) carrierUpdateSubmarine(&g,buffers);
    if(carrierSubBubbleFrame || g.defence.subState || g.defence.subHeight || !g.defence.ballisticPhase) return 193;
    /* A launched missile survives sinking its boat; only one boat per raid. */
    for(UBYTE t=0;t<102;t++) carrierUpdateSubmarine(&g,buffers);
    if(g.defence.subState || g.defence.ballisticPhase!=CARRIER_BALLISTIC_DESCENT) return 194;
    g.defence.ballisticPhase=g.defence.ballistic.active=0;
    g.enemyPlane.active=g.helicopter.active=g.enemyMissile.active=0;
    g.defence.quota=g.defence.spawned=0; g.defence.wave=g.defence.waves=1;
    g.defence.subState=CARRIER_SUB_SINKING; g.defence.subHeight=1; g.defence.subClock=0;
    InputState input={0}; Player2InputState p2={0};
    updateCarrierDefence(&g,&input,&input,&p2,buffers);
    if(g.defence.phase!=DEFENCE_WAVE) return 195;
    g.defence.subState=g.defence.subHeight=0;
    updateCarrierDefence(&g,&input,&input,&p2,buffers);
    if(g.defence.phase!=DEFENCE_SECURE) return 196;
    UBYTE* pixels=AllocMem(GAME_WORLD_BITMAP_BYTES,MEMF_PUBLIC|MEMF_CLEAR);
    if(!pixels) return 197;
    UBYTE valid=1;
    static const WORD heights[]={-15,-7,0,52,96};
    for(UBYTE pass=0;pass<2;pass++) for(UBYTE n=0;n<5;n++) {
        resetRocketShotPixelBobFootprints();
        g.defence.ballistic.active=1; g.defence.ballistic.x=g.defence.ballistic.worldX=185;
        g.defence.ballistic.y=heights[n]; g.defence.ballisticPhase=pass ? 3 : 1;
        drawCarrierBallistic(pixels,0,&g); retireEncounterTransientBobs(pixels,0);
        for(ULONG i=0;i<GAME_WORLD_BITMAP_BYTES;i++) if(pixels[i]) valid=0;
    }
    FreeMem(pixels,GAME_WORLD_BITMAP_BYTES);
    /* Bomb must travel below the old Y110 cutoff to reach the waterline. */
    initGameState(&g,12040,12040,1); g.gameMode=GAME_MODE_ENHANCED;
    g.defence.phase=DEFENCE_WAVE; g.defence.hull=100;
    g.defence.subState=CARRIER_SUB_SURFACED; g.defence.subHeight=8;
    g.defence.subUsed=1; g.defence.subX=248; g.defence.spawnDelay=200;
    g.playerX=80; g.playerY=60; g.respawnSafeTimer=255;
    g.takeoffState=TAKEOFF_STATE_AIRBORNE;
    g.bombShot.active=1; g.bombShot.x=258; g.bombShot.y=108;
    for(UBYTE t=0;t<10;t++) updateCarrierDefence(&g,&input,&input,&p2,buffers);
    if(g.bombShot.active || g.defence.subHits!=1) return 199;
    /* Released VTOL bombs retain the actual velocity, including ascent,
     * regardless of facing or subsequent Harrier countersteering. */
    initGameState(&g,12040,12040,1); g.gameMode=GAME_MODE_ENHANCED;
    g.defence.phase=DEFENCE_WAVE; g.bombs=10; g.playerX=120; g.playerY=50;
    g.defence.vtolVX=384; g.defence.vtolVY=-256;
    g.defence.facing=MAVERICK_DIRECTION_LEFT;
    if(!launchBomb(&g) || g.bombShot.dx!=384 || g.bombShot.dy!=-256) return 200;
    WORD releaseX=g.bombShot.x,releaseY=g.bombShot.y;
    g.defence.vtolVX=-384; g.defence.vtolVY=384;
    for(UBYTE t=0;t<6;t++) advanceCarrierBombMotion(&g);
    if(g.bombShot.x!=releaseX+9 || g.bombShot.y>=releaseY || g.bombShot.dx!=384) return 201;
    for(UBYTE t=0;t<42;t++) advanceCarrierBombMotion(&g);
    if(g.bombShot.y<=releaseY || g.bombShot.dy!=512) return 202;
    g.bombShot.active=0; g.bombLaunchCooldown=0;
    if(!launchBomb(&g)) return 203;
    releaseX=g.bombShot.x;
    for(UBYTE t=0;t<6;t++) advanceCarrierBombMotion(&g);
    if(g.bombShot.x!=releaseX-9) return 204;
    g.bombShot.x=-4; advanceCarrierBombMotion(&g);
    if(g.bombShot.active) return 205;
    carrierSubHeight=0; carrierSubX=0;
    return valid ? 0 : 198;
}

/* Pixel oracle for both partially visible bomber edges, including clipping
 * before the physical ring-buffer margin. Restore must leave every byte intact. */
static UBYTE referenceCarrierBomberEdges(void) {
    const ULONG bytes=42UL*SCREEN_PLANES*GAME_WORLD_ROW_BYTES;
    UBYTE* pixels=AllocMem(bytes,MEMF_CHIP);
    CarrierBlitMemory* memory=AllocMem(sizeof(CarrierBlitMemory),MEMF_CHIP|MEMF_CLEAR);
    if(!pixels || !memory) {
        if(pixels) FreeMem(pixels,bytes); if(memory) FreeMem(memory,sizeof(CarrierBlitMemory)); return 199;
    }
    OwnBlitter(); WaitBlit(); UWORD oldDma=custom->dmaconr;
    custom->dmacon=DMAF_SETCLR|DMAF_MASTER|DMAF_BLITTER;
    static GameState g; memset(&g,0,sizeof(g)); g.gameMode=GAME_MODE_ENHANCED;
    g.defence.phase=DEFENCE_WAVE; g.defence.jetType=2; g.enemyPlane.active=1; g.enemyPlane.y=16;
    static const WORD positions[]={-31,-17,-16,-9,-1,289,303,319};
    UBYTE result=0;
    for(UBYTE pose=0;pose<2 && !result;pose++) for(UBYTE n=0;n<8 && !result;n++) {
        resetRocketShotPixelBobFootprints(); memset(pixels,0xa5,bytes);
        carrierBlitMemory=memory; carrierBlitPose=255;
        g.enemyPlane.x=g.enemyPlane.worldX=positions[n]; g.enemyPlane.direction=pose;
        updateCarrierBomberBob(pixels,0,&g);
        if(!bomberBlitValid[0]) { result=200; break; }
        for(WORD y=0;y<42;y++) for(WORD x=0;x<320;x++) for(UBYTE p=0;p<SCREEN_PLANES;p++) {
            UWORD px=x+GAME_WORLD_BUFFER_MARGIN_PIXELS; UBYTE bit=0x80>>(px&7);
            UBYTE expected=0xa5&bit;
            WORD sx=x-positions[n], sy=y-16;
            if(sx>=0 && sx<32 && sy>=0 && sy<16 && p<4) {
                const UBYTE* row=carrierBomberTiles+pose*320+((sy/8)*4+sx/8)*40+(sy&7)*5;
                if(row[4]&(0x80>>(sx&7))) expected=(row[p]&(0x80>>(sx&7))) ? bit : 0;
            }
            if((pixels[(ULONG)y*SCREEN_PLANES*GAME_WORLD_ROW_BYTES+p*GAME_WORLD_ROW_BYTES+(px>>3)]&bit)!=expected) result=201;
        }
        retireEncounterBobs(pixels,0);
        for(ULONG i=0;i<bytes;i++) if(pixels[i]!=0xa5) result=202;
    }
    WaitBlit(); custom->dmacon=(DMAF_MASTER|DMAF_BLITTER)&~oldDma; DisownBlitter();
    carrierBlitMemory=0; carrierBlitPose=255; resetRocketShotPixelBobFootprints();
    FreeMem(memory,sizeof(CarrierBlitMemory)); FreeMem(pixels,bytes); return result;
}
#endif
