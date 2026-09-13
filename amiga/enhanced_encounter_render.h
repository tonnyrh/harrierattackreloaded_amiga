static __attribute__((noinline, optimize("O2"))) void eraseHelicopter(UBYTE* bitmap, UBYTE index) {
    HelicopterFootprint* fp = &helicopterFootprints[index];
    if (!fp->valid) return;
    for (UBYTE remaining = fp->count; remaining; remaining--) {
        UBYTE i = remaining - 1;
        UBYTE* dest = bitmap + (ULONG)fp->y * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + fp->byteX[i];
        const UBYTE* old = (const UBYTE*)&fp->background[i];
        UBYTE count = fp->bytes[i];
        for (UBYTE y = 0; y < 8; y++) {
            for (UBYTE plane = 0; plane < 4; plane++) {
                dest[0] = old[0];
                if (count > 1) dest[1] = old[1];
                if (count > 2) dest[2] = old[2];
                old += 3; dest += GAME_WORLD_ROW_BYTES;
            }
            dest += (SCREEN_PLANES - 4) * GAME_WORLD_ROW_BYTES;
        }
    }
    fp->valid = 0;
}

static __attribute__((noinline, optimize("O2"))) void drawHelicopterPlacement(UBYTE* dest, UBYTE* old,
    const UBYTE* source, UBYTE count) {
    for (UBYTE y = 0; y < 8; y++) {
        UBYTE keep0 = ~source[0], keep1 = ~source[1], keep2 = ~source[2];
        source += 3;
        for (UBYTE plane = 0; plane < 4; plane++) {
            old[0] = dest[0]; dest[0] = (dest[0] & keep0) | source[0];
            if (count > 1) { old[1] = dest[1]; dest[1] = (dest[1] & keep1) | source[1]; }
            if (count > 2) { old[2] = dest[2]; dest[2] = (dest[2] & keep2) | source[2]; }
            old += 3; source += 3; dest += GAME_WORLD_ROW_BYTES;
        }
        dest += (SCREEN_PLANES - 4) * GAME_WORLD_ROW_BYTES;
    }
}

/* At a ring boundary each half has a different mirror destination. Keep
 * that rare case tile-based; ordinary positions use the preshifted path. */
static __attribute__((noinline, optimize("Os"))) void drawHelicopterSeam(UBYTE* bitmap,
    HelicopterFootprint* fp, UBYTE frame) {
    fp->count = 0;
    for (UBYTE half = 0; half < 2; half++) {
        UWORD x = GAME_WORLD_BUFFER_MARGIN_PIXELS +
            (UWORD)(fp->worldX + half * 8) % (UWORD)(GAME_WORLD_SCROLL_PAGE_BYTES * 8);
        UBYTE copies = x < (GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) * 8 ? 2 : 1;
        for (UBYTE copy = 0; copy < copies; copy++) {
            UWORD byteX = (x >> 3) + (copy ? GAME_WORLD_SCROLL_PAGE_BYTES : 0);
            UBYTE shift = x & 7, count = shift ? 2 : 1, slot = fp->count++;
            if (byteX + count > GAME_WORLD_ROW_BYTES) count = GAME_WORLD_ROW_BYTES - byteX;
            fp->byteX[slot] = byteX; fp->bytes[slot] = count;
            UBYTE* dest = bitmap + (ULONG)fp->y * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + byteX;
            UBYTE* old = (UBYTE*)&fp->background[slot];
            const UBYTE* src = enhancedEncounterTiles + 160 + frame * 80 + half * 40;
            for (UBYTE row = 0; row < 8; row++, src += 5) {
                UWORD mask = ((UWORD)src[4] << 8) >> shift;
                for (UBYTE plane = 0; plane < 4; plane++) {
                    UWORD bits = ((UWORD)src[plane] << 8) >> shift;
                    old[0] = dest[0]; dest[0] = (dest[0] & ~(mask >> 8)) | (bits >> 8);
                    if (count > 1) { old[1] = dest[1]; dest[1] = (dest[1] & ~(UBYTE)mask) | (UBYTE)bits; }
                    old += 3; dest += GAME_WORLD_ROW_BYTES;
                }
                dest += (SCREEN_PLANES - 4) * GAME_WORLD_ROW_BYTES;
            }
        }
    }
}

static __attribute__((noinline, optimize("Os"))) void drawHelicopterBob(UBYTE* bitmap, UBYTE index,
    const GameState* game) {
    const WeaponState* heli = &game->helicopter;
    if (!heli->active || heli->x <= -16 || heli->x >= SCREEN_WIDTH || heli->y < 0 || heli->y + 8 > GAME_WORLD_HEIGHT) return;
    UWORD local = (UWORD)heli->worldX % (UWORD)(GAME_WORLD_SCROLL_PAGE_BYTES * 8);
    UWORD x = GAME_WORLD_BUFFER_MARGIN_PIXELS + local;
    UBYTE frame = game->helicopterHits >= 2 ? 0 : (game->helicopterAge >> 2) & 1;
    const UBYTE* source = enhancedHelicopterShifted + ((frame * 8 + (x & 7)) * 120);
    HelicopterFootprint* fp = &helicopterFootprints[index];
    fp->valid = 1; fp->y = heli->y; fp->worldX = heli->worldX;
    if (local >= (GAME_WORLD_SCROLL_PAGE_BYTES - 1) * 8 ||
        (x >> 3) == GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES - 1) {
        drawHelicopterSeam(bitmap, fp, frame); return;
    }
    fp->count = x < (GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) * 8 ? 2 : 1;
    for (UBYTE i = 0; i < fp->count; i++) {
        UWORD byteX = (x >> 3) + (i ? GAME_WORLD_SCROLL_PAGE_BYTES : 0);
        UBYTE count = (x & 7) ? 3 : 2;
        if (byteX + count > GAME_WORLD_ROW_BYTES) count = GAME_WORLD_ROW_BYTES - byteX;
        fp->byteX[i] = byteX; fp->bytes[i] = count;
        drawHelicopterPlacement(bitmap + (ULONG)heli->y * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + byteX,
            (UBYTE*)&fp->background[i], source, count);
    }
}

