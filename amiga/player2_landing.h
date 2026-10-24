/* Human P2 shares P1's fixed-point VTOL integrator during final approach.
 * Coordinate-only swaps reuse the authoritative carrier deck/tower probes;
 * no GameState copy and no persistent changes to Player 1 are made. */
static void updatePlayer2LandingVtol(GameState* g, UBYTE** buffers,
    const Player2InputState* in) {
    WingmanState* w=&g->wingman;
    if(!w->landingVtol) {
        w->landingVtol=1;w->landingHealth=100;w->landingSafe=0;
        w->landingVX=w->landingVY=w->landingAX=w->landingAY=w->landingSubX=w->landingSubY=0;
        w->landingFacing=MAVERICK_DIRECTION_RIGHT;w->landingTurn=0;w->landingPose=1;
        w->mode=WINGMAN_LANDING_APPROACH;
    }
    if(w->landingSafe)w->landingSafe--;
    if(w->mode==WINGMAN_LANDING_DECK) {
        if(!in->up)return;
        w->mode=WINGMAN_LANDING_APPROACH;w->screenY-=3;
    }
    WORD oldY=w->screenY;
    WORD x=w->interceptScreenX+carrierVtolAxis(&w->landingVX,&w->landingAX,&w->landingSubX,
        (in->right!=0)-(in->left!=0));
    WORD y=oldY+carrierVtolAxis(&w->landingVY,&w->landingAY,&w->landingSubY,
        (in->down!=0)-(in->up!=0));
    if(x<8 || x>296){w->landingVX=w->landingAX=w->landingSubX=0;x=x<8?8:296;}
    if(y<8){y=8;w->landingVY=w->landingAY=w->landingSubY=0;}
    WORD savedX=g->playerX,savedY=g->playerY;
    g->playerX=x;g->playerY=y;
    LONG hitColumn;WORD hitRow;UBYTE damage=0;
    if(playerHitsNativeCarrierObstruction(g,&hitColumn,&hitRow)) {
        while(g->playerY>8 && playerHitsNativeCarrierObstruction(g,&hitColumn,&hitRow))g->playerY--;
        y=g->playerY;damage=38;w->landingVY=-320;w->landingAY=w->landingSubY=0;
    } else if(y>=oldY && playerOnNativeCarrierDeckPixels(g)) {
        y=playerCarrierDeckY(g);
        if(w->landingVY>CARRIER_SAFE_LANDING_VY) {
            damage=30;y-=3;w->landingVY=-320;w->landingAY=w->landingSubY=0;
        } else {
            w->mode=WINGMAN_LANDING_DECK;w->landingTargetX=x;w->landingTargetSet=1;
            w->landingVX=w->landingVY=w->landingAX=w->landingAY=w->landingSubX=w->landingSubY=0;
        }
    } else if(y>=SEA_SURFACE_Y-8)damage=100;
    g->playerX=savedX;g->playerY=savedY;
    w->interceptScreenX=x;w->screenY=y;w->row=y/8;
    if(damage && (!w->landingSafe || damage==100)) {
        w->landingHealth=w->landingHealth>damage?w->landingHealth-damage:0;
        w->landingSafe=30;playSfxAt(SFX_GROUND_BOUNCE,x);
        if(!w->landingHealth){setWingmanDestroyedState(g,buffers);return;}
    }
    CarrierDefenceState pose;InputState input={0};input.left=in->left;input.right=in->right;
    pose.landed=w->mode==WINGMAN_LANDING_DECK;pose.facing=w->landingFacing;
    pose.turnTicks=w->landingTurn;pose.turnTarget=w->landingTarget;pose.vtolPose=w->landingPose;
    carrierUpdateVtolHeading(&pose,&input);
    w->landingFacing=pose.facing;w->landingTurn=pose.turnTicks;
    w->landingTarget=pose.turnTarget;w->landingPose=pose.vtolPose;
}

