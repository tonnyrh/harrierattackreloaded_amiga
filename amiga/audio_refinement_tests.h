#if HAR_HEADLESS_AUDIO_REFINEMENT_TEST_ONLY
/* Exercise scheduling against RAM-backed custom registers. Real DMA timing
 * still belongs to the emulator/hardware; this detects wrong reload lengths,
 * volume scaling, accidental repeats and a drone left running after death. */
static UBYTE referenceAudioRefinements(void) {
    static struct Custom registers;
    volatile struct Custom* hardware=custom;
    UBYTE oldMod=modPlaying, result=0;
    stopAllSfx(); custom=&registers; modPlaying=0;
    UBYTE savedVariant=depotBoomVariant, c=0;
    depotBoomVariant=0;
    /* All three pairs, including wrapping 2 -> 0. Rate changes must leave
     * the half-first/full-second reload and final silence intact. */
    for(UBYTE variant=0;variant<3 && !result;variant++) {
        stopAllSfx();
        playSfxAt(SFX_DEPOT_BOOM,160);
        c=0; while(c<4 && sfxChannelCurrentId[c]!=SFX_DEPOT_BOOM) c++;
        if(c==4) { result=1; break; }
        const SfxSample* first=&sfxSamples[SFX_GROUND_TARGET_HIT_1+variant];
        const SfxSample* second=&sfxSamples[SFX_GROUND_TARGET_HIT_1+(variant+1)%3];
        updateSfx();
        if(custom->aud[c].ac_len!=((first->byteLength/2)&~1U)/2 ||
            custom->aud[c].ac_ptr!=(volatile UWORD*)first->data ||
            custom->aud[c].ac_per!=SFX_PAULA_PERIOD || custom->aud[c].ac_vol!=64) result=2;
        updateSfx();
        if(custom->aud[c].ac_len!=second->byteLength/2 ||
            custom->aud[c].ac_ptr!=(volatile UWORD*)second->data || depotBoomReload[c]) result=3;
        UBYTE ticks=sfxChannelSilenceQueueDelay[c];
        for(UBYTE i=0;i<ticks;i++) updateSfx();
        if(custom->aud[c].ac_len!=1 || custom->aud[c].ac_ptr!=(volatile UWORD*)sfxSilenceLoop) result=4;
        for(UWORD i=0;i<200;i++) updateSfx();
        if(sfxChannelFrames[c]) result=5;
    }
    if(depotBoomVariant!=0) result=11;
    depotBoomVariant=savedVariant;
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
        if(custom->aud[c].ac_vol!=AUDIO_MIX_VOLUME(20)) result=9;
        g.enemyPlane.active=0; serviceBomberDrone(&g);
        if(sfxChannelFrames[c]) result=10;
    }
    stopAllSfx();
    static UBYTE idlePcm[CARRIER_IDLE_DECODE_BUFFER_BYTES];
    UBYTE* savedIdle=carrierIdleDecodeBuffer;
    carrierIdleDecodeBuffer=idlePcm;
    for(UWORD tick=0;tick<150;tick++) { updateCarrierIdleSfx(1); updateSfx(); }
    if(carrierIdleChannel!=0xff || carrierIdlePreparedVariant!=0 ||
        !carrierIdlePreparedLength || carrierIdlePreparedLength>sizeof(idlePcm)) result=12;
    carrierGullCallPending=1; updateCarrierIdleSfx(1); updateSfx();
    c=carrierIdleChannel;
    if(c>=4 || carrierGullCallPending) result=13;
    else {
        if(sfxChannelCurrentId[c]!=SFX_CARRIER_IDLE_1 ||
            custom->aud[c].ac_ptr!=(volatile UWORD*)idlePcm) result=14;
        updateCarrierIdleSfx(0);
        if(sfxChannelFrames[c] || carrierIdleChannel!=0xff) result=15;
    }
    carrierGullCallPending=1; updateCarrierIdleSfx(0);
    if(carrierGullCallPending) result=16;
    carrierIdleDecodeBuffer=savedIdle;
    stopAllSfx(); custom=hardware; modPlaying=oldMod; return result;
}
#endif
