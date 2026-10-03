#if HAR_HEADLESS_AUDIO_REFINEMENT_TEST_ONLY
/* Exercise scheduling against RAM-backed custom registers. Real DMA timing
 * still belongs to the emulator/hardware; this detects wrong reload lengths,
 * volume scaling, accidental repeats and a drone left running after death. */
static UBYTE referenceAudioRefinements(void) {
    static struct Custom registers;
    volatile struct Custom* hardware=custom;
    UBYTE oldMod=modPlaying, result=0;
    stopAllSfx(); custom=&registers; modPlaying=0;
    playSfxAt(SFX_DEPOT_BOOM,160);
    UBYTE c=0; while(c<4 && sfxChannelCurrentId[c]!=SFX_DEPOT_BOOM) c++;
    if(c==4) result=1;
    if(!result) {
        updateSfx();
        if(custom->aud[c].ac_len!=sizeof(sfxGroundMissSample)/6 || custom->aud[c].ac_vol!=64) result=2;
        updateSfx();
        if(custom->aud[c].ac_len!=sizeof(sfxGroundMissSample)/2 || depotBoomReload[c]) result=3;
        UBYTE ticks=sfxChannelSilenceQueueDelay[c];
        for(UBYTE i=0;i<ticks;i++) updateSfx();
        if(custom->aud[c].ac_len!=1 || custom->aud[c].ac_ptr!=(volatile UWORD*)sfxSilenceLoop) result=4;
        for(UWORD i=0;i<200;i++) updateSfx();
        if(sfxChannelFrames[c]) result=5;
    }
    static GameState g; memset(&g,0,sizeof(g));
    g.defence.phase=DEFENCE_WAVE; g.defence.jetType=2; g.enemyPlane.active=1;
    serviceBomberDrone(&g); updateSfx();
    c=0; while(c<4 && sfxChannelCurrentId[c]!=SFX_BOMBER_DRONE) c++;
    if(c==4) result=6;
    else {
        if(custom->aud[c].ac_len!=128 || sfxChannelSilenceQueueDelay[c]) result=7;
        g.defence.jetHits=1; g.defence.clock=30; serviceBomberDrone(&g);
        if(custom->aud[c].ac_vol) result=8;
        g.defence.clock=0; serviceBomberDrone(&g);
        if(custom->aud[c].ac_vol!=AUDIO_MIX_VOLUME(12)) result=9;
        g.enemyPlane.active=0; serviceBomberDrone(&g);
        if(sfxChannelFrames[c]) result=10;
    }
    stopAllSfx(); custom=hardware; modPlaying=oldMod; return result;
}
#endif
