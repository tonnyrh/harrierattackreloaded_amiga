/* Preserve run resources explicitly; transient projectiles and render caches
 * belong to the departing scene and must never survive a scene jump. */
typedef struct {
 __typeof__(((GameState*)0)->score) score;
 __typeof__(((GameState*)0)->bonusScore) bonusScore;
 __typeof__(((GameState*)0)->missionStartScore) missionStartScore;
 __typeof__(((GameState*)0)->scoreFraction) scoreFraction;
 __typeof__(((GameState*)0)->hitsCount) hitsCount;
 __typeof__(((GameState*)0)->lives) lives;
 __typeof__(((GameState*)0)->armour) armour;
 __typeof__(((GameState*)0)->missileDamageThirds) missileDamageThirds;
 __typeof__(((GameState*)0)->flakDamageCount) flakDamageCount;
 __typeof__(((GameState*)0)->fuel) fuel;
 __typeof__(((GameState*)0)->fuelGaugeLevel) fuelGaugeLevel;
 __typeof__(((GameState*)0)->fuelSubCounter) fuelSubCounter;
 __typeof__(((GameState*)0)->fuelClockAccumulator) fuelClockAccumulator;
 __typeof__(((GameState*)0)->rockets) rockets;
 __typeof__(((GameState*)0)->bombs) bombs;
 __typeof__(((GameState*)0)->extraAircraftBonusSpawned) extraAircraftBonusSpawned;
 __typeof__(((GameState*)0)->wingmanReviveDelayColumns) wingmanReviveDelayColumns;
 UBYTE hull,cargo,guns[2],wingmanDestroyed;
 WORD playerX,playerY,wingX,wingY;
 UBYTE speed,wingActive;
 UWORD scroll,sky[7];
 UBYTE palette[3],fade[4];
} CampaignCarry;
static void campaignSave(CampaignCarry* keep,const GameState* g) {
 keep->score=g->score;
 keep->bonusScore=g->bonusScore;
 keep->missionStartScore=g->missionStartScore;
 keep->scoreFraction=g->scoreFraction;
 keep->hitsCount=g->hitsCount;
 keep->lives=g->lives;
 keep->armour=g->armour;
 keep->missileDamageThirds=g->missileDamageThirds;
 keep->flakDamageCount=g->flakDamageCount;
 keep->fuel=g->fuel;
 keep->fuelGaugeLevel=g->fuelGaugeLevel;
 keep->fuelSubCounter=g->fuelSubCounter;
 keep->fuelClockAccumulator=g->fuelClockAccumulator;
 keep->rockets=g->rockets;
 keep->bombs=g->bombs;
 keep->extraAircraftBonusSpawned=g->extraAircraftBonusSpawned;
 keep->wingmanReviveDelayColumns=g->wingmanReviveDelayColumns;
 keep->hull=oil.active?oil.carrierHull:g->defence.hull;
 UWORD cargo=g->defence.cargo+(oil.active?oil.carrierCargo:0);
 keep->cargo=cargo>180?180:cargo;
 memcpy(keep->guns,oil.active?oil.carrierGuns:g->defence.gunHealth,2);
 keep->wingmanDestroyed=g->wingman.destroyed;
 keep->playerX=g->playerX;keep->playerY=g->playerY;
 keep->wingX=wingmanScreenX(g);keep->wingY=g->wingman.screenY;
 keep->speed=g->speedLevel;keep->wingActive=g->wingman.active;keep->scroll=g->scrollX;
 keep->sky[0]=currentSkyTopRgb;keep->sky[1]=currentSkyMidRgb;keep->sky[2]=currentSkyLowRgb;
 keep->sky[3]=currentCloudTopRgb;keep->sky[4]=currentLandRgb;keep->sky[5]=currentSeaLowRgb;keep->sky[6]=currentPanelSeaRgb;
 keep->palette[0]=currentMissionOpeningPalettePhase;keep->palette[1]=currentMissionFlightPalettePhase;keep->palette[2]=currentMissionTownPalettePhase;
 keep->fade[0]=g->takeoffPaletteFadeStep;keep->fade[1]=g->takeoffPaletteFadeTimer;keep->fade[2]=g->cityFadeStep;keep->fade[3]=g->cityFadeTimer;
}
/* The boss uses a local camera origin, but aircraft must not teleport when
 * entering it from the campaign. Ease its relative ship speed from the
 * actual terrain scrolling speed instead of accelerating from rest. */
