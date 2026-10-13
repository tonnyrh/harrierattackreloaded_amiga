# Oil platform art and landing geometry

`source-v2.png` was generated with the built-in image generator (2026-10-09),
using the previous platform as reference. `platform.png` is the deterministic
96x64 indexed game master; `platform_tiles.h` is 3840 bytes including masks.
Run `python tools/pack-oil-platform.py` to repack. Palette pens are restricted
to grey, black and yellow/orange. Index 0 is transparent. The validation report
checks dimensions, OCS colours and transparency.

Generation brief: transparent side-elevation Amiga oil platform, strongly
simplified for 96x64 pixels; a broad flat left helipad halfway down the image,
thick grey deck, four support legs reaching the bottom, sparse cross-braces,
compact right-side machinery, derrick and crane; grey steel with restrained
yellow accents; no sea, sky, fire, green or cyan noise, or fine texture.

Runtime origin is (64,64), eight pixels lower than the previous version.
Legs continue to Y=127, below the water surface at Y=121. The visible flat pad
starts at local Y=30, so landing uses world Y=94 and aircraft top Y=86.
Wheel contact must fit X=73..111. Neither P1 nor P2 is teleported sideways
on touchdown. Existing hard-landing damage applies to both players.

Oil contract tests verify the visible pad mask, submerged legs, actual soft
landing coordinates, hard-landing bounce/damage, P2 landing, and restoration
of the rendered platform after moving objects. Passed on 2026-10-09.