static const UBYTE carrierDefenceBombRows[40] = {
    0,0x18,0x18,0,0x18, 0,0x18,0,0x18,0x18,
    0,0x3c,0,0x3c,0x3c, 0,0x3c,0,0x3c,0x3c,
    0,0x18,0,0x18,0x18, 0,0x18,0x18,0,0x18,
    0,0,0,0,0, 0,0,0,0,0
};
/* Single pixels save/restore their own bit, never a whole neighbour byte. */
static __attribute__((noinline, optimize("Os"))) void eraseHelicopterBullets(UBYTE* bitmap, UBYTE index) {
    for (UBYTE n = HELICOPTER_BULLET_MAX; n; n--) {
        BulletFootprint* fp = &bulletFootprints[n - 1][index];
        if (!fp->valid) continue;
        for (UBYTE copy = 0; copy < fp->count; copy++) {
            UBYTE* dest = bitmap + (ULONG)fp->y * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + fp->byteX[copy];
            for (UBYTE plane = 0; plane < 4; plane++, dest += GAME_WORLD_ROW_BYTES)
                *dest = (*dest & ~fp->mask) | fp->old[copy][plane];
        }
        fp->valid = 0;
    }
}

static __attribute__((noinline, optimize("Os"))) void drawHelicopterBullets(UBYTE* bitmap, UBYTE index, const GameState* game) {
    for (UBYTE i = 0; i < HELICOPTER_BULLET_MAX; i++) {
        const HelicopterBullet* b = &game->helicopterBullets[i];
        WORD sx = (WORD)(b->worldX - game->scrollX);
        if (!b->active || sx < 0 || sx >= SCREEN_WIDTH || b->y < 0 || b->y >= GAME_WORLD_HEIGHT) continue;
        UWORD x = GAME_WORLD_BUFFER_MARGIN_PIXELS + (UWORD)b->worldX % (UWORD)(GAME_WORLD_SCROLL_PAGE_BYTES * 8);
        BulletFootprint* fp = &bulletFootprints[i][index];
        fp->valid = 1; fp->y = b->y; fp->worldX = b->worldX; fp->mask = 0x80 >> (x & 7);
        fp->count = x < (GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) * 8 ? 2 : 1;
        for (UBYTE copy = 0; copy < fp->count; copy++) {
            fp->byteX[copy] = (x >> 3) + (copy ? GAME_WORLD_SCROLL_PAGE_BYTES : 0);
            UBYTE* dest = bitmap + (ULONG)b->y * SCREEN_PLANES * GAME_WORLD_ROW_BYTES + fp->byteX[copy];
            for (UBYTE plane = 0; plane < 4; plane++, dest += GAME_WORLD_ROW_BYTES) {
                fp->old[copy][plane] = *dest & fp->mask;
                /* Carrier and helicopter share the stable white playfield pen. */
                *dest = (*dest & ~fp->mask) | ((GAME_COLOR_WHITE & (1 << plane)) ? fp->mask : 0);
            }
        }
    }
}

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
            const UBYTE* src = (frame >= 2 ? enhancedHelicopterMirrored + (frame - 2) * 80 : enhancedEncounterTiles + 160 + frame * 80) + half * 40;
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
    /* Packer base frames face right; the mirrored bank faces left. */
    frame += heli->direction ? 0 : 2;
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
static __attribute__((noinline, optimize("Os"))) void retireEncounterTransientBobs(UBYTE* bitmap, UBYTE bufferIndex) {
    /* Persistent damage may retire a layer earlier than the normal late
     * group. Finish its displayed rows before restoring the saved bytes. */
    if (bufferIndex >= GAME_WORLD_BUFFER_COUNT) return;
    WORD lastY = -1;
    for (UBYTE i = 0; i < HELICOPTER_BULLET_MAX; i++)
        if (bulletFootprints[i][bufferIndex].valid && bulletFootprints[i][bufferIndex].y > lastY)
            lastY = bulletFootprints[i][bufferIndex].y;
    for (UBYTE i = 0; i < ENCOUNTER_TILE_LAYERS; i++)
        if ((i<6 || i>=14) && encounterFootprints[i][bufferIndex].valid && encounterFootprints[i][bufferIndex].y > lastY)
            lastY = encounterFootprints[i][bufferIndex].y;
    if (helicopterFootprints[bufferIndex].valid && helicopterFootprints[bufferIndex].y > lastY)
        lastY = helicopterFootprints[bufferIndex].y;
    /* No saved pixels exist: avoid the per-layer retirement calls on land. */
    if (lastY < 0) return;
#if !HAR_HEADLESS_CLASSIC_CONTRACT_TEST
    if (lastY >= 0) {
        UWORD target = SCREEN_DIWSTRT_Y + lastY + 8;
        while (currentRasterY() <= target) { }
    }
#endif
    for (UBYTE i=ENCOUNTER_TILE_LAYERS;i>14;i--)
        eraseRocketPixelBobFootprint(bitmap,bufferIndex,encounterFootprints[i-1]);
    for (UBYTE i = 6; i > 2; i--)
        eraseRocketPixelBobFootprint(bitmap, bufferIndex, encounterFootprints[i - 1]);
    eraseHelicopterBullets(bitmap, bufferIndex);
    /* Reverse composition order is required when the halves share a byte. */
    eraseRocketPixelBobFootprint(bitmap, bufferIndex, encounterFootprints[1]);
    eraseHelicopter(bitmap, bufferIndex);
    eraseRocketPixelBobFootprint(bitmap, bufferIndex, encounterFootprints[0]);
}

