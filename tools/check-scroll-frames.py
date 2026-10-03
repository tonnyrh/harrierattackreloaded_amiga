"""Measure actual WinUAE PNG output, independently of game counters.

Capture a short stationary-terrain band with WinUAE's native multiscreenshot
API. Exclude the HUD, moving actors and the streamed right edge. This offline
pixel comparison complements timing traces; recording itself can affect host I/O.
Default crop/step match a 2x low-resolution capture at maximum cruise speed.
"""
import argparse
import json
from collections import Counter
from pathlib import Path
from PIL import Image, ImageChops

parser=argparse.ArgumentParser()
parser.add_argument("directory",type=Path)
parser.add_argument("--crop",type=int,nargs=4,default=[100,260,500,60],metavar=("X","Y","W","H"))
parser.add_argument("--expected-step",type=int,default=6)
args=parser.parse_args()
files=sorted(args.directory.glob("*.png"))
assert len(files)>=32,"Need consecutive fields spanning several word boundaries"
x,y,w,h=args.crop
steps=[]; worst=0
previous=Image.open(files[0]).convert("RGB")
for filename in files[1:]:
    current=Image.open(filename).convert("RGB")
    costs=[]
    for step in range(-32,33):
        before=previous.crop((x+max(step,0),y,x+w+min(step,0),y+h))
        after=current.crop((x+max(-step,0),y,x+w+min(-step,0),y+h))
        difference=ImageChops.difference(before,after).convert("L")
        costs.append(1-difference.histogram()[0]/(difference.width*difference.height))
    best=min(range(len(costs)),key=costs.__getitem__)
    steps.append(best-32); worst=max(worst,costs[best]); previous=current
print(json.dumps({"frames":len(files),"screen_pixel_steps":dict(Counter(steps)),"worst_pixel_mismatch":worst},indent=2))
assert all(step==args.expected_step for step in steps),"Captured output contains repeated or uneven steps"