static void campaignBattleshipArrival(GameState* g,const CampaignCarry* keep) {
 g->playerX=keep->playerX;g->playerY=keep->playerY;g->speedLevel=keep->speed;
 g->landingState=LANDING_STATE_NONE;g->lowSpeedLanding=0;
 /* Sea phase is independent of the ship camera; avoid a visible wave jump. */
 battleship.water=keep->scroll&255;
 /* Only 96 pixels of open sea remain before the stern enters view. */
 battleship.camera16=96*16;g->scrollX=96;
 battleship.velocity16=scrollPixelsForSpeedLevel(keep->speed)*16;
 g->wingman.active=keep->wingActive && !keep->wingmanDestroyed;
 g->wingman.interceptScreenX=keep->wingX;g->wingman.screenY=keep->wingY;
}
static void campaignOceanArrival(GameState* g,const CampaignCarry* keep,UBYTE scene) {
 g->playerX=keep->playerX;g->playerY=keep->playerY;
 g->speedLevel=keep->speed;
 /* Keep sub-byte position and the 32-column sea pattern in phase. */
 g->scrollX-=(g->scrollX-keep->scroll)&255;
 if(scene==CAMPAIGN_OIL)oil.approachVX16=scrollPixelsForSpeedLevel(keep->speed)*16;
 if(g->wingman.active) {g->wingman.interceptScreenX=keep->wingX;g->wingman.screenY=keep->wingY;}
}
static void campaignRestore(GameState* g,const CampaignCarry* keep) {
 currentSkyTopRgb=keep->sky[0];currentSkyMidRgb=keep->sky[1];currentSkyLowRgb=keep->sky[2];
 currentCloudTopRgb=keep->sky[3];currentLandRgb=keep->sky[4];currentSeaLowRgb=keep->sky[5];currentPanelSeaRgb=keep->sky[6];
 currentMissionOpeningPalettePhase=keep->palette[0];currentMissionFlightPalettePhase=keep->palette[1];currentMissionTownPalettePhase=keep->palette[2];
 g->takeoffPaletteFadeStep=keep->fade[0];g->takeoffPaletteFadeTimer=keep->fade[1];g->cityFadeStep=keep->fade[2];g->cityFadeTimer=keep->fade[3];
 g->score=keep->score;
 g->bonusScore=keep->bonusScore;
 g->missionStartScore=keep->missionStartScore;
 g->scoreFraction=keep->scoreFraction;
 g->hitsCount=keep->hitsCount;
 g->lives=keep->lives;
 g->armour=keep->armour;
 g->missileDamageThirds=keep->missileDamageThirds;
 g->flakDamageCount=keep->flakDamageCount;
 g->fuel=keep->fuel;
 g->fuelGaugeLevel=keep->fuelGaugeLevel;
 g->fuelSubCounter=keep->fuelSubCounter;
 g->fuelClockAccumulator=keep->fuelClockAccumulator;
 g->rockets=keep->rockets;
 g->bombs=keep->bombs;
 g->extraAircraftBonusSpawned=keep->extraAircraftBonusSpawned;
 g->wingmanReviveDelayColumns=keep->wingmanReviveDelayColumns;
 g->defence.hull=keep->hull;g->defence.cargo=keep->cargo;
 memcpy(g->defence.gunHealth,keep->guns,2);
 g->wingman.destroyed=keep->wingmanDestroyed;
 carrierParkedWingmanVisible=g->wingmanControl!=WINGMAN_CONTROL_OFF && !keep->wingmanDestroyed;
 if(keep->wingmanDestroyed) {g->wingman.active=0;g->wingman.mode=WINGMAN_DESTROYED;}
}
/* Let existing attackers leave the ocean corridor before replacing its
 * actor pools. Debug scene jumps need not wait for the natural coast. */
