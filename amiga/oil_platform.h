/* Oil-platform development encounter. Shared carrier enemy/weapon pools and
 * VTOL arithmetic; no heap allocation, extra screen, audio bank or sprite RAM. */
static UBYTE oilQuota(UBYTE skill,UBYTE control) {
 if(skill<1) skill=1; if(skill>5) skill=5;
 return 7+(skill-1)*2+(control==WINGMAN_CONTROL_CPU?2:control==WINGMAN_CONTROL_PLAYER2?4:0);
}
static void oilBegin(GameState* g) {
 memset(&oil,0,sizeof(oil)); oil.active=oil.scene=1;
 oil.carrierHull=g->defence.hull;oil.carrierCargo=g->defence.cargo;memcpy(oil.carrierGuns,g->defence.gunHealth,2);
 memset(&g->defence,0,sizeof(g->defence));
 g->defence.cargo=oil.carrierCargo;oil.carrierCargo=0;
 g->defence.hull=100; g->defence.phase=DEFENCE_ALARM;
 g->defence.phaseTicks=150; g->defence.waves=1;
 g->defence.facing=MAVERICK_DIRECTION_RIGHT; g->defence.vtolPose=1;
 g->scrollX=0; g->playerX=52; g->playerY=48;
 g->takeoffState=TAKEOFF_STATE_AIRBORNE; g->speedLevel=0; g->lowSpeedLanding=1;
 carrierParkedWingmanVisible=0; carrierSceneHull=100;
 g->wingman.active=0;
 if(g->wingmanControl==WINGMAN_CONTROL_CPU && !g->wingman.destroyed) {
  oil.cpuState=1;oil.crewX=48;oil.crewY=32;oil.cpuGunPose=3; g->wingman.destroyed=1;
 } else if(g->wingmanControl==WINGMAN_CONTROL_PLAYER2 && !g->wingman.destroyed) {
  oil.p2Present=1; oil.p2Health=100; oil.p2Safe=100; oil.p2FuelQuanta=CPC_FUEL_TOTAL_QUANTA;oil.fuelTicks=0;
  oil.facing=MAVERICK_DIRECTION_RIGHT; oil.pose=1;
  g->wingman.active=1; g->wingman.mode=WINGMAN_PLAYER2_FLIGHT;
  g->wingman.interceptScreenX=24; g->wingman.screenY=32;
 }
}
static UBYTE oilPad(WORD x) { return x+2>=OIL_PAD_LEFT && x+14<=OIL_PAD_RIGHT; }
/* A prebuilt ocean ring lets the rig enter from the right without moving
 * its tiles every frame. At the seam it becomes the existing stationary scene. */