/* Tiny masked BOBs share the weapon renderer and its saved-byte format. */
static __attribute__((noinline, optimize("Os"))) void retireEncounterBobs(UBYTE* bitmap, UBYTE bufferIndex) {
    /* Persistent damage may retire a layer earlier than the normal late
     * group. Finish its displayed rows before restoring the saved bytes. */
    if (bufferIndex >= GAME_WORLD_BUFFER_COUNT) return;
    WORD lastY = -1;
    for (UBYTE i = 0; i < 2; i++)
        if (encounterFootprints[i][bufferIndex].valid && encounterFootprints[i][bufferIndex].y > lastY)
            lastY = encounterFootprints[i][bufferIndex].y;
    if (helicopterFootprints[bufferIndex].valid && helicopterFootprints[bufferIndex].y > lastY)
        lastY = helicopterFootprints[bufferIndex].y;
#if !HAR_HEADLESS_CLASSIC_CONTRACT_TEST
    if (lastY >= 0) {
        UWORD target = SCREEN_DIWSTRT_Y + lastY + 8;
        while (currentRasterY() <= target) { }
    }
#endif
    /* Reverse composition order is required when the halves share a byte. */
    eraseRocketPixelBobFootprint(bitmap, bufferIndex, encounterFootprints[1]);
    eraseHelicopter(bitmap, bufferIndex);
    eraseRocketPixelBobFootprint(bitmap, bufferIndex, encounterFootprints[0]);
}

static __attribute__((noinline, optimize("Os"))) void retireEncounterRegion(UBYTE* bitmap, UBYTE index, LONG x, WORD y, UWORD w, UWORD h) {
    if (index >= GAME_WORLD_BUFFER_COUNT) return;
    const HelicopterFootprint* heli = &helicopterFootprints[index];
    if (heli->valid && y < heli->y + 8 && y + h > heli->y &&
        (x >> 3) <= ((heli->worldX + 15) >> 3) && ((x + w - 1) >> 3) >= (heli->worldX >> 3)) {
        retireEncounterBobs(bitmap, index); return;
    }
    for (UBYTE i = 0; i < 2; i++) {
        const RocketShotFootprint* fp = &encounterFootprints[i][index];
        /* Saved bytes include transparent neighbours; align both X bounds. */
        if (fp->valid && y < fp->y + 8 && y + h > fp->y &&
            (x >> 3) <= ((fp->worldX + 7) >> 3) &&
            ((x + w - 1) >> 3) >= (fp->worldX >> 3)) {
            retireEncounterBobs(bitmap, index); return;
        }
    }
}

static __attribute__((noinline, optimize("Os"))) void drawEncounterTile(UBYTE* bitmap, UBYTE bufferIndex, const WeaponState* pose,
    UBYTE layer, WORD offsetX, const UBYTE* source) {
    if (bufferIndex >= GAME_WORLD_BUFFER_COUNT || !pose->active || pose->y < 0 ||
        pose->y + 8 > GAME_WORLD_HEIGHT || pose->x + offsetX <= -8 || pose->x + offsetX >= SCREEN_WIDTH) return;
    const LONG page = (LONG)GAME_WORLD_SCROLL_PAGE_BYTES * 8;
    LONG worldX = pose->worldX + offsetX;
    LONG local = worldX % page;
    if (local < 0) local += page;
    UWORD x = GAME_WORLD_BUFFER_MARGIN_PIXELS + local;
    RocketShotFootprint* saved = &encounterFootprints[layer][bufferIndex];
    saved->valid = 1; saved->placementCount = 1;
    saved->y = pose->y; saved->worldX = worldX;
    if (x < (GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) * 8) saved->placementCount = 2;
    for (UBYTE i = 0; i < saved->placementCount; i++) {
        UWORD pixelX = x + (i ? page : 0);
        saved->byteX[i] = pixelX >> 3;
        saved->byteCount[i] = drawEnhancedWeaponRows(bitmap,
            (UBYTE*)&saved->background[i], pixelX, pose->y, source, 8, 8);
    }
}

static __attribute__((noinline, optimize("Os"))) void drawEnhancedEncounterBobs(UBYTE* bitmap, UBYTE bufferIndex, const GameState* game) {
    if (game->gameMode != GAME_MODE_ENHANCED) return;
#if HAR_DEBUG_PERF_LOG
    UWORD begin = currentRasterY();
#endif
    drawEncounterTile(bitmap, bufferIndex, &game->siloMissile, 0, 0,
        enhancedWeaponRows + 7 * 40); /* Existing editable vertical missile. */
    drawHelicopterBob(bitmap, bufferIndex, game);
    /* The same small neutral plume character as Harrier failure particles,
     * at a much lower rate, with no collision-map entry. */
    static const UBYTE smoke[40] = {
        0,0x60,0,0,0x60, 0,0x90,0,0,0x90, 0,0x60,0,0,0x60,
        0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0
    };
    drawEncounterTile(bitmap, bufferIndex, &game->helicopterSmoke, 1, 0, smoke);
#if HAR_DEBUG_PERF_LOG
    UWORD end = currentRasterY();
    ULONG cost = end >= begin ? end - begin : end + 312 - begin;
    encounterCosts[1] += cost;
    if (cost > encounterCosts[5]) encounterCosts[5] = cost;
#endif
}