#if HAR_HEADLESS_P2_LANDING_TEST
static UBYTE referenceP2Landing(void) {
    static GameState g;UBYTE* buffers[GAME_WORLD_BUFFER_COUNT]={0};
    Player2InputState in={0},old={0};UBYTE dirty=0;
    initGameState(&g,12040,12040,2);g.gameMode=GAME_MODE_ENHANCED;
    g.wingmanControl=WINGMAN_CONTROL_PLAYER2;g.takeoffState=TAKEOFF_STATE_AIRBORNE;
    g.landingState=LANDING_STATE_HOVER;g.scrollX=LANDING_HOVER_SCROLL_X;
    g.wingman.active=1;g.wingman.mode=WINGMAN_LANDING_APPROACH;
    g.wingman.interceptScreenX=24;g.wingman.screenY=32;
    carrierParkedWingmanVisible=0;modPlaying=1;
    WORD px=g.playerX,py=g.playerY;
    in.right=1;
    for(UBYTE i=0;i<16;i++)updateWingmanPlayer2Control(&g,buffers,&in,&old,&dirty);
    if(g.wingman.interceptScreenX<=24 || g.wingman.landingVX<=0)return 1;
    WORD x=g.wingman.interceptScreenX,v=g.wingman.landingVX;in.right=0;
    for(UBYTE i=0;i<8;i++)updateWingmanPlayer2Control(&g,buffers,&in,&old,&dirty);
    if(g.wingman.interceptScreenX<=x || g.wingman.landingVX!=v)return 2;
    x=g.wingman.interceptScreenX;updateWingmanLanding(&g);
    if(g.wingman.interceptScreenX!=x)return 3;
    in.left=1;
    for(UBYTE i=0;i<24;i++)updateWingmanPlayer2Control(&g,buffers,&in,&old,&dirty);
    if(g.wingman.landingVX>=0 || g.wingman.landingPose!=2)return 4;
    if(g.playerX!=px || g.playerY!=py)return 5;
    in.left=0;
    /* Find a real clear landing position using the same native carrier probes. */
    WORD pad=-1;LONG col;WORD row;
    for(WORD candidate=8;candidate<296;candidate++) {
        g.playerX=candidate;g.playerY=playerCarrierDeckY(&g);
        if(playerOnNativeCarrierDeckPixels(&g) && !playerHitsNativeCarrierObstruction(&g,&col,&row)){pad=candidate;break;}
    }
    g.playerX=px;g.playerY=py;if(pad<0)return 6;
    g.wingman.interceptScreenX=pad;g.wingman.screenY=playerCarrierDeckY(&g)-1;
    g.wingman.landingVX=g.wingman.landingAX=g.wingman.landingSubX=0;
    g.wingman.landingVY=256;g.wingman.landingAY=g.wingman.landingSubY=0;
    updateWingmanPlayer2Control(&g,buffers,&in,&old,&dirty);
    if(g.wingman.mode!=WINGMAN_LANDING_DECK || g.wingman.screenY!=playerCarrierDeckY(&g))return 7;
    if(wingmanObjectMapCollision(&g))return 14;
    x=g.wingman.interceptScreenX;
    for(UBYTE i=0;i<8;i++){updateWingmanPlayer2Control(&g,buffers,&in,&old,&dirty);updateWingmanLanding(&g);}
    if(g.wingman.interceptScreenX!=x || g.wingman.screenY!=playerCarrierDeckY(&g))return 12;
    in.up=1;updateWingmanPlayer2Control(&g,buffers,&in,&old,&dirty);in.up=0;
    if(g.wingman.mode==WINGMAN_LANDING_DECK || g.wingman.screenY>=playerCarrierDeckY(&g))return 8;
    g.wingman.screenY=playerCarrierDeckY(&g)-1;g.wingman.landingVY=384;
    g.wingman.landingSubY=0;
    updateWingmanPlayer2Control(&g,buffers,&in,&old,&dirty);
    if(g.wingman.landingHealth!=70 || g.wingman.landingVY>=0 || g.wingman.mode==WINGMAN_LANDING_DECK)return 9;
    if(g.playerX!=px || g.playerY!=py)return 13;
    /* The exact oil/landing builder must emit only one opaque grey sprite,
     * preserving silhouette for all five headings, with attached DMA hidden. */
    UWORD sprite[PLAYER_SPRITE_WORDS],attach[PLAYER_SPRITE_WORDS];
    for(UBYTE pose=0;pose<5;pose++) {
        memset(attach,0xff,sizeof(attach));buildP2VtolSprite(sprite,attach,20,30,pose);
        const UBYTE* art=oilPoseArt(pose);
        for(UBYTE y=0;y<8;y++)for(UBYTE x=0;x<16;x++) {
            UWORD bit=0x8000>>x;UBYTE pen=((sprite[2+y*2]&bit)?1:0)|((sprite[3+y*2]&bit)?2:0);
            if(pen!=p2VtolPen(art[y*16+x]))return 10;
        }
        if(attach[0] || attach[1])return 11;
    }
    return 0;
}
#endif
