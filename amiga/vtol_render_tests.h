/* Pixel oracle for the periodic VTOL drawing paths. Test builds only. */
static UBYTE referenceVtolPeriodicRendering(void) {
    UBYTE* actual=AllocMem(GAME_WORLD_BITMAP_BYTES,MEMF_PUBLIC);
    UBYTE* expected=AllocMem(GAME_WORLD_BITMAP_BYTES,MEMF_PUBLIC);
    UBYTE result=0;
    static SeaWaveFootprint saved[SEA_WAVE_MAX];
    static GameState g;
    ULONG oldFrame=frameCounter;
    if(!actual || !expected) result=1;
    if(!result) {
        initGameState(&g,12040,12040,1);
        g.gameMode=GAME_MODE_ENHANCED;
        currentWorldPresentationMode=GAME_MODE_ENHANCED;
        carrierSceneHull=255;
        for(UBYTE seam=0;seam<2;seam++) {
            g.scrollX=seam ? (GAME_WORLD_SCROLL_PAGE_BYTES-8)*8 : 0;
            memset(actual,0x55,GAME_WORLD_BITMAP_BYTES);
            memset(seaWaveFootprints,0,sizeof(seaWaveFootprints));
            seaWaveCandidatesValid=0;
            frameCounter=0;
            drawSeaWavesFrom(actual,0,&g,0);
            for(UBYTE tick=1;tick<=4*SEA_WAVE_PHASE_FRAMES;tick++) {
                frameCounter=tick;
                updateSeaWavePhasesInPlace(actual,0);
                memcpy(saved,seaWaveFootprints[0],sizeof(saved));
                memset(expected,0x55,GAME_WORLD_BITMAP_BYTES);
                memset(seaWaveFootprints,0,sizeof(seaWaveFootprints));
                drawSeaWavesFrom(expected,0,&g,0);
                if(!referenceBuffersEqual(actual,expected,GAME_WORLD_BITMAP_BYTES)) result=2;
                memcpy(seaWaveFootprints[0],saved,sizeof(saved));
            }
        }
        memset(seaWaveFootprints,0,sizeof(seaWaveFootprints));
        resetRocketShotPixelBobFootprints();
        carrierDefenceSinkPixels=carrierWingmanLiftDepth=0;
        carrierParkedWingmanVisible=1;
        g.wingmanControl=WINGMAN_CONTROL_CPU; g.wingman.destroyed=0;
        g.defence.phase=DEFENCE_WAVE;
        memset(actual,0x55,GAME_WORLD_BITMAP_BYTES);
        for(LONG col=17;col<=19;col++) renderRingWorldColumn(actual,col);
        memcpy(expected,actual,GAME_WORLD_BITMAP_BYTES);
        UBYTE* buffers[]={actual};
        for(UBYTE tick=1;tick<=16;tick++) {
            if(tick==9) g.defence.phase=DEFENCE_SECURE;
            g.defence.clock=tick*6;
            carrierUpdateDeckLift(&g,buffers);
            for(LONG col=17;col<=19;col++) renderRingWorldColumn(expected,col);
            if(!referenceBuffersEqual(actual,expected,GAME_WORLD_BITMAP_BYTES)) result=3;
        }
    }
    frameCounter=oldFrame;
    if(actual) FreeMem(actual,GAME_WORLD_BITMAP_BYTES);
    if(expected) FreeMem(expected,GAME_WORLD_BITMAP_BYTES);
    return result;
}
