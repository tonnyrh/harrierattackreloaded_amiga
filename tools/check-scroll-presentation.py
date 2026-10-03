"""Inspect HAR_SCROLL_PRESENT_TRACE output (big endian Amiga words).

CIA-A TOD measures real PAL fields, independent of software loop counts.
The trace records Copper operand memory after patching, not write-only custom
register readback. This checks submission and deadlines, not host video judder.
"""
import argparse
import json
import struct
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument("trace", type=Path)
parser.add_argument("--page-bytes", type=int, default=128)
args=parser.parse_args()
words=struct.unpack(">689H",args.trace.read_bytes())
count=words[0]
assert count<=128
records=[words[1+i*5:6+i*5] for i in range(count)]
for scroll, gap, offset, fine, beam in records:
    assert fine == ((fine&15)*17), ("playfield mismatch",scroll,fine)
    # DDF prefetch 16px cancels the bitmap's 16px left margin.
    visible=(offset*8-(fine&15)) % (args.page_bytes*8)
    assert visible==scroll % (args.page_bytes*8), ("coarse/fine discontinuity",scroll,visible)
    assert beam<16 or beam>=300, ("late Copper patch",scroll,beam)
summary={"updates":sum(words[641:657]),"extra_pal_fields":sum(words[657:673]),
         "max_fields_between_presentations":max(words[673:689]),"copper_records_checked":count,
         "missed_by_previous_scroll_phase":list(words[657:673])}
print(json.dumps(summary,indent=2))
