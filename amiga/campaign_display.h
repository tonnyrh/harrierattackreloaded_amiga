/* Reuse the menu bitmap for frozen debug jumps and live ocean preparation.
 * No allocation and no menu/loading frame between missions. */
#define CAMPAIGN_STILL_WORLD_BYTES (GAME_WORLD_HEIGHT*GAME_WORLD_DISPLAY_PLANES*SCREEN_ROW_BYTES)
#define CAMPAIGN_STILL_SPRITE_BYTES ((4*PLAYER_SPRITE_WORDS+2*AUXILIARY_SPRITE_WORDS+ENEMY_SPRITE_WORDS+ENEMY_MISSILE_SPRITE_WORDS)*2)
#if CAMPAIGN_STILL_WORLD_BYTES+HUD_BITMAP_BYTES+CAMPAIGN_STILL_SPRITE_BYTES+COPPER_BYTES > SCREEN_BITMAP_BYTES
#error Campaign still image exceeds the reusable menu buffer
#endif
static void campaignCopyVisibleRow(UBYTE* dst,const UBYTE* src,UWORD skip) {
 src+=skip>>3;UBYTE shift=skip&7;
 if(!shift) {memcpy(dst,src,SCREEN_ROW_BYTES);return;}
 for(UWORD x=0;x<SCREEN_ROW_BYTES;x++)dst[x]=(src[x]<<shift)|(src[x+1]>>(8-shift));
}
static UWORD* campaignTransitSprites[8];
static HudRenderState campaignTransitHudState;
static UWORD campaignTransitFrames,campaignTransitMaxFields;
/* Compact ocean entry needs no shifted full-screen snapshot. The blitter
 * clears the known empty sky/sea and copies the HUD while the old display
 * is still visible. D-only fills and A->D copies need no extra Chip RAM. */
static void campaignEntryBlit(UBYTE* dst,const UBYTE* src,UWORD rows,UBYTE fill) {
 WaitBlit();custom->bltcon0=src?0x09f0:(fill?0x01ff:0x0100);custom->bltcon1=0;
 custom->bltafwm=custom->bltalwm=0xffff;
 custom->bltamod=custom->bltdmod=0;
 if(src)custom->bltapt=(APTR)src;
 custom->bltdpt=dst;custom->bltsize=(rows<<6)|(SCREEN_ROW_BYTES/2);WaitBlit();
}
static void freezeCampaignDisplay(UBYTE* storage,UWORD* liveCopper,const UBYTE* world,
 const UBYTE* hud,const GameState* g,UWORD* player,UWORD* attach,UWORD* crash,
 UWORD* enemyAttach,UWORD* enemy,UWORD* missile,UWORD* wing,UWORD* wingAttach,UBYTE oceanTransit) {
 campaignTransitHudState=hudRenderState[0];
 UWORD fine=*activeCopperBplcon1&15;
 UWORD skip=oil.active && !oil.approach ? 0 : 16-fine;
 if(oceanTransit) {
  campaignEntryBlit(storage,0,120*GAME_WORLD_DISPLAY_PLANES,0);
  campaignEntryBlit(storage+122UL*GAME_WORLD_DISPLAY_PLANES*SCREEN_ROW_BYTES,0,
   (GAME_WORLD_HEIGHT-122)*GAME_WORLD_DISPLAY_PLANES,1);
 }
 /* Preserve the two-row waterline; non-ocean debug jumps keep their snapshot. */
 for(UBYTE p=0;p<GAME_WORLD_DISPLAY_PLANES;p++) {
  const UBYTE* src=(const UBYTE*)(((ULONG)*activeCopperPlaneHigh[p]<<16)|*activeCopperPlaneLow[p]);
  for(UWORD y=oceanTransit?120:0;y<(oceanTransit?122:GAME_WORLD_HEIGHT);y++) {
   const UBYTE* row=src+(ULONG)y*SCREEN_PLANES*GAME_WORLD_ROW_BYTES;
   UWORD rowSkip=skip;
   if(battleship.active && y>=BATTLESHIP_SEA_Y && battleshipWaterFine) {
    row=(const UBYTE*)(((ULONG)*battleshipWaterHigh[p]<<16)|*battleshipWaterLow[p]);
    row+=(ULONG)(y-BATTLESHIP_SEA_Y)*SCREEN_PLANES*GAME_WORLD_ROW_BYTES;
    rowSkip=16-(*battleshipWaterFine&15);
   }
   campaignCopyVisibleRow(storage+((ULONG)y*GAME_WORLD_DISPLAY_PLANES+p)*SCREEN_ROW_BYTES,row,rowSkip);
  }
 }
 UBYTE* stillHud=storage+CAMPAIGN_STILL_WORLD_BYTES;
 campaignEntryBlit(stillHud,hud,HUD_BITMAP_BYTES/SCREEN_ROW_BYTES,0);
 const UWORD* sources[8]={player,attach,crash,enemyAttach,enemy,missile,wing,wingAttach};
 const UWORD words[8]={PLAYER_SPRITE_WORDS,PLAYER_SPRITE_WORDS,AUXILIARY_SPRITE_WORDS,
  AUXILIARY_SPRITE_WORDS,ENEMY_SPRITE_WORDS,ENEMY_MISSILE_SPRITE_WORDS,PLAYER_SPRITE_WORDS,PLAYER_SPRITE_WORDS};
 UWORD* sprites[8];UBYTE* next=stillHud+HUD_BITMAP_BYTES;
 for(UBYTE i=0;i<8;i++){sprites[i]=campaignTransitSprites[i]=(UWORD*)next;memcpy(next,sources[i],words[i]*2);next+=words[i]*2;}
 UWORD* stillCopper=(UWORD*)next;
 campaignFrozenDisplay=1;
 buildGameHudCopper(stillCopper,storage,stillHud,(const UWORD*)gamePalette,0,0,
  sprites[0],sprites[1],sprites[2],sprites[3],sprites[4],sprites[5],sprites[6],sprites[7]);
 campaignFrozenDisplay=0;
 WaitVbl();custom->cop1lc=(ULONG)stillCopper;custom->copjmp1=0x7fff;
 /* Subsequent palette/session resets must patch the hidden live list only. */
 buildGameHudCopper(liveCopper,world,hud,(const UWORD*)gamePalette,
  scrollDelayForBplcon1(displayScrollXForGameState(g)),displayByteOffsetForGameState(g),
  player,attach,crash,enemyAttach,enemy,missile,wing,wingAttach);
}

