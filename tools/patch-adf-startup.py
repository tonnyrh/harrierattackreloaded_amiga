"""Replace exe2adf's one-command OFS startup with sequential title/game stages.
Both commands fit the existing single data block; no filesystem allocation changes.
"""
import argparse
import struct
from pathlib import Path


def patch(path):
    disk = bytearray(path.read_bytes())
    if disk[:4] != b'DOS\0' or len(disk) != 901120:
        raise ValueError('Expected a standard OFS DD ADF')
    matches = [i for i in range(2, len(disk)//512)
               if disk[i*512+432:i*512+449] == b'\x10startup-sequence'
               and struct.unpack_from('>I', disk, i*512)[0] == 2]
    if len(matches) != 1:
        raise ValueError('Expected one startup-sequence header')
    header = matches[0]*512
    words = struct.unpack_from('>128I', disk, header)
    data = words[4]*512
    block = struct.unpack_from('>128I', disk, data)
    if words[2] != 1 or words[127] != 0xfffffffd or block[:3] != (8, matches[0], 1) or block[4] != 0:
        raise ValueError('Startup is not a single OFS block')
    old = bytes(disk[data+24:data+24+block[3]])
    script = b'harrier_loader.exe\nharrier_amiga.exe\n'
    if old not in (b'harrier_loader.exe\n', script):
        raise ValueError('Unexpected startup command; refusing to replace it')
    for offset in (header, data):
        if sum(struct.unpack_from('>128I', disk, offset)) & 0xffffffff:
            raise ValueError('Invalid original block checksum')
    disk[data+24:data+512] = script + bytes(488-len(script))
    struct.pack_into('>I', disk, header+81*4, len(script))
    struct.pack_into('>I', disk, data+3*4, len(script))
    for offset in (header, data):
        struct.pack_into('>I', disk, offset+20, 0)
        checksum = -sum(struct.unpack_from('>128I', disk, offset)) & 0xffffffff
        struct.pack_into('>I', disk, offset+20, checksum)
        assert sum(struct.unpack_from('>128I', disk, offset)) & 0xffffffff == 0
    path.write_bytes(disk)
    print('ADF boot: title exits and frees memory, then game loads')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('adf', type=Path)
    patch(parser.parse_args().adf)