static UBYTE campaignCanChangeScene(const GameState* g) {
 if((campaign.request==CAMPAIGN_OIL || campaign.request==CAMPAIGN_BATTLESHIP) &&
    g->scrollX>=LANDING_APPROACH_SCROLL_X && !campaign.specialDone && !campaign.cheated)
  return !g->enemyPlane.active && !g->enemyMissile.active && !g->helicopter.active &&
         !g->siloMissile.active && !g->crashTimer && !g->ejectState;
 return 1;
}
static void campaignApplyScene(GameState* g,UBYTE scene) {
 g->defence.phase=0;
 campaign.oceanReturn=scene==CAMPAIGN_RETURN;
 if(campaign.oceanReturn)campaign.oilRoute=0;
 if(scene==CAMPAIGN_CARRIER) {
  UBYTE mission=g->missionNumber;if(mission<2)g->missionNumber=2;
  carrierBeginDefence(g);g->missionNumber=mission;
  g->takeoffState=TAKEOFF_STATE_READY;g->scrollX=0;setTakeoffDeckPosition(g);
 } else if(scene==CAMPAIGN_OIL) oilBeginApproach(g);
 else if(scene==CAMPAIGN_BATTLESHIP) {battleship.finished=0;battleshipBegin(g);}
 else if(scene==CAMPAIGN_LANDING || scene==CAMPAIGN_TERRAIN || scene==CAMPAIGN_RETURN) {
  /* The surviving Wingman arrives as a live aircraft. Do not bake a
   * second copy into the destination carrier during lazy world loading. */
  carrierParkedWingmanVisible=0;
  g->takeoffState=TAKEOFF_STATE_AIRBORNE;g->playerFrigateStatus=PLAYER_FRIGATE_STATUS_CLEAR;
  g->playerX=80;g->playerY=56;g->respawnSafeTimer=75;
  g->scrollX=scene==CAMPAIGN_LANDING?LANDING_HOVER_SCROLL_X:CPC_LAND_PROCEDURAL_WORLD_START*8;
  if(scene==CAMPAIGN_RETURN) g->scrollX=LANDING_APPROACH_SCROLL_X-320;
  g->landingState=scene==CAMPAIGN_LANDING?LANDING_STATE_HOVER:LANDING_STATE_NONE;
  g->speedLevel=scene==CAMPAIGN_LANDING?0:GAME_SPEED_LEVEL_DEFAULT;
  g->lowSpeedLanding=scene==CAMPAIGN_LANDING;g->defence.landed=0;carrierResetVtol(&g->defence);
  g->defence.facing=MAVERICK_DIRECTION_RIGHT;g->defence.vtolPose=1;
  if(g->wingmanControl!=WINGMAN_CONTROL_OFF && !g->wingman.destroyed) {
   g->wingman.active=1;g->wingman.screenY=40;g->wingman.interceptScreenX=48;
   g->wingman.mode=g->wingmanControl==WINGMAN_CONTROL_PLAYER2?WINGMAN_PLAYER2_FLIGHT:WINGMAN_FORMATION;
  }
 } else {g->takeoffState=TAKEOFF_STATE_READY;g->scrollX=0;setTakeoffDeckPosition(g);}
}

