/* Bounded combat for the compact four-plane ocean display. No world BOBs:
 * those use the ring bitmap stride and would corrupt the loader scratch area. */
#define TRANSIT_SHOTS 8
#if AUXILIARY_SPRITE_WORDS < ENEMY_SPRITE_WORDS || ENEMY_MISSILE_SPRITE_WORDS < ENEMY_SPRITE_WORDS
#error Transit fighter pairs require two eight-line sprite buffers each
#endif
typedef struct { WORD x,y,vx; UWORD age; UBYTE active,burst,retreating; } TransitFighter;
typedef struct { WORD x,y,vx,vy; UBYTE active,owner,bomb; } TransitShot;
typedef struct {
 TransitFighter enemy[2]; TransitShot shot[TRANSIT_SHOTS];
 UWORD ticks,quiet,spawned,kills,shots; UBYTE cooldown[2],bombCooldown[2],paused,previousPause;
 UBYTE previousFire,previousWingFire,previousBomb,previousWingBomb,closed;
} TransitCombat;
static UWORD campaignTransitSpawned,campaignTransitKills,campaignTransitShots;
static UBYTE campaignTransitClear;
/* At most one HUD field per tick, and one score digit. Full drawHudValues
 * can change both six-digit scores and several gauges in one A500 field. */
static void campaignTransitHud(UBYTE* hud,const GameState* g,UWORD tick) {
 HudRenderState* s=&campaignTransitHudState;UBYTE slot=tick%20;
 if(slot<12) {
  static const ULONG place[6]={100000,10000,1000,100,10,1};
  UBYTE digit=slot%6;ULONG unit=place[digit];
  ULONG* old=slot<6?&s->score:&s->highScore;
  ULONG value=g->score;
  if(slot>=6 && highScoreTables[1][0].score>value)value=highScoreTables[1][0].score;
  UBYTE a=(*old/unit)%10,b=(value/unit)%10;
  if(a!=b) {drawUnsignedPaddedDelta(hud,(slot<6?52:144)+digit*8,4,a,b,1,FONT_STYLE_CPC_HUD);*old=*old-a*unit+b*unit;}
  return;
 }
 UBYTE bombs,rockets;ammoForGame(g,&bombs,&rockets);
 switch(slot) {
 case 12: {
  UBYTE level=quantiseGauge(g->armour,100,8),color=g->armour?HUD_COLOR_VALUE:HUD_COLOR_WARN;
  if(level!=s->armourLevel || color!=s->armourColor)drawHudCellGaugeDelta(hud,8,17,8,12,s->armourLevel,level,color,s->armourColor);
  s->armourLevel=level;s->armourColor=color;break;
 }
 case 13: {
  UBYTE level=(g->fuelGaugeLevel+1)>>1,color=g->fuel<100?HUD_COLOR_WARN:HUD_COLOR_FUEL;
  if(level!=s->fuelLevel || color!=s->fuelColor)drawHudCellGaugeDelta(hud,168,46,8,10,s->fuelLevel,level,color,s->fuelColor);
  s->fuelLevel=level;s->fuelColor=color;break;
 }
 case 14: {
  UBYTE level=quantiseGauge(g->rockets,rockets,8);
  if(level!=s->rocketsLevel)drawHudCellGaugeDelta(hud,8,76,8,10,s->rocketsLevel,level,HUD_COLOR_POWERUP_ROCKETS,HUD_COLOR_POWERUP_ROCKETS);
  s->rocketsLevel=level;break;
 }
 case 15: {
  UBYTE level=quantiseGauge(g->bombs,bombs,8);
  if(level!=s->bombsLevel)drawHudCellGaugeDelta(hud,168,76,8,10,s->bombsLevel,level,HUD_COLOR_POWERUP_BOMBS,HUD_COLOR_POWERUP_BOMBS);
  s->bombsLevel=level;break;
 }
 case 16: {
  UBYTE lamp=ejectLampIsLit(g)?COCKPIT_INDICATOR_RED:COCKPIT_INDICATOR_OFF;
  if(lamp!=s->ejectIndicatorVisual)drawCockpitIndicator(hud,208,'E',lamp);
  s->ejectIndicatorVisual=lamp;break;
 }
 case 17:
  if(s->landingIndicatorVisual!=COCKPIT_INDICATOR_OFF)drawCockpitIndicator(hud,248,'L',COCKPIT_INDICATOR_OFF);
  s->landingIndicatorVisual=COCKPIT_INDICATOR_OFF;break;
 case 18:case 19: {
  UBYTE unit=slot==18?10:1,a=(s->lives/unit)%10,b=(g->lives/unit)%10;
  if(a!=b){fillRect(hud,232+(slot-18)*8,4,8,8,HUD_COLOR_BACKGROUND);drawUnsignedPadded(hud,232+(slot-18)*8,4,b,1,HUD_COLOR_SAFE);s->lives=s->lives-a*unit+b*unit;}
  break;
 }
 }
}
static UBYTE campaignTransitBusy(const TransitCombat* c) {
 for(UBYTE i=0;i<2;i++)if(c->enemy[i].active || c->enemy[i].burst)return 1;
 for(UBYTE i=0;i<TRANSIT_SHOTS;i++)if(c->shot[i].active)return 1;
 return 0;
}
static void campaignTransitMark(UBYTE* display,WORD x,WORD y,UBYTE pen) {
 if(x>=0 && x<SCREEN_WIDTH && y>=0 && y<120)campaignOceanPixel(display,x,y,pen);
}
/* Use the same indexed rocket artwork as the world renderer. The compact
 * buffer has a different stride, so only its final pixel writes are local. */