/* Bomber is below moving projectiles. It persists until its integer pose
 * changes or an underlying world/impact region invalidates the snapshot. */
static void carrierCopyBlit(const void* source, void* destination, WORD sourceModulo, WORD destinationModulo) {
    WaitBlit();
    custom->bltcon0 = 0x09f0; /* A -> D */
    custom->bltcon1 = 0;
    custom->bltafwm = custom->bltalwm = 0xffff;
    custom->bltapt = (APTR)source; custom->bltdpt = destination;
    custom->bltamod = sourceModulo; custom->bltdmod = destinationModulo;
    custom->bltsize = (CARRIER_BLIT_ROWS << 6) | 3;
    WaitBlit();
}

static UBYTE carrierCanBlit(const WeaponState* tile) {
    return carrierBlitMemory && tile->x == tile->worldX && tile->x > -32 &&
        tile->x < SCREEN_WIDTH && tile->y >= 0 && tile->y + 16 <= GAME_WORLD_HEIGHT;
}

/* Damage is a pose change, not an animated overlay. Apply small scorch/
 * burning-engine masks while packing; no extra resident aircraft bank. */
static UBYTE carrierBomberScar(UBYTE pose,UBYTE column,UBYTE y) {
    if(pose<8) return 0;
    UBYTE damage=1+(pose-8)/2, mirror=pose&1;
    if(mirror) column=3-column;
    /* Sparse, irregular engine scars leave the original shading visible.
     * Never replace a solid rectangle of fuselage with a flat black pen. */
    static const UBYTE engineScar[7]={0x00,0x08,0x14,0x0c,0x10,0x08,0x00};
    static const UBYTE heavyScar[7]={0x02,0x00,0x03,0x01,0x02,0x00,0x01};
    UBYTE bits=0;
    if(y>=5 && y<=11) {
        if(column==1 || (damage>=2 && column==2)) bits=engineScar[y-5];
        if(damage>=3 && column==1) bits|=heavyScar[y-5];
    }
    if(mirror) {bits=(bits>>4)|(bits<<4); bits=((bits&0xcc)>>2)|((bits&0x33)<<2); bits=((bits&0xaa)>>1)|((bits&0x55)<<1);}
    return bits;
}
static UBYTE carrierBomberDamagePen(UBYTE pose) {(void)pose; return 4;}

