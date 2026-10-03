#if HAR_HEADLESS_CLASSIC_CONTRACT_TEST
/* Exercise real collision/update paths, not just a setting predicate. */
static UBYTE referenceFriendlyFireMatches(void) {
    static GameState g;
    UBYTE* buffers[GAME_WORLD_BUFFER_COUNT] = {0};
    UBYTE dirty = 0;
    if (menuFriendlyFire) return 201;
    char label[24];
    for (UBYTE enabled = 0; enabled < 2; enabled++) {
        menuFriendlyFire = enabled;
        menuItemText(MENU_ITEM_FRIENDLY_FIRE, 1, GAME_MODE_ENHANCED, 0, label);
        const char* expected = enabled ? "Friendly fire: On" : "Friendly fire: Off";
        for (UBYTE c=0; c<=strlen(expected); c++) if (label[c]!=expected[c]) return 207;
        initGameState(&g, 12040, 12040, 2);
        if (g.friendlyFire != enabled || g.gameOverPresented) return 208;
    }
    menuFriendlyFire = 0;
    for (UBYTE item = 1; item < MENU_ITEM_COUNT; item++)
        if (menuItemY(item) - menuItemY(item-1) < 10) return 209;
    if (menuItemY(MENU_ITEM_COUNT-1)+8 > SCREEN_HEIGHT) return 209;
    for (UBYTE mode = 0; mode < 2; mode++) {
        for (UBYTE enabled = 0; enabled < 2; enabled++) {
            for (UBYTE bomb = 0; bomb < 2; bomb++) {
                initGameState(&g, 12040, 12040, 1);
                g.gameMode = mode; g.friendlyFire = enabled;
                g.takeoffState = TAKEOFF_STATE_AIRBORNE;
                g.playerX = 40; g.playerY = 24;
                g.wingman.active = 1; g.wingman.destroyed = 0;
                g.wingmanControl = WINGMAN_CONTROL_PLAYER2;
                g.wingman.interceptScreenX = 160; g.wingman.screenY = 32;
                WeaponState* shot = bomb ? &g.bombShot : &g.rocketShot;
                shot->active = 1; shot->x = wingmanScreenX(&g); shot->y = 32;
                updateGameCollisions(&g, buffers, &dirty, &dirty, &dirty, &dirty);
                if (g.wingman.active != !enabled || shot->active != !enabled) return 202;
            }
        }
    }
    for (UBYTE enabled = 0; enabled < 2; enabled++) {
        initGameState(&g, 12040, 12040, 1);
        g.gameMode = GAME_MODE_ENHANCED; g.friendlyFire = enabled;
        g.defence.phase = DEFENCE_WAVE; g.defence.hull = 100;
        g.playerX = 200; g.playerY = 40; g.respawnSafeTimer = 0;
        WeaponState* m = &g.wingman.rocket;
        memset(m, 0, sizeof(*m)); m->active = 1;
        m->targetWorldX = 200L * 256; m->targetY = 40 * 256;
        carrierAdvanceMissile(&g);
        if (g.armour != (enabled ? 67 : 100) || m->active != !enabled) return 203;
        g.rocketHeightLock=0; /* Test fixed trajectory into the carrier. */
        carrierDefenceLaunch(&g, &g.rocketShot, 80, 100, MAVERICK_DIRECTION_RIGHT);
        for (UBYTE t = 0; t < 15 && g.rocketShot.active; t++) carrierDefenceShot(&g, &g.rocketShot);
        if (g.defence.hull != (enabled ? 100 - CARRIER_ROCKET_HULL_DAMAGE : 100)) return 204;
        /* Player bombs versus hull, followed by a hostile bomb at the same spot. */
        InputState in = {0}; Player2InputState p2 = {0};
        g.defence.phase = DEFENCE_SECURE; g.defence.hull = 100;
        g.takeoffState = TAKEOFF_STATE_AIRBORNE;
        g.bombShot.active = 1; g.bombShot.x = 80; g.bombShot.y = 110;
        updateCarrierDefence(&g, &in, &in, &p2, buffers);
        if (g.bombShot.active || g.defence.hull != 100 - CARRIER_BOMB_HULL_DAMAGE) return 205;
        /* Exposed AA guns share the player-bomb exception. */
        g.defence.hull=100; g.defence.raidDamaged=0;
        g.defence.gunHealth[0]=2; g.defence.gunHeight[0]=8;
        g.bombShot.active=1; g.bombShot.x=72; g.bombShot.y=104;
        updateCarrierDefence(&g,&in,&in,&p2,buffers);
        if(g.bombShot.active || g.defence.gunHealth[0]!=1 ||
            g.defence.hull!=100-CARRIER_BOMB_HULL_DAMAGE || !g.defence.raidDamaged) return 210;
        g.defence.hull = 100;
        carrierDropBomb(&g, 80, 110, 0); carrierDefenceBombs(&g);
        if (g.defence.hull != 100 - CARRIER_BOMB_HULL_DAMAGE) return 206;
    }
    return 0;
}
#endif
