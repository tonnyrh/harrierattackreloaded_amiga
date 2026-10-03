"""Prebaked 8x8 terrain: preserve CPC silhouettes; no runtime pixel work."""
SURFACE_IDS = (1, *range(24, 36), 97)
TERRAIN_TILE_COUNT = len(SURFACE_IDS) + 4


def terrain_pixels(source: bytes) -> list[list[int]]:
    def soil(x, y, variant):
        # Sparse, clustered earth flecks. Pen 5 follows mission lighting;
        # no sea/cloud pen, bright pickup colour or new palette register.
        if variant == 1 and (x, y) in ((5, 2), (6, 2)):
            return 8
        if variant == 2:
            return {(2, 5): 8, (3, 5): 8}.get((x, y), 5)
        return 5

    tiles = []
    for variant, tile_id in enumerate(SURFACE_IDS):
        original = [sum(((source[tile_id*40+y*5+p] >> (7-x)) & 1) << p
                        for p in range(4)) for y in range(8) for x in range(8)]
        top = [next((y for y in range(8) if original[y*8+x]), 8) for x in range(8)]
        result = []
        for y in range(8):
            for x in range(8):
                if not original[y*8+x]:
                    result.append(0)
                    continue
                depth = y-top[x]
                if depth == 0:
                    pen = 11 if (x+variant*3) % 11 == 5 else 5
                elif depth == 1:
                    pen = 5 if (x+variant) % 3 else 8
                elif depth == 2:
                    pen = 8 if (x+variant) % 4 else 10
                else:
                    pen = soil(x, y, variant & 3)
                result.append(pen)
        assert [bool(v) for v in result] == [bool(v) for v in original]
        tiles.append(result)
    tiles.extend([[soil(x, y, variant) for y in range(8) for x in range(8)]
                  for variant in range(4)])
    return tiles


def build_terrain_bank(source: bytes) -> bytes:
    tiles = terrain_pixels(source)
    packed = bytes(sum(((tile[y*8+x] >> p) & 1) << (7-x) for x in range(8))
                   for tile in tiles for y in range(8) for p in range(5))
    assert len(packed) == TERRAIN_TILE_COUNT * 40
    assert all(packed[i] == 0 for i in range(4, len(packed), 5))
    return packed