/* The menu bitmap is also our small ocean transit display. The large world
 * bitmap remains completely hidden until every required column is resident.
 * No extra Chip RAM, no framebuffer copy inside the per-field loop. */
static void campaignOceanPixel(UBYTE* bitmap,UWORD x,UWORD y,UBYTE pen) {
 UBYTE mask=128>>(x&7);
 for(UBYTE p=0;p<4;p++) {
  UBYTE* b=bitmap+((ULONG)y*4+p)*SCREEN_ROW_BYTES+(x>>3);
  if(pen&(1<<p))*b|=mask;else *b&=~mask;
 }
}
static void campaignOceanFlight(GameState* g,const InputState* in,const Player2InputState* in2) {
 g->playerX+=((in->right!=0)-(in->left!=0))*2;
 g->playerY+=((in->down!=0)-(in->up!=0))*2;
 if(g->playerX<8)g->playerX=8;if(g->playerX>296)g->playerX=296;
 if(g->playerY<8)g->playerY=8;if(g->playerY>104)g->playerY=104;
 if(g->wingman.active) {
  if(g->wingmanControl==WINGMAN_CONTROL_PLAYER2) {
   g->wingman.interceptScreenX+=((in2->right!=0)-(in2->left!=0))*2;
   g->wingman.screenY+=((in2->down!=0)-(in2->up!=0))*2;
  } else {
   WORD x=g->playerX-24,y=g->playerY-16;
   g->wingman.interceptScreenX+=(g->wingman.interceptScreenX<x)-(g->wingman.interceptScreenX>x);
   g->wingman.screenY+=(g->wingman.screenY<y)-(g->wingman.screenY>y);
  }
  if(g->wingman.interceptScreenX<8)g->wingman.interceptScreenX=8;
  if(g->wingman.interceptScreenX>296)g->wingman.interceptScreenX=296;
  if(g->wingman.screenY<8)g->wingman.screenY=8;if(g->wingman.screenY>104)g->wingman.screenY=104;
 }
}
#include "campaign_transit_combat.h"

