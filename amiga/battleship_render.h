#include "assets/battleship_tiles.h"
/* Cached CPU-resident tiles avoid regenerating 64 pixels per background repair. */
typedef struct { ULONG key; UBYTE tile[40]; } BattleshipTileCache;
static BattleshipTileCache* battleshipTileCache;
/* One shared overlay path for initial drawing and every BOB background repair. */
static void battleshipDrawRow(UBYTE* bitmap,UWORD physical,LONG col,WORD row) {
    /* The logical map has 25 rows, but the world bitmap ends at row 21.
     * The raw sea writer below must obey the same clip as the tile blitter. */
    if(row<0 || row*8>=GAME_WORLD_HEIGHT)return;
    if(row>=BATTLESHIP_SEA_Y/8) {
        /* The Copper scrolls this sea independently of the ship, wrapping
         * every 256 pixels. Repeat the wave pattern at exactly that period.
         * Use physical columns so BOB repairs reproduce the same water. */
        UBYTE column=(physical-GAME_WORLD_BUFFER_MARGIN_TILES)&31;
        /* Keep the normal sea's rippled horizon instead of flattening it.
         * Select by physical phase, like the independently scrolling wavelets. */
        if(row==BATTLESHIP_SEA_Y/8)
            drawGameScrollTile(bitmap,physical,row,seaTileForColumn(column,row));
        UBYTE waveY=125+(((column*13+7)>>2)&3)*8;
        for(UBYTE y=0;y<8;y++) {
            WORD screenY=row*8+y;
            if(screenY<BATTLESHIP_SEA_Y+2)continue;
            UBYTE mask=0, color=GAME_COLOR_WHITE;
            if(!(column&3)) {
                if(screenY==waveY) mask=(column&4)?0x3c:0x7e;
                else if(screenY==waveY+1) {mask=0x18;color=GAME_COLOR_LIGHT_GREY;}
            }
            UBYTE* sea=bitmap+(ULONG)screenY*SCREEN_PLANES*GAME_WORLD_ROW_BYTES+physical;
            for(UBYTE p=0;p<GAME_WORLD_DISPLAY_PLANES;p++)
                sea[p*GAME_WORLD_ROW_BYTES]=(UBYTE)(255 ^ ((color&(1<<p))?0:mask));
        }
        return;
    }
    LONG local=col-BATTLESHIP_START_COLUMN;
    if(local<0 || local>=120) return;
    if(row>=7 && row<15) {
        UWORD tile=battleshipTileMap[(row-7)*120+local];
        const UBYTE* art=battleshipTiles+tile*40UL;
        drawGameScrollTileMasked(bitmap,physical,row,art);
    }
    for(UBYTE n=0;n<BATTLESHIP_TARGETS;n++) {
        WORD localX=col*8-battleshipTargetX[n], localY=row*8-battleship.targetY[n];
        if(localX<0 || localX>=battleshipWidth(n) || localY<0 || localY>=battleshipHeight(n)) continue;
        UBYTE slot=(localY/8)*3+localX/8;
        ULONG key=0x80000000UL | ((ULONG)battleship.hp[n]<<24) | ((ULONG)n<<20) |
            ((ULONG)slot<<16) | ((ULONG)battleship.pose[n]<<12) | ((ULONG)battleship.lift[n]<<4) | (battleship.flash[n]!=0);
        UBYTE cache=(n*9+slot)&15;
        UBYTE scratch[40];
        UBYTE* tile=battleshipTileCache ? battleshipTileCache[cache].tile : scratch;
        if(!battleshipTileCache || battleshipTileCache[cache].key!=key) {
        memset(tile,0,40);
        for(UBYTE y=0;y<8;y++) for(UBYTE x=0;x<8;x++) {
            UBYTE px=localX+x,py=localY+y,pen=0;
            UBYTE height=battleshipHeight(n),width=battleshipWidth(n);
            if(!battleship.hp[n]) {
                if(py>=height-4 && py<height-1 && px>1 && px<width-1) pen=(px+py)&1 ? 4 : 10;
            } else if(n==8) {
                /* High-contrast, broad bridge windows: the whole 24x24 block is hittable. */
                if(px>0 && px<23 && py>=4) pen=(px==1 || py==4) ? 2 : 4;
                if(py>=7 && py<=11 && px>=3 && px<=20) pen=(px%6==2) ? 4 : 7;
                if(py==13 && px>2 && px<21) pen=2;
                if(py<4 && px==11) pen=2;
            } else if(n==9) {
                if(py>=10) pen=(py==10 || px==0 || px==23) ? 2 : 4;
                if(py>=11 && py<=14 && (px==8 || px==15 || (py==12 && px>8 && px<15))) pen=6;
            } else if(n==10 || n==11) {
                if(px>=3 && px<13 && py>=2) pen=(px==3 || py==2) ? 2 : 4;
                if((px==6 && py>=5 && py<=11) || ((py==5 || py==8) && px>=6 && px<=10)) pen=6;
            } else {
                WORD sourceY=py-(16-battleship.lift[n]);
                if(sourceY>=0 && sourceY<16) {
                    const UBYTE* src=(battleshipIsMissile(n) ? carrierMissilePoses[battleship.pose[n]] : smoothAaPoses[battleship.pose[n]])+(sourceY/2)*5;
                    UBYTE mask=128>>(px/2);
                    if(src[4]&mask) for(UBYTE p=0;p<4;p++) if(src[p]&mask) pen|=1<<p;
                }
                if(!battleship.lift[n] && py==15 && px>1 && px<15) pen=4;
            }
            if(battleship.flash[n] && pen) pen=2;
            if(pen) {
                UBYTE mask=128>>x;
                for(UBYTE p=0;p<4;p++) if(pen&(1<<p)) tile[y*5+p]|=mask;
                tile[y*5+4]|=mask;
            }
        }
        if(battleshipTileCache)battleshipTileCache[cache].key=key;
        }
        drawGameScrollTileMasked(bitmap,physical,row,tile);
    }
    if(battleship.burning)for(UBYTE i=0;i<5;i++)
        if(col==battleshipFireColumn[i] && row==battleship.fireRow[i])
            drawGameScrollTileMasked(bitmap,physical,row,battleshipFireTiles[battleship.fireFrame[i]]);

}