static const UBYTE* campaignTransitShotArt(const TransitShot* s) {
 BYTE index=enhancedProjectileIndex(s->vx<0?55:56,!s->owner);
 return enhancedWeaponRows+(UWORD)index*40;
}
static void campaignTransitInk(UBYTE* display,const TransitCombat* c,UBYTE erase) {
 for(UBYTE i=0;i<TRANSIT_SHOTS;i++) {
  const TransitShot* s=&c->shot[i];if(!s->active)continue;
  if(s->bomb) {
   for(UBYTE n=0;n<5;n++)campaignTransitMark(display,s->x+n/3,s->y+n%3,erase?0:GAME_COLOR_WHITE);
  } else {
   const UBYTE* art=campaignTransitShotArt(s);
   WORD byteX=s->x>>3;UBYTE shift=s->x&7;
   for(UBYTE y=0;y<8;y++,art+=5)if(art[4] && s->y+y>=0 && s->y+y<120) {
    UWORD mask=((UWORD)art[4]<<8)>>shift;
    UBYTE* row=display+((ULONG)(s->y+y)*4)*SCREEN_ROW_BYTES;
    for(UBYTE p=0;p<4;p++,row+=SCREEN_ROW_BYTES) {
     UWORD bits=erase?0:((UWORD)art[p]<<8)>>shift;
     if(byteX>=0 && byteX<SCREEN_ROW_BYTES)
      row[byteX]=(row[byteX]&~(mask>>8))|((bits&mask)>>8);
     if(byteX+1>=0 && byteX+1<SCREEN_ROW_BYTES)
      row[byteX+1]=(row[byteX+1]&~(UBYTE)mask)|(UBYTE)(bits&mask);
    }
   }
  }
 }
 for(UBYTE i=0;i<2;i++)if(c->enemy[i].burst) {
  const TransitFighter* e=&c->enemy[i];WORD r=2+(12-e->burst)/2;
  for(WORD d=-1;d<=1;d+=2) {
   campaignTransitMark(display,e->x+8+d*r,e->y+4,erase?0:GAME_COLOR_WHITE);
   campaignTransitMark(display,e->x+8,e->y+4+d*r,erase?0:GAME_COLOR_WHITE);
   campaignTransitMark(display,e->x+8+d*r,e->y+4+d*r,erase?0:GAME_COLOR_WHITE);
  }
 }
}
static UBYTE campaignTransitShoot(TransitCombat* c,WORD x,WORD y,WORD vx,WORD vy,UBYTE owner,UBYTE bomb) {
 /* Reserve half the slots for the players, so hostile fire cannot lock them out. */
 UBYTE first=owner?0:4,last=owner?4:TRANSIT_SHOTS;
 for(UBYTE i=first;i<last;i++)if(!c->shot[i].active) {
  TransitShot* s=&c->shot[i];s->x=x;s->y=y;s->vx=vx;s->vy=vy;
  s->active=1;s->owner=owner;s->bomb=bomb;c->shots++;
  playSfxAt(bomb?SFX_BOMB:SFX_FIRE,x);return 1;
 }
 return 0;
}
static void campaignTransitKill(TransitCombat* c,GameState* g,TransitFighter* e,UBYTE score) {
 e->active=0;e->burst=12;playSfxAt(SFX_IMPACT,e->x);
 if(score){awardGameScore(g,ENEMY_SCORE_VALUE);g->hitsCount++;c->kills++;}
}
static void campaignTransitLoseWing(GameState* g) {
 /* No ring-buffer footprints exist in the compact display. */
 g->wingman.active=0;g->wingman.destroyed=1;g->wingman.mode=WINGMAN_DESTROYED;
 g->wingman.rocket.active=g->wingman.bomb.active=0;
 g->wingmanReviveDelayColumns=POWERUP_WINGMAN_REVIVE_DELAY_COLUMNS;
 carrierParkedWingmanVisible=0;playSfxAt(SFX_IMPACT,g->wingman.interceptScreenX);
}
static void campaignTransitWeapons(TransitCombat* c,GameState* g,const InputState* in,const Player2InputState* in2) {
 UBYTE wingFire=in2->fire;
 if(g->wingmanControl==WINGMAN_CONTROL_CPU && g->wingman.active) {
  for(UBYTE i=0;i<2;i++)if(c->enemy[i].active && c->enemy[i].x>g->wingman.interceptScreenX &&
   c->enemy[i].y-g->wingman.screenY>-9 && c->enemy[i].y-g->wingman.screenY<9)wingFire=1;
 }
 for(UBYTE p=0;p<2;p++) {
  if(c->cooldown[p])c->cooldown[p]--;
  if(c->bombCooldown[p])c->bombCooldown[p]--;
  if(p && !g->wingman.active)continue;
  WORD x=p?g->wingman.interceptScreenX:g->playerX,y=p?g->wingman.screenY:g->playerY;
  UBYTE fire=p?wingFire:in->fire,bomb=p?in2->bomb:in->bomb;
  UBYTE previousBomb=p?c->previousWingBomb:c->previousBomb;
  UBYTE fresh=fire && !(p?c->previousWingFire:c->previousFire);
  if(p && g->wingmanControl==WINGMAN_CONTROL_CPU)fresh=fire;
  if(fresh && !c->cooldown[p] && g->rockets && campaignTransitShoot(c,x+16,y,ROCKET_SPEED_PIXELS,0,p+1,0)) {
   g->rockets--;c->cooldown[p]=24;
  }
  if(bomb && !previousBomb && !c->bombCooldown[p] && g->bombs &&
     campaignTransitShoot(c,x+8,y+8,1,1,p+1,1)) {g->bombs--;c->bombCooldown[p]=20;}
 }
 c->previousFire=in->fire;c->previousWingFire=in2->fire;
 c->previousBomb=in->bomb;c->previousWingBomb=in2->bomb;
}
/* The sea corridor has no deck or terrain. Reuse damage/eject rules, but do
 * not probe the destination's still-hidden terrain for an impact/rescue. */