#if HAR_HEADLESS_CAMPAIGN_TEST
static UBYTE campaignContract(void) {
 static GameState g;InputState neutral={0};modPlaying=1;
#if HAR_HEADLESS_CAMPAIGN_WINGMAN_TEST
 static const UBYTE scenes[]={CAMPAIGN_TERRAIN,CAMPAIGN_LANDING,CAMPAIGN_RETURN};
 for(UBYTE control=0;control<3;control++)for(UBYTE dead=0;dead<2;dead++)
 for(UBYTE n=0;n<3;n++) {
  initGameState(&g,123,123,2);g.gameMode=GAME_MODE_ENHANCED;
  g.wingmanControl=control;g.wingman.destroyed=dead;
  g.wingman.active=control && !dead;
  CampaignCarry keep;campaignSave(&keep,&g);
  /* Match the session reset before restore/apply in the live handoff. */
  g.wingman.active=0;g.wingman.mode=WINGMAN_ON_DECK;
  campaignRestore(&g,&keep);campaignApplyScene(&g,scenes[n]);
  if(carrierParkedWingmanVisible)return 80;
  if(!!g.wingman.active!=!!(control && !dead))return 81;
  if(g.wingman.active && g.wingman.mode!=(control==WINGMAN_CONTROL_PLAYER2?WINGMAN_PLAYER2_FLIGHT:WINGMAN_FORMATION))return 82;
 }
 return 0;
#endif

 /* Still-frame extraction must preserve every pixel at all 16 scroll phases. */
 UBYTE source[44],copy[40];for(UWORD i=0;i<44;i++)source[i]=(i*71)^0xa5;
 for(UWORD skip=0;skip<=16;skip++) {
  campaignCopyVisibleRow(copy,source,skip);
  for(UWORD x=0;x<320;x++)if(!!(copy[x/8]&(128>>(x&7))) !=
    !!(source[(x+skip)/8]&(128>>((x+skip)&7))))return 23;
 }

 for(UBYTE m=1;m<=12;m++)if(campaignSortie(m)!=((m-1)%3)+1)return 1;
 if(cpcDifficultyForMission(1,4)<=cpcDifficultyForMission(1,1))return 2;
 initGameState(&g,123,123,2);g.gameMode=GAME_MODE_ENHANCED;g.missionNumber=2;g.levelDifficulty=2;
 g.takeoffState=TAKEOFF_STATE_AIRBORNE;g.respawnSafeTimer=0;
 g.playerX=70;g.playerY=TAKEOFF_PLAYER_DECK_Y-1;g.defence.vtolVY=384;
 carrierParkedWingmanVisible=0;carrierMoveVtol(&g,&neutral);
 if(g.armour!=70 || g.defence.landed || g.defence.vtolVY>=0 || g.playerY>=TAKEOFF_PLAYER_DECK_Y)return 3;
 carrierMoveVtol(&g,&neutral);if(g.armour!=70)return 4;
 g.playerY=TAKEOFF_PLAYER_DECK_Y-1;g.defence.vtolVY=256;g.defence.vtolSubY=0;
 carrierMoveVtol(&g,&neutral);if(!g.defence.landed || g.armour!=70)return 5;
 g.score=12345;g.lives=2;g.rockets=7;g.bombs=5;g.defence.hull=63;g.defence.cargo=40;
 g.defence.gunHealth[0]=1;g.defence.gunHealth[1]=0;g.wingman.destroyed=1;
 CampaignCarry keep;campaignSave(&keep,&g);
 campaignApplyScene(&g,CAMPAIGN_OIL);
 if(!oil.active || oil.carrierHull!=63 || g.defence.hull!=100)return 6;
 g.defence.hull=20;g.defence.cargo+=20;campaignSave(&keep,&g);
 if(keep.hull!=63 || keep.cargo!=60 || keep.guns[0]!=1 || keep.guns[1]!=0)return 7;
 oilFinish(&g);oil.scene=0;campaign.request=0;
 if(g.defence.hull!=63 || g.defence.cargo!=60 || g.defence.gunHealth[0]!=1)return 8;
 campaignRestore(&g,&keep);campaignApplyScene(&g,CAMPAIGN_BATTLESHIP);
 if(!battleship.active || g.wingman.active || g.lives!=2 || g.score!=12345 || g.rockets!=7 || g.bombs!=5)return 9;
 battleship.active=0;campaignRestore(&g,&keep);campaignApplyScene(&g,CAMPAIGN_LANDING);
 if(g.landingState!=LANDING_STATE_HOVER || g.defence.hull!=63 || g.defence.phase)return 10;
 g.helicopter.active=1;g.helicopter.type=0;g.helicopter.x=0;g.helicopter.y=24;g.helicopter.direction=1;
 g.helicopterAge=0;g.defence.phase=0;carrierDefenceAircraft(&g);
 if(g.helicopter.x!=1 || !g.helicopter.direction || helicopterVisualFrame(&g)!=0)return 11;
 g.helicopter.type=1;g.defence.phase=DEFENCE_SECURE;g.defence.heliAge=10;g.helicopterAge=8;
 if(helicopterVisualFrame(&g)<4)return 12;
 g.defence.heliAge=230;g.helicopter.direction=0;carrierDefenceAircraft(&g);
 if(g.helicopter.x!=0 || g.helicopter.direction || !g.helicopterTurnTicks)return 13;
 /* Camera overtakes a forward-moving land helicopter; it must face right.
  * Stopping at the city retains heading; actual reverse motion turns left. */
 g.helicopterHits=0;g.helicopterAge=1;g.helicopter.direction=0;
 g.helicopter.worldX=102;g.helicopter.x=40;
 updateHelicopterTravelHeading(&g,100);
 if(!g.helicopter.direction || helicopterVisualFrame(&g)!=0)return 14;
 g.helicopter.x=37;updateHelicopterTravelHeading(&g,102);
 if(!g.helicopter.direction)return 15;
 g.helicopter.worldX=101;updateHelicopterTravelHeading(&g,102);
 if(g.helicopter.direction)return 16;

 memset(&oil,0,sizeof(oil));battleship.active=0;memset(&campaign,0,sizeof(campaign));
 g.helicopter.active=0;g.scrollX=LANDING_APPROACH_SCROLL_X;g.missionComplete=g.gameOver=g.crashTimer=g.ejectState=0;
 g.playerFrigateStatus=PLAYER_FRIGATE_STATUS_CLEAR;g.missionNumber=2;
 updateLandingApproach(&g);if(campaign.request!=CAMPAIGN_OIL)return 14;
 campaign.request=0;g.missionNumber=3;updateLandingApproach(&g);if(campaign.request!=CAMPAIGN_BATTLESHIP)return 15;
 campaign.request=0;campaign.specialDone=1;updateLandingApproach(&g);if(campaign.request)return 16;
 campaign.specialDone=0;g.crashTimer=10;updateLandingApproach(&g);if(campaign.request)return 17;
 g.crashTimer=0;g.gameMode=GAME_MODE_CLASSIC;updateLandingApproach(&g);if(campaign.request)return 18;
 g.gameMode=GAME_MODE_ENHANCED;campaign.specialDone=1;
 campaignApplyScene(&g,CAMPAIGN_RETURN);
 if(g.scrollX!=LANDING_APPROACH_SCROLL_X-320 || g.landingState!=LANDING_STATE_NONE ||
    g.takeoffState!=TAKEOFF_STATE_AIRBORNE || !g.speedLevel || g.defence.phase)return 19;
 g.scrollX=LANDING_APPROACH_SCROLL_X;updateLandingApproach(&g);
 if(campaign.request || g.landingState==LANDING_STATE_NONE)return 20;
 g.playerX=172;g.playerY=43;g.speedLevel=5;
 g.wingmanControl=WINGMAN_CONTROL_PLAYER2;g.wingman.destroyed=0;
 g.wingman.active=1;g.wingman.mode=WINGMAN_PLAYER2_FLIGHT;
 g.wingman.interceptScreenX=123;g.wingman.screenY=27;
 campaignSave(&keep,&g);campaignApplyScene(&g,CAMPAIGN_BATTLESHIP);
 campaignBattleshipArrival(&g,&keep);
 if(g.playerX!=172 || g.playerY!=43 || g.speedLevel!=5 ||
    !g.wingman.active || wingmanScreenX(&g)!=123 || g.wingman.screenY!=27 ||
    battleship.velocity16!=48 || g.landingState || g.lowSpeedLanding)return 21;
 for(UBYTE t=0;t<48;t++) {
  WORD oldScroll=g.scrollX,oldVelocity=battleship.velocity16;
  battleshipMoveCamera(&g);
  if(g.scrollX<oldScroll || g.scrollX-oldScroll>3 ||
     battleshipAbs(battleship.velocity16-oldVelocity)>1)return 22;
 }
 battleship.active=0;
 /* Sea handoffs preserve aircraft pose, camera phase and the current sky. */
 g.playerX=96;g.playerY=24;g.scrollX=354;g.speedLevel=5;
 currentSkyTopRgb=0x123;currentSkyMidRgb=0x456;currentSkyLowRgb=0x789;
 g.takeoffPaletteFadeStep=CITY_FADE_STEP_COUNT;g.cityFadeStep=CITY_FADE_STEP_COUNT;
 campaignSave(&keep,&g);resetCityFade(&g);campaignRestore(&g,&keep);
 campaignApplyScene(&g,CAMPAIGN_RETURN);campaignOceanArrival(&g,&keep,CAMPAIGN_RETURN);
 if(g.playerX!=96 || g.playerY!=24 || (g.scrollX&255)!=(354&255) ||
    currentSkyTopRgb!=0x123 || currentSkyMidRgb!=0x456 || currentSkyLowRgb!=0x789)return 27;
 for(LONG col=g.scrollX/8;col<(LANDING_HOVER_SCROLL_X+320)/8;col++) {
  const LevelSegmentDef* sea=levelSegmentForWorldColumn(col);
  if(!sea || sea->terrainKind!=HAR_TERRAIN_SEA || cpcCloudTileAtColumnRow(col,3))return 28;
  RenderColumn water;buildWorldTileColumn(col,&water);
  for(UBYTE row=0;row<15;row++)if(water.tile[row])return 29;
  if(col<663+cpcLandRouteExtension+cpcTownRouteOverflow) {
   ObjectCell cell;objectCellForWorldColumnTile(col,14,&cell);
   if(cell.id!=HAR_OBJ_SKY)return 30;
  }
 }
 updateCityFade(&g);
 if(currentSkyTopRgb!=0x123 || currentSkyMidRgb!=0x456 || currentSkyLowRgb!=0x789)return 31;
 campaign.oceanReturn=0;campaign.oilRoute=1;
 if(campaignOceanColumn(611+cpcLandRouteExtension+cpcTownRouteOverflow) ||
    !campaignOceanColumn(612+cpcLandRouteExtension+cpcTownRouteOverflow))return 32;
 campaignSave(&keep,&g);campaignApplyScene(&g,CAMPAIGN_OIL);campaignOceanArrival(&g,&keep,CAMPAIGN_OIL);
 if(g.playerX!=keep.playerX || g.playerY!=keep.playerY || oil.approachVX16!=48)return 33;
 g.wingman.active=1;g.wingman.interceptScreenX=20;g.wingman.screenY=32;g.defence.clock=2;
 oilApproachUpdate(&g);
 if(g.wingman.interceptScreenX!=21 || g.wingman.screenY!=31)return 35;
 /* Departure must not teleport either aircraft before the scene handoff. */
 g.playerY=24;g.wingman.screenY=16;oil.p2Present=1;oilFinish(&g);
 if(g.playerY!=24 || g.wingman.screenY!=16)return 34;
 /* The quiet transit must retain steering for both players and clamp at sea. */
 g.playerX=100;g.playerY=50;g.wingmanControl=WINGMAN_CONTROL_PLAYER2;g.wingman.active=1;
 g.wingman.interceptScreenX=60;g.wingman.screenY=40;
 InputState transitInput={0};Player2InputState transitP2={0};transitInput.up=1;transitP2.right=1;
 campaignOceanFlight(&g,&transitInput,&transitP2);
 if(g.playerY!=48 || g.playerX!=100 || g.wingman.interceptScreenX!=62)return 36;
 for(UWORD n=0;n<200;n++)campaignOceanFlight(&g,&transitInput,&transitP2);
 if(g.playerY!=8 || g.wingman.interceptScreenX!=296)return 37;
 memset(&campaign,0,sizeof(campaign));
 /* Loss animation and zero-fuel debug mode still dispatch flight input. */
 initGameState(&g,123,123,2);g.gameMode=GAME_MODE_ENHANCED;oilBegin(&g);
 g.playerX=32;g.playerY=16;g.defence.phase=DEFENCE_SINKING;g.defence.hull=0;g.defence.phaseTicks=0;
 g.defence.vtolVX=256;InputState right={0};right.right=1;
 Player2InputState p2={0};UBYTE* noBuffers[GAME_WORLD_BUFFER_COUNT]={0};
 oilUpdate(&g,&right,&neutral,&p2,noBuffers);
 if(g.playerX<=32 || g.gameOver || g.defence.phaseTicks!=1)return 24;
 UBYTE savedInvulnerable=debugInvulnerable;debugInvulnerable=1;g.fuel=0;
 WORD beforeX=g.playerX;oilUpdate(&g,&right,&neutral,&p2,noBuffers);
 debugInvulnerable=savedInvulnerable;
 if(g.playerX<=beforeX || g.aircraftFailureState)return 25;
 g.defence.phaseTicks=180;oilUpdate(&g,&right,&neutral,&p2,noBuffers);
 if(!g.gameOver)return 26;
 memset(&oil,0,sizeof(oil));
 memset(&campaign,0,sizeof(campaign));
 /* Hidden route frigates must consume their triggers without ghost shots.
  * Visible frigates retain the same launch in both game modes. */
 for(UBYTE mode=GAME_MODE_CLASSIC;mode<=GAME_MODE_ENHANCED;mode++) {
  g.gameMode=mode;g.enemyMissile.active=0;g.enemyShipMissileTriggerIndex=1;
  g.scrollX=(harEnemyShipMissileTriggers[1]-GAME_MAP_WIDTH)*8;
  if(!updateEnemyShipMissileTrigger(&g) || !g.enemyMissile.active)return 38;
 }
 g.enemyMissile.active=0;g.enemyShipMissileTriggerIndex=1;campaign.oilRoute=1;
 if(updateEnemyShipMissileTrigger(&g) || g.enemyMissile.active ||
    g.enemyShipMissileTriggerIndex!=HAR_ENEMY_SHIP_TRIGGER_COUNT)return 39;
 campaign.oilRoute=0;
 if(updateEnemyShipMissileTrigger(&g) || g.enemyMissile.active)return 40;
 campaign.oilRoute=1;g.enemyShipMissileTriggerIndex=0;
 g.scrollX=(harEnemyShipMissileTriggers[0]-GAME_MAP_WIDTH)*8;
 if(!updateEnemyShipMissileTrigger(&g) || !g.enemyMissile.active)return 41;
 campaign.oceanReturn=1;g.enemyMissile.active=0;g.enemyShipMissileTriggerIndex=0;
 g.scrollX=(harEnemyShipMissileTriggers[1]-GAME_MAP_WIDTH)*8;
 if(updateEnemyShipMissileTrigger(&g) || g.enemyMissile.active ||
    g.enemyShipMissileTriggerIndex!=HAR_ENEMY_SHIP_TRIGGER_COUNT)return 42;
 memset(&campaign,0,sizeof(campaign));
 g.enemyMissile.active=1;g.enemyMissile.x=77;g.enemyShipMissileTriggerIndex=1;
 if(updateEnemyShipMissileTrigger(&g) || g.enemyMissile.x!=77 ||
    g.enemyShipMissileTriggerIndex!=HAR_ENEMY_SHIP_TRIGGER_COUNT)return 43;
 g.enemyMissile.active=0;
 /* Full-tank endurance exercises real fuel depletion in both modes, all
  * difficulty reserves and VTOL, against the literal 300*1.15 burn rate. */
 UBYTE savedInfiniteFuel=debugInfiniteFuel;debugInfiniteFuel=0;
 for(UBYTE mode=GAME_MODE_CLASSIC;mode<=GAME_MODE_ENHANCED;mode++)
  for(UBYTE difficulty=1;difficulty<=5;difficulty++)
   for(UBYTE vtol=0;vtol<2;vtol++) {
    g.gameMode=mode;g.levelDifficulty=difficulty;g.lowSpeedLanding=vtol;
    resetPlayerFuel(&g);ULONG ticks=0;
    ULONG rate=345UL*(mode==GAME_MODE_ENHANCED && vtol?3:1);
    ULONG capacity=(ULONG)CPC_FUEL_TOTAL_QUANTA*playerFuelClockLimit(&g);
    while(g.fuel && ticks<15000){updatePlayerFuel(&g);ticks++;}
    if(ticks!=(capacity+rate-1)/rate){debugInfiniteFuel=savedInfiniteFuel;return 44;}
   }
 debugInfiniteFuel=savedInfiniteFuel;
 return 0;
}
#endif
