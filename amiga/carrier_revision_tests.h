#if HAR_HEADLESS_CLASSIC_CONTRACT_TEST
static UBYTE referenceCarrierRevisionMatches(void) {
    static GameState g;
    InputState in = {0};
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_SECURE; g.playerX = 83; g.playerY = 88;
    in.down = 1; carrierMoveVtol(&g,&in);
    if (g.playerY != 90) return 130; /* Exact 17.09.2026.uss position. */
    for (UBYTE i=0;i<10;i++) carrierMoveVtol(&g,&in);
    if (!g.defence.landed || g.playerY != TAKEOFF_PLAYER_DECK_Y || !carrierDefenceOnDeck(83,TAKEOFF_PLAYER_DECK_Y)) return 131;
    g.playerX = 112; g.playerY = 88; g.defence.landed = 0;
    carrierMoveVtol(&g,&in);
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
    g.rocketShot.active = 1; g.rocketShot.x = 182; g.rocketShot.y = 41;
    carrierDefenceShot(&g,&g.rocketShot);
    if (g.helicopter.active || g.rocketShot.active || g.defence.repairDropPending) return 145;
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_WAVE; g.defence.wave = 1; g.defence.quota = 3;
    carrierDefenceAircraft(&g);
    if (g.defence.jetType == 2) return 146;
    g.enemyPlane.active = 0; g.defence.wave = 2; g.defence.spawned = 2; g.defence.spawnDelay = 0;
    carrierDefenceAircraft(&g);
    if (g.defence.jetType != 2 || g.enemyPlane.y > 28) return 147;
    /* High bomber reverses at both edges without healing or ending its wave. */
    g.defence.quota = g.defence.spawned; g.defence.jetHits = 2;
    WORD bomberY = g.enemyPlane.y;
    for (UBYTE side=0; side<2; side++) {
        g.enemyPlane.direction = side; g.enemyPlane.x = side ? 320 : -16;
        carrierDefenceAircraft(&g);
        if (!g.enemyPlane.active || g.enemyPlane.direction == side ||
            g.enemyPlane.y != bomberY || g.defence.jetHits != 2) return 165;
        memset(g.defence.bombs,0,sizeof(g.defence.bombs));
        g.enemyPlane.x = 110; g.defence.jetAge = 27;
        carrierDefenceAircraft(&g);
        if (!carrierBombsActive(&g) || !g.defence.bombs[0].type) return 166;
    }
    UBYTE* pixels = AllocMem(GAME_WORLD_BITMAP_BYTES, MEMF_PUBLIC | MEMF_CLEAR);
    if (!pixels) return 148;
    UBYTE mode = currentWorldPresentationMode; currentWorldPresentationMode = GAME_MODE_ENHANCED;
    carrierDefenceSinkPixels = carrierParkedWingmanVisible = 0;
    drawPromotedCpcCarrierRangeRowAt(pixels,0,5,0);
    UBYTE tip = pixels[103UL * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + GAME_WORLD_ROW_BYTES] & 2;
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
    updateCarrierDefence(&g,&hitTower,&idle,&idle2,noBuffers);
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
    InputState chord = {0}; chord.fire = chord.bomb = 1;
    for (UBYTE tick=0; tick<5; tick++) if (updateEjectChord(&g,&chord)) return 182;
    chord.bomb = 0;
    if (updateEjectChord(&g,&chord) || g.ejectChordTicks != 4) return 183;
    chord.bomb = 1;
    if (updateEjectChord(&g,&chord) || !updateEjectChord(&g,&chord) || updateEjectChord(&g,&chord)) return 184;
    chord.fire = chord.bomb = 0; updateEjectChord(&g,&chord);
    chord.fire = 1;
    for (UBYTE tick=0; tick<20; tick++) if (updateEjectChord(&g,&chord)) return 185;
    initGameState(&g,12040,12040,1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_SECURE; g.ejectState = 2;
    g.ejectX = 180; g.ejectY = 60; g.ejectTimer = 0;
    InputState steer = {0}; steer.left = 1;
    for (UBYTE tick=0; tick<30; tick++) updateCarrierDefence(&g,&steer,&steer,&idle2,noBuffers);
    if (g.ejectState != 2 || g.ejectX != 150 || g.ejectY != 70) return 186;
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
        /* Carrier missiles also inflict their own 33% friendly-fire damage;
         * carrier bullets are harmless to Harrier. */
        updateCarrierDefence(&g,&idle,&idle,&idle2,noBuffers);
        if (g.armour != (carrierShot == 1 ? 17 : 50) || g.defence.bombs[0].active) return 192;
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
