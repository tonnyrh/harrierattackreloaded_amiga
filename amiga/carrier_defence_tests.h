#include "carrier_gunnery_tests.h"
#include "carrier_revision_tests.h"
#include "carrier_submarine_tests.h"
#if HAR_HEADLESS_CLASSIC_CONTRACT_TEST
#if HAR_HEADLESS_ATTRACT_CARRIER_TEST_ONLY
static UBYTE referenceAttractDefenceMatches(void) {
    static GameState g; initGameState(&g,12040,12040,1);
    g.gameMode=GAME_MODE_ENHANCED; g.skillLevel=g.levelDifficulty=1;
    g.defence.phase=DEFENCE_ALARM; g.defence.phaseTicks=150; g.defence.waves=2;
    g.defence.landed=1; g.defence.facing=MAVERICK_DIRECTION_RIGHT;
    g.playerX=80; g.playerY=TAKEOFF_PLAYER_DECK_Y; g.takeoffState=TAKEOFF_STATE_AIRBORNE;
    g.wingmanControl=WINGMAN_CONTROL_CPU;
    AttractDemoState demo={0}; resetAttractDemoRun(&demo,1);
    InputState input={0},prior={0}; Player2InputState p2={0}; UBYTE* buffers[GAME_WORLD_BUFFER_COUNT]={0};
    UBYTE oldMod=modPlaying; modPlaying=1;
    UWORD ticks;
    for(ticks=0;ticks<6000 && g.defence.phase && !g.gameOver;ticks++) {
        frameCounter++; driveAttractDemoInput(&demo,&g,&input);
        updateCarrierDefence(&g,&input,&prior,&p2,buffers); prior=input;
    }
    BPTR f=Open((CONST_STRPTR)"DH1:attract_carrier_state.bin",MODE_NEWFILE);
    if(f) { UWORD state[]={ticks,g.defence.phase,g.defence.hull,g.defence.wave,g.defence.spawned,g.defence.subState,g.playerX,g.playerY,g.crashTimer,g.fuel,g.rockets,g.bombs,g.defence.jetHits,g.enemyPlane.active,g.helicopter.active}; Write(f,state,sizeof(state)); Close(f); }
    modPlaying=oldMod;
    return g.defence.phase ? 1 : 0;
}
#endif
/* Actual impact/repair and wave transitions exercise sticky raid history. */
static UBYTE referenceCarrierBonusMatches(void) {
    static GameState g;
    UBYTE* buffers[GAME_WORLD_BUFFER_COUNT]={0};
    InputState idle={0}; Player2InputState p2={0};
    UBYTE oldMod=modPlaying, result=0; modPlaying=1;
    for(UBYTE scenario=0;scenario<5 && !result;scenario++) {
        initGameState(&g,12040,12040,1); g.gameMode=GAME_MODE_ENHANCED;
        if(g.defence.raidDamaged || g.defence.raidBonus) { result=1; break; }
        g.defence.phase=DEFENCE_WAVE; g.defence.wave=1; g.defence.waves=2;
        g.defence.spawned=g.defence.quota=3; g.defence.spawnDelay=200; g.defence.subUsed=1;
        g.playerX=80; g.playerY=60; g.takeoffState=TAKEOFF_STATE_AIRBORNE;
        if(scenario==1 || scenario==2) carrierDefenceImpact(&g,80,12);
        if(scenario==3) {
            g.defence.gunHeight[0]=8;
            if(!carrierWeaponHitsGun(&g,72,104,6)) { result=2; break; }
        }
        if(scenario==4) carrierDefenceImpact(&g,80,0);
        updateCarrierDefence(&g,&idle,&idle,&p2,buffers);
        if(g.defence.phase!=DEFENCE_LULL || g.defence.raidBonus) { result=3; break; }
        if(scenario==1 || scenario==3) carrierDeliverRepair(&g,g.defence.hull,20);
        g.defence.phase=DEFENCE_WAVE; g.defence.wave=2;
        ULONG before=g.missionBaseScore;
        updateCarrierDefence(&g,&idle,&idle,&p2,buffers);
        UBYTE expected=scenario==2 ? 0 : (scenario==1 || scenario==3 ? 1 : 2);
        if(g.defence.phase!=DEFENCE_SECURE || g.defence.raidBonus!=expected ||
            !!g.defence.perfectTicks!=!!expected ||
            g.missionBaseScore-before!=(expected==2 ? 2000 : expected==1 ? 1000 : 0)) { result=4+scenario; break; }
        before=g.missionBaseScore;
        updateCarrierDefence(&g,&idle,&idle,&p2,buffers);
        if(g.missionBaseScore!=before) { result=9; break; }
        carrierDefenceImpact(&g,80,12);
        if(g.defence.raidBonus!=expected) { result=10; break; }
    }
    modPlaying=oldMod; return result;
}
/* Compare the real rescue tick against an ordinary raid tick: enemy
 * simulation must advance identically while only Harrier is replaced. */