static void campaignTransitFailure(GameState* g,const InputState* in,UBYTE previousFire) {
 if(g->ejectState) {
  g->ejectTimer++;
  g->ejectX+=((in->right!=0)-(in->left!=0))*2;
  if(g->ejectX<0)g->ejectX=0;if(g->ejectX>304)g->ejectX=304;
  if(g->ejectState==1) {g->ejectY--;if(g->ejectTimer>=12){g->ejectState=2;g->ejectTimer=0;}}
  else if(in->down || (!in->up && !(g->ejectTimer&1)) || !(g->ejectTimer&3))g->ejectY++;
  if(g->ejectY>=120-HAR_CPC_PARACHUTE_HEIGHT) {
   UWORD scroll=g->scrollX;
   g->abandonedAircraftActive=g->abandonedAircraftCrash=0;
   UBYTE result=completePlayerEject(g);
   if(result==EJECT_UPDATE_CARRIER_RESTART) {
    respawnPlayer(g);g->scrollX=scroll;g->playerX=48;g->playerY=48;
    startEngineSound(engineSpeedForSpeedLevel(g->speedLevel));
   }
  }
  return;
 }
 if(g->aircraftFailureState==AIRCRAFT_FAILURE_NONE)return;
 if(in->eject || (in->fire && !previousFire)) {startPlayerEject(g);if(g->ejectState)return;}
 g->aircraftFailureTimer++;
 g->aircraftFailureY256+=g->aircraftFailureFallSpeed256;
 g->playerY=g->aircraftFailureY256>>8;
 if(g->aircraftFailureFallSpeed256<AIRCRAFT_FAILURE_FALL_MAX_256)
  g->aircraftFailureFallSpeed256+=AIRCRAFT_FAILURE_FALL_ACCEL_256;
 updateAircraftFailureAlarm(g);
 if(g->playerY>=112) {
  stopAircraftFailureAlarm();stopAircraftFailureSmokeEmission();
  g->aircraftFailureState=AIRCRAFT_FAILURE_NONE;playSfxAt(SFX_IMPACT,g->playerX);triggerGameOver(g);
 }
}
static void campaignTransitTick(TransitCombat* c,GameState* g,const InputState* in,const Player2InputState* in2,UBYTE ready) {
 c->ticks++;
 /* Closing is monotonic: finishing the buffer or losing P1 stops all new attacks. */
 if(ready || g->gameOver || g->aircraftFailureState || g->ejectState)c->closed=1;
 if(g->respawnSafeTimer)g->respawnSafeTimer--;
 if(!g->gameOver && !g->aircraftFailureState && !g->ejectState) {
  campaignOceanFlight(g,in,in2);
  if(!c->closed || c->enemy[0].active || c->enemy[1].active)campaignTransitWeapons(c,g,in,in2);
  updatePlayerFuel(g);if(!g->fuel)startAircraftFailure(g,AIRCRAFT_FAILURE_CAUSE_FUEL);
 } else if(!g->gameOver)campaignTransitFailure(g,in,c->previousFire);
 c->previousFire=in->fire;
 for(UBYTE i=0;i<2;i++) {
  TransitFighter* e=&c->enemy[i];
  if(e->burst)e->burst--;
  /* Stagger both directions; cap the encounter to eight fighters even if loading is slow. */
  if(!c->closed && !e->active && !e->burst && c->spawned<8 &&
     c->ticks>=(UWORD)(35+c->spawned*65) && i==(c->spawned&1)) {
   e->active=1;e->age=0;e->retreating=0;e->vx=i?2:-2;e->x=i?-16:320;
   e->y=24+(c->spawned*23)%64;c->spawned++;
  }
  if(!e->active)continue;
  e->age++;
  if(i) {
   if(c->closed)e->retreating=1;
   /* Left is braking in the corridor, just as over land. */
   e->x=landPursuerStepX(e->x,g->playerX,in->left?4:g->speedLevel,&e->retreating);
   if(!e->retreating && !(e->age&3))e->y+=(e->y<g->playerY)-(e->y>g->playerY);
   if(!c->closed && !e->retreating && e->age>=100 && (e->age-100)%125==0 &&
      e->x>=0 && e->x+16<g->playerX)
    campaignTransitShoot(c,e->x+16,e->y,6,0,0,0);
  } else {
   e->x+=e->vx;
   if(c->closed)e->retreating=1;
   if(e->retreating) {if(!(e->age&1))e->y--;}
   else {
    e->y+=(e->y<g->playerY)-(e->y>g->playerY);
    WORD distance=e->x-g->playerX;
    if(distance>=0 && distance<ENEMY_CPC_FIRE_RANGE_TILES*GAME_TILE_WIDTH) {
     if(campaignTransitShoot(c,e->x,e->y,-4,0,0,0))e->retreating=1;
    }
   }
  }
  if(e->x < -16 || e->x>320 || e->y < -8){e->active=0;continue;}
  if(!g->gameOver && !g->aircraftFailureState && !g->ejectState &&
     rectsOverlap(e->x,e->y,16,8,g->playerX,g->playerY,16,8)) {
   applyPlayerAircraftCollision(g);campaignTransitKill(c,g,e,0);
  } else if(g->wingman.active && rectsOverlap(e->x,e->y,16,8,g->wingman.interceptScreenX,g->wingman.screenY,16,8)) {
   campaignTransitLoseWing(g);campaignTransitKill(c,g,e,0);
  }
 }
 for(UBYTE i=0;i<TRANSIT_SHOTS;i++) {
  TransitShot* s=&c->shot[i];if(!s->active)continue;
  s->x+=s->vx;s->y+=s->vy;
  if(!s->bomb && s->owner==1 && g->rocketHeightLock && !g->ejectState && !g->aircraftFailureState)
   s->y=g->playerY+2;
  if(s->bomb && !(c->ticks&7) && s->vy<4)s->vy++;
  if(s->x<=-8 || s->x>=SCREEN_WIDTH || s->y<=-8 || s->y>=120){s->active=0;continue;}
  if(s->owner) {
   for(UBYTE j=0;j<2;j++)if(c->enemy[j].active &&
      rectsOverlap(s->x,s->y,s->bomb?2:8,s->bomb?3:8,c->enemy[j].x,c->enemy[j].y,16,8)) {
    campaignTransitKill(c,g,&c->enemy[j],1);s->active=0;break;
   }
   if(s->active && g->friendlyFire) {
    if(s->owner==2 && !g->aircraftFailureState && !g->ejectState &&
       rectsOverlap(s->x,s->y,s->bomb?2:8,s->bomb?3:8,g->playerX,g->playerY,16,8)) {
     applyPlayerMissileDamage(g,0);s->active=0;
    } else if(s->owner==1 && g->wingman.active &&
       rectsOverlap(s->x,s->y,s->bomb?2:8,s->bomb?3:8,g->wingman.interceptScreenX,g->wingman.screenY,16,8)) {
     campaignTransitLoseWing(g);s->active=0;
    }
   }
  } else if(!g->gameOver && !g->aircraftFailureState && !g->ejectState &&
     rectsOverlap(s->x,s->y,s->bomb?2:8,s->bomb?3:8,g->playerX,g->playerY,16,8)) {
   applyPlayerMissileDamage(g,0);s->active=0;
  } else if(g->wingman.active && rectsOverlap(s->x,s->y,s->bomb?2:8,s->bomb?3:8,g->wingman.interceptScreenX,g->wingman.screenY,16,8)) {
   campaignTransitLoseWing(g);s->active=0;
  }
 }
 if(ready && !campaignTransitBusy(c) && !g->aircraftFailureState && !g->ejectState)c->quiet++;
 else c->quiet=0;
}