static void prepareCarrierBlitPose(UBYTE pose, const UBYTE* art, WORD x) {
    UBYTE clip=x < -GAME_WORLD_BUFFER_MARGIN_PIXELS ? -x-GAME_WORLD_BUFFER_MARGIN_PIXELS : 0;
    if (carrierBlitPose != pose || carrierBlitClip != clip) {
        /* Every row ends in a zero word: shifter carry cannot leak into
         * the next plane/row. Mask zero preserves the unused fifth plane. */
        memset(carrierBlitMemory->image,0,sizeof(carrierBlitMemory->image));
        memset(carrierBlitMemory->mask,0,sizeof(carrierBlitMemory->mask));
        for (UBYTE y=0;y<16;y++) for (UBYTE p=0;p<GAME_WORLD_DISPLAY_PLANES;p++) {
            UWORD* image=carrierBlitMemory->image[y*SCREEN_PLANES+p];
            UWORD* mask=carrierBlitMemory->mask[y*SCREEN_PLANES+p];
            for (UBYTE half=0;half<2;half++) {
                const UBYTE* a=art+((y>>3)*4+half*2)*40+(y&7)*5;
                mask[half]=((UWORD)a[4]<<8)|a[44];
                image[half]=(((UWORD)a[p]<<8)|a[40+p])&mask[half];
                if(pose>=8) {
                    UWORD scar=(((UWORD)carrierBomberScar(pose,half*2,y)<<8)|carrierBomberScar(pose,half*2+1,y))&mask[half];
                    image[half]=(image[half]&~scar)|((carrierBomberDamagePen(pose)&(1<<p)) ? scar : 0);
                }
            }
            if(clip) {
                ULONG bits=(((ULONG)image[0]<<16)|image[1])<<clip;
                ULONG opaque=(((ULONG)mask[0]<<16)|mask[1])<<clip;
                image[0]=bits>>16; image[1]=bits;
                mask[0]=opaque>>16; mask[1]=opaque;
            }
        }
        carrierBlitPose=pose; carrierBlitClip=clip;
    }
}

static void drawCarrierBlit(UBYTE* bitmap, UBYTE index, const WeaponState* tile) {
    UWORD x=tile->x < -GAME_WORLD_BUFFER_MARGIN_PIXELS ? 0 : GAME_WORLD_BUFFER_MARGIN_PIXELS+tile->x;
    UWORD byteX=(x>>4)*2;
    UBYTE* dest=bitmap+(ULONG)tile->y*SCREEN_PLANES*GAME_WORLD_ROW_BYTES+byteX;
    carrierCopyBlit(dest,carrierBlitMemory->background[index],GAME_WORLD_ROW_BYTES-6,0);
    custom->bltcon0=((x&15)<<12)|0x0fca; /* (A & B) | (~A & C) */
    custom->bltcon1=(x&15)<<12;
    custom->bltafwm=custom->bltalwm=0xffff;
    custom->bltadat=custom->bltbdat=0;
    custom->bltapt=(APTR)carrierBlitMemory->mask;
    custom->bltbpt=(APTR)carrierBlitMemory->image;
    custom->bltcpt=dest; custom->bltdpt=dest;
    custom->bltamod=custom->bltbmod=0;
    custom->bltcmod=custom->bltdmod=GAME_WORLD_ROW_BYTES-6;
    custom->bltsize=(CARRIER_BLIT_ROWS<<6)|3;
    WaitBlit(); /* Later CPU overlays must see the completed image. */
    bomberBlitByteX[index]=byteX; bomberBlitValid[index]=1;
}

