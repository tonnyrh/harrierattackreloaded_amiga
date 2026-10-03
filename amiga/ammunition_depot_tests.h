#if HAR_HEADLESS_CLASSIC_CONTRACT_TEST
static UBYTE referenceAmmoDepotMatches(void) {
    static GameState g;
    initGameState(&g,12040,12040,1);
    g.gameMode=GAME_MODE_ENHANCED; currentWorldPresentationMode=GAME_MODE_ENHANCED;
    LONG origin=-1;
    for (UWORD c=0;c<currentGameLevelWidthTiles;c++) if(isAmmoDepotColumn(c)) {
        if (isFuelDepotColumn(c) || (origin>=0 && c-origin<160)) return 0;
        origin=c;
    }
    if(origin<0 || !ammoDepotCount) return 0;
    for (UWORD c=0;c<currentGameLevelWidthTiles;c++) if(isAmmoDepotColumn(c)) {origin=c;break;}
    g.gameMode=GAME_MODE_CLASSIC; startAmmoDepotBlast(&g,origin);
    if(g.ammoBlast.active) return 0;
    g.gameMode=GAME_MODE_ENHANCED; g.scrollX=origin*8-160;
    startAmmoDepotBlast(&g,origin);
    if(!g.ammoBlast.active || g.ammoBlast.timer!=36) return 0;
    markTargetDestroyedAtColumn(origin);
    UWORD hits=g.hitsCount;
    for(UBYTE t=0;t<40;t++) updateAmmoDepotBlast(&g,0);
    if(g.ammoBlast.active || g.hitsCount<=hits || g.hitsCount>hits+2) return 0;
    for(UWORD c=0;c<currentGameLevelWidthTiles;c++)
        if(isFuelDepotColumn(c) && isTargetDestroyedAtColumn(c)) return 0;
    startAmmoDepotBlast(&g,origin);
    if(g.ammoBlast.active) return 0;
    /* Fuel uses the same burst but must never chain into neighbours. */
    for(UWORD c=0;c<currentGameLevelWidthTiles;c++) if(isFuelDepotColumn(c)) {
        startAmmoDepotBlast(&g,c); hits=g.hitsCount;
        if(!g.ammoBlast.active || g.ammoBlast.guidanceDistance!=2) return 0;
        for(UBYTE t=0;t<40;t++) updateAmmoDepotBlast(&g,0);
        if(g.hitsCount!=hits || g.ammoBlast.active) return 0;
        break;
    }
    /* Real retained explosion drawing must restore both ring placements,
     * including the seam, without changing any neighbouring byte. */
    UBYTE* pixels=AllocMem(2UL*GAME_WORLD_BITMAP_BYTES,MEMF_PUBLIC);
    if(!pixels) return 0;
    UBYTE* expected=pixels+GAME_WORLD_BITMAP_BYTES;
    UBYTE ok=1;
    static const LONG positions[]={400, 404, GAME_WORLD_SCROLL_PAGE_BYTES*8-8};
    for(UBYTE pass=0;pass<3 && ok;pass++) {
        memset(pixels,0x5a,GAME_WORLD_BITMAP_BYTES);
        resetRocketShotPixelBobFootprints(); resetBombShotPixelBobFootprints();
        g.ammoBlast.active=1; g.ammoBlast.worldX=positions[pass];
        g.ammoBlast.y=72; g.scrollX=positions[pass]-100;
        for(UBYTE phase=0;phase<3;phase++) {
            g.ammoBlast.timer=36-phase*12;
            updateCarrierBomberBob(pixels,0,&g);
            if(!bomberValid[0]) ok=0;
            memset(expected,0x5a,GAME_WORLD_BITMAP_BYTES);
            for(UBYTE n=0;n<4;n++) {
                LONG world=positions[pass]+(n&1)*8;
                UWORD x=GAME_WORLD_BUFFER_MARGIN_PIXELS+world%(GAME_WORLD_SCROLL_PAGE_BYTES*8);
                UBYTE saved[64];
                const UBYTE* art=ammunitionDepotBlast+phase*160+n*40;
                drawEnhancedWeaponRows(expected,saved,x,72+(n>>1)*8,art,8,8);
                if(x<(GAME_WORLD_BUFFER_MARGIN_TILES+GAME_FETCH_BYTES)*8)
                    drawEnhancedWeaponRows(expected,saved,x+GAME_WORLD_SCROLL_PAGE_BYTES*8,72+(n>>1)*8,art,8,8);
            }
            if(!referenceBuffersEqual(pixels,expected,GAME_WORLD_BITMAP_BYTES)) ok=0;
            ULONG before=bomberDrawCount;
            g.scrollX+=3; updateCarrierBomberBob(pixels,0,&g);
            if(bomberDrawCount!=before) ok=0;
        }
        g.ammoBlast.active=0; updateCarrierBomberBob(pixels,0,&g);
        for(ULONG i=0;i<GAME_WORLD_BITMAP_BYTES;i++) if(pixels[i]!=0x5a) {ok=0;break;}
    }
    FreeMem(pixels,2UL*GAME_WORLD_BITMAP_BYTES);
    initGameState(&g,12040,12040,1);
    return ok && !g.ammoBlast.active;
}
#endif