static void oilBeginApproach(GameState* g) {
 oilBegin(g);oil.approach=1;oil.approachVX16=32;
 g->scrollX=GAME_WORLD_SCROLL_PAGE_BYTES*8-352;
 g->playerX=80;g->playerY=48;g->speedLevel=GAME_SPEED_LEVEL_DEFAULT;
 g->lowSpeedLanding=0;
 g->wingman.active=oil.cpuState || oil.p2Present;
 if(g->wingman.active) {
  g->wingman.destroyed=0;g->wingman.mode=WINGMAN_FORMATION;
  g->wingman.interceptScreenX=48;g->wingman.screenY=32;
 }
}
static void oilApproachUpdate(GameState* g) {
 CarrierDefenceState* d=&g->defence;
 UWORD remaining=GAME_WORLD_SCROLL_PAGE_BYTES*8-g->scrollX;
 WORD target=remaining>224?32:remaining>64?16:8;
 if(oil.approachVX16>target)oil.approachVX16--;else if(oil.approachVX16<target)oil.approachVX16++;
 oil.approachSub16+=oil.approachVX16;
 UWORD step=oil.approachSub16/16;oil.approachSub16&=15;
 if(step>remaining)step=remaining;
 g->scrollX+=step;
 g->speedLevel=remaining>224?GAME_SPEED_LEVEL_DEFAULT:remaining>64?1:0;
 g->lowSpeedLanding=remaining<=288;
 d->fullVtol=remaining<=256;
 if(!(d->clock&1)) {
  if(g->playerX>52)g->playerX--;else if(g->playerX<52)g->playerX++;
  if(g->playerY>48)g->playerY--;else if(g->playerY<48)g->playerY++;
 }
 if(g->wingman.active && !(d->clock&1)) {
  WORD targetX=g->playerX-24,targetY=g->playerY-16;
  if(g->wingman.interceptScreenX<targetX)g->wingman.interceptScreenX++;
  else if(g->wingman.interceptScreenX>targetX)g->wingman.interceptScreenX--;
  if(g->wingman.screenY<targetY)g->wingman.screenY++;
  else if(g->wingman.screenY>targetY)g->wingman.screenY--;
 }
 updatePlayerFuel(g);updateEngineSound(engineSpeedForSpeedLevel(g->speedLevel),g->lowSpeedLanding);
 if(g->scrollX>=GAME_WORLD_SCROLL_PAGE_BYTES*8) {
  oil.approach=0;g->scrollX=0;g->speedLevel=0;g->lowSpeedLanding=1;
  carrierResetVtol(d);d->fullVtol=1;d->phaseTicks=150;
  g->wingman.active=oil.p2Present;
  if(oil.cpuState){g->wingman.destroyed=1;oil.crewX=g->wingman.interceptScreenX;oil.crewY=g->wingman.screenY;}
  if(oil.p2Present)g->wingman.mode=WINGMAN_PLAYER2_FLIGHT;
 }
}
static UBYTE oilAircraftHitsStructure(WORD x,WORD y) {
 /* The helipad rail is behind an approaching aircraft, not a wall across
  * its landing path. Deck contact below is handled by the landing sweep. */
 return (!oilPad(x) || y>OIL_PLAYER_Y) && oilHullOverlap(x+2,y,12,7);
}
static void oilDamageP2(GameState* g,UBYTE amount) {
 if(!oil.p2Present || oil.p2Safe) return;
 oil.p2Health=oil.p2Health>amount?oil.p2Health-amount:0;
 oil.p2Safe=40;
 startWorldImpact(g,g->wingman.interceptScreenX,g->wingman.screenY);
 if(oil.p2Health) return;
 oil.p2Present=oil.p2Landed=0; g->wingman.active=0; g->wingman.destroyed=1;
 g->wingman.rocket.active=g->wingman.bomb.active=0;
 /* Recovery now comes from a red drop, not an automatic timer. */
 oil.p2Respawn=0;oil.crewDropTicks=300;
 if(g->lives>1 && !debugInfiniteLives)g->lives--;

}
static void oilAdvanceBomb(GameState* g,WeaponState* b) {
 if(!b->active) return; b->timer++;
 advanceVtolBombMotion(b,0);
 if(b->x<0 || b->x>=320 || b->y>=SEA_SURFACE_Y) {
  if(b->y>=SEA_SURFACE_Y) startWaterSplash(g,b->x);
  b->active=0; return;
 }
 if(carrierBombHitsAircraft(g,b)) return;
 if(oilHullOverlap(b->x,b->y,6,6)) {
  b->active=0; carrierDefenceImpact(g,b->x,CARRIER_BOMB_HULL_DAMAGE);
 }
}
static void oilP2Update(GameState* g,const Player2InputState* in) {
 if(!oil.p2Present) {oil.previousFire=in->fire;oil.previousBomb=in->bomb;return;}
 if(oil.p2Safe) oil.p2Safe--;
 g->wingman.active=1; g->wingman.mode=WINGMAN_PLAYER2_FLIGHT;
 /* The heading routine reads only these fields; no large state copy/clear. */
 CarrierDefenceState pose;
 InputState input={0}; input.left=in->left; input.right=in->right;
 pose.landed=oil.p2Landed;pose.facing=oil.facing;pose.turnTicks=oil.turnTicks;
 pose.turnTarget=oil.turnTarget;pose.vtolPose=oil.pose;
 carrierUpdateVtolHeading(&pose,&input);
 oil.facing=pose.facing;oil.turnTicks=pose.turnTicks;oil.turnTarget=pose.turnTarget;oil.pose=pose.vtolPose;
 WORD oldY=g->wingman.screenY;
 if(oil.p2Landed) {
  oil.vx=oil.vy=oil.ax=oil.ay=oil.subX=oil.subY=0;
  if(!(g->defence.clock&7)) {
   if(oil.p2Health<100){oil.p2Health+=2;if(oil.p2Health>100)oil.p2Health=100;}
   if(oil.p2FuelQuanta<CPC_FUEL_TOTAL_QUANTA){oil.p2FuelQuanta+=4;if(oil.p2FuelQuanta>CPC_FUEL_TOTAL_QUANTA)oil.p2FuelQuanta=CPC_FUEL_TOTAL_QUANTA;}
  }
  if(in->up && g->defence.phase!=DEFENCE_SECURE) {oil.p2Landed=0;g->wingman.screenY-=3;}
 } else {
  if(!debugInfiniteFuel) {
   oil.fuelTicks+=GAME_FUEL_BURN_HZ*3;
   if(oil.fuelTicks>=playerFuelClockLimit(g)) {
    oil.fuelTicks-=playerFuelClockLimit(g);if(oil.p2FuelQuanta)oil.p2FuelQuanta--;
   }
  }
  if(!oil.p2FuelQuanta) {oil.vy=256;oil.ay=0;}
  WORD x=g->wingman.interceptScreenX+carrierVtolAxis(&oil.vx,&oil.ax,&oil.subX,(in->right!=0)-(in->left!=0));
  WORD y=oldY+carrierVtolAxis(&oil.vy,&oil.ay,&oil.subY,oil.p2FuelQuanta?((in->down!=0)-(in->up!=0)):0);
  if(x<8){x=8;oil.vx=oil.ax=oil.subX=0;} if(x>296){x=296;oil.vx=oil.ax=oil.subX=0;}
  if(y<8){y=8;oil.vy=oil.ay=oil.subY=0;}
  if(y>=OIL_PLAYER_Y && oldY<=OIL_PLAYER_Y && oilPad(x)) {
   y=OIL_PLAYER_Y;
   if(oil.vy>CARRIER_SAFE_LANDING_VY) {
    y-=3;oil.vy=-320;oil.ay=oil.subY=0;oil.p2Safe=0;oilDamageP2(g,30);
    playSfxAt(SFX_GROUND_BOUNCE,x);
   } else {
    oil.p2Landed=1;
    /* Touchdown shares P1 geometry and keeps the real horizontal position. */
    /* Keep the actual touchdown position; do not teleport sideways. */
   }
  } else if(oilAircraftHitsStructure(x,y)) {
   while(y>PLAYER_MIN_Y && oilAircraftHitsStructure(x,y))y--;
   oil.vy=-768;oil.ay=oil.subY=0;
   if(!oil.p2Safe) {oilDamageP2(g,38);playSfxAt(SFX_GROUND_BOUNCE,x);}
  } else if(y>=SEA_SURFACE_Y-8) {
   oil.p2Safe=0;oilDamageP2(g,100);return;
  }
  g->wingman.interceptScreenX=x;g->wingman.screenY=y;
  if(in->fire && !oil.previousFire && !oil.turnTicks && !g->wingman.rocket.active && g->rockets) {
   carrierDefenceLaunch(g,&g->wingman.rocket,x+4,y,oil.facing);g->rockets--;
  }
  if(in->bomb && !oil.previousBomb && !g->wingman.bomb.active && g->bombs) {
   WeaponState* b=&g->wingman.bomb; memset(b,0,sizeof(*b)); b->active=1;
   b->x=b->worldX=x+6;b->y=y+7;b->dx=oil.vx;b->dy=oil.vy;b->type=1;
   g->bombs--;playSfxAt(SFX_BOMB,x);
  }
 }
 oil.previousFire=in->fire;oil.previousBomb=in->bomb;
 carrierDefenceShot(g,&g->wingman.rocket); oilAdvanceBomb(g,&g->wingman.bomb);
 if(!oil.p2Safe) {
  if(g->enemyPlane.active && rectsOverlap(g->wingman.interceptScreenX,g->wingman.screenY,16,8,g->enemyPlane.x,g->enemyPlane.y,CARRIER_JET_WIDTH(g),CARRIER_JET_HEIGHT(g))) {
   carrierDefenceKillJet(g);oilDamageP2(g,50);
  } else if(g->helicopter.active && rectsOverlap(g->wingman.interceptScreenX,g->wingman.screenY,16,8,g->helicopter.x,g->helicopter.y,16,8)) {
   carrierDefenceKillHeli(g);oilDamageP2(g,50);
  }
 }
}
static void oilCpuUpdate(GameState* g) {
 for(UBYTE i=0;i<2;i++) carrierAdvanceBullet(g,&g->helicopterBullets[i]);
 if(oil.cpuState==1) {
  if(oil.crewX<104)oil.crewX++;else if(oil.crewX>104)oil.crewX--;
  if(!(g->defence.clock&1) && oil.crewY<OIL_DECK_Y-HAR_CPC_PARACHUTE_HEIGHT)oil.crewY++;
  if(oil.crewX==104 && oil.crewY>=OIL_DECK_Y-HAR_CPC_PARACHUTE_HEIGHT)oil.cpuState=2;
  return;
 }
 if(oil.cpuState!=2 && !oil.p1Gunner) return;
 if(!(g->defence.clock&7) && !carrierChooseAim(g)) return;
 UBYTE target=smoothAaAimPose(g->defence.aimX-108,OIL_DECK_Y-4-g->defence.aimY);
 oil.cpuGunPose=smoothAaTurn(oil.cpuGunPose,target,g->defence.clock);
 if((g->defence.clock&7) || oil.cpuGunPose!=target || (oil.p1Gunner && !oil.p1GunFire)) return;
 for(UBYTE i=0;i<2;i++) {
  HelicopterBullet* b=&g->helicopterBullets[i]; if(b->active) continue;
  WORD dx=g->defence.aimX-108,dy=g->defence.aimY-(OIL_DECK_Y-4);
  WORD distance=carrierAbs(dx);if(carrierAbs(dy)>distance) distance=carrierAbs(dy);
  if(!distance) return;
  memset(b,0,sizeof(*b));b->active=1;b->worldX=108;b->y=OIL_DECK_Y-4;
  b->vx16=(LONG)dx*64/distance;b->vy16=(LONG)dy*64/distance;
  playSfxAtTuned(SFX_FLAK_GUN_1,108,18,SFX_PAULA_PERIOD);break;
 }
}
static void oilPlayerMove(GameState* g,const InputState* in) {
 CarrierDefenceState* d=&g->defence;
 if(g->groundBounceTicks) {carrierResetVtol(d);updatePlayerGroundBounce(g);return;}
 if(d->landed) {
  carrierResetVtol(d);
  if(!(d->clock&7)) carrierServiceAircraft(g);
  if(in->up && d->phase!=DEFENCE_SECURE) {d->landed=0;g->playerY-=3;}
  return;
 }
 WORD x,y,oldY=g->playerY;carrierVtolStep(g,in,&x,&y);
 if(y>=OIL_PLAYER_Y && oldY<=OIL_PLAYER_Y && oilPad(x)) {
  y=OIL_PLAYER_Y;
  if(!carrierHardLanding(g,&y)) {d->landed=1;carrierResetVtol(d);}
 } else if(oilAircraftHitsStructure(x,y)) {
  while(y>PLAYER_MIN_Y && oilAircraftHitsStructure(x,y))y--;
  g->playerX=x;g->playerY=y;d->landed=0;carrierResetVtol(d);
  startEnhancedSurfaceBounce(g,y+PLAYER_SPRITE_HEIGHT+1,115);return;
 } else if(y>=SEA_SURFACE_Y-8) {
  startPlayerCrash(g,x,y);return;
 }
 g->playerX=x;g->playerY=y;
 if(!d->landed) updatePlayerFuel(g);
}
static void oilCrewSupply(GameState* g) {
 UBYTE needed=g->wingmanControl==WINGMAN_CONTROL_CPU?!oil.cpuState:
  g->wingmanControl==WINGMAN_CONTROL_PLAYER2?(!oil.p2Present || oil.p1Gunner):0;
 if(!needed || (g->defence.phase!=DEFENCE_WAVE && g->defence.phase!=DEFENCE_SECURE))return;
 if(oil.crewDropTicks<450){oil.crewDropTicks++;return;}
 if(g->powerup.active || g->defence.repairDropPending)return;
 spawnPowerup(g,POWERUP_WINGMAN,1);
 g->powerup.worldX=104;oil.crewDropTicks=0;
}
static void oilCrewBonus(GameState* g) {
 if(oil.cpuState==2)awardGameScore(g,500);
 oil.cpuState=0;
}
static void oilFinish(GameState* g) {
 oil.active=oil.approach=0;oil.completed=1;oil.cpuState=0;
 campaign.specialDone=1;campaign.request=CAMPAIGN_RETURN;
 g->defence.phase=0;g->defence.landed=0;
 g->defence.hull=oil.carrierHull;memcpy(g->defence.gunHealth,oil.carrierGuns,2);UWORD cargo=(UWORD)g->defence.cargo+oil.carrierCargo; g->defence.cargo=cargo>180?180:cargo;
 g->takeoffState=TAKEOFF_STATE_AIRBORNE;g->speedLevel=GAME_SPEED_LEVEL_DEFAULT;g->lowSpeedLanding=0;
 g->rocketShot.active=g->bombShot.active=0;
 g->wingman.rocket.active=g->wingman.bomb.active=0;
 memset(g->helicopterBullets,0,sizeof(g->helicopterBullets));
 if(oil.p2Present) {
  g->wingman.mode=WINGMAN_PLAYER2_FLIGHT;g->wingman.active=1;g->wingman.destroyed=0;
 } else {g->wingman.active=0;g->wingman.destroyed=1;}
 carrierSceneHull=255;
}
static void oilDepartUpdate(GameState* g) {
 CarrierDefenceState* d=&g->defence;
 /* Lift clear of the structure before accelerating away. No mandatory
  * touchdown, including P2; this also takes off an already serviced plane. */
 UBYTE lifting=g->playerY>24 || (oil.p2Present && g->wingman.screenY>16);
 if(lifting) {
  if(g->playerY>24)g->playerY--;
  if(oil.p2Present && g->wingman.screenY>16)g->wingman.screenY--;
 } else {
  g->scrollX+=g->scrollX<64?1:2;
  if(g->playerX<80)g->playerX++;else if(g->playerX>80)g->playerX--;
  if(oil.p2Present) {
   if(g->wingman.interceptScreenX<48)g->wingman.interceptScreenX++;
   else if(g->wingman.interceptScreenX>48)g->wingman.interceptScreenX--;
  }
 }
 g->speedLevel=lifting?0:g->scrollX<64?1:GAME_SPEED_LEVEL_DEFAULT;
 g->lowSpeedLanding=lifting || g->scrollX<64;d->fullVtol=lifting;
 updatePlayerFuel(g);updateEngineSound(engineSpeedForSpeedLevel(g->speedLevel),g->lowSpeedLanding);
 if(g->scrollX>=352)oilFinish(g);
}
/* Reuse the single cosmetic smoke slot; never displace aircraft smoke. */
static void oilDamageEffects(GameState* g,UBYTE** buffers) {
 CarrierDefenceState* d=&g->defence;
 updateEncounterSmoke(g);
 if(d->hull<75 && !g->helicopterSmoke.active &&
    !(d->clock & (d->hull<25 ? 31 : d->hull<50 ? 63 : 127))) {
  UBYTE point=(d->clock>>6)% (d->hull<25 ? 3 : d->hull<50 ? 2 : 1);
  static const UBYTE x[3]={112,136,152}, y[3]={84,68,84};
  startEncounterSmoke(&g->helicopterSmoke,x[point],x[point],y[point]);
 }
 if(d->hull<75 && !(d->clock&7)) {
  oil.fireFrame=(oil.fireFrame+1)%3;
  if(buffers && buffers[0]) {
   bobCompositorErase(buffers[0],14,11,1);
   if(d->hull<50)bobCompositorErase(buffers[0],17,9,1);
   if(d->hull<25)bobCompositorErase(buffers[0],19,11,1);
  }
 }
}