static __attribute__((noinline, optimize("Os"))) void eraseCarrierBomberBob(UBYTE* bitmap, UBYTE index) {
    if (bomberBlitValid[index]) {
        carrierCopyBlit(carrierBlitMemory->background[index],
            bitmap+(ULONG)bomberY[index]*SCREEN_PLANES*GAME_WORLD_ROW_BYTES+bomberBlitByteX[index],
            0,GAME_WORLD_ROW_BYTES-6);
        bomberBlitValid[index]=0;
    }
    for (UBYTE i=14; i>6; i--)
        eraseRocketPixelBobFootprint(bitmap,index,encounterFootprints[i-1]);
    bomberValid[index]=0;
}
static __attribute__((noinline, optimize("Os"))) void retireEncounterBobs(UBYTE* bitmap, UBYTE index) {
    if (index>=GAME_WORLD_BUFFER_COUNT) return;
    retireEncounterTransientBobs(bitmap,index);
    if (!bomberValid[index]) return;
    eraseRocketPixelBobFootprint(bitmap,index,rocketShotFootprints);
    eraseRocketPixelBobFootprint(bitmap,index,wingmanRocketFootprints);
    eraseRocketPixelBobFootprint(bitmap,index,enemyMissileFootprints);
    eraseBombPixelBobFootprint(bitmap,index,bombShotFootprints);
    eraseBombPixelBobFootprint(bitmap,index,wingmanBombFootprints);
#if !HAR_HEADLESS_CLASSIC_CONTRACT_TEST
    while (currentRasterY() <= SCREEN_DIWSTRT_Y+bomberY[index]+16) { }
#endif
    eraseCarrierBomberBob(bitmap,index);
}
static __attribute__((noinline, optimize("Os"))) UBYTE encounterRegionHits(const RocketShotFootprint* fp,
    LONG x, WORD y, UWORD w, UWORD h) {
    return fp->valid && y<fp->y+8 && y+h>fp->y &&
        (x>>3)<=((fp->worldX+7)>>3) && ((x+w-1)>>3)>=(fp->worldX>>3);
}
static __attribute__((noinline, optimize("Os"))) void retireEncounterTransientRegion(UBYTE* bitmap, UBYTE index,
    LONG x, WORD y, UWORD w, UWORD h) {
    if(index>=GAME_WORLD_BUFFER_COUNT) return;
    for (UBYTE i=0;i<HELICOPTER_BULLET_MAX;i++) {
        const BulletFootprint* fp=&bulletFootprints[i][index];
        if(fp->valid && y<=fp->y && y+h>fp->y && (x>>3)<=(fp->worldX>>3) && ((x+w-1)>>3)>=(fp->worldX>>3)) {
            retireEncounterTransientBobs(bitmap,index); return;
        }
    }
    const HelicopterFootprint* heli=&helicopterFootprints[index];
    if(heli->valid && y<heli->y+8 && y+h>heli->y && (x>>3)<=((heli->worldX+15)>>3) && ((x+w-1)>>3)>=(heli->worldX>>3)) {
        retireEncounterTransientBobs(bitmap,index); return;
    }
    for(UBYTE i=0;i<ENCOUNTER_TILE_LAYERS;i++) if((i<6 || i>=14) && encounterFootprints[i][index].valid && encounterRegionHits(&encounterFootprints[i][index],x,y,w,h)) {
        retireEncounterTransientBobs(bitmap,index); return;
    }
}
static __attribute__((noinline, optimize("Os"))) void retireEncounterRegion(UBYTE* bitmap, UBYTE index,
    LONG x, WORD y, UWORD w, UWORD h) {
    if(index>=GAME_WORLD_BUFFER_COUNT) return;
    if(bomberBlitValid[index]) {
        LONG left=(LONG)bomberBlitByteX[index]*8-GAME_WORLD_BUFFER_MARGIN_PIXELS;
        if(y<bomberY[index]+16 && y+h>bomberY[index] &&
            (x>>3)<=((left+47)>>3) && ((x+w-1)>>3)>=(left>>3)) {
            retireEncounterBobs(bitmap,index); return;
        }
    }
    for(UBYTE i=6;i<14;i++) if(encounterFootprints[i][index].valid && encounterRegionHits(&encounterFootprints[i][index],x,y,w,h)) {
        retireEncounterBobs(bitmap,index); return;
    }
    retireEncounterTransientRegion(bitmap,index,x,y,w,h);
}

/* Byte-aligned terrain blasts need no shifts or second-byte branches. */
static __attribute__((noinline, optimize("O2"))) void drawAlignedEncounterRows(
    UBYTE* dest, UBYTE* saved, const UBYTE* source) {
    for (UBYTE y=0;y<8;y++) {
        UBYTE keep=~source[4];
#define AMMO_PLANE(p) do { \
    saved[(p)*2]=dest[(p)*GAME_WORLD_ROW_BYTES]; \
    dest[(p)*GAME_WORLD_ROW_BYTES]=(dest[(p)*GAME_WORLD_ROW_BYTES]&keep)|(source[p]&~keep); \
} while(0)
        AMMO_PLANE(0); AMMO_PLANE(1); AMMO_PLANE(2); AMMO_PLANE(3);
#undef AMMO_PLANE
        dest+=SCREEN_PLANES*GAME_WORLD_ROW_BYTES; saved+=8; source+=5;
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
    /* The bomber only flies in the stationary carrier arena (scroll zero).
     * Its visible primary page needs no duplicate offscreen mirror write. */
    if (x < (GAME_WORLD_BUFFER_MARGIN_TILES + GAME_FETCH_BYTES) * 8 &&
        !(layer >= 6 && layer < 14 && worldX >= 0 && worldX < SCREEN_WIDTH)) saved->placementCount = 2;
    for (UBYTE i = 0; i < saved->placementCount; i++) {
        UWORD pixelX = x + (i ? page : 0);
        saved->byteX[i] = pixelX >> 3;
        if (!(pixelX & 7)) {
            saved->byteCount[i] = 1;
            drawAlignedEncounterRows(bitmap+(ULONG)pose->y*SCREEN_PLANES*GAME_WORLD_ROW_BYTES+(pixelX>>3),
                (UBYTE*)&saved->background[i],source);
        } else saved->byteCount[i] = drawEnhancedWeaponRows(bitmap,
            (UBYTE*)&saved->background[i], pixelX, pose->y, source, 8, 8);
    }
}

