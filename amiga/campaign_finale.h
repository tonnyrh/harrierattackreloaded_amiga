/* Finale only: three eight-spark groups, no Chip allocation or new samples. */
typedef struct {WORD x,y,vx,vy;UBYTE life,rocket,pen;} FinaleSpark;
typedef struct {UWORD x;UBYTE y,pen;} FinalePixel;
static struct {FinaleSpark spark[24];FinalePixel saved[128];UWORD tick,count;UBYTE active;} finale;
static void finaleErase(UBYTE* bitmap) {
 while(finale.count) {FinalePixel* p=&finale.saved[--finale.count];putPixelScroll(bitmap,p->x,p->y,p->pen);}
}
static void finalePixel(UBYTE* bitmap,UWORD x,WORD y,UBYTE pen) {
 if(x>=GAME_WORLD_BUFFER_WIDTH || y<0 || y>=102 || finale.count>=128)return;
 UBYTE mask=128>>(x&7),old=0;
 const UBYTE* row=bitmap+(ULONG)y*SCREEN_PLANES*GAME_WORLD_ROW_BYTES+(x>>3);
 for(UBYTE p=0;p<SCREEN_PLANES;p++)if(row[p*GAME_WORLD_ROW_BYTES]&mask)old|=1<<p;
 FinalePixel* save=&finale.saved[finale.count++];save->x=x;save->y=y;save->pen=old;
 putPixelScroll(bitmap,x,y,pen);
}
static void finalePoint(UBYTE* bitmap,const GameState* g,WORD x,WORD y,UBYTE pen) {
 if(x<4 || x>=316)return;
 UWORD local=((ULONG)g->scrollX+x)%(GAME_WORLD_SCROLL_PAGE_BYTES*8);
 UWORD px=GAME_WORLD_BUFFER_MARGIN_PIXELS+local;
 finalePixel(bitmap,px,y,pen);
 if(px<(GAME_WORLD_BUFFER_MARGIN_TILES+GAME_FETCH_BYTES)*8)
  finalePixel(bitmap,px+GAME_WORLD_SCROLL_PAGE_BYTES*8,y,pen);
}
static void finaleStart(void) {memset(&finale,0,sizeof(finale));finale.active=1;}
static void finaleDraw(UBYTE* bitmap,const GameState* g) {
 if(!finale.active)return;
 UWORD t=finale.tick++;
 if(t>=480){finale.active=0;return;}
 if(t<365 && !(t%28)) {
  UBYTE slot=(t/28)%3;FinaleSpark* s=&finale.spark[slot*8];
  s->x=(g->playerX+((t/28)%3)*24-8)*16;s->y=100*16;
  s->vx=(WORD)((t/28)%3)*6-6;s->vy=-38-(t/28%3)*5;
  s->life=28;s->rocket=1;s->pen=1;
  playSfxAtTuned(SFX_FIRE,s->x/16,22,SFX_PAULA_PERIOD+100);
 }
 static const BYTE dx[8]={16,11,0,-11,-16,-11,0,11};
 static const BYTE dy[8]={0,11,16,11,0,-11,-16,-11};
 static const UBYTE colors[4]={1,6,14,9};
 for(UBYTE i=0;i<24;i++) {
  FinaleSpark* s=&finale.spark[i];if(!s->life)continue;
  s->x+=s->vx;s->y+=s->vy;s->life--;
  if(s->rocket && !s->life) {
   WORD x=s->x,y=s->y;UBYTE pen=colors[(t/28)&3];
   playSfxAtTuned(SFX_GROUND_TARGET_HIT_1+(t/28)%3,x/16,44,SFX_PAULA_PERIOD+(t%3)*45);
   for(UBYTE n=0;n<8;n++) {FinaleSpark* q=&finale.spark[(i/8)*8+n];
    q->x=x;q->y=y;q->vx=dx[n]*2;q->vy=dy[n]*2-8;q->life=42;q->rocket=0;q->pen=pen;
   }
  }
  if(!s->rocket && !(t&1))s->vy++;
  if(s->rocket || s->life>14 || !((t+i)&1)) {
   WORD x=s->x/16,y=s->y/16;
   finalePoint(bitmap,g,x,y,s->life>34?1:s->pen);
   if(s->rocket) {finalePoint(bitmap,g,x,y+1,6);finalePoint(bitmap,g,x,y+3,9);}
   else if(s->life>24)finalePoint(bitmap,g,x+1,y,s->pen);
  }
 }
}
#if HAR_HEADLESS_FINALE_TEST
static UBYTE referenceFinale(void) {
 UBYTE* b=AllocMem(GAME_WORLD_BITMAP_BYTES,MEMF_CHIP);if(!b)return 1;
 memset(b,0x5a,GAME_WORLD_BITMAP_BYTES);static GameState g;
 initGameState(&g,123,123,3);g.playerX=88;g.scrollX=LANDING_HOVER_SCROLL_X;
 UBYTE music=modPlaying;modPlaying=1;finaleStart();UWORD peak=0;
 for(UWORD t=0;t<481;t++) {finaleErase(b);finaleDraw(b,&g);if(finale.count>peak)peak=finale.count;}
 finaleErase(b);UBYTE result=finale.active || !peak || peak>128 ? 2:0;
 for(ULONG n=0;n<GAME_WORLD_BITMAP_BYTES;n++)if(b[n]!=0x5a){result=3;break;}
 /* Export an actual renderer frame for host-side pixel inspection. */
 memset(b,0,GAME_WORLD_BITMAP_BYTES);g.scrollX=0;
 fillRectScroll(b,0,120,GAME_WORLD_BUFFER_WIDTH,GAME_WORLD_HEIGHT-120,GAME_COLOR_SEA);
 drawPromotedCpcCarrierAt(b,GAME_WORLD_BUFFER_MARGIN_PIXELS+64,64);
 finaleStart();for(UWORD t=0;t<115;t++){finaleErase(b);finaleDraw(b,&g);}
 BPTR file=Open((CONST_STRPTR)"DH1:finale-preview.bpl",MODE_NEWFILE);
 if(file){Write(file,b,GAME_WORLD_BITMAP_BYTES);Close(file);}
 finaleErase(b);memset(&finale,0,sizeof(finale));
 modPlaying=music;FreeMem(b,GAME_WORLD_BITMAP_BYTES);return result;
}
#endif
