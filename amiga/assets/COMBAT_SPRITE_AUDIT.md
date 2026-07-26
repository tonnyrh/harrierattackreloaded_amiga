# Combat sprite rip audit

Sprint 14.6.3 reviewed the current Amiga runtime against the CPC graphics
sources for player Harrier, enemy plane, missiles/rockets, and bombs.

## Source assets

Relevant CPC sources:

- `AMSTRADFONT3.asm`
  - `sprite_pixel_data1` + `sprite_pixel_data2`: flying Harrier, two CPC Plus sprite halves.
  - `sprite_pixel_data4` + `sprite_pixel_data3`: landing Harrier, two CPC Plus sprite halves.
  - `sprite_pixel_data5` + `sprite_pixel_data6`: enemy plane flying, two CPC Plus sprite halves.
  - `sprite_pixel_data7` + `sprite_pixel_data8`: enemy plane broken, two CPC Plus sprite halves.
  - tile 40: bomb launched.
  - tile 41: bomb descending.
  - tile 55: missile left.
  - tile 56: missile right.
  - tiles 98-101: diagonal/vertical missile directions for later Maverick/guided missile work.

Generated visual QA:

```text
amiga/assets/generated/cpc/previews/cpc_combat_sprites_audit.bmp
```

## Runtime status

- Player Harrier: now built from promoted CPC Plus `sprite_pixel_data1/2`.
- Enemy plane: now built from promoted CPC Plus `sprite_pixel_data5/6`.
- Player rocket: now built from CPC tile 56, `MISSILE RIGHT`.
- Enemy missile: now built from CPC tile 55, `MISSILE LEFT`.
- Player bomb: now uses CPC tile 40 briefly, then CPC tile 41 while descending.

## Important caveat

The Amiga runtime still uses OCS hardware sprites for these moving objects. OCS
hardware sprites are only 2 bitplanes, so the CPC Plus pen data must be reduced
to three visible sprite colors plus transparency. Shape/provenance is now from
the CPC data; exact CPC Plus color richness is not possible through a single
standard hardware sprite.

If later visual fidelity is more important than hardware-sprite economy, these
can be promoted again as blitter Bobs with more bitplanes.
