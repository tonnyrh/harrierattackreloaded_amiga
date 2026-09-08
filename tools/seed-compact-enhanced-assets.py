"""Seed compact Enhanced art at the original gameplay tile dimensions."""

import argparse

from amiga_graphics_format import GROUND_ASSETS as ASSETS, build_runtime_banks, save_indexed_master


# Native pixel rows, not resized large artwork. All pens use the game palette.
PENS = {'.': 0, 'L': 2, 'M': 3, 'D': 4, 'G': 5, 'Y': 6,
        'g': 8, 'R': 9, 'K': 10, 'C': 11, 'B': 13}
ART = {
    'tank': (
        '.........K......',
        '.......KBGGK....',
        'KLLLLLLGGgGK....',
        '......KGGggGKK..',
        '..KBBGGgGCCGGGK.',
        '.KGGggGCCGGggGgK',
        'KDMDDMDDMDDMDDDK',
        '.KKKKKKKKKKKKKK.',
    ),
    'radar': (
        'LLL.....',
        '.LLLK...',
        '..LMK...',
        '...K....',
        '...MK...',
        '..KBGGK.',
        '.KGRggGK',
        '.KKKKKKK',
    ),
    'launcher': (
        '......Y.',
        '.....LLK',
        '....LGK.',
        '...LGK..',
        '..KGKLL.',
        '.KBGGgLK',
        'KGGggGGK',
        '.KMKKMK.',
    ),
    'gun': (
        'L..L....',
        '.M..M...',
        '..D.D...',
        '...MK...',
        '..KBGK..',
        '.KGGggK.',
        'KBGgCCgK',
        '.KKKKKK.',
    ),
}


def generate(force=False):
    existing = [spec.filename for spec in ASSETS if spec.path.exists()]
    if existing and not force:
        raise FileExistsError(f'Refusing to overwrite edited masters: {existing}; use --force to reset')
    for spec in ASSETS:
        rows = ART[spec.key]
        if len(rows) != spec.height or any(len(row) != spec.width for row in rows):
            raise ValueError(f'Invalid pixel rows for {spec.key}')
    for spec in ASSETS:
        save_indexed_master(spec.path, (spec.width, spec.height),
                            [PENS[c] for row in ART[spec.key] for c in row])
    for path, bank in build_runtime_banks().items():
        path.write_bytes(bank)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--force', action='store_true', help='reset edited compact masters')
    generate(parser.parse_args().force)