static void oilUpdate(GameState* g,const InputState* in,const InputState* previous,
 const Player2InputState* in2,UBYTE** buffers) {
 CarrierDefenceState* d=&g->defence;d->clock++;
 if(oil.approach) {
  if(oil.approach==2)oilDepartUpdate(g);else oilApproachUpdate(g);
  return;
 }
 oilDamageEffects(g,buffers);
 g->scrollX=0;g->speedLevel=0;g->lowSpeedLanding=1;g->landingState=LANDING_STATE_NONE;
 if(d->bomberBlastTicks) d->bomberBlastTicks--;
 if(g->impact.active && g->impact.timer && !--g->impact.timer) g->impact.active=0;
 UBYTE platformLost=d->phase==DEFENCE_SINKING;
 if(platformLost) {
  if(!(d->clock&31)) startWorldImpact(g,72+((d->clock>>5)%5)*16,80);
  if(++d->phaseTicks>180) {triggerGameOver(g);return;}
  /* Keep both pilots responsive during the loss animation. The mission
   * still ends after the same delay; a wreck must not freeze input. */
 }
 oil.p1GunFire=in->fire;
 if(oil.p1Gunner) {
  if(!oil.p2Present) {
   oil.p1Gunner=0;g->ejectState=0;oil.rescueRestart=1;
   g->wingman.destroyed=0;g->wingmanReviveDelayColumns=0;return;
  }
 } else if(g->ejectState) {
  g->ejectX+=(in->right!=0)-(in->left!=0);
  if(g->ejectX<0)g->ejectX=0;if(g->ejectX>304)g->ejectX=304;
  updateAbandonedAircraft(g);if(g->crashTimer)updatePlayerCrash(g);
  if(updatePlayerEject(g)==EJECT_UPDATE_CARRIER_RESTART && !g->gameOver) {
   /* completePlayerEject already charged the lost aircraft. Defer the
    * display/session reset to the common rescue path in the main loop. */
   if(g->wingmanControl==WINGMAN_CONTROL_PLAYER2 && oil.p2Present) {
    oil.p1Gunner=1;oil.cpuGunPose=3;oil.crewDropTicks=300;
    g->ejectState=3;g->playerX=104;g->playerY=OIL_PLAYER_Y;
    g->defence.landed=0;carrierResetVtol(d);
   } else {
    oil.rescueRestart=1;
    if(g->wingmanControl==WINGMAN_CONTROL_PLAYER2) {
     g->wingman.destroyed=0;g->wingmanReviveDelayColumns=0;
    }
   }
   g->takeoffState=TAKEOFF_STATE_AIRBORNE;
   if(oil.rescueRestart)return;
  }
 } else if(g->crashTimer) {
  carrierResetVtol(d);updatePlayerCrash(g);
  if(!g->crashTimer && !g->gameOver) {g->playerX=32;g->playerY=16;g->scrollX=0;d->landed=0;}
 } else if(g->aircraftFailureState) {
  if(in->eject)startPlayerEject(g);
  if(!g->ejectState){updateAircraftFailure(g,in);g->scrollX=0;}
 } else if(!debugInvulnerable && (!g->fuel || !g->armour)) {
  startAircraftFailure(g,!g->fuel?AIRCRAFT_FAILURE_CAUSE_FUEL:AIRCRAFT_FAILURE_CAUSE_ARMOUR);
 } else {
  if(g->respawnSafeTimer)g->respawnSafeTimer--;
  carrierUpdateVtolHeading(d,in);oilPlayerMove(g,in);
  if(!d->landed && !g->crashTimer) {
   if(Pressed(in->fire,previous->fire) && !d->turnTicks && !g->rocketShot.active && g->rockets) {
    carrierDefenceLaunch(g,&g->rocketShot,g->playerX+4,g->playerY,d->facing);g->rockets--;
   }
   if(BombPressed(in,previous) && !g->bombShot.active && g->bombs) launchBomb(g);
   updateEngineSound(1,1);
  } else if(engineActive)stopSfxChannel(ENGINE_CHANNEL);
 }
 if(g->bombLaunchCooldown)g->bombLaunchCooldown--;
 if(g->gameOver)return;
 oilP2Update(g,in2);
 if(platformLost)return;
 oilCpuUpdate(g);
 oilAdvanceBomb(g,&g->bombShot);
 carrierDefenceAircraft(g);serviceBomberDrone(g);carrierDefenceBombs(g);
 carrierDefenceShot(g,&g->rocketShot);
 if(!g->crashTimer && !g->ejectState && !g->respawnSafeTimer) {
  if(g->enemyPlane.active && rectsOverlap(g->playerX,g->playerY,16,8,g->enemyPlane.x,g->enemyPlane.y,CARRIER_JET_WIDTH(g),CARRIER_JET_HEIGHT(g))) {
   carrierDefenceKillJet(g);applyPlayerAircraftCollision(g);
  } else if(g->helicopter.active && rectsOverlap(g->playerX,g->playerY,16,8,g->helicopter.x,g->helicopter.y,16,8)) {
   carrierDefenceKillHeli(g);applyPlayerAircraftCollision(g);
  }
 }
 /* Either landed pilot can deliver a shared repair kit to the platform. */
 carrierServiceRepair(g,d->landed?in->up:in2->up);
 serviceCarrierRepairDrop(g);oilCrewSupply(g);updatePowerup(g);
 if(d->phase==DEFENCE_ALARM) {
  carrierTryAlarm(d);
  if(d->phaseTicks)d->phaseTicks--;
  else {d->phase=DEFENCE_WAVE;d->wave=1;d->quota=oilQuota(g->levelDifficulty,g->wingmanControl);d->spawnDelay=30;}
 } else if(d->phase==DEFENCE_WAVE && d->spawned>=d->quota && !g->enemyPlane.active && !g->helicopter.active && !g->enemyMissile.active && !carrierBombsActive(g)) {
  d->phase=DEFENCE_SECURE;d->phaseTicks=100;
 } else if(d->phase==DEFENCE_SECURE) {
  if(d->phaseTicks)d->phaseTicks--;
  else if(!g->crashTimer && !g->ejectState && !g->aircraftFailureState &&
          g->armour && g->fuel && !oil.p2Respawn) {
   d->phase=DEFENCE_DEPART;oil.approach=2;g->scrollX=0;
   carrierResetVtol(d);d->landed=oil.p2Landed=0;
   d->facing=MAVERICK_DIRECTION_RIGHT;d->vtolPose=1;oil.facing=MAVERICK_DIRECTION_RIGHT;oil.pose=1;
   oilCrewBonus(g);g->wingman.active=oil.p2Present;
   g->rocketShot.active=g->bombShot.active=g->wingman.rocket.active=g->wingman.bomb.active=0;
  }
 }
}

