"""Locate discontinuities in consecutive WinUAE source-frame PNGs.

This is a content check, NOT a wall-clock measurement. PNG file timestamps are
never used. Use check-host-presentation.py separately for host timing; never
join independent captures by image/row number.
"""
import argparse
import csv
import json
from collections import Counter
from pathlib import Path
from PIL import Image, ImageChops


def measure_pair(previous, current, crop, max_step=32, max_mismatch=.05, margin=.002):
    x, y, w, h = crop
    if previous.size != current.size:
        raise ValueError('Frame dimensions changed during capture')
    if min(x, y) < 0 or w <= 2*max_step or h <= 0 or x+w > previous.width or y+h > previous.height:
        raise ValueError('Crop must be inside the image and wider than twice max-step')
    costs = []
    # Fixed overlap size for all candidates avoids favouring large displacements.
    before = previous.crop((x+max_step, y, x+w-max_step, y+h))
    for step in range(-max_step, max_step+1):
        after = current.crop((x+max_step-step, y, x+w-max_step-step, y+h))
        difference = ImageChops.difference(before, after)
        channels = difference.split()
        nonzero = ImageChops.lighter(ImageChops.lighter(channels[0], channels[1]), channels[2])
        costs.append(1 - nonzero.histogram()[0] / (nonzero.width * nonzero.height))
    rank = sorted(range(len(costs)), key=costs.__getitem__)
    best, second = rank[:2]
    status = 'measured'
    if costs[best] > max_mismatch:
        status = 'unreliable_content_change'
    elif costs[second]-costs[best] <= margin:
        status = 'ambiguous_texture'
    elif abs(best-max_step) == max_step:
        status = 'search_limit'
    return {'step': best-max_step, 'mismatch': costs[best],
            'confidence_gap': costs[second]-costs[best], 'status': status}


def analyze(files, crop=(100,260,500,60), expected_step=6, max_step=32,
            max_mismatch=.05, margin=.002):
    if len(files) < 2:
        raise ValueError('Need at least two consecutive images')
    if abs(expected_step) >= max_step:
        raise ValueError('Expected step must be strictly inside the search range')
    events = []
    with Image.open(files[0]) as source:
        previous = source.convert('RGB')
    for index, filename in enumerate(files[1:], 1):
        with Image.open(filename) as source:
            current = source.convert('RGB')
        result = measure_pair(previous, current, crop, max_step, max_mismatch, margin)
        kind = result['status']
        previous_stem, current_stem = Path(files[index-1]).stem, Path(filename).stem
        capture_gap = (previous_stem.isdecimal() and current_stem.isdecimal()
                       and int(current_stem)-int(previous_stem) != 1)
        if capture_gap:
            kind = 'capture_sequence_gap'
        elif kind == 'measured':
            step = result['step']
            if step == expected_step:
                kind = 'ok'
            elif step == 0:
                kind = 'repeated_image'
            elif step * expected_step < 0:
                kind = 'reverse_step'
            else:
                kind = 'wrong_step'
        events.append({'transition': index, 'previous': str(files[index-1]),
                       'current': str(filename), **result,
                       'expected_step': expected_step, 'error_pixels': result['step']-expected_step,
                       'kind': kind})
        previous = current
    counts = Counter(e['kind'] for e in events)
    faults = sum(counts[k] for k in ('repeated_image','reverse_step','wrong_step'))
    uncertain = sum(counts[k] for k in ('ambiguous_texture','unreliable_content_change','search_limit','capture_sequence_gap'))
    return {'frames': len(files), 'transitions': len(events),
            'time_basis': 'source image order; no host timestamps',
            'screen_pixel_steps': dict(Counter(e['step'] for e in events if e['status']=='measured')),
            'classifications': dict(counts), 'faults': faults, 'uncertain': uncertain,
            'result': 'fail' if faults else ('inconclusive' if uncertain else 'pass'),
            'worst_pixel_mismatch': max(e['mismatch'] for e in events)}, events


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--crop', type=int, nargs=4, default=[100,260,500,60], metavar=('X','Y','W','H'))
    parser.add_argument('--expected-step', type=int, default=6)
    parser.add_argument('--max-step', type=int, default=32)
    parser.add_argument('--max-mismatch', type=float, default=.05)
    parser.add_argument('--ambiguity-margin', type=float, default=.002)
    parser.add_argument('--output', type=Path, help='Report prefix; defaults to DIRECTORY/scroll-check')
    args = parser.parse_args()
    if args.max_step < 1 or not 0 <= args.max_mismatch <= 1 or not 0 <= args.ambiguity_margin <= 1:
        parser.error('Invalid search or confidence parameters')
    files = sorted(args.directory.glob('*.png'))
    try:
        summary, events = analyze(files, args.crop, args.expected_step, args.max_step,
                                  args.max_mismatch, args.ambiguity_margin)
    except ValueError as error:
        parser.error(str(error))
    prefix = args.output or args.directory/'scroll-check'
    prefix.parent.mkdir(parents=True, exist_ok=True)
    prefix.with_suffix('.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    with prefix.with_suffix('.csv').open('w',newline='',encoding='utf-8') as stream:
        writer = csv.DictWriter(stream,fieldnames=events[0].keys())
        writer.writeheader()
        writer.writerows(events)
    print(json.dumps(summary,indent=2))
    raise SystemExit(1 if summary['faults'] else (2 if summary['uncertain'] else 0))


if __name__ == '__main__':
    main()
