"""Lossless LZ packing of the mutually exclusive MOD tracks; no audio changes."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TRACKS = ('harrier_menu_fixed', 'raf_game_over', 'carrier_landing_fanfare')

def pack(data):
    out = bytearray()
    pos = 0
    while pos < len(data):
        flag_pos = len(out)
        out.append(0)
        for bit in range(8):
            if pos == len(data):
                break
            best_len, best_distance = 0, 0
            for source in range(max(0, pos - 4095), pos):
                length = 0
                while length < 18 and pos + length < len(data) and data[source + length] == data[pos + length]:
                    length += 1
                if length > best_len:
                    best_len, best_distance = length, pos - source
            if best_len >= 3:
                out[flag_pos] |= 1 << bit
                token = (best_distance << 4) | (best_len - 3)
                out.extend(token.to_bytes(2, 'big'))
                pos += best_len
            else:
                out.append(data[pos])
                pos += 1
    return out

def unpack(data, size):
    out = bytearray()
    pos = 0
    while len(out) < size:
        flags = data[pos]
        pos += 1
        for bit in range(8):
            if len(out) == size:
                break
            if flags & (1 << bit):
                token = int.from_bytes(data[pos:pos+2], 'big')
                pos += 2
                for _ in range((token & 15) + 3):
                    out.append(out[-(token >> 4)])
            else:
                out.append(data[pos])
                pos += 1
    assert pos == len(data)
    return out

if __name__ == '__main__':
    sizes = []
    sample_sizes = []
    for name in TRACKS:
        path = ROOT / 'amiga/assets/music' / (name + '.mod')
        raw = path.read_bytes()
        sample_sizes.append(sum(int.from_bytes(raw[42+i*30:44+i*30], 'big') * 2 for i in range(31)))
        packed = pack(raw)
        assert unpack(packed, len(raw)) == raw
        dest = path.with_suffix('.lz')
        if not dest.exists() or dest.read_bytes() != packed:
            dest.write_bytes(packed)
        sizes.append(len(raw))
        print(f'{name}: {len(raw)} -> {len(packed)} bytes, lossless verified')
    header = '#pragma once\n#define MOD_TRACK_BUFFER_BYTES %d\n' % max(sizes)
    header += '#define MENU_MOD_SAMPLE_BYTES %d\n' % sample_sizes[0]
    header += '#define MOD_MAX_SAMPLE_BYTES %d\n' % max(sample_sizes)
    for name, size in zip(TRACKS, sizes):
        header += '#define MOD_SIZE_%s %d\n' % (name.upper(), size)
    dest = ROOT / 'amiga/assets/music/packed_sizes.h'
    if not dest.exists() or dest.read_text() != header:
        dest.write_text(header)
