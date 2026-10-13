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
 keep->speed=g->speedLevel;keep->wingActive=g->wingman.active;
}
/* The boss uses a local camera origin, but aircraft must not teleport when
 * entering it from the campaign. Ease its relative ship speed from the
 * actual terrain scrolling speed instead of accelerating from rest. */
static void campaignBattleshipArrival(GameState* g,const CampaignCarry* keep) {
 g->playerX=keep->playerX;g->playerY=keep->playerY;g->speedLevel=keep->speed;
 g->landingState=LANDING_STATE_NONE;g->lowSpeedLanding=0;
 battleship.velocity16=scrollPixelsForSpeedLevel(keep->speed)*16;
 g->wingman.active=keep->wingActive && !keep->wingmanDestroyed;
 g->wingman.interceptScreenX=keep->wingX;g->wingman.screenY=keep->wingY;
}
static void campaignRestore(GameState* g,const CampaignCarry* keep) {
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
static void campaignApplyScene(GameState* g,UBYTE scene) {
 g->defence.phase=0;
 if(scene==CAMPAIGN_CARRIER) {
  UBYTE mission=g->missionNumber;if(mission<2)g->missionNumber=2;
  carrierBeginDefence(g);g->missionNumber=mission;
  g->takeoffState=TAKEOFF_STATE_READY;g->scrollX=0;setTakeoffDeckPosition(g);
 } else if(scene==CAMPAIGN_OIL) oilBeginApproach(g);
 else if(scene==CAMPAIGN_BATTLESHIP) {battleship.finished=0;battleshipBegin(g);}
 else if(scene==CAMPAIGN_LANDING || scene==CAMPAIGN_TERRAIN || scene==CAMPAIGN_RETURN) {
  g->takeoffState=TAKEOFF_STATE_AIRBORNE;g->playerFrigateStatus=PLAYER_FRIGATE_STATUS_CLEAR;
  g->playerX=80;g->playerY=56;g->respawnSafeTimer=75;
  g->scrollX=scene==CAMPAIGN_LANDING?LANDING_HOVER_SCROLL_X:CPC_LAND_PROCEDURAL_WORLD_START*8;
  if(scene==CAMPAIGN_RETURN) g->scrollX=LANDING_APPROACH_SCROLL_X-192;
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
 g.defence.hull=20;g.defence.cargo=20;campaignSave(&keep,&g);
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
 if(g.scrollX!=LANDING_APPROACH_SCROLL_X-192 || g.landingState!=LANDING_STATE_NONE ||
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
 memset(&campaign,0,sizeof(campaign));
 return 0;
}
#endif