static __attribute__((noinline, optimize("Os"))) void updateCarrierBomberBob(UBYTE* bitmap, UBYTE index, const GameState* game) {
    if(index>=GAME_WORLD_BUFFER_COUNT) return;
    UBYTE depot = game->gameMode==GAME_MODE_ENHANCED && !game->defence.phase && game->ammoBlast.active;
    UBYTE live=depot || (game->gameMode==GAME_MODE_ENHANCED && game->defence.phase &&
        (game->defence.bomberBlastTicks || (game->defence.jetType==2 && game->enemyPlane.active)));
    /* Terrain frames normally have neither a bomber nor a retained background.
     * Do not copy its pose or resolve explosion art in that common case. */
    if (!live && !bomberValid[index]) return;
    WeaponState tile=game->enemyPlane;
    UBYTE pose=tile.direction ? 1 : 0;
    if(game->defence.bomberBlastTicks) {
        tile.x=game->defence.bomberBlastX; tile.worldX=tile.x; tile.y=game->defence.bomberBlastY;
        pose=2+(24-game->defence.bomberBlastTicks)/8;
    }
    if (depot) {
        tile = game->ammoBlast;
        tile.x = tile.worldX - game->scrollX;
        pose = 5 + (36 - tile.timer) / 12;
    }
    if (!depot && live && !game->defence.bomberBlastTicks && game->defence.jetHits)
        pose=8+(game->defence.jetHits-1)*2+(tile.direction&1);
    /* Retain no empty footprint while the bomber turns offscreen. */
    live = live && tile.x>-32 && tile.x<SCREEN_WIDTH;
    if(live && bomberValid[index] && bomberWorldX[index]==tile.worldX && bomberY[index]==tile.y && bomberPose[index]==pose) return;
    if(!live && !bomberValid[index]) return;
    const UBYTE* art=depot ? ammunitionDepotBlast+(pose-5)*160 :
        pose>=8 ? carrierBomberTiles+(pose&1)*320 :
        pose<2 ? carrierBomberTiles+pose*320 : carrierBomberBlast+(pose-2)*320;
    /* Pack a changed pose before the beam-synchronised erase/draw window. */
    if(live && carrierCanBlit(&tile)) prepareCarrierBlitPose(pose,art,tile.x);
    /* Update high-flying bomber early, before waiting for unrelated low
     * bullets/bombs. Remove only overlays intersecting its old/new area. */
    LONG left=tile.worldX,right=left+32;
    WORD top=tile.y,bottom=top+16;
    if(live && carrierCanBlit(&tile)) {
        left=((tile.worldX+GAME_WORLD_BUFFER_MARGIN_PIXELS)&~15L)-GAME_WORLD_BUFFER_MARGIN_PIXELS;
        if(left < -GAME_WORLD_BUFFER_MARGIN_PIXELS) left=-GAME_WORLD_BUFFER_MARGIN_PIXELS;
        right=left+48;
    }
    if(bomberValid[index]) {
        if(bomberWorldX[index]<left) left=bomberWorldX[index];
        if(bomberWorldX[index]+32>right) right=bomberWorldX[index]+32;
        if(bomberY[index]<top) top=bomberY[index];
        if(bomberY[index]+16>bottom) bottom=bomberY[index]+16;
    }
    if(bomberBlitValid[index]) {
        LONG oldLeft=(LONG)bomberBlitByteX[index]*8-GAME_WORLD_BUFFER_MARGIN_PIXELS;
        if(oldLeft<left) left=oldLeft;
        if(oldLeft+48>right) right=oldLeft+48;
    }
    retireEncounterTransientRegion(bitmap,index,left,top,right-left,bottom-top);
    RocketShotFootprint* shots[3]={rocketShotFootprints,wingmanRocketFootprints,enemyMissileFootprints};
    BombShotFootprint* bombs[2]={bombShotFootprints,wingmanBombFootprints};
    UBYTE bombOverlap=0;
    for(UBYTE n=0;n<2;n++) {
        const BombShotFootprint* fp=&bombs[n][index];
        if(fp->valid && fp->worldX<right && fp->worldX+4>left && fp->y<bottom && fp->y+3>top) bombOverlap=1;
    }
    for(UBYTE n=0;n<3;n++)
        if(bombOverlap || encounterRegionHits(&shots[n][index],left,top,right-left,bottom-top))
            eraseRocketPixelBobFootprint(bitmap,index,shots[n]);
    if(bombOverlap) for(UBYTE n=0;n<2;n++) eraseBombPixelBobFootprint(bitmap,index,bombs[n]);
#if !HAR_HEADLESS_CLASSIC_CONTRACT_TEST
    UWORD end=SCREEN_DIWSTRT_Y+bottom;
    /* The CPU fallback needs the old early window. Three small DMA blits
     * can also finish later in the field: reserve 96 PAL lines before the
     * beam next reaches the FIRST affected row.
     * Avoid an unnecessary extra field merely because end+48 has passed. */
    UWORD latest=end+48;
    /* The compact, aligned depot path measures <=86 PAL lines including
     * restoration. Reserve 128 before the next first affected row rather
     * than waiting a whole field after the old 48-line start window. */
    if ((depot && !(tile.worldX&7)) || (!live && bomberValid[index] && bomberPose[index]>=5 && bomberPose[index]<8))
        latest=312+SCREEN_DIWSTRT_Y+top-128;
    if(carrierBlitMemory && (!live || carrierCanBlit(&tile)) &&
        (!bomberValid[index] || bomberBlitValid[index]))
        latest=312+SCREEN_DIWSTRT_Y+top-96;
    if(currentRasterY()>latest) while(currentRasterY()>latest) { }
    while(currentRasterY()<=end) { }
#endif
#if HAR_DEBUG_PERF_LOG
    UWORD drawBegin=currentRasterY();
#endif
    eraseCarrierBomberBob(bitmap,index);
    if(!live) return;
    bomberValid[index]=1; bomberWorldX[index]=tile.worldX; bomberY[index]=tile.y; bomberPose[index]=pose;
    tile.active=1;
    if(carrierCanBlit(&tile)) {
        drawCarrierBlit(bitmap,index,&tile);
#if HAR_DEBUG_PERF_LOG
        carrierRenderStats[2]++;
#endif
    } else {
#if HAR_DEBUG_PERF_LOG
        carrierRenderStats[3]++;
#endif
    WORD x=tile.x,y=tile.y; LONG world=tile.worldX;
    /* A complete 16x16 depot burst keeps the four-tile rendering budget. */
    for(UBYTE n=0;n<(depot ? 4 : 8);n++) {
        UBYTE column=depot ? (n&1) : (n&3);
        UBYTE row=depot ? (n>>1) : (n>>2);
        UBYTE source=n;
        tile.x=x+column*8; tile.worldX=world+column*8; tile.y=y+row*8;
        const UBYTE* pixels=art+source*40;
        UBYTE damaged[40];
        if(!depot && pose>=8) {
            memcpy(damaged,pixels,40);
            for(UBYTE y=0;y<8;y++) {
                UBYTE scar=carrierBomberScar(pose,column,row*8+y)&damaged[y*5+4];
                for(UBYTE p=0;p<4;p++) damaged[y*5+p]=(damaged[y*5+p]&~scar)|
                    ((carrierBomberDamagePen(pose)&(1<<p)) ? scar : 0);
            }
            pixels=damaged;
        }
        drawEncounterTile(bitmap,index,&tile,6+n,0,pixels);
    }
    }
#if HAR_HEADLESS_CLASSIC_CONTRACT_TEST
    bomberDrawCount++;
#endif
#if HAR_DEBUG_PERF_LOG
    UWORD drawEnd=currentRasterY();
    UWORD drawLines=drawEnd>=drawBegin ? drawEnd-drawBegin : drawEnd+312-drawBegin;
    if(drawLines>carrierRenderStats[5]) carrierRenderStats[5]=drawLines;
#endif
}