static void campaignTransitFighterSprites(TransitCombat* c) {
 for(UBYTE i=0;i<2;i++) {
  /* buildGameHudCopper routes storage slots 4/3 to HW 2/3 and
   * slots 5/2 to HW 4/5. Storage order is NOT hardware channel order. */
  TransitFighter* e=&c->enemy[i];UWORD* a=campaignTransitSprites[i?5:4];UWORD* b=campaignTransitSprites[i?2:3];
  if(e->active && e->x>=0 && e->x<=304) {
   if(i)buildAttachedSpriteFromCpcPlusHalves(a,b,ENEMY_SPRITE_HEIGHT,e->x,e->y,carrierFighterRight,carrierFighterRight+8);
   else buildEnemyPlaneSprite(a,b,e->x,e->y);
  } else {hideHardwareSprite(a);hideHardwareSprite(b);}
 }
}

#if HAR_HEADLESS_CAMPAIGN_TRANSITION_TEST
/* Inspect the displayed planes after loader work, not just shot.active. */
static UBYTE campaignTransitShotsVisible(const UBYTE* display,const TransitCombat* c) {
 for(UBYTE i=0;i<TRANSIT_SHOTS;i++) {
  const TransitShot* s=&c->shot[i];
  if(!s->active || s->bomb || s->x<0 || s->x>312 || s->y<0 || s->y>112)continue;
  const UBYTE* art=campaignTransitShotArt(s);UBYTE visible=0;
  for(UBYTE y=0;y<8 && !visible;y++,art+=5)for(UBYTE x=0,mask=128;x<8 && !visible;x++,mask>>=1) {
   if(!(art[4]&mask))continue;
   WORD sx=s->x+x,sy=s->y+y;
   for(UBYTE p=0;p<4;p++)if(art[p]&mask) {
    if(display[((ULONG)sy*4+p)*SCREEN_ROW_BYTES+(sx>>3)]&(128>>(sx&7)))visible=1;
   }
  }
  if(!visible)return 0;
 }
 return 1;
}
static UBYTE campaignTransitContract(const GameState* source) {
 static GameState g;TransitCombat c={0};InputState in={0};Player2InputState in2={0};
 memcpy(&g,source,sizeof(g));g.wingman.active=0;g.rockets=2;g.bombs=1;
 if(campaignTransitHardwareSprites[2]!=(ULONG)campaignTransitSprites[4] ||
    campaignTransitHardwareSprites[3]!=(ULONG)campaignTransitSprites[3] ||
    campaignTransitHardwareSprites[4]!=(ULONG)campaignTransitSprites[5] ||
    campaignTransitHardwareSprites[5]!=(ULONG)campaignTransitSprites[2])return 13;
 g.playerX=48;g.playerY=40;g.aircraftFailureState=g.ejectState=g.gameOver=0;
 g.defence.phase=0;g.landingState=LANDING_STATE_NONE;
 in.fire=1;campaignTransitWeapons(&c,&g,&in,&in2);
 if(g.rockets!=1 || !c.shot[0].active)return 1;
 campaignTransitWeapons(&c,&g,&in,&in2);if(g.rockets!=1)return 2;
 in.fire=0;campaignTransitWeapons(&c,&g,&in,&in2);in.fire=1;
 c.cooldown[0]=0;campaignTransitWeapons(&c,&g,&in,&in2);
 c.cooldown[0]=0;campaignTransitWeapons(&c,&g,&in,&in2);
 if(g.rockets || c.shots!=2)return 3;
 memset(&c,0,sizeof(c));in.fire=0;
 c.enemy[0].active=1;c.enemy[0].x=100;c.enemy[0].y=40;c.enemy[0].vx=-2;
 c.shot[0].active=1;c.shot[0].owner=1;c.shot[0].x=94;c.shot[0].y=42;c.shot[0].vx=5;
 campaignTransitTick(&c,&g,&in,&in2,1);
 if(c.enemy[0].active || c.kills!=1 || !c.enemy[0].burst || c.quiet)return 4;
 for(UBYTE i=0;i<50;i++)campaignTransitTick(&c,&g,&in,&in2,1);
 if(campaignTransitBusy(&c) || c.quiet<35 || c.spawned)return 5;
 /* A hostile projectile prevents the hand-off even after the aircraft is gone. */
 memset(&c,0,sizeof(c));c.shot[4].active=1;c.shot[4].x=20;c.shot[4].y=10;c.shot[4].vx=-4;
 campaignTransitTick(&c,&g,&in,&in2,1);if(c.quiet || !c.closed)return 6;
 for(UBYTE i=0;i<50;i++)campaignTransitTick(&c,&g,&in,&in2,1);
 if(campaignTransitBusy(&c) || c.quiet<35 || c.spawned)return 7;
 memset(&c,0,sizeof(c));c.ticks=99;c.spawned=1;
 campaignTransitTick(&c,&g,&in,&in2,0);
 if(!c.enemy[1].active || c.enemy[1].vx<=0 || c.spawned!=2)return 8;
 /* Closing/loading readiness must never retire a live missile. */
 memset(&c,0,sizeof(c));in.left=in.right=in.up=in.down=0;
 c.shot[0].active=1;c.shot[0].owner=1;c.shot[0].x=20;c.shot[0].y=10;c.shot[0].vx=7;
 for(UBYTE n=0;n<40;n++) {
  campaignTransitTick(&c,&g,&in,&in2,1);
  if(!c.shot[0].active || c.shot[0].x!=20+(n+1)*7)return 9;
 }
 /* Shared rear fighter holds station until the player brakes. */
 UBYTE retreat=0;
 if(landPursuerStepX(80,144,8,&retreat)!=80 || retreat)return 10;
 if(landPursuerStepX(80,144,4,&retreat)!=82)return 11;
 if(landPursuerStepX(140,144,8,&retreat)!=142 || !retreat)return 12;
 /* Check the data actually addressed by the live Copper sprite pairs. */
 memset(&c,0,sizeof(c));g.playerX=48;g.playerY=90;g.rocketHeightLock=1;
 for(UBYTE i=0;i<2;i++) {c.enemy[i].active=1;c.enemy[i].x=100+i*120;c.enemy[i].y=20+i*30;c.enemy[i].vx=i?2:-2;}
 c.shot[0].active=1;c.shot[0].owner=1;c.shot[0].x=20;c.shot[0].y=10;c.shot[0].vx=7;
 campaignTransitTick(&c,&g,&in,&in2,1);
 if(c.shot[0].y!=g.playerY+2 || !c.shot[0].active)return 14;
 campaignTransitFighterSprites(&c);
 static UWORD expectedA[ENEMY_SPRITE_WORDS],expectedB[ENEMY_SPRITE_WORDS];
 for(UBYTE i=0;i<2;i++) {
  TransitFighter* e=&c.enemy[i];
  if(i)buildAttachedSpriteFromCpcPlusHalves(expectedA,expectedB,8,e->x,e->y,carrierFighterRight,carrierFighterRight+8);
  else buildEnemyPlaneSprite(expectedA,expectedB,e->x,e->y);
  const UWORD* a=(const UWORD*)campaignTransitHardwareSprites[2+i*2];
  const UWORD* b=(const UWORD*)campaignTransitHardwareSprites[3+i*2];
  for(UBYTE n=0;n<ENEMY_SPRITE_WORDS;n++)if(a[n]!=expectedA[n] || b[n]!=expectedB[n])return 15;
 }
 return 0;
}
#endif
