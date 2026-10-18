#if HAR_HEADLESS_OIL_TEST
static UBYTE oilContract(void) {
 static GameState g;
 UBYTE* buffers[GAME_WORLD_BUFFER_COUNT]={0};
 InputState in={0},old={0};Player2InputState p2={0};
 UBYTE savedMod=modPlaying;modPlaying=1;
#if HAR_HEADLESS_OIL_RESCUE_TEST
 /* Every visible pad pixel accepts the pilot, including its right edge. */
 for(WORD px=OIL_PAD_LEFT;px<OIL_PAD_RIGHT;px++) {
  initGameState(&g,12040,12040,2);g.gameMode=GAME_MODE_ENHANCED;
  g.lives=3;g.defence.hull=63;g.defence.cargo=40;
  oilBegin(&g);g.defence.hull=37;
  g.ejectState=2;g.ejectTimer=2;
  g.ejectX=px-HAR_CPC_PARACHUTE_WIDTH/2;
  g.ejectY=OIL_DECK_Y-HAR_CPC_PARACHUTE_HEIGHT-1;
  oilUpdate(&g,&in,&old,&p2,buffers);
  if(!oil.rescueRestart || g.ejectState || g.gameOver || g.lives!=2)return 70;
  if(oil.carrierHull!=63 || g.defence.cargo!=40 || g.missionNumber!=2)return 71;
  if(oil.completed || g.takeoffState!=TAKEOFF_STATE_AIRBORNE)return 72;
 }
 /* Landing waits for the wreck animation, then charges exactly one life. */
 oil.rescueRestart=0;g.lives=3;g.ejectState=3;g.crashTimer=1;
 if(updatePlayerEject(&g)!=EJECT_UPDATE_NONE || g.lives!=3)return 73;
 g.crashTimer=0;
 if(updatePlayerEject(&g)!=EJECT_UPDATE_CARRIER_RESTART || g.lives!=2)return 74;
 if(updatePlayerEject(&g)!=EJECT_UPDATE_NONE || g.lives!=2)return 75;
 /* Water beside the helipad is not a rescue surface. */
 for(UBYTE side=0;side<2;side++) {
  initGameState(&g,12040,12040,2);g.gameMode=GAME_MODE_ENHANCED;g.lives=3;oilBegin(&g);
  g.ejectState=2;g.ejectTimer=2;
  g.ejectX=(side?OIL_PAD_RIGHT:OIL_PAD_LEFT-1)-HAR_CPC_PARACHUTE_WIDTH/2;
  g.ejectY=SEA_SURFACE_Y-HAR_CPC_PARACHUTE_HEIGHT-1;
  updatePlayerEject(&g);
  if(!g.gameOver || oil.rescueRestart)return 76;
 }
 memset(&oil,0,sizeof(oil));modPlaying=savedMod;return 0;
#endif
 initGameState(&g,12040,12040,1);g.gameMode=GAME_MODE_ENHANCED;
 g.skillLevel=g.levelDifficulty=1;g.lives=3;g.wingmanControl=WINGMAN_CONTROL_PLAYER2;
 g.wingman.destroyed=0;oilBegin(&g);
 if(!oil.p2Present || oilQuota(1,0)!=7 || oilQuota(1,1)!=9 || oilQuota(1,2)!=11 || oilQuota(5,2)!=19)return 1;
 /* Two live Harriers, attached sprite data, neutral preserves momentum. */
 static UWORD a[20],b[20];updateWingmanSprite(a,b,&g);
 if(!(b[1]&0x80))return 2;
 oil.vx=256;WORD x=g.wingman.interceptScreenX;oilP2Update(&g,&p2);
 if(g.wingman.interceptScreenX!=x+1 || oil.vx!=256)return 3;
 oil.p2Safe=0;oilDamageP2(&g,50);if(oil.p2Health!=50 || g.lives!=3)return 4;
 oil.p2Safe=0;oilDamageP2(&g,50);if(oil.p2Present || g.lives!=2 || !oil.p2Respawn)return 5;
 for(UWORD t=0;t<75;t++)oilP2Update(&g,&p2);
 if(!oil.p2Present || oil.p2Health!=100 || g.wingman.screenY!=8)return 6;
 g.lives=1;oil.p2Safe=0;oilDamageP2(&g,100);
 if(oil.p2Present || oil.p2Respawn || g.lives!=1)return 7;
 g.defence.landed=1;
 /* Optional P2 touchdown still supports servicing during combat. */
 oil.p2Present=1;oil.p2Landed=0;
 g.wingman.active=1;g.wingman.screenY=OIL_PLAYER_Y-1;g.wingman.interceptScreenX=80;
 oil.vy=256;oil.vx=0;oil.subY=oil.ay=0;oilP2Update(&g,&p2);
 if(!oil.p2Landed || g.wingman.interceptScreenX!=80)return 10;
 /* Shared kits repair only the platform, after a complete landing interval. */
 g.defence.hull=40;g.defence.cargo=20;g.defence.phase=DEFENCE_WAVE;
 g.defence.spawnDelay=200;g.defence.quota=8;g.defence.clock=7;
 g.playerX=66;g.playerY=OIL_PLAYER_Y;g.defence.landed=1;
 oilUpdate(&g,&in,&old,&p2,buffers);
 if(g.defence.hull!=40 || g.defence.cargo!=20)return 11;
 UBYTE savedCarrierHull=oil.carrierHull;
 for(UWORD t=1;t<CARRIER_REPAIR_TICKS;t++)oilUpdate(&g,&in,&old,&p2,buffers);
 if(g.defence.hull!=50 || g.defence.cargo || oil.carrierHull!=savedCarrierHull)return 58;
 g.defence.hull=95;g.defence.cargo=40;g.defence.landed=0;
 for(UWORD t=0;t<CARRIER_REPAIR_TICKS;t++)carrierServiceRepair(&g,0);
 if(g.defence.hull!=100 || g.defence.cargo!=20)return 59; /* P2 on pad. */
 for(UWORD t=0;t<CARRIER_REPAIR_TICKS;t++)carrierServiceRepair(&g,0);
 if(g.defence.cargo!=20)return 60; /* Never waste kits at full health. */
 oil.p2Landed=0;g.defence.hull=40;
 for(UWORD t=0;t<CARRIER_REPAIR_TICKS;t++)carrierServiceRepair(&g,0);
 if(g.defence.hull!=40 || g.defence.cargo!=20)return 61;
 /* Wheel contact follows the visible top edge, not an unrelated carrier Y. */
 for(WORD px=OIL_PAD_LEFT;px<OIL_PAD_RIGHT;px++)
  if(!oilHullOverlap(px,OIL_DECK_Y,1,1))return 41;
 if(!oilHullOverlap(80,SEA_SURFACE_Y+2,1,1))return 42;
 g.defence.landed=0;g.playerX=80;g.playerY=OIL_PLAYER_Y-1;
 carrierResetVtol(&g.defence);g.defence.vtolVY=256;oilPlayerMove(&g,&in);
 if(!g.defence.landed || g.playerX!=80 || g.playerY!=OIL_PLAYER_Y)return 43;
 g.defence.landed=0;g.playerY=OIL_PLAYER_Y-1;g.armour=100;
 g.flakDamageCount=g.missileDamageThirds=0;g.defence.vtolVY=384;
 oilPlayerMove(&g,&in);
 /* Easiest scales 90 thirds to 59; the HUD rounds remaining armour up. */
 if(g.defence.landed || g.armour!=81 || g.missileDamageThirds!=59 || g.defence.vtolVY>=0)return 44;
 g.playerX=80;g.playerY=OIL_PLAYER_Y;g.defence.landed=1;g.defence.hull=40;
 /* Enemy bombs hit a solid rig tile above the former carrier deck. */
 WeaponState* bomb=&g.defence.bombs[0];memset(bomb,0,sizeof(*bomb));
 bomb->active=1;bomb->worldX=bomb->x=80;bomb->y=OIL_DECK_Y;
 g.playerX=20;g.playerY=20;g.defence.landed=0;
 carrierDefenceBombs(&g);if(bomb->active || g.defence.hull>=40)return 12;
 /* Actual renderer survives dirty-tile restoration byte for byte. */
 UBYTE* bitmap=AllocMem(GAME_WORLD_BITMAP_BYTES,MEMF_ANY|MEMF_CLEAR);
 if(!bitmap)return 13;
 if(oilPlatformTiles[4])return 27; /* top-left stays transparent */
 static UWORD copper[2048];
 buildGameHudCopper(copper,bitmap,bitmap,(const UWORD*)gamePalette,0,displayByteOffsetForGameState(&g),a,b,a,b,a,b,a,b);
 if(copper[2]!=offsetof(struct Custom,ddfstrt) || copper[3]!=0x38)return 28;
 if(displayByteOffsetForGameState(&g)!=scrollLocalByteOffset(0)+2)return 29;
 /* Platform damage must never replace carrier health in the cockpit. */
 HudRenderState hs={0};oil.carrierHull=83;oil.carrierCargo=40;g.defence.cargo=20;
 drawCarrierInstruments(bitmap,&g,&hs,0);
 if(hs.carrierHull!=83 || hs.carrierCargo!=60)return 30;
 hs.valid=1;g.defence.hull=10;
 drawCarrierInstruments(bitmap,&g,&hs,0);
 if(hs.carrierHull!=83 || hs.carrierCargo!=60)return 31;
 g.helicopterSmoke.active=0;g.defence.clock=32;
 oilDamageEffects(&g,buffers);
 if(!g.helicopterSmoke.active)return 32;
 LONG smokeX=g.helicopterSmoke.worldX;g.defence.clock=64;
 oilDamageEffects(&g,buffers);
 if(g.helicopterSmoke.worldX!=smokeX || g.helicopterSmoke.timer!=1)return 33;
 oil.active=0;drawCarrierInstruments(bitmap,&g,&hs,0);
 if(hs.oilP2Displayed)return 34;
 oil.active=1;oil.carrierHull=100;oil.carrierCargo=0;
 carrierSceneHull=100;oil.fireFrame=0;
 initRingWorldBuffer(bitmap,0);
 ULONG before=0,after=0;
 for(ULONG n=0;n<GAME_WORLD_BITMAP_BYTES;n++)before=before*33+bitmap[n];
 for(WORD row=7;row<15;row++)bobCompositorErase(bitmap,8,row,12);
 for(ULONG n=0;n<GAME_WORLD_BITMAP_BYTES;n++)after=after*33+bitmap[n];
 if(before!=after)return 14;
 /* Right -> front -> left; hovering uses front, wrecks retain side heading. */
 g.helicopter.active=1;g.helicopter.type=0;g.helicopterHits=0;
 g.helicopterAge=1;g.helicopter.direction=1;updateHelicopterHeading(&g);
 if(helicopterVisualFrame(&g)!=0)return 35;
 g.helicopterAge=2;g.helicopter.direction=0;updateHelicopterHeading(&g);
 if(helicopterVisualFrame(&g)!=4)return 36;
 for(UBYTE n=0;n<6;n++)updateHelicopterHeading(&g);
 if(helicopterVisualFrame(&g)!=2)return 37;
 g.helicopter.type=1;g.defence.heliAge=10;
 if(helicopterVisualFrame(&g)!=4)return 38;
 g.helicopterHits=2;if(helicopterVisualFrame(&g)!=2)return 39;
 g.helicopterHits=0;
 /* Both rotor frames restore the background at every sub-byte alignment. */
 for(UBYTE rotor=0;rotor<2;rotor++)for(UBYTE shift=0;shift<8;shift++) {
  g.helicopterAge=rotor*4;g.helicopter.worldX=g.helicopter.x=104+shift;g.helicopter.y=40;
  drawHelicopterBob(bitmap,0,&g);eraseHelicopter(bitmap,0);
  after=0;for(ULONG n=0;n<GAME_WORLD_BITMAP_BYTES;n++)after=after*33+bitmap[n];
  if(before!=after)return 40;
 }
 g.helicopter.active=0;
 FreeMem(bitmap,GAME_WORLD_BITMAP_BYTES);
 oilFinish(&g);if(oil.active || g.defence.phase || g.defence.hull!=100)return 15;
 g.wingmanControl=WINGMAN_CONTROL_CPU;g.wingman.destroyed=0;oilBegin(&g);
 for(UWORD t=0;t<140;t++){g.defence.clock++;oilCpuUpdate(&g);}
 if(oil.cpuState!=2 || !g.wingman.destroyed)return 16;
 /* Bombing removes the gun, then a collected red drop remans it without
  * creating an aircraft. Only a surviving crew earns the departure reward. */
 g.defence.phase=DEFENCE_WAVE;carrierDefenceImpact(&g,108,12);
 if(oil.cpuState || g.wingman.active)return 74;
 g.powerup.active=0;g.defence.repairDropPending=0;
 for(UWORD t=0;t<451;t++)oilCrewSupply(&g);
 if(!g.powerup.active || g.powerup.type!=POWERUP_WINGMAN)return 75;
 g.powerup.logicalY=24;activatePowerup(&g,POWERUP_WINGMAN);g.powerup.active=0;
 if(oil.cpuState!=1 || g.wingman.active || !g.wingman.destroyed)return 76;
 for(UWORD t=0;t<160;t++){g.defence.clock++;oilCpuUpdate(&g);}
 if(oil.cpuState!=2)return 77;
 ULONG crewScore=g.missionBaseScore;oilCrewBonus(&g);
 if(g.missionBaseScore!=crewScore+500)return 78;
 oilCrewBonus(&g);if(g.missionBaseScore!=crewScore+500)return 79;
 oilFinish(&g);if(g.wingman.active || !g.wingman.destroyed)return 17;
 /* Drive actual spawn/kill/secure states for all three party sizes. No
  * artificial transition to SECURE, and no unlimited simultaneous enemies. */
 for(UBYTE control=0;control<3;control++) {
  initGameState(&g,12040,12040,1);g.gameMode=GAME_MODE_ENHANCED;
  g.wingmanControl=control;g.wingman.destroyed=0;g.skillLevel=g.levelDifficulty=1;g.lives=3;
  oilBegin(&g);UWORD ticks=0;
  for(;ticks<10000 && g.defence.phase!=DEFENCE_SECURE;ticks++) {
   g.respawnSafeTimer=100;g.playerX=32;g.playerY=16;
   if(g.enemyPlane.active)carrierDefenceKillJet(&g);
   if(g.helicopter.active)carrierDefenceKillHeli(&g);
   memset(g.defence.bombs,0,sizeof(g.defence.bombs));g.enemyMissile.active=0;
   oilUpdate(&g,&in,&old,&p2,buffers);
  }
  if(ticks==10000 || g.defence.spawned!=oilQuota(1,control))return 18+control;
  /* Neither pilot lands or presses Up: victory must depart automatically. */
  if(g.defence.landed || (control==2 && oil.p2Landed))return 21+control;
  for(UWORD t=0;t<512 && oil.active;t++)oilUpdate(&g,&in,&old,&p2,buffers);
  if(oil.active || !oil.completed || g.defence.phase ||
     campaign.request!=CAMPAIGN_RETURN || !campaign.specialDone)return 24+control;
 }
 /* Approach must start outside the rig's view and finish before enemies
  * spawn, preserving each party's aircraft and carrier resources. */
 for(UBYTE control=0;control<3;control++) {
  initGameState(&g,12040,12040,2);g.gameMode=GAME_MODE_ENHANCED;
  g.wingmanControl=control;g.wingman.destroyed=0;g.defence.hull=63;g.defence.cargo=20;
  oilBeginApproach(&g);
  if(!oil.approach || OIL_ORIGIN_X+GAME_WORLD_SCROLL_PAGE_BYTES*8-g.scrollX<=320 ||
     playerLandingLampVisual(&g)!=COCKPIT_INDICATOR_OFF)return 50;
  UBYTE sawWheels=0,sawVtol=0;
  UWORD ticks=0;
  for(;ticks<512 && oil.approach;ticks++) {
   UWORD oldScroll=g.scrollX;
   oilUpdate(&g,&in,&old,&p2,buffers);
   if(g.enemyPlane.active || g.helicopter.active || g.defence.spawned)return 51;
   if(oil.approach && (g.scrollX<oldScroll || g.scrollX-oldScroll>2))return 52;
   sawWheels|=playerLandingLampVisual(&g)==COCKPIT_INDICATOR_YELLOW;
   sawVtol|=playerLandingLampVisual(&g)==COCKPIT_INDICATOR_RED;
  }
  if(ticks==512 || !sawWheels || !sawVtol || g.scrollX || g.speedLevel ||
     g.defence.phase!=DEFENCE_ALARM || g.defence.phaseTicks!=150 ||
     oil.carrierHull!=63 || oil.carrierCargo || g.defence.cargo!=20)return 53;
  if((control==WINGMAN_CONTROL_PLAYER2)!=!!g.wingman.active)return 54;
 }

 /* Platform loss ends the mission, but must not freeze a healthy aircraft. */
 initGameState(&g,12040,12040,2);g.gameMode=GAME_MODE_ENHANCED;oilBegin(&g);
 g.playerX=32;g.playerY=16;g.defence.phase=DEFENCE_SINKING;g.defence.hull=0;g.defence.phaseTicks=0;
 g.defence.vtolVX=256;in.right=1;
 oilUpdate(&g,&in,&old,&p2,buffers);
 if(g.playerX<=32 || g.gameOver || g.defence.phaseTicks!=1)return 55;
 /* Invulnerability also remains controllable after fuel runs out. */
 UBYTE savedInvulnerable=debugInvulnerable;debugInvulnerable=1;g.fuel=0;
 WORD beforeX=g.playerX;oilUpdate(&g,&in,&old,&p2,buffers);
 debugInvulnerable=savedInvulnerable;
 if(g.playerX<=beforeX || g.aircraftFailureState)return 56;
 g.defence.phaseTicks=180;oilUpdate(&g,&in,&old,&p2,buffers);
 if(!g.gameOver)return 57;
 /* Platform-only resilience, without the separate Easy damage reduction. */
 g.skillLevel=3;
 oil.active=1;g.defence.phase=DEFENCE_WAVE;g.defence.hull=100;
 carrierDefenceImpact(&g,112,CARRIER_BOMB_HULL_DAMAGE);
 if(g.defence.hull!=92)return 62;
 carrierDefenceImpact(&g,112,CARRIER_HEAVY_BOMB_HULL_DAMAGE);
 if(g.defence.hull!=76)return 63;
 carrierDefenceImpact(&g,112,CARRIER_ROCKET_HULL_DAMAGE);
 if(g.defence.hull!=70)return 64;
 oil.active=0;g.defence.hull=100;
 carrierDefenceImpact(&g,112,CARRIER_BOMB_HULL_DAMAGE);
 if(g.defence.hull!=88)return 65;
 memset(&oil,0,sizeof(oil));modPlaying=savedMod;return 0;
}
#endif
