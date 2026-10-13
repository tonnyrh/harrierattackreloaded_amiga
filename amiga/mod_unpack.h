#pragma once
/* Bounds-checked lossless LZ decoder. Called only at a music transition. */
static int unpackModTrack(const unsigned char* source, unsigned long sourceSize,
    unsigned char* dest, unsigned long destSize) {
    unsigned long in = 0, out = 0;
    while (out < destSize) {
        if (in >= sourceSize) return 0;
        unsigned char flags = source[in++];
        for (unsigned char bit = 0; bit < 8 && out < destSize; bit++) {
            if (flags & (1 << bit)) {
                if (sourceSize - in < 2) return 0;
                unsigned short token = (source[in] << 8) | source[in + 1];
                in += 2;
                unsigned short distance = token >> 4;
                unsigned char length = (token & 15) + 3;
                if (!distance || distance > out || length > destSize - out) return 0;
                while (length--) { dest[out] = dest[out - distance]; out++; }
            } else {
                if (in >= sourceSize) return 0;
                dest[out++] = source[in++];
            }
        }
    }
    return in == sourceSize;
}