static ULONG campaignHardwareField(void) {
 ULONG value=(ULONG)ciaa->ciatodhi<<16;
 value|=(ULONG)ciaa->ciatodmid<<8;value|=ciaa->ciatodlow;return value;
}
static void prepareCampaignOceanWorld(UBYTE* display,UBYTE* world,GameState* g) {
 /* Keep the palette and horizon. Natural entrances occur beyond the coast;
  * debug jumps deliberately replace their old scenery with this same sea. */
 /* Empty sea was prepared before display handoff, not cleared live here. */
 for(UBYTE i=2;i<6;i++)hideHardwareSprite(campaignTransitSprites[i]);
 UWORD waveX[8];for(UBYTE i=0;i<8;i++)waveX[i]=(i*83+19)%315;
 ULONG cleared=0;
 UWORD bossVisualStep=0;UBYTE bossVisualReady=!battleship.active;
 UWORD first=(oil.scene || battleship.active)?0:scrollLeftWorldColumnForScroll(g->scrollX);
 UWORD count=(oil.scene || battleship.active)?GAME_WORLD_SCROLL_PAGE_BYTES:
  TAKEOFF_SCROLL_START_PIXELS/GAME_TILE_WIDTH+GAME_FETCH_BYTES+2;
 UWORD done=0,row=0;RenderColumn column;
 campaignTransitFrames=campaignTransitMaxFields=0;
 TransitCombat combat={0};
#if HAR_HEADLESS_CAMPAIGN_TRANSITION_TEST
 UBYTE combatContract=campaignTransitContract(g);
 UBYTE waterMoved=0,waterWrapped=0,waterFailed=0;
 KPrintF("TRANSIT combat contract=%ld\n",(ULONG)combatContract);
#endif
 UBYTE savedPhase=g->defence.phase,savedLanding=g->landingState,savedLowSpeed=g->lowSpeedLanding;
 g->defence.phase=0;g->landingState=LANDING_STATE_NONE;g->lowSpeedLanding=0;
 UBYTE* transitHud=display+CAMPAIGN_STILL_WORLD_BYTES;
 while(combat.quiet<35) {
  WaitVbl();ULONG start=campaignHardwareField();
  serviceModMusicToCurrentVbl();
  InputState in={0};Player2InputState in2={0};ReadInput(&in,g->wingmanControl==WINGMAN_CONTROL_PLAYER2);
  if(g->wingmanControl==WINGMAN_CONTROL_PLAYER2)ReadPlayer2Input(&in2,g->gameMode);
#if HAR_HEADLESS_CAMPAIGN_TRANSITION_TEST
  /* Exercise real weapon movement, score and ammo in the live compact display. */
  in.fire=(combat.ticks%24)==0;
  for(UBYTE i=0;i<2;i++)if(combat.enemy[i].active && combat.enemy[i].x>g->playerX+24) {
   in.up=combat.enemy[i].y<g->playerY;in.down=combat.enemy[i].y>g->playerY;break;
  }
#endif
  UBYTE pause=in.p || in.cancel;
  if(pause && !combat.previousPause)combat.paused=!combat.paused;
  combat.previousPause=pause;
  if(combat.paused){updateSfx();continue;}
  UBYTE ready=bossVisualReady && cleared>=GAME_WORLD_BITMAP_BYTES && done>=count;
  campaignTransitInk(display,&combat,1);
  campaignTransitTick(&combat,g,&in,&in2,ready);
  campaignTransitInk(display,&combat,0);
#if HAR_HEADLESS_CAMPAIGN_TRANSITION_TEST
  ULONG combatEnd=campaignHardwareField();UWORD combatRaster=currentRasterY();
#endif
  /* Retire a departing mission label/bar a scanline at a time. */
  if(combat.ticks<=17)memset(transitHud+(57UL+combat.ticks)*SCREEN_PLANES*SCREEN_ROW_BYTES,0,SCREEN_PLANES*SCREEN_ROW_BYTES);
  updateHudValues(g);campaignTransitHud(transitHud,g,combat.ticks);
#if HAR_HEADLESS_CAMPAIGN_TRANSITION_TEST
  ULONG hudEnd=campaignHardwareField();UWORD hudRaster=currentRasterY();
#endif
  updatePlayerSprite(campaignTransitSprites[0],campaignTransitSprites[1],g);
  updateWingmanSprite(campaignTransitSprites[6],campaignTransitSprites[7],g);
  /* Eight tiny wavelets, independent of loader progress and aircraft speed.
   * Animate before spending the remaining raster budget on hidden columns. */
#if HAR_HEADLESS_CAMPAIGN_TRANSITION_TEST
  UWORD previousWaveX=waveX[0];
#endif
  for(UBYTE i=0;i<8;i++) {
   UWORD y=126+i*4;
   for(UBYTE n=0;n<5;n++)campaignOceanPixel(display,waveX[i]+n,y,GAME_COLOR_SEA);
   waveX[i]=waveX[i]>=2?waveX[i]-2:314;
   for(UBYTE n=0;n<5;n++)campaignOceanPixel(display,waveX[i]+n,y,GAME_COLOR_WHITE);
  }
#if HAR_HEADLESS_CAMPAIGN_TRANSITION_TEST
  /* Check actual displayed bitplanes, not merely the animation counter. */
  if(waveX[0]!=previousWaveX)waterMoved=1;
  if(waveX[0]>previousWaveX)waterWrapped=1;
  for(UBYTE p=0;p<4;p++) {
   const UBYTE* waterRow=display+(126UL*4+p)*SCREEN_ROW_BYTES;
   UWORD oldTail=previousWaveX+4;
   UBYTE newBit=(waterRow[waveX[0]>>3]>>(7-(waveX[0]&7)))&1;
   UBYTE oldBit=(waterRow[oldTail>>3]>>(7-(oldTail&7)))&1;
   if(newBit!=((GAME_COLOR_WHITE>>p)&1) || oldBit!=((GAME_COLOR_SEA>>p)&1))waterFailed=1;
  }
#endif
  updateSfx();
  if(currentRasterY()>=160) { /* Combat owns the field; loading can wait. */
  } else if(!bossVisualReady) {
   bossVisualReady=battleshipPrepareVisualStep(&bossVisualStep);
  } else if(cleared<GAME_WORLD_BITMAP_BYTES) {
   ULONG bytes=GAME_WORLD_BITMAP_BYTES-cleared;if(bytes>2048)bytes=2048;
   memset(world+cleared,0,bytes);cleared+=bytes;
  } else if(done<count) {
   UWORD col=first+done,x=ringWorldTileXForColumn(col);
   if(!row)buildWorldTileColumn(col,&column);
   /* Stop with raster time in reserve, including unusually costly artwork.
    * At least one row advances each field; never spend a whole column at once. */
   UBYTE budget=13;
   do {
    drawWorldColumnRowsFromCache(world,x,&column,row,1);
    drawDirectColumnRangeObjectRow(world,x,col,row);
    if(x<GAME_WORLD_BUFFER_MARGIN_TILES+GAME_FETCH_BYTES) {
     drawWorldColumnRowsFromCache(world,x+GAME_WORLD_SCROLL_PAGE_BYTES,&column,row,1);
     drawDirectColumnRangeObjectRow(world,x+GAME_WORLD_SCROLL_PAGE_BYTES,col,row);
    }
    row++;
    if(currentRasterY()>=180)break;
   } while(--budget && row<GAME_OBJECT_MAP_HEIGHT_TILES);
   if(row==GAME_OBJECT_MAP_HEIGHT_TILES) {
    if(battleship.active)battleshipResident[col&127]=col;
    done++;row=0;
   }
  }
  UWORD fields=(campaignHardwareField()-start)&0xffff;
#if HAR_HEADLESS_CAMPAIGN_TRANSITION_TEST
  if(fields>1)KPrintF("TRANSIT slow tick=%ld combat=%ld/%ld hud=%ld/%ld end=%ld/%ld\n",(ULONG)combat.ticks,(combatEnd-start)&65535,(ULONG)combatRaster,(hudEnd-start)&65535,(ULONG)hudRaster,(ULONG)fields,(ULONG)currentRasterY());
#endif
  if(fields>campaignTransitMaxFields)campaignTransitMaxFields=fields;
  campaignTransitFrames++;
 }
 campaignTransitSpawned=combat.spawned;campaignTransitKills=combat.kills;campaignTransitShots=combat.shots;
 campaignTransitClear=!campaignTransitBusy(&combat) && combat.quiet>=35;
#if HAR_HEADLESS_CAMPAIGN_TRANSITION_TEST
 KPrintF("TRANSIT water moved=%ld wrapped=%ld failed=%ld\n",(ULONG)waterMoved,(ULONG)waterWrapped,(ULONG)waterFailed);
 if(combatContract || !waterMoved || !waterWrapped || waterFailed)campaignTransitClear=0;
#endif
 g->defence.phase=savedPhase;g->landingState=savedLanding;g->lowSpeedLanding=savedLowSpeed;
 g->impact.active=0;
 for(UBYTE i=2;i<6;i++)hideHardwareSprite(campaignTransitSprites[i]);
 ringWorldLastStreamedColumn=first+count-1;ringStreamColumn=-1;
 ringStreamRow=ringStreamRowCredit=0;battleshipStreamColumn=-1;
}
