#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../amiga/mod_unpack.h"

static unsigned long readFile(const char* path, unsigned char* data) {
    FILE* f = fopen(path, "rb"); assert(f);
    unsigned long n = (unsigned long)fread(data, 1, 20000, f);
    assert(!ferror(f)); fclose(f); return n;
}
int main(void) {
    const char* names[] = {"harrier_menu_fixed", "raf_game_over", "carrier_landing_fanfare"};
    unsigned char source[20000], expected[20000], actual[20002];
    char path[256];
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "amiga/assets/music/%s.mod", names[i]);
        unsigned long n = readFile(path, expected);
        snprintf(path, sizeof(path), "amiga/assets/music/%s.lz", names[i]);
        unsigned long packed = readFile(path, source);
        memset(actual, 0xcd, sizeof(actual));
        assert(unpackModTrack(source, packed, actual + 1, n));
        assert(!memcmp(expected, actual + 1, n));
        assert(actual[0] == 0xcd && actual[n + 1] == 0xcd);
        for (unsigned long cut = 0; cut < packed; cut++)
            assert(!unpackModTrack(source, cut, actual + 1, n));
        assert(!unpackModTrack(source, packed, actual + 1, n - 1));
        printf("%s: exact round-trip, truncation and bounds OK\n", names[i]);
    }
    const unsigned char bad[] = {1, 0, 0};
    assert(!unpackModTrack(bad, sizeof(bad), actual, 3));
    return 0;
}