/* Two tiles only; clip at the top so ascent/return never pops in 8px steps. */
static void drawCarrierBallistic(UBYTE* bitmap, UBYTE index, const GameState* g) {
    const WeaponState* missile=&g->defence.ballistic;
    if(!missile->active) return;
    const UBYTE* art=carrierBallisticTiles+(g->defence.ballisticPhase==CARRIER_BALLISTIC_DESCENT ? 80 : 0);
    UBYTE descending=g->defence.ballisticPhase==CARRIER_BALLISTIC_DESCENT;
    for(UBYTE half=0;half<(descending ? 3 : 2);half++) {
        WeaponState tile=*missile; tile.y+=(WORD)half*8-(descending ? 8 : 0);
        if(tile.y<=-8) continue;
        UBYTE clipped[40]; const UBYTE* source=descending && !half ? carrierBallisticCanopy :
            art+(half-(descending ? 1 : 0))*40;
        if(tile.y<0) {
            UBYTE cut=-tile.y;
            memset(clipped,0,sizeof(clipped)); memcpy(clipped,source+cut*5,(8-cut)*5);
            source=clipped; tile.y=0;
        }
        drawEncounterTile(bitmap,index,&tile,14+half,0,source);
    }
}

static __attribute__((noinline, optimize("Os"))) void drawEnhancedEncounterBobs(UBYTE* bitmap, UBYTE bufferIndex, const GameState* game) {
    if (game->gameMode != GAME_MODE_ENHANCED) return;
#if HAR_DEBUG_PERF_LOG
    UWORD begin = currentRasterY();
#endif
    updateCarrierBomberBob(bitmap,bufferIndex,game);
    drawEncounterTile(bitmap, bufferIndex, &game->siloMissile, 0, 0,
        enhancedWeaponRows + 7 * 40); /* Existing editable vertical missile. */
    drawHelicopterBob(bitmap, bufferIndex, game);
    /* Filled, irregular grey wisps expand then break up; no outlined bubble.
     * Four prepacked 8x8 poses, still only one saved-background BOB. */
    static const UBYTE smoke[4][40] = {
        {0,0,0,0,0,0,0,0,0,0,0,24,0,0,24,24,60,0,0,60,24,56,0,0,56,0,24,0,0,24,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,24,0,0,24,24,60,0,0,60,56,124,0,0,124,28,62,0,0,62,8,28,0,0,28,0,8,0,0,8,0,0,0,0,0},
        {0,24,0,0,24,0,60,0,0,60,32,118,0,0,118,24,124,0,0,124,4,62,0,0,62,0,20,0,0,20,0,8,0,0,8,0,0,0,0,0},
        {0,16,0,0,16,0,36,0,0,36,0,66,0,0,66,0,20,0,0,20,0,40,0,0,40,0,4,0,0,4,0,0,0,0,0,0,0,0,0,0}
    };
    UBYTE smokeFrame = game->helicopterSmoke.timer / 10;
    if (smokeFrame > 3) smokeFrame = 3;
    drawEncounterTile(bitmap, bufferIndex, &game->helicopterSmoke, 1, 0, smoke[smokeFrame]);
    drawHelicopterBullets(bitmap, bufferIndex, game);
    {
        for (UBYTE i = 0; i < CARRIER_DEFENCE_BOMBS; i++)
            drawEncounterTile(bitmap, bufferIndex, &game->defence.bombs[i], i + 2, 0,
                carrierDefenceBombRows);

    }

    drawCarrierBallistic(bitmap,bufferIndex,game);


#if HAR_DEBUG_PERF_LOG
    UWORD end = currentRasterY();
    ULONG cost = end >= begin ? end - begin : end + 312 - begin;
    encounterCosts[1] += cost;
    if (cost > encounterCosts[5]) encounterCosts[5] = cost;
#endif
}