static UBYTE carrierRescueWeaponsDiffer(const WeaponState* a, const WeaponState* b) {
    const UBYTE* left=(const UBYTE*)a; const UBYTE* right=(const UBYTE*)b;
    for(UWORD i=0;i<sizeof(*a);i++) if(left[i]!=right[i]) return 1;
    return 0;
}
static UBYTE referenceCarrierRescueContinuity(void) {
    static GameState rescued, control;
    UBYTE* buffers[GAME_WORLD_BUFFER_COUNT]={0};
    InputState idle={0}; Player2InputState p2={0};
    UBYTE oldMod=modPlaying, result=0; modPlaying=1;
    for(UBYTE boss=0;boss<2 && !result;boss++) {
        initGameState(&rescued,12040,12040,1);
        rescued.gameMode=GAME_MODE_ENHANCED;
        rescued.defence.phase=DEFENCE_WAVE; rescued.defence.wave=1; rescued.defence.waves=2;
        rescued.defence.quota=4; rescued.defence.spawned=2; rescued.defence.spawnDelay=200;
        rescued.defence.subUsed=1; rescued.defence.hull=76; rescued.defence.raidDamaged=1;
        rescued.defence.jetType=boss ? 2 : 0; rescued.defence.jetHits=2;
        rescued.enemyPlane.active=1; rescued.enemyPlane.x=220; rescued.enemyPlane.y=30;
        rescued.enemyMissile.active=1; rescued.enemyMissile.x=180; rescued.enemyMissile.y=40;
        rescued.enemyMissileFromShip=0; rescued.enemyRespawnTimer=77;
        rescued.playerX=80; rescued.playerY=TAKEOFF_PLAYER_DECK_Y;
        rescued.defence.landed=1; rescued.lives=3;
        rescued.takeoffState=TAKEOFF_STATE_AIRBORNE;
        control=rescued;
        rescued.ejectState=3; rescued.armour=0;
        updateCarrierDefence(&rescued,&idle,&idle,&p2,buffers);
        updateCarrierDefence(&control,&idle,&idle,&p2,buffers);
        if(rescued.ejectState || rescued.lives!=2 || !rescued.defence.landed || rescued.armour!=100) result=1;
        else if(!rescued.enemyPlane.active || carrierRescueWeaponsDiffer(&rescued.enemyPlane,&control.enemyPlane)) result=2;
        else if(!rescued.enemyMissile.active || carrierRescueWeaponsDiffer(&rescued.enemyMissile,&control.enemyMissile)) result=3;
        else if(rescued.enemyRespawnTimer!=control.enemyRespawnTimer || rescued.enemyMissileFromShip!=control.enemyMissileFromShip) result=4;
        else if(rescued.defence.phase!=control.defence.phase || rescued.defence.spawnDelay!=control.defence.spawnDelay ||
            rescued.defence.spawned!=control.defence.spawned || rescued.defence.jetHits!=control.defence.jetHits ||
            rescued.defence.hull!=76 || !rescued.defence.raidDamaged) result=5;
    }
    /* Terrain respawns retain their established enemy-clear behaviour. */
    rescued.defence.phase=0; rescued.enemyPlane.active=rescued.enemyMissile.active=1;
    respawnPlayer(&rescued);
    if(rescued.enemyPlane.active || rescued.enemyMissile.active || rescued.enemyRespawnTimer) result=6;
    modPlaying=oldMod; return result;
}
static UBYTE referenceCarrierProgressionMatches(void) {
    static GameState g; UBYTE* buffers[GAME_WORLD_BUFFER_COUNT]={0};
    InputState idle={0}; Player2InputState p2={0};
    UBYTE oldMod=modPlaying, result=0; modPlaying=1;
    for(UBYTE mode=0;mode<2;mode++) for(UBYTE skill=1;skill<=5;skill++) {
        UBYTE lastQuota=0,lastWaves=0,lastDelay=255;
        for(UBYTE mission=1;mission<=10;mission++) {
            initGameState(&g,12040,12040,mission); g.gameMode=mode;
            g.missionNumber=mission; g.levelDifficulty=cpcDifficultyForMission(skill,mission);
            carrierBeginDefence(&g);
            if(mode==GAME_MODE_CLASSIC || mission==1) {
                if(g.defence.phase) result=1;
                continue;
            }
            if(g.defence.phase!=DEFENCE_ALARM || !g.defence.landed || g.defence.waves<lastWaves) result=2;
            lastWaves=g.defence.waves;
            g.playerX=80; g.playerY=TAKEOFF_PLAYER_DECK_Y; g.defence.phaseTicks=0;
            updateCarrierDefence(&g,&idle,&idle,&p2,buffers);
            if(g.defence.phase!=DEFENCE_WAVE || g.defence.quota<lastQuota) result=3;
            lastQuota=g.defence.quota; g.defence.spawnDelay=0;
            carrierDefenceAircraft(&g);
            if(g.defence.spawned!=1 || g.defence.spawnDelay>lastDelay) result=4;
            lastDelay=g.defence.spawnDelay;
        }
    }
    if(!result) result=referenceCarrierSubmarineMatches();
    modPlaying=oldMod; return result;
}
static UBYTE referenceCarrierDefenceMatches(void) {
    UBYTE progression=referenceCarrierProgressionMatches();
    if(progression) return progression;
    UBYTE rescueResult=referenceCarrierRescueContinuity();
    if(rescueResult) return rescueResult;
    UBYTE bonusResult=referenceCarrierBonusMatches();
    if(bonusResult) return bonusResult;
    UBYTE sceneResult=referenceCarrierSceneStatusMatches();
    if(sceneResult) return sceneResult;
    UBYTE friendlyResult=referenceFriendlyFireMatches();
    if(friendlyResult) return friendlyResult;
    UBYTE smoothResult=referenceHelicopterHeightMatches();
    if(smoothResult) return smoothResult;
    UBYTE towerResult=referenceCarrierPickupTowerMatches();
    if(towerResult) return towerResult;
    UBYTE deckReadyResult=referenceCarrierDeckReadyMatches();
    if(deckReadyResult) return deckReadyResult;
#if HAR_HEADLESS_CARRIER_DECK_TEST_ONLY
    return 0;
#endif
#if HAR_HEADLESS_CARRIER_SUBMARINE_TEST_ONLY
    return referenceCarrierSubmarineMatches();
#endif
    static GameState g;
    UBYTE* buffers[GAME_WORLD_BUFFER_COUNT] = {0};
    UBYTE oldMod = modPlaying; modPlaying = 1;
    initGameState(&g, 12040, 12040, 1);
    g.gameMode = GAME_MODE_ENHANCED; g.levelDifficulty = 1;
    /* Two kits require two complete service intervals; lift-off cancels
     * progress, full hull/guns preserve cargo, and a kit repairs both guns. */
    g.defence.phase=DEFENCE_WAVE; g.defence.landed=1;
    g.defence.hull=40; g.defence.cargo=40; g.defence.gunHealth[0]=0;
    for(UBYTE t=0;t<CARRIER_REPAIR_TICKS-1;t++) carrierServiceRepair(&g,0);
    if(g.defence.hull!=40 || g.defence.cargo!=40) return 202;
    carrierServiceRepair(&g,1);
    if(g.defence.service || g.defence.cargo!=40) return 203;
    for(UBYTE t=0;t<CARRIER_REPAIR_TICKS;t++) carrierServiceRepair(&g,0);
    if(g.defence.hull!=60 || g.defence.cargo!=20 || g.defence.gunHealth[0]!=2) return 204;
    for(UBYTE t=0;t<CARRIER_REPAIR_TICKS;t++) carrierServiceRepair(&g,0);
    if(g.defence.hull!=80 || g.defence.cargo) return 205;
    g.defence.hull=100; g.defence.cargo=20;
    for(UBYTE t=0;t<CARRIER_REPAIR_TICKS;t++) carrierServiceRepair(&g,0);
    if(g.defence.cargo!=20) return 206;
    /* Down accelerates, up slows descent, fire alone leaves normal descent. */
    for(UBYTE fast=0;fast<4;fast++) {
        initGameState(&g,12040,12040,1); g.gameMode=GAME_MODE_ENHANCED;
        g.defence.phase=DEFENCE_SECURE; g.defence.hull=100; g.defence.landed=0;
        g.ejectState=2; g.ejectTimer=0; g.ejectY=20; g.ejectX=80;
        g.abandonedAircraftActive=g.crashTimer=0;
        InputState chute={0}, prior={0}; Player2InputState p2={0}; chute.down=fast==1; chute.up=fast==2; chute.fire=fast==3;
        for(UBYTE t=0;t<6;t++) updateCarrierDefence(&g,&chute,&prior,&p2,buffers);
        if(g.ejectY!=(fast==1 ? 24 : fast==2 ? 21 : 22)) return 207;
    }
    /* Height lock follows the player; disabling it preserves the launch Y. */
    for(UBYTE locked=0;locked<2;locked++) {
        initGameState(&g,12040,12040,1); g.gameMode=GAME_MODE_ENHANCED;
        g.defence.phase=DEFENCE_WAVE; g.playerY=30; g.rocketHeightLock=locked;
        carrierDefenceLaunch(&g,&g.rocketShot,40,50,MAVERICK_DIRECTION_RIGHT);
        carrierDefenceShot(&g,&g.rocketShot);
        if(!g.rocketShot.active || g.rocketShot.y!=(locked?32:50)) return 208;
    }
    /* The final hull value controls Perfect, including a repaired carrier;
     * transition awards once, subsequent secure/landing ticks never repeat it. */
    for(UBYTE perfect=0;perfect<2;perfect++) {
        initGameState(&g,12040,12040,1); g.gameMode=GAME_MODE_ENHANCED;
        g.defence.phase=DEFENCE_WAVE; g.defence.wave=g.defence.waves=1;
        g.defence.spawned=g.defence.quota=3; g.defence.spawnDelay=200; g.defence.subUsed=1;
        g.defence.hull=perfect?100:99; g.playerX=80; g.playerY=60;
        g.takeoffState=TAKEOFF_STATE_AIRBORNE;
        InputState idle={0}; Player2InputState p2={0};
        ULONG score=g.bonusScore; updateCarrierDefence(&g,&idle,&idle,&p2,buffers);
        if(g.defence.phase!=DEFENCE_SECURE || (g.bonusScore>score)!=perfect || !!g.defence.perfectTicks!=perfect) return 209;
        score=g.bonusScore; updateCarrierDefence(&g,&idle,&idle,&p2,buffers);
        if(g.bonusScore!=score) return 210;
    }
    initGameState(&g,12040,12040,1); g.gameMode=GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_WAVE; g.defence.hull = 100;
    for (UBYTE i = 0; i < 8; i++) carrierDefenceImpact(&g, 80, CARRIER_BOMB_HULL_DAMAGE);
    if (g.defence.hull != 4 || g.defence.phase != DEFENCE_WAVE) return 1;
    carrierDefenceImpact(&g, 80, CARRIER_BOMB_HULL_DAMAGE);
    if (g.defence.hull || g.defence.phase != DEFENCE_SINKING) return 2;
    g.defence.hull = 40; g.armour = 20; g.fuelGaugeLevel = 2; g.fuelSubCounter = 1;
    g.fuel = cpcFuelHudValue(&g); UWORD before = g.fuel;
    carrierServiceAircraft(&g);
    if (g.defence.hull != 40 || g.armour != 22 || g.fuel <= before || g.fuel == 999) return 3;
    for (UBYTE i = 0; i < 100; i++) carrierServiceAircraft(&g);
    if (g.armour != 100 || g.fuel != 999 || g.missileDamageThirds || g.defence.hull != 40) return 4;
    if (!carrierDefenceOnDeck(80, TAKEOFF_PLAYER_DECK_Y) || carrierDefenceOnDeck(104, TAKEOFF_PLAYER_DECK_Y) || carrierDefenceOnDeck(80, TAKEOFF_PLAYER_DECK_Y - 1)) return 5;
    memset(g.defence.bombs, 0, sizeof(g.defence.bombs));
    for (UBYTE i = 0; i < 12; i++) carrierDropBomb(&g, 80, 20, 0);
    UBYTE count = 0; for (UBYTE i = 0; i < CARRIER_DEFENCE_BOMBS; i++) count += g.defence.bombs[i].active != 0;
    if (count != 4) return 6;
    memset(&g.rocketShot, 0, sizeof(g.rocketShot));
    g.rocketHeightLock=0;
    g.rocketShot.active = 1; g.rocketShot.x = 80; g.rocketShot.y = 20;
    carrierDefenceShot(&g, &g.rocketShot);
    if (g.rocketShot.active || g.defence.bombs[0].active) return 7;
    /* Real state machine traverses alarm, mixed wave, lull and mandatory
     * landing. Clear bombs to model perfect defence, not time-based victory. */
    initGameState(&g, 12040, 12040, 1); g.levelDifficulty = 1;
    g.gameMode = GAME_MODE_ENHANCED; g.defence.phase = DEFENCE_ALARM;
    g.defence.phaseTicks = 2; g.defence.waves = 2;
    g.takeoffState = TAKEOFF_STATE_AIRBORNE; g.playerX = 80; g.playerY = 70;
    g.defence.landed = 0; carrierParkedWingmanVisible = 0;
    InputState in = {0}, prev = {0}; Player2InputState in2 = {0};
    UBYTE sawJet = 0, sawHeli = 0, sawLull = 0;
    for (UWORD t = 0; t < 7000 && g.defence.phase != DEFENCE_SECURE; t++) {
        if(g.defence.subState==CARRIER_SUB_SURFACED) {
            WeaponState testBomb={0}; testBomb.active=1; testBomb.x=g.defence.subX+8; testBomb.y=SEA_SURFACE_Y-4;
            carrierBombHitsSubmarine(&g,&testBomb); testBomb.active=1; carrierBombHitsSubmarine(&g,&testBomb);
        }
        memset(g.defence.bombs, 0, sizeof(g.defence.bombs));
        g.enemyMissile.active = 0; g.respawnSafeTimer = 2;
        updateCarrierDefence(&g, &in, &prev, &in2, buffers);
        sawJet |= g.enemyPlane.active; sawHeli |= g.helicopter.active;
        if(g.enemyPlane.active && g.defence.jetType==2) {
            if(g.defence.wave!=g.defence.waves || g.defence.spawned!=g.defence.quota) return 201;
            for(UBYTE hit=0;hit<4;hit++) carrierDefenceKillJet(&g);
        }
        sawLull |= g.defence.phase == DEFENCE_LULL;
    }
    if (g.defence.phase != DEFENCE_SECURE || !sawJet || !sawHeli || !sawLull || g.defence.wave != 2) return 8;
    g.playerY = TAKEOFF_PLAYER_DECK_Y; g.defence.landed = 1;
    updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    if (g.defence.phase != DEFENCE_DEPART) return 9;
    for (UBYTE tick=0; tick<60; tick++) updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    g.defence.phaseTicks = 0; in.up = 1;
    updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    if (g.defence.phase || g.takeoffState != TAKEOFF_STATE_LIFTING) return 10;
    memset(&in, 0, sizeof(in));
    g.defence.phase = DEFENCE_SECURE; g.defence.hull = 60;
    g.defence.cargo = 20; g.armour = 0; g.lives = 2;
    g.ejectState = 3; g.abandonedAircraftActive = g.crashTimer = 0;
    g.aircraftFailureState = AIRCRAFT_FAILURE_NONE;
    memset(g.defence.bombs, 0, sizeof(g.defence.bombs));
    updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    if (g.ejectState || g.gameOver || g.armour != 100 || g.defence.hull != 60 ||
        g.defence.cargo != 20 || g.playerY != TAKEOFF_PLAYER_DECK_Y || !g.defence.landed || g.lives != 1) return 31;
    /* Regression: the real parked X=81 must lift straight up beside the island. */
    memset(&in, 0, sizeof(in)); in.up = 1;
    g.playerX = 81; g.playerY = 102; g.defence.landed = 0;
    carrierResetVtol(&g.defence);
    carrierMoveVtol(&g,&in);
    if(g.playerX!=81 || g.playerY!=102 || g.defence.vtolVY>=0) return 32;
    for(UBYTE t=0;t<16;t++) carrierMoveVtol(&g,&in);
    if(g.playerX!=81 || g.playerY>=102) return 33;
    static const WORD pads[] = {64, 81, 82, 128, 144};
    in.up = in.right = 0; in.down = 1;
    for (UBYTE i = 0; i < 5; i++) {
        g.playerX = pads[i]; g.playerY = 104; g.defence.landed = 0; carrierResetVtol(&g.defence);
        carrierMoveVtol(&g, &in);
        if (!g.defence.landed || g.playerY != TAKEOFF_PLAYER_DECK_Y) return 34;
    }
    g.playerX = 104; g.playerY = 88; g.defence.landed = 0; g.respawnSafeTimer = 0;
    for(UBYTE t=0;t<24 && !g.crashTimer;t++) carrierMoveVtol(&g,&in);
    if (!g.crashTimer || !g.crashEndsGame || g.defence.landed) return 35;
    /* The generated reverse pose is an exact pixel mirror, not a recolour. */
    for (UBYTE y = 0; y < 8; y++) for (UBYTE x = 0; x < 16; x++) {
        UBYTE rx = 15 - x;
        UBYTE expected = (rx < 8 ? harCpcHarrierLandingLeftPixels : harCpcHarrierLandingRightPixels)[y * 16 + (rx & 7)];
        if (harrierLandingReverse[y * 16 + x] != expected) return 36;
    }
    static UWORD normal[PLAYER_SPRITE_WORDS], attached[PLAYER_SPRITE_WORDS];
    static UWORD reverse[PLAYER_SPRITE_WORDS], reverseAttached[PLAYER_SPRITE_WORDS];
    g.gameOver = g.crashTimer = g.ejectState = 0; g.takeoffState = TAKEOFF_STATE_AIRBORNE;
    g.defence.phase = DEFENCE_WAVE; g.defence.vtolPose = 1;
    updatePlayerSprite(normal, attached, &g);
    g.defence.vtolPose = 2; updatePlayerSprite(reverse, reverseAttached, &g);
    for (UBYTE i = 2; i < PLAYER_SPRITE_WORDS - 2; i++) {
        UWORD r = 0, a = 0;
        for (UBYTE bit = 0; bit < 16; bit++) { r = (r << 1) | ((normal[i] >> bit) & 1); a = (a << 1) | ((attached[i] >> bit) & 1); }
        if (reverse[i] != r || reverseAttached[i] != a) return 37;
    }
    g.defence.vtolPose = 0; updatePlayerSprite(reverse, reverseAttached, &g);
    if (referenceBuffersEqual((const UBYTE*)normal + 4, (const UBYTE*)reverse + 4, 32)) return 38;
    /* HUD messages may touch only the empty rows, never the instruments. */
    UBYTE* hud = AllocMem(2UL * HUD_BITMAP_BYTES, MEMF_PUBLIC | MEMF_CLEAR);
    if (!hud) return 39;
    UBYTE* baseline = hud + HUD_BITMAP_BYTES;
    g.defence.perfectTicks=0;
    g.defence.phase = 0; g.defence.fullVtol = 0; g.defence.hull = 76; g.defence.cargo = 40;
    memset(hudRenderState, 0, sizeof(hudRenderState));
    drawHudStatic(hud, GAME_MODE_ENHANCED); drawHudValues(hud, &g, 0, 0);
    memcpy(baseline, hud, HUD_BITMAP_BYTES);
    static const UBYTE phases[] = {DEFENCE_ALARM, DEFENCE_WAVE, DEFENCE_SECURE};
    static const char* captures[] = {"DH1:carrier_hud_alarm.bpl", "DH1:carrier_hud_wave.bpl", "DH1:carrier_hud_land.bpl"};
    for (UBYTE i = 0; i < 3; i++) {
        g.defence.phase = phases[i]; drawHudValues(hud, &g, 0, 0);
        if (!referenceBuffersEqual(hud + 46UL * SCREEN_PLANES * SCREEN_ROW_BYTES,
            baseline + 46UL * SCREEN_PLANES * SCREEN_ROW_BYTES, 10UL * SCREEN_PLANES * SCREEN_ROW_BYTES) ||
            !referenceBuffersEqual(hud + 76UL * SCREEN_PLANES * SCREEN_ROW_BYTES,
            baseline + 76UL * SCREEN_PLANES * SCREEN_ROW_BYTES, 10UL * SCREEN_PLANES * SCREEN_ROW_BYTES)) { FreeMem(hud, 2UL * HUD_BITMAP_BYTES); return 40; }
        if (hudRenderState[0].carrierStatus != (i == 0 ? 1 : i == 1 ? 0 : 2)) { FreeMem(hud, 2UL * HUD_BITMAP_BYTES); return 41; }
        BPTR file = Open((CONST_STRPTR)captures[i], MODE_NEWFILE);
        if (file) { Write(file, hud, HUD_BITMAP_BYTES); Close(file); }
    }
    FreeMem(hud, 2UL * HUD_BITMAP_BYTES);
    if (sfxSamples[SFX_CARRIER_KLAXON].byteLength != 8400 || sfxSamples[SFX_CARRIER_KLAXON].period != 591) return 42;
    /* Independent gun durability, automatic clearance and physical collisions. */
    carrierWingmanLiftDepth = 8;
    initGameState(&g, 12040, 12040, 1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_WAVE; g.takeoffState = TAKEOFF_STATE_AIRBORNE;
    g.playerX = 81; g.playerY = TAKEOFF_PLAYER_DECK_Y; g.defence.landed = 1;
    for (UBYTE t = 0; t < 20; t++) { g.defence.clock++; carrierUpdateGuns(&g); }
    if (g.defence.gunHeight[0] || g.defence.gunHeight[1]) return 43;
    g.defence.landed = 0; g.playerY = 60;
    for (UBYTE t = 0; t < 16; t++) { g.defence.clock++; carrierUpdateGuns(&g); }
    if (g.defence.gunHeight[0] != 8 || g.defence.gunHeight[1] != 8) return 44;
    g.playerY = 74;
    for (UBYTE t = 0; t < 8; t++) { g.defence.clock++; carrierUpdateGuns(&g); }
    if (g.defence.gunHeight[0] || g.defence.gunHealth[0] != 2 || g.armour != 100) return 45;
    if (carrierWeaponHitsGun(&g, 72, 104, 6) || g.defence.gunHealth[0] != 2) return 46;
    g.defence.gunHeight[0] = g.defence.gunHeight[1] = 8;
    g.playerX = 220; g.playerY = 20;
    /* Run the enemy-bomb path, including hull damage, for two real impacts. */
    for (UBYTE t = 0; t < 2; t++) {
        carrierDropBomb(&g, 72, 101, t); carrierDefenceBombs(&g);
        if (g.defence.gunHealth[0] != 1 - t || carrierBombsActive(&g)) return 47;
    }
    if (g.defence.hull != 64 || g.defence.gunHeight[0] || g.defence.gunHealth[1] != 2) return 48;
    carrierDeliverRepair(&g, 64, 0);
    if (g.defence.gunHealth[0] || g.defence.gunHealth[1] != 2) return 49;
    carrierDeliverRepair(&g, 64, 20);
    if (g.defence.gunHealth[0] != 2 || g.defence.gunHealth[1] != 2 || g.defence.hull != 84 || g.defence.cargo) return 50;
    g.defence.gunHeight[1] = 8; g.playerX = 144; g.playerY = 100;
    g.respawnSafeTimer = 0; carrierUpdateGuns(&g);
    if (g.defence.gunHealth[1] || g.defence.gunHeight[1] || g.armour != 67 || g.missileDamageThirds != 100) return 51;
    carrierUpdateGuns(&g);
    if (g.armour != 67) return 52; /* Wreckage cannot repeatedly drain armour. */
    memset(g.helicopterBullets, 0, sizeof(g.helicopterBullets));
    g.defence.gunHeight[0] = 0; g.defence.clock = 75;
    g.defence.aimX = 120; g.defence.aimY = 40; carrierGunBullet(&g, 0);
    if (g.helicopterBullets[0].active || g.helicopterBullets[1].active) return 53;
    g.defence.gunHeight[0] = 8; carrierGunBullet(&g, 0);
    if (!g.helicopterBullets[0].active) return 54;
    /* Low hull no longer destroys an otherwise healthy gun. */
    g.defence.hull = 20; g.playerX = 220; g.playerY = 20;
    carrierUpdateGuns(&g);
    if (g.defence.gunHealth[0] != 2 || g.defence.gunHeight[0] != 8) return 55;
    carrierSyncGuns(&g, buffers);
    if (carrierDefenceGunMask != 1 || carrierDefenceGunHeight[0] != 8 || carrierDefenceGunHeight[1]) return 56;
    g.defence.phase = DEFENCE_SINKING; carrierSyncGuns(&g, buffers);
    if (carrierDefenceGunMask) return 57;
    /* Gun cells contain no carrier pixels: the skip table must not hide them.
     * Verify every height against the actual masked tile on both deck sites. */
    UBYTE* gunPixels = AllocMem(GAME_WORLD_BITMAP_BYTES, MEMF_PUBLIC | MEMF_CLEAR);
    if (!gunPixels) return 58;
    carrierParkedWingmanVisible = carrierDefenceSinkPixels = 0;
    for (UBYTE pose = 0; pose < 3; pose++) for (UBYTE gun = 0; gun < 2; gun++) for (UBYTE height = 0; height <= 8; height++) {
        carrierDefenceGunPose[gun] = pose;
        carrierDefenceGunMask = height ? 1 << gun : 0;
        carrierDefenceGunHeight[gun] = height;
        for (UBYTE r = 0; r < 8; r++) for (UBYTE p = 0; p < 4; p++)
            gunPixels[(ULONG)(104 + r) * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + p * GAME_WORLD_ROW_BYTES] = 0;
        drawPromotedCpcCarrierRangeRowAt(gunPixels, 0, gun ? 10 : 1, 1);
        for (UBYTE r = 0; r < 8; r++) for (UBYTE p = 0; p < 4; p++) {
            UBYTE expected = 0;
            if (r >= 8 - height) {
                const UBYTE* source = carrierAaPoses[pose] + (r - 8 + height) * 5;
                expected = source[p] & source[4];
            }
            if (gunPixels[(ULONG)(104 + r) * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + p * GAME_WORLD_ROW_BYTES] != expected) {
                FreeMem(gunPixels, GAME_WORLD_BITMAP_BYTES); return 59;
            }
        }
    }
    /* Runtime base art is already right-facing (the packer mirrors masters).
     * Verify heading against the unshifted bank, not the same frame selector. */
    memset(gunPixels, 0, GAME_WORLD_BITMAP_BYTES);
    memset(helicopterFootprints, 0, sizeof(helicopterFootprints));
    g.helicopter.active = 1; g.helicopter.x = 80; g.helicopter.worldX = 80;
    g.helicopter.y = 32; g.helicopterHits = g.helicopterAge = 0;
    for (UBYTE direction = 0; direction < 2; direction++) {
        g.helicopter.direction = direction; drawHelicopterBob(gunPixels, 0, &g);
        const UBYTE* art = direction ? enhancedEncounterTiles + 160 : enhancedHelicopterMirrored;
        UWORD tileX = ringWorldTileXForColumn(10);
        for (UBYTE half = 0; half < 2; half++) for (UBYTE y = 0; y < 8; y++) for (UBYTE plane = 0; plane < 4; plane++) {
            const UBYTE* row = art + half * 40 + y * 5;
            if (gunPixels[(ULONG)(32 + y) * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + plane * GAME_WORLD_ROW_BYTES + tileX + half] != (row[plane] & row[4])) {
                FreeMem(gunPixels, GAME_WORLD_BITMAP_BYTES); return 60;
            }
        }
        retireEncounterBobs(gunPixels, 0);
    }
    FreeMem(gunPixels, GAME_WORLD_BITMAP_BYTES);
    carrierDefenceGunMask = carrierDefenceGunHeight[0] = carrierDefenceGunHeight[1] = 0;
    memset(&in,0,sizeof(in)); carrierResetVtol(&g.defence);
    g.defence.phase=DEFENCE_WAVE;
    carrierUpdateHeading(&g.defence,&in);
    g.defence.facing=MAVERICK_DIRECTION_RIGHT; g.defence.vtolVX=384;
    in.left=1;
    for(UBYTE t=0;t<9;t++) {
        carrierVtolAxis(&g.defence.vtolVX,&g.defence.vtolAX,&g.defence.vtolSubX,-1);
        carrierUpdateHeading(&g.defence,&in);
    }
    if(g.defence.vtolPose!=2 || g.defence.facing!=MAVERICK_DIRECTION_LEFT || g.defence.vtolVX<=0) return 62;
    in.left=0; carrierUpdateHeading(&g.defence,&in);
    if(g.defence.facing!=MAVERICK_DIRECTION_LEFT || g.defence.vtolPose!=2) return 63;
    g.defence.facing=MAVERICK_DIRECTION_RIGHT;
    carrierDefenceLaunch(&g, &g.rocketShot, 100, 40, g.defence.facing);
    if (g.rocketShot.dx != 4 || g.rocketShot.dy) return 65;
    g.defence.alarmPlayed = 0; modPlaying = 1; stopAllSfx();
    carrierTryAlarm(&g.defence);
    if (g.defence.alarmPlayed) return 66;
    modPlaying = 0; carrierTryAlarm(&g.defence);
    if (!g.defence.alarmPlayed) return 67;
    UBYTE alarmChannel = 255;
    for (UBYTE i = 0; i < SFX_CHANNEL_COUNT; i++)
        if (sfxChannelCurrentId[i] == SFX_CARRIER_KLAXON) alarmChannel = i;
    if (alarmChannel == 255 || sfxPendingSample[alarmChannel] != &sfxSamples[SFX_CARRIER_KLAXON] ||
        sfxChannelPendingVolume[alarmChannel] != AUDIO_MIX_VOLUME(64)) return 68;
    updateSfx();
    if (sfxPendingSample[alarmChannel] || sfxChannelFrames[alarmChannel] != 211) return 69;
    playSfxAt(SFX_FIRE, SFX_POSITION_CENTER);
    if (sfxChannelCurrentId[alarmChannel] != SFX_CARRIER_KLAXON || sfxChannelSilenceQueueDelay[alarmChannel]) return 70;
    for (UWORD t = 0; t < 210; t++) updateSfx();
    if (sfxChannelFrames[alarmChannel] != 1) return 72;
    updateSfx();
    if (sfxChannelFrames[alarmChannel]) return 73;
    stopAllSfx();
    UBYTE peak = 0;
    for (UWORD i = 0; i < sizeof(sfxCarrierKlaxon); i++) {
        WORD v = (BYTE)sfxCarrierKlaxon[i]; if (v < 0) v = -v;
        if (v > peak) peak = v;
    }
    if (peak != 120) return 71;
    modPlaying = 1;
    initGameState(&g, 12040, 12040, 1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_WAVE; g.enemyPlane.active = 1;
    g.enemyPlane.x = 100; g.enemyPlane.y = 40;
    g.bombShot.active = 1; g.bombShot.x = 100; g.bombShot.y = 40;
    if (!carrierBombHitsAircraft(&g, &g.bombShot) || g.enemyPlane.active || g.bombShot.active || g.hitsCount != 1) return 74;
    g.helicopter.active = 1; g.helicopter.x = 100; g.helicopter.y = 40; g.helicopter.type = 0;
    g.bombShot.active = 1;
    if (!carrierBombHitsAircraft(&g, &g.bombShot) || g.bombShot.active || g.helicopterHits != 2) return 75;
    if (g.helicopter.type != 2 || g.hitsCount != 2) return 76;
    g.enemyPlane.active = 1; g.defence.jetType = 2; g.defence.jetHits = 0;
    g.bombShot.active = 1;
    if (!carrierBombHitsAircraft(&g, &g.bombShot) || g.enemyPlane.active || g.bombShot.active || g.hitsCount != 3) return 193;

    g.bombShot.active = 1;
    if (carrierBombHitsAircraft(&g, &g.bombShot)) return 77; /* No duplicate wreck score. */
    g.helicopterHits = 0; g.helicopter.type = 0; g.defence.phase = 0;
    g.playerX = 200; g.playerY = 20; g.respawnSafeTimer = 20;
    UBYTE hudDirty = 0, weaponDirty = 0, wingDirty = 0;
    collideEnhancedEncounters(&g, buffers, &hudDirty, &weaponDirty, &wingDirty);
    if (g.helicopterHits != 2 || g.bombShot.active || !weaponDirty) return 78;
    /* Post-raid landing uses AIRBORNE internally: verify real idle spawning,
     * then the existing scatter path on departure and exclusion during raids. */
    initGameState(&g, 12040, 12040, 1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_DEPART; g.defence.landed = 1;
    g.takeoffState = TAKEOFF_STATE_AIRBORNE; g.playerX = 81; g.playerY = TAKEOFF_PLAYER_DECK_Y;
    resetCarrierGullActors(); UBYTE sawGull = 0;
    for (UWORD tick = 0; tick < 2000 && !sawGull; tick++) {
        updateCarrierGulls(&g, carrierDeckIdleEligible(&g), 1);
        for (UBYTE bird = 0; bird < CARRIER_GULL_MAX; bird++) sawGull |= carrierGulls[bird].active;
    }
    if (!sawGull) return 83;
    g.defence.landed = 0; updateCarrierGulls(&g, carrierDeckIdleEligible(&g), 1);
    for (UBYTE bird = 0; bird < CARRIER_GULL_MAX; bird++)
        if (carrierGulls[bird].active && !carrierGulls[bird].scattering) return 84;
    g.defence.landed = 1; g.defence.phase = DEFENCE_WAVE;
    if (carrierDeckIdleEligible(&g)) return 85;
    g.defence.phase = DEFENCE_SECURE;
    if (!carrierDeckIdleEligible(&g)) return 86;
    resetCarrierGullActors();
    /* LAND NOW must not turn a fatal water impact into a replacement plane,
     * even with spare aircraft available. Eject rescue is covered above. */
    initGameState(&g, 12040, 12040, 1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_SECURE; g.defence.landed = 0;
    g.takeoffState = TAKEOFF_STATE_AIRBORNE; g.playerX = 220;
    g.playerY = SEA_SURFACE_Y - 9; g.lives = 3; g.respawnSafeTimer = 0;
    memset(&in, 0, sizeof(in)); in.down = 1;
    for(UBYTE t=0;t<24 && !g.crashTimer;t++) updateCarrierDefence(&g,&in,&prev,&in2,buffers);
    if (!g.crashTimer || !g.crashEndsGame || g.defence.landed) return 87;
    memset(&in, 0, sizeof(in));
    for (UWORD tick = 0; tick <= PLAYER_CRASH_FRAMES && !g.gameOver; tick++)
        updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    if (!g.gameOver || g.armour || g.playerY == 72) return 88;
    initGameState(&g, 12040, 12040, 1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_SECURE; g.takeoffState = TAKEOFF_STATE_AIRBORNE;
    g.playerX = 220; g.playerY = 106; g.respawnSafeTimer = 0;
    startAircraftFailure(&g, AIRCRAFT_FAILURE_CAUSE_ARMOUR);
    updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    if (!g.crashTimer || !g.crashEndsGame) return 89;
    initGameState(&g, 12040, 12040, 1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_DEPART; g.defence.landed = 0;
    g.takeoffState = TAKEOFF_STATE_AIRBORNE; g.playerX = 81; g.playerY = 104;
    g.respawnSafeTimer = 0; memset(&in, 0, sizeof(in)); in.down = 1;
    engineActive = 1;
    updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    if (!g.defence.landed || engineActive) return 90;
    memset(&in, 0, sizeof(in));
    for (UBYTE tick = 0; tick < 12; tick++) updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    if (engineActive) return 91;
    /* The real engine generator must restart on lift-off. */
    UBYTE* previousEngineBuffer = engineBuffer;
    engineBuffer = AllocMem(ENGINE_BUFFER_BYTES, MEMF_CHIP | MEMF_CLEAR);
    if (!engineBuffer) return 92;
    g.defence.phase = DEFENCE_WAVE; g.defence.spawnDelay = 100;
    in.up = 1; updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    UBYTE engineRestarted = engineActive && !g.defence.landed;
    stopSfxChannel(ENGINE_CHANNEL);
    FreeMem(engineBuffer, ENGINE_BUFFER_BYTES); engineBuffer = previousEngineBuffer;
    if (!engineRestarted) return 93;
    /* Exercise two real button presses through LAND NOW, waiting for both
     * the bomb slot and launch cooldown to clear between them. */
    initGameState(&g, 12040, 12040, 1); g.gameMode = GAME_MODE_ENHANCED;
    g.defence.phase = DEFENCE_SECURE; g.takeoffState = TAKEOFF_STATE_AIRBORNE;
    g.playerX = 220; g.playerY = 70; g.bombs = 4; g.respawnSafeTimer = 0;
    memset(&in, 0, sizeof(in)); memset(&prev, 0, sizeof(prev)); in.bomb = 1;
    updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    if (!g.bombShot.active || g.bombs != 3) return 94;
    in.bomb = 0;
    for (UBYTE tick = 0; tick < 100; tick++) updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    if (g.bombShot.active || g.bombLaunchCooldown) return 95;
    in.bomb = 1; updateCarrierDefence(&g, &in, &prev, &in2, buffers);
    if (!g.bombShot.active || g.bombs != 2) return 96;
    /* Opt-in friendly fire preserves the original carrier damage. */
    g.friendlyFire = 1;
    /* From either direction, fixed-height rockets meet the island. */
    g.rocketHeightLock=0;
    g.defence.hull = 100;
    for (UBYTE side = 0; side < 2; side++) {
        carrierDefenceLaunch(&g, &g.rocketShot, side ? 136 : 80, 100,
            side ? MAVERICK_DIRECTION_LEFT : MAVERICK_DIRECTION_RIGHT);
        for (UBYTE tick = 0; tick < 15 && g.rocketShot.active; tick++) carrierDefenceShot(&g, &g.rocketShot);
        if (g.rocketShot.active || g.defence.hull != 100 - (side + 1) * CARRIER_ROCKET_HULL_DAMAGE) return 97;
    }
    carrierDefenceLaunch(&g, &g.rocketShot, 80, 80, MAVERICK_DIRECTION_RIGHT);
    for (UBYTE tick = 0; tick < 20; tick++) carrierDefenceShot(&g, &g.rocketShot);
    if (!g.rocketShot.active || g.defence.hull != 84) return 98;
    UBYTE ejectResult=referenceMissileDamageEjectMatch();
    if(ejectResult) return 220+ejectResult;
    UBYTE gunneryResult = referenceCarrierGunneryMatches();
    if (!gunneryResult) gunneryResult = referenceCarrierRevisionMatches();
    if (!gunneryResult) gunneryResult = referenceLandingVtolMatches();
    if (!gunneryResult) gunneryResult = referenceCarrierSubmarineMatches();
    if (!gunneryResult) gunneryResult = referenceCarrierBomberEdges();
    if (!gunneryResult) gunneryResult = referenceCarrierBlitterMatches();
    modPlaying = oldMod; return gunneryResult;
}
#endif
