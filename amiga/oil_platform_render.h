/* Static overlay shared by campaign and prototype. Every dirty-row reconstruction follows this
 * path too, so explosions/parachutes cannot erase parts of the platform. */
#include "assets/oil-platform/platform_tiles.h"
static UBYTE oilHullOverlap(WORD x,WORD y,UBYTE w,UBYTE h) {
 for(WORD py=y;py<y+h;py++) for(WORD px=x;px<x+w;px++) {
  if(px<OIL_ORIGIN_X || px>=OIL_ORIGIN_X+OIL_WIDTH || py<OIL_ORIGIN_Y || py>=OIL_ORIGIN_Y+OIL_HEIGHT) continue;
  UWORD t=((py-OIL_ORIGIN_Y)/8)*12+(px-OIL_ORIGIN_X)/8;
  if(oilPlatformTiles[t*40+(py&7)*5+4]&(128>>(px&7))) return 1;
 }
 return 0;
}
static void oilDrawRow(UBYTE* bitmap,UWORD tileX,LONG col,WORD row) {
 if(col<8 || col>=20 || row<OIL_ORIGIN_Y/8 || row>=(OIL_ORIGIN_Y+OIL_HEIGHT)/8) return;
 drawGameScrollTileMasked(bitmap,tileX,row,oilPlatformTiles+((row-OIL_ORIGIN_Y/8)*12+col-8)*40);
 /* At most three 8x8 fires. Only one tiny group changes every eight ticks. */
 UBYTE count=carrierSceneHull<25?3:carrierSceneHull<50?2:carrierSceneHull<75?1:0;
 static const UBYTE cols[3]={14,17,19};
 static const UBYTE rows[3]={11,9,11};
 for(UBYTE f=0;f<count;f++) if(col==cols[f] && row==rows[f]) {
  UBYTE tile[40]={0};
  for(UBYTE y=0;y<8;y++) for(UBYTE x=0;x<8;x++) {
   UBYTE width=1+(y>>1),center=3+((oil.fireFrame+f+y)&1);
   if(x+width<center || x>center+width || (y<2 && ((x+f+oil.fireFrame)&1))) continue;
   UBYTE pen=y>4 && x>2 && x<6 ? GAME_COLOR_YELLOW : GAME_COLOR_RED;
   for(UBYTE p=0;p<4;p++) if(pen&(1<<p)) tile[y*5+p]|=128>>x;
   tile[y*5+4]|=128>>x;
  }
  drawGameScrollTileMasked(bitmap,tileX,row,tile);
 }
}