/* Reuse the existing gun artwork and P1 sprite channel; P2 keeps channel 6. */
static void drawOilPlayerGun(UWORD* sprite,UWORD* attach) {
 hideHardwareSprite(attach);invalidateAttachedSpriteResident(sprite);
 setHardwareSpritePosition(sprite,8,104,OIL_PLAYER_Y);
 const UBYTE* gun=smoothAaPoses[oil.cpuGunPose];
 for(UBYTE row=0;row<8;row++) {
  const UBYTE* src=gun+row*5;
  sprite[2+row*2]=(UWORD)(src[4]&~src[0])<<8;
  sprite[3+row*2]=(UWORD)(src[4]&src[1])<<8;
 }
 sprite[18]=sprite[19]=0;
}
static void oilCollectCrew(GameState* g) {
 if(oil.p1Gunner) {
  oil.p1Gunner=0;respawnPlayer(g);resetPlayerFuel(g);
  g->scrollX=0;g->playerX=52;g->playerY=16;g->defence.landed=0;
  carrierResetVtol(&g->defence);g->defence.fullVtol=1;
 } else if(!oil.p2Present) {
  oil.p2Present=1;oil.p2Landed=0;oil.p2Health=100;oil.p2Safe=100;
  oil.p2FuelQuanta=CPC_FUEL_TOTAL_QUANTA;oil.fuelTicks=0;
  oil.vx=oil.vy=oil.ax=oil.ay=oil.subX=oil.subY=0;
  oil.facing=MAVERICK_DIRECTION_RIGHT;oil.pose=1;oil.turnTicks=0;
  g->wingman.active=1;g->wingman.destroyed=0;g->wingman.mode=WINGMAN_PLAYER2_FLIGHT;
  g->wingman.interceptScreenX=24;g->wingman.screenY=8;
 }
 oil.crewDropTicks=0;
}
