# A500 rendering strategy

Status: architectural direction under implementation, not a completed migration.

See [performance targets and historical developer references](PERFORMANCE_TARGETS.md)
for the feasibility assessment, machine definitions and criteria for smoothness.

## Evidence from the CPC implementation

The original `HarrierAttackSourceNew2_alt_CRTC_CART16.asm` is available in
Git at `c1b9fc9388740b25cb9c70822107ee745c01d598` (before the Amiga repository
cleanup). The inspected copy is `.tmp/cpc-scroll-reference.asm`.

- `scrollscenery`, line 4414, advances the CRTC view, scrolls the object map,
  generates the fresh edge and draws new tiles there.
- `scrollobjecttilemap`, line 7097, shifts 639 bytes with `LDIR`. This is a
  small active object grid, not a reconstruction of every historical object.
- `getskytilemapid`, line 4796, computes an address in that grid.
- `drawspritecheckifsky2`, around line 4831, writes the object ID to the grid
  and draws the corresponding graphic tile into bitmap memory.
- `drawsmokesprite`, line 6259, draws smoke tile 52 with object ID 10, then
  optionally tile 51 one row above. Smoke becomes part of the active map
  and scrolled bitmap.
- `scroll_right` and `update_scroll`, lines 625 and 732, update the scroll
  address and CRTC registers. This is bitmap scrolling with software tile
  drawing, not a hardware character display mode. Collision is primarily
  object-grid lookup rather than decoding rendered RGB/palette pixels.

## What Amiga already does

The Amiga port already scrolls a single planar ring buffer through Copper
pointer/fine-scroll changes. It displays four planes and retains five-plane
storage. The current default ring period is 128 tile columns, with 42 mirrored
fetch columns and two margin bytes (172 bytes per row). The earlier default
used an 86-column period. New columns are drawn under an adaptive row budget.

The expensive difference is surrounding work: many cell queries and BOB
erasures reconstruct world contents from generation tables and accumulated
damage. One render column is retained for the in-progress stream; several
small caches reduce repeated queries but do not form an authoritative active
world grid. Global revision invalidation also discards useful unrelated data.

## Target design

1. Keep persistent terrain, buildings, flak, wrecks and smoke drawn in the
   planar ring. Change pixels only on insertion, actual damage, required
   animation or ring reuse. Scroll movement itself must not rebuild them.
2. Keep a parallel active ring of visual tile IDs and collision cells, tagged
   with absolute world columns. Separate the two: Enhanced visual art and
   promoted ship/carrier graphics cannot safely be used as collision IDs.
3. Update pixel data and active-map cells together at each mutation. BOB
   restoration should use these current cells, including masked static
   overlays, instead of walking global damage history.
4. Keep independent moving actors separate. Preserve useful hardware sprites
   for aircraft; use BOBs for remaining projectiles and transient effects.
   Audit saved-background overlap order and raster timing separately. An
   active tile map does not automatically fix single-buffer tearing.
5. Retain deterministic generation and off-screen damage history as a cold
   path for new columns, rescue/restart and backwards/revisited columns.
   Do not discard persistence merely because the normal flight is forwards.

The full straightforward 128 x 25 representation would need 3200 bytes for
visual tile IDs plus 12800 bytes for four-byte `ObjectCell`s, before tags and
validity bits. This is a sizing estimate, not an approved allocation. A
smaller upper-playfield collision grid or compact IDs may suffice, but must
preserve flags, tile identity, ship geometry and smoke/flak distinctions.

## Migration and acceptance

### First shadow implementation

`HAR_DEBUG_OBJECT_SHADOW=1` now records the existing generic object-cell
resolver into 86 tagged ring slots, with one four-byte cell per row. The
original resolver always runs and remains authoritative. Per-column known
and current-revision row masks distinguish warm hits, newly observed rows,
slot recycling and rows revisited after a world revision. A same-revision
content mismatch is recorded rather than silently serving cached data.

The `#object_shadow` record is written to `perf_log.csv` only after AmigaOS
has been restored. Its eight counters are: successful queries, current
revision hits, new rows, recycled columns, revision changes, stale equal
rows, stale changed rows, and unexpected current-revision mismatches.
Counters include initialization and mission setup as well as flight. This
is a query-populated observation ring, not yet a streamed authoritative map.
It does not measure all collision work: facade-specific town queries can
bypass the generic resolver, and existing higher-level caches can intercept
queries. These paths need explicit coverage before replacing them.

This mode adds observation overhead and roughly 10 KB of debug-only state;
its frame-rate readings must not be claimed as release performance. With
the flag off, the probe and state compile out. No gameplay caller receives
the shadow value. The next step depends on observed mismatch and reuse data.

A500 Enhanced `object_shadow_full` completes the full combat/landing route
with the same final gameplay record as `tod_smoke_filter_landing`. It records
18162 successful queries, 3791 current-revision hits, 5461 new rows, 851 slot
recycles and 8409 per-column revision changes. Of stale rows revisited,
8781 are unchanged and 129 differ. Current-revision mismatches are zero.
These counts support investigating local invalidation; they do not prove
that future local invalidation is correct or quantify a release FPS gain.
Town-specific composite queries and initialization caveats still apply.

A500 Classic `object_shadow_classic_full` also completes combat and landing
with zero current-revision mismatches. It records 18000 successful queries,
3606 current-revision hits, 5281 new rows, 851 slot recycles and 8506
per-column revision changes. Of stale rows revisited, 8980 are unchanged
and 133 differ. No equivalent prior Classic full-route baseline was compared;
this is shadow consistency evidence, not a measured Classic performance gain.

`HAR_DEBUG_OBJECT_SHADOW_LOCAL=1` now tests local invalidation in the shadow
only. Target destruction invalidates the anchor and rear column; ship-cell
damage, smoke, craters, and runtime flak mutations invalidate their column.
The helper retains current observations in other columns across that mutation.
It never promotes observations already stale before the mutation, preserving
global reset semantics. Matching UWORD column aliases are invalidated because
damage history uses UWORD keys. Every production cache still receives the
original global revision increment, and the original resolver still runs.
Both debug flags must be enabled; ordinary builds compile out the shadow loop.
Rescue restores damage arrays after session initialization. The restore path
now explicitly invalidates world caches and the powerup background afterward.

A500 Enhanced `object_shadow_local_full` completes combat and landing with
an identical parity CSV to `object_shadow_full`, including zero shadow
mismatches. Its record is `18162,12336,5461,851,0,236,129,0`: current hits
rise from 3791 to 12336, and unchanged stale revisits fall from 8781 to 236.
The same 129 changed revisits remain invalidated. The revision-change counter
now counts only revisions discovered at query time; local mutations advance
the shadow tags eagerly, so zero in that field does not mean no mutations.
This is evidence for retaining unrelated columns in this route, not release
FPS evidence. The debug helper deliberately scans all 86 tags; a production
implementation should invalidate tagged slots directly rather than copy this
observation overhead into gameplay.

A500 Classic `object_shadow_local_classic_full` also completes combat and
landing with identical parity CSV to `object_shadow_classic_full` and zero
mismatches. Its record is `18000,12320,5281,851,0,266,133,0`: current hits rise
from 3606 to 12320, unchanged stale revisits fall from 8980 to 266, and all
133 changed revisits remain invalidated. Both local-shadow runners exited
successfully and restored the normal executable and ADF afterward.

### Active object-cell cache

`HAR_OBJECT_CELL_CACHE=1` enables actual cached generic object-cell returns
in 64 tagged columns, populated by queries. The 6912-byte table covers 25
rows per column; a low-six-bit index avoids division on 68000. Slot reuse
checks the full absolute column tag. Local mutations invalidate one or two
slots directly, while route/seed/history resets clear all row-valid masks.
The existing global revision remains in place for the other caches.
Town facade-specific queries keep their separate semantics and implementation.
This is the generic-cell part of the migration, not an authoritative visual
tile map or a replacement for all collision and drawing paths.

The cache is now enabled by default after the tests below. It can be disabled
with `HAR_OBJECT_CELL_CACHE=0` for comparison or the older shadow probes.
`HAR_OBJECT_CELL_CACHE_VERIFY=1`
independently recomputes every hit and counts hits, misses, and incorrect hits
in `#object_cache` after a parity run. It still returns the cached cell so a
mismatch cannot be hidden by falling back to the reference. The contract
runner accepts `-ExtraCcFlags` to exercise this mode and fails on any incorrect
hit. Active caching and the earlier shadow mode cannot be enabled together.

The full A500 contract passed with both active-cache and verification flags
enabled (`.tmp/a500-object-cache-contract.log`). Its added check requires
observed cache hits and zero incorrect hits across the suite, with extra
mutation/reset/slot-reuse queries. The runner restored the normal build.
This does not yet prove live rescue input handling or full-route performance.

A500 Enhanced `object_cache_verify_full` completes combat and landing with
12336 verified hits, 5826 misses and zero incorrect hits. Its parity CSV is
identical to `object_shadow_local_full`. Per-hit reference computation is
enabled in this run, so its frame rate is not a cache performance benchmark.

A500 Classic `object_cache_verify_classic_full` also completes combat and
landing with 12320 verified hits, 5680 misses, and zero incorrect hits. Its
parity CSV is identical to `object_shadow_local_classic_full`. This covers
active cached returns in both gameplay modes, independently checked on A500.

Stock A1200 Enhanced `A1200_object_cache_verify_full` completes combat and
landing with 12336 verified hits, 5826 misses and zero incorrect hits, matching
the A500 Enhanced parity CSV exactly. This verifies the active cache on the
2 MB Chip RAM, no-expansion/JIT 68EC020 profile as well.

Matched current-source A500 Enhanced full-combat runs (verification off):

| Setting / result tag | Combat window FPS range | Mean reported window FPS | Maximum field gap |
| --- | --- | --- | --- |
| Cache off / `object_cache_off_full` | 31–42 | 37.00 | 3 |
| Cache on / `object_cache_on_full` | 35–43 | 38.78 | 3 |

The combat selection is speed 15, scroll 1000–6500, using 100-field windows
(24 off, 23 on). Windows cross slightly different world positions, so the
mean comparison is approximate; both runs complete landing with identical
parity CSVs. The maximum measured field gap is unchanged in this matched
comparison. Do not attribute the larger difference from the older smoke-filter
baseline to this cache alone: intervening changes and compiled code layout
are also present. The normal executable/ADF was restored after both runs,
with caching disabled by default at that point, pending Classic and A1200 trials.
The result is a modest improvement, not smooth 50 Hz combat or visual proof.

Stock A1200 Enhanced `A1200_object_cache_on_full`, without reference checking,
completes combat and landing with identical parity CSV to its verified run.
All 18 combat windows (speed 15, scroll 1000–6500) report 50 FPS, maximum
field gap 1, and zero hitches. The final landing window is also 50 FPS.
This is measured pacing evidence; continuous visual motion/tearing inspection
and actual player damage/audio coverage remain outstanding. After these
checks, active generic object caching was enabled in the normal build.

Extend the instrumented shadow coverage: keep existing generation/collision
results authoritative and compare cached cells against them across ordinary
flight, the town boundary, destruction, flak insertion/removal, ring reuse,
landing, rescue and restart. Record mismatches and cache hit/miss causes.
Do not infer correctness merely from the same final score.

Then centralize column/row invalidation and mutation publication. Route
reset and restored state must rebuild or invalidate the active map; a local
flak change must not invalidate every unrelated column. Explicitly cover
paired tanks, smoke above hits, procedural towns and composite ships.

Only after shadow comparisons pass, use the active map for hot collision
queries and BOB background restoration. Measure full combat routes on stock
A500 and A1200 and compare the actual frame pacing, not WinUAE's refresh FPS.
Retire superseded caches only after confirming their semantics are covered.

Completion still needs continuous visual scrolling/tearing review, genuine
player damage/audio/crash coverage, and good A500 pacing under combat. The
current 35–43 Hz full-combat A500 cache trial does not meet that end state.

## Retained visual columns trial

`HAR_RENDER_COLUMN_CACHE=1` adds a separate 64-slot map of complete visual
columns. Each slot carries the full absolute column tag and presentation mode.
Local world mutations invalidate only their slots; world resets clear all
slots. The original builder remains available as the miss path. Direct masked
overlays continue to be drawn from current object state, outside this tile map.

The builder also publishes town radar height and enemy-plane passability.
The retained entries include these derived values and publish them on hits,
preserving the original side effects rather than returning tile bytes alone.
Town table reset now explicitly resets world caches too. The existing two-slot
BOB cache remains temporarily; the wider map handles its misses and other
column consumers without changing their interfaces.

The feature defaults off. `HAR_RENDER_COLUMN_CACHE_VERIFY=1` rebuilds each
hit independently and compares all 25 tiles plus both navigation summaries.
Parity runs write `#render_cache,hits,misses,incorrect_hits`; the full contract
requires observed hits and zero mismatches. Reference builds are correctness
checks, not performance measurements.

The full A500 contract passed with visual caching and verification enabled
(`.tmp/a500-render-cache-contract.log`). Enhanced full combat/landing
`render_cache_verify_full` then recorded 256 hits, 1184 misses and zero
incorrect hits, with parity CSV identical to `object_cache_on_full`.
Most requests were still cold columns, so this evidence predicts limited
reuse benefit; it does not establish a performance gain.

Performance run without reference checking `render_cache_on_full` completes with unchanged
parity. Combat windows remain 35–43 FPS, mean reported 38.87 versus 38.78
in `object_cache_on_full`, and maximum field gap remains 3. Final completed
sample is field 3973 versus 3970. This is no convincing improvement; do not
enable the visual cache by default based on these results. The experiments
retain the same gameplay behavior, but only 256 of 1440 column requests are
reusable in this route. Profile the current release hot paths before extending
this approach or replacing more caches. The original generic object cache
remains enabled; the visual-column trial remains disabled.

Fresh A500 `object_cache_town_stages` profiling (visual cache off, generic
object cache on), with 128 hitch records starting at scroll 5200, reports
mean/max raster-line samples: logic 146.7/403, sprite/erase/ambience selection
49.7/81, whole streaming 47.6/68, column calculation 14.6/29, tile copying
26.0/35. Within logic: engine 25.4/27, weapons+flak 37.6/158, enemy/target
10.0/58, wingman 16.0/150, collisions 36.2/102. Weapons alone are 20.3/157
and flak spawning 17.3/35. These are overlapping instrumented stage samples,
not additive independent timings or release FPS; compare scope carefully.
The parity CSV remains identical to `object_cache_on_full`. Current evidence
prioritizes logic/weapon/collision costs over further visual-column caching.
The runner restored the normal EXE/ADF after profiling.

## Aircraft collision locality trial

`HAR_LOCAL_AIRCRAFT_CACHE=1` gives the existing 64-entry aircraft collision
cache per-column revision stamps. A 64-element ULONG table (256 bytes)
tracks local changes; global world resets update every stamp. Mutations touch
only their one/two column slots. Hash collisions can invalidate an unrelated
entry but cannot preserve a changed one, including UWORD history aliases.
The existing cache still checks full column and row tags. Its facade-first
collision semantics remain separate from generic object cells.

The feature defaults off while tested. With `HAR_LOCAL_AIRCRAFT_CACHE_VERIFY`,
each hit is compared to the facade resolver plus the uncached generic
resolver, including validity and all ObjectCell fields. Parity logs write
`#aircraft_cache,hits,misses,incorrect_hits`; the contract requires observed
hits and zero incorrect hits. Other caches retain their current revisions.

The full A500 contract passed (`.tmp/a500-aircraft-local-contract.log`).
Enhanced combat/landing `aircraft_local_verify_full` records 9732 checked
hits, 2396 misses and zero incorrect hits, with unchanged parity CSV versus
`object_cache_on_full`. The non-verifying `aircraft_local_on_full` also has
unchanged parity and reports combat windows at 34–44 FPS (mean 39.64), max
field gap 3. The earlier generic-cache run was 35–43, mean 38.78, max gap 3;
this mixed small difference needs a current-source baseline before enabling.

Matched current-source `aircraft_local_off_full` completes with identical
parity to the enabled run: off 35–43 FPS (23 windows, mean 39.00), on 34–44
(22 windows, mean 39.64), both max field gap 3. Linear interpolation between
the 100-field samples estimates scroll 1000–6500 at 46.64 seconds off versus
45.91 on; this is approximate, not a precise endpoint timer. The improvement
is small and does not establish better worst-case pacing. Keep this trial
disabled in the normal build and prioritize larger remaining costs. The
runner completed successfully and restored normal EXE/ADF with the proven
generic object cache on and both experimental caches off.

## Local repaint for town bomb hits

Player and Wingman bombs still repainted a full 25-row town column after a
hit, while rockets already repainted only the smoke cell and optional row
above. Bombs now use the same `dirtyRedrawWorldTileIfSmoke` calls. Town
facades are part of the base tile column, so the old comment requiring a
whole-column pass for a separate facade overlay was obsolete.

The contract compares entire bitmaps against full column redraws for the
generated building types/rows in both modes and both ring placements.
Alternate cases place runtime flak above the hit to exercise suppression of
the extra smoke row. The initial test's pixel comparisons passed but its
one/two-row coverage assertion failed: the sampled facade rows all appeared
as sky to the generic smoke-creation probe. Explicit upper-cell occupancy
corrects that test setup without changing smoke generation in gameplay.
Contract failures now retain their check names in the result file instead
of reporting only `FAIL`, making future failures diagnosable after restoration.

The corrected full contract passed in
`.tmp/a500-town-hit-rows-covered-contract.log`, including the complete bitmap
comparison and explicit smoke-pattern/ring-placement coverage checks.

A500 Enhanced `town_bomb_rows_full` completes combat/landing with identical
parity to `aircraft_local_off_full`. Combat windows stay at 35–43 FPS (mean
38.87 versus 39.00), while maximum field gap falls from 3 to 2 in scroll
1000–6500 at speed 15. This is a reduction in the measured worst gap from
60 to 40 ms, not a higher average frame rate or proof of smooth scrolling.

Stock A1200 `A1200_town_bomb_rows_full` completes with the same parity CSV
as A500. All 18 selected combat windows report 50 FPS, max field gap 1 and
zero hitches. Both runners exited successfully and rebuilt normal EXE/ADF;
local town bomb repaint is retained in the normal build.

## Four-sample engine synthesis trial

The engine synthesizes 48 noise bytes per gameplay update. Existing profiling
measured roughly 25 raster lines in the engine stage every frame. With
`HAR_ENGINE_BATCH4=1`, three small tables (2560 bytes total) produce four
samples and advance the LFSR by four steps. Feedback cannot reach either
output tap during those first four steps, allowing independent white/rumble
byte lanes. Guard bits prevent arithmetic carries between output bytes.
The sample count, speed-dependent bias, final LFSR state, and buffer position
are unchanged. Odd addresses, short tails and unsupported speeds retain the
scalar fallback. Word-aligned long stores are legal on the 68000.

`python tools/check-engine-noise-tables.py` reads the actual stored tables and
checks all 65536 input states at all five engine speeds against four original
scalar steps; it passes. The full A500 contract also passes with this flag
(`.tmp/a500-engine-batch4-contract.log`), comparing output bytes and state
over repeated updates from aligned, unaligned and wrapping buffer offsets.
This verifies generated data, not cycle-identical concurrent Paula playback.
The feature was initially off pending the performance and machine checks below.

Matched A500 Enhanced combat/landing runs `engine_batch4_off_full` and
`engine_batch4_on_full` have identical parity CSVs. Both select 23 combat
windows at scroll 1000–6500, speed 15. Off reports 35–43 FPS, mean 38.87;
on reports 36–45 FPS, mean 40.83 (about 5% higher). Both have max field gap 2.
The improvement reduces steady CPU cost, but does not establish 50 Hz A500
combat or eliminate alternating one/two-field updates.

Stock A1200 `A1200_engine_batch4_on_full` completes with identical parity to
A500; all 18 selected combat windows report 50 FPS, max field gap 1, zero
hitches. Four-sample synthesis is now enabled by default in the normal build.

## Sea candidate invalidation trial

`HAR_LOCAL_SEA_CANDIDATES=1` separates the revision used by wave eligibility
from general world edits. The candidate selector queries only four rows
derived from its wave Y formula (currently rows 15–18). Single-cell changes
outside those rows preserve the candidate revision; full-column and world
resets remain conservative. Other wave drawing/background invalidation still
uses the existing world revision, so this changes selection reuse only.

Persistent row keys wrap to UBYTE, which the eligibility check respects.
Flak slot replacement additionally invalidates for the old row: new sky flak
can evict an existing sea-row entry. Pruning uses the actual lookup row when
it clears a matching slot, even if an older list entry has a different row.
The contract's full-window wave reference now bypasses all cell caches and
adds explicit sky/sea replacement and prune cases. The feature was initially
off pending correctness and performance checks.

The full A500 contract passed with this flag enabled
(`.tmp/a500-sea-local-contract.log`), including the independent full-window
reference and the new sky/sea replacement and pruning cases.

Matched A500 Enhanced `sea_local_off_full` / `sea_local_on_full` complete
combat and landing with identical parity CSVs. Both select 22 combat windows
at scroll 1000–6500, speed 15: off 36–44 FPS, mean 40.64; on 39–47 FPS,
mean 43.23 (about 6% higher). Both retain max field gap 2. This improves
steady pacing but does not yet establish smooth 50 Hz A500 combat.

Stock A1200 `A1200_sea_local_on_full` matches A500's parity CSV, with all 18
selected combat windows at 50 FPS, max field gap 1 and zero hitches. Selective
sea-candidate invalidation is now enabled by default in the normal build.

## A500 ordinary flight, Classic stress and opening profile

With current defaults, `optimized_ordinary_full` (Enhanced, AI wingman,
normal enemy rate) records 21 consecutive cruise windows at scroll 761–6761
with 50 FPS, max field gap 1 and zero hitches. Opening and landing still
include occasional two-field updates. This is scripted ceiling flight with
headless invulnerability and infinite fuel, not unrestricted gameplay.

`optimized_classic_full` completes the full Classic stress/landing route
with parity identical to `object_cache_verify_classic_full`. Its 21 combat
windows (speed 15, scroll 1000–6500) report 34–49 FPS, mean 44.62, max field
gap 2. The 34 FPS window is at scroll 4844. Neither this result nor the
Enhanced stress result establishes constant 50 Hz A500 combat.

`optimized_opening_stages` associates several early two-field updates with
phase-only sea animation: logical frames 240, 264 and 288 have wave-stage
costs 101, 94 and 59 raster lines. A missile raster wait also contributes at
240; a separate logic spike occurs at 202. Thus waves are a candidate cost,
not a complete explanation of all opening stalls. Instrumented stage costs
include measurement overhead and may wrap for stages exceeding a field.

A trial explicitly addressed four display planes with constant strides and
byte accesses. The full A500 contract passed (`a500-wave-phase-contract.log`).
However, `wave_phase_opening_stages` reports exactly the same FPS windows
and hitch frames as `optimized_opening_stages`, with identical parity CSVs.
The wave stage at frame 288 falls from 59 to 46 raster lines, while the other
opening hitches remain. The trial was reverted because it did not improve
observed frame pacing. The added direct full-bitmap contract comparison
against erase/redraw remains, covering all four phases at two ring positions.

Real damage, failure audio, crash/recovery and continuous visual motion
remain unverified: the current headless driver suppresses actual damage.

### Retention across animation phases (trial)

The retention matcher previously discarded otherwise unchanged placements
on a phase transition. When a scrolling candidate change also required a
full update, that forced erase/save/draw work for the whole group. The trial
allows retained placements across phase changes, updates their masks after
streaming and then draws incoming waves. World revisions, stream collision
checks and shared-byte boundary checks remain conservative. Phase writes
skip footprints already at the requested phase. The full A500 contract
passes (`.tmp/a500-retain-wave-phases-contract.log`), including whole-bitmap
comparison with erase/redraw across animation phases and ring positions.
`retained_phases_opening` has identical parity and FPS windows to
`optimized_opening_stages`. Wave-stage costs at hitch frames 240/264 fall
from 101/94 to 71/64 raster lines; the associated missile wait at 240 falls
from 115 to 2. These are instrumented observations, not proof that all
opening stalls are resolved.

Full A500 Enhanced `retained_phases_full` has identical parity to
`sea_local_on_full`. Both select 22 combat windows at 39–47 FPS, max field
gap 2; means are 43.27 versus 43.23, effectively unchanged. Stock A1200
`A1200_retained_phases_full` matches the same parity and has 18 combat windows
at 50 FPS, max gap 1, zero hitches. The change is retained to avoid redundant
erase/save/draw work, without claiming a demonstrated FPS improvement.
The runner restored and rebuilt the normal EXE/ADF successfully.

Next measurement target: the opening logic spike at frame 202 spends much
more time than the sum of the instrumented engine/weapons/enemy/wingman/
collision sections. HUD drawing lies outside these subsections and needs
its own raster timing; the current field counter is too coarse to attribute
the spike. This is a candidate to measure, not an established cause.

### HUD attribution and carrier asset delta

Profiler stage 28 now measures raster time inside `drawHudValues`; stage 20
remains its raw field count. `hud_opening_stages` shows no HUD drawing at
hitch frame 202, ruling it out for that spike. HUD cost at frame 288 is 36
raster lines. FPS windows match the preceding opening run.

The live CPU-Wingman launch path calls `dirtyRedrawNativeCarrierAt`, which
compared the immutable parked/unparked composites tile by tile on every
launch. The current trial prepares its 36-byte changed-tile table once in
world-index setup. The launch still rebuilds exactly the same changed tiles
and both physical placements from world truth. It adds 37 bytes of static
state and retains compatibility with repacked assets. The full A500 contract
passes (`.tmp/a500-carrier-delta-contract.log`), including whole-bitmap carrier
transition equivalence. `carrier_delta_opening` matches the preceding
`hud_opening_stages` parity CSV. The two-field hitch at logical frame 202
disappears; the other opening hitches at 240/264/288 remain. This removes one
observed takeoff stall, not all startup pauses or combat frame overruns.
Without detailed profiling, A500 `carrier_delta_ordinary_full` matches
`optimized_ordinary_full` parity, including landing. The first two flight
windows now have one hitch each instead of two each. This comparison includes
both wave-phase retention and the carrier delta change. All 21 cruise windows
at scroll 750–6800, speed 15 remain 50 FPS. The menu-to-game setup gap remains
separate from flight pacing. Stock A1200 `A1200_carrier_delta_ordinary_full`
matches A500 parity; all 20 selected cruise windows report 50 FPS, max field
gap 1 and zero hitches. The carrier delta preparation is retained.

The general world revision remains in the saved-background retention guard.
Changing that to the sea eligibility revision needs separate validation of
every framebuffer rewrite; candidate eligibility alone does not prove that
saved pixels remain valid after column or overlay drawing.

## Current combat profile and complete rocket shape cache trial

`current_combat_stages` profiles the current A500 Enhanced stress route,
recording the first 128 hitches after scroll 3500. Mean raster costs include
logic 135.8, stream 47.6, missile draw 36.5, impact 22.8 and bomb draw 19.3.
HUD averages 6.7 (max 82) inside logic. Engine averages 15.1, weapons/flak
35.9, wingman 19.6 and collision 35.8, also inside logic. These overlapping
subsections must not be summed with their parents. Wave draw is only 1 here;
further sea work cannot resolve this combat bottleneck.

`HAR_ROCKET_FULL_SHAPE_CACHE=1` trials dedicated shifted-shape slots for the
gameplay tile groups (53–56 and 98–101, plus black enemy tiles 53–55), keeping
all eight shifts resident. Other tiles retain a keyed fallback. This replaces
the eight-slot direct-mapped cache with 88 slots plus fallback, about 3 KB
additional state, without changing drawing or saved-background layout.
Default remains off pending correctness and performance validation.

The full A500 contract passes with the flag enabled
(`.tmp/a500-full-rocket-shapes-contract.log`). `full_rocket_shapes_full` has
identical parity to `retained_phases_full`, but the selected combat windows
remain 39–47 FPS, max field gap 2. Means are 43.48 (21 windows) versus 43.27
(22 windows); differing window alignment and such a small difference do not
justify promotion. The feature stays off. The later carrier-delta change in
the current build also means this is not a strict single-flag A/B benchmark.

Independent pixel-decoder checks now also compare cached masks for all
gameplay directions, monochrome variants and shifts, plus fallback tiles.
These additional checks pass in the full A500 contract with the default
small cache (`.tmp/a500-rocket-cache-reference-contract.log`). The normal
EXE/ADF was rebuilt successfully afterwards.
The profile and trial suggest prioritizing composition/background traffic
over further expansion of the immutable mask cache; they do not prove its
exact share without direct cache-hit/miss timing.

Next candidate: `bobEraseColumn` retains two world-derived columns but rejects
both after any global world revision. It may rebuild for remote flak/smoke
changes unrelated to either cached column. Inspect its mutation dependencies
and test column-local invalidation against full world reconstruction before
using it; unlike the immutable masks, these values must track real damage.

## Local BOB erase-column invalidation trial

`HAR_LOCAL_BOB_ERASE_CACHE=1` retains the existing two column entries but
invalidates only matching UWORD column ranges on local world mutations.
World resets invalidate both; presentation changes and full column tags are
still checked on lookup. It adds no new cache storage. Replacing an occupied
flak lookup slot explicitly invalidates the former column as well as the
new one; their actual column tags need not match even though their lookup
slots collide. Direct masked overlays are still redrawn from current state.

The contract now uses the real world-reset notification rather than directly
incrementing a private revision. Added cases retain both entries on distant
flak, compare local flak and displaced-slot restoration with a fresh world
column, and check persistent smoke. Existing crater/target/town pixel tests
also exercise this background reconstruction path. Default is off pending
correctness and measured performance.

The full A500 contract passes with local BOB invalidation enabled
(`.tmp/a500-local-bob-cache-contract.log`). Matched A500 Enhanced runs
`local_bob_cache_off_full` and `local_bob_cache_full` have identical parity.
Both select 21 combat windows, 40–47 FPS, max gap 2. Mean FPS is 43.33 off
and 43.67 on (under 1% difference). This is too small to prioritize or claim
meaningfully smoother combat; the feature remains off. The off run restored
and rebuilt the normal EXE/ADF successfully.

Another geometry candidate to investigate is the 86-byte ring period:
projectile and tile placement repeatedly reduce world coordinates modulo
688 pixels / 86 columns. A 64-column period could also reduce buffer memory,
but requires a lower stream-ahead cap (currently 64), correct mirror extent,
negative/reverse scrolling, and full seam/landing checks. Do not change the
period alone: the present ahead cap would reach the same physical column as
the left edge. This is an unmeasured candidate, not an implemented change.

## Compact 64-column ring trial

`HAR_COMPACT_WORLD_RING=1` changes the row stride from 130 to 108 bytes:
64 primary columns, 42 mirrored fetch columns and two margin bytes. The
world bitmap falls from 109200 to 90720 bytes, saving 18480 bytes of chip
RAM. The world-coordinate period becomes 512 pixels / 64 columns.

The stream-ahead cap falls from 64 to 60 columns, leaving four columns
before a physical slot can alias the left edge. A compile-time assertion
rejects an ahead cap at or beyond the ring period. Initial carrier priming
still fits: its 96-pixel camera excursion and 336-pixel fetch are unchanged.
The existing geometry-dependent pixel tests are running with this flag.
The existing full A500 contract passes with this flag
(`.tmp/a500-compact-ring-contract.log`). It remains off pending full route
and performance validation. Promotion also needs a direct visible-column
streaming check over multiple ring periods: existing placement comparisons
alone do not prove that ahead streaming never corrupts the visible window.

A500 `compact_ring_full` completes with identical parity to
`local_bob_cache_off_full`. Both select 21 combat windows: compact 37–47 FPS,
mean 43.38; reference 40–47, mean 43.33. Both have max gap 2. There is no
demonstrated FPS gain and the minimum is worse, so compact stays off. A
smaller period also increases the fraction of columns needing a mirrored
copy (42/64 instead of 42/86), which may offset cheaper placement arithmetic.

`HAR_WIDE_WORLD_RING=1` is the alternative 128-column trial: 172-byte rows,
1024-pixel period, unchanged 64-column ahead cap. It needs 144480 bitmap
bytes, 35280 more than the default, but only 42/128 primary columns need
mirroring. The two alternate flags are mutually exclusive. The full A500
contract passes (`.tmp/a500-wide-ring-contract.log`), including allocations
for its pixel references. Both geometries were initially disabled pending
the full route measurements and visible-fetch checks below.

A500 Enhanced `wide_ring_full` has identical parity to the reference.
Combat windows report 38–49 FPS, mean 45.40 (20 windows), versus 40–47,
mean 43.33 (21 windows) with the default geometry. Max gap remains 2.
This is about 4.8% higher mean window FPS, not constant 50 Hz or a better
minimum. Window boundaries differ because the route advances faster.
Interpolating hardware-frame timestamps at scroll 1000 and 6500 gives about
42.10 seconds for the default, 42.07 for compact and 39.95 for wide. This
common-distance estimate supports a roughly 5% improvement for wide; it is
interpolated from logging windows, not an exact per-pixel timestamp.

The added visible-fetch reference streams two ring periods through sea,
town and land in both modes. It compares actual DMA fetch positions at
three offsets with fresh world reconstruction at the corresponding absolute
columns, including mirrored copies. Its first run was deliberately stopped
after finding missing world-seed initialization in the fixture; the corrected
fixture initializes the procedural route before selecting scenes. That
interrupted run is not a pass. The corrected fixture exceeded the former
240-second full-suite limit, with no completed result; that is not a pass
either. The runner now permits up to 600 seconds and defaults to 360 for
the expanded suite, matching the verified wide-ring run. This only
changes the test execution deadline, not emulated speed or FPS requirements.
The corrected full suite passes with the 360-second deadline
(`.tmp/a500-wide-ring-visible-contract-v3.log`), including the visible-fetch
comparison through two periods in all six mode/terrain combinations.

A500 Classic `wide_ring_classic_full` matches `optimized_classic_full` parity.
Its 20 selected combat windows report 42–50 FPS, mean 46.90, versus 34–49,
mean 44.62 over 21 reference windows. Both have max gap 2. The earlier
carrier-delta change is also present relative to that Classic reference;
this comparison is not a strict single-flag A/B test.

Stock A1200 `A1200_wide_ring_full` matches A500's parity and reports 50 FPS,
max gap 1, zero hitches in all 18 selected combat windows. The wide geometry
is now enabled by default (`HAR_WIDE_WORLD_RING=1`). Compact stays off; to
reproduce its trial, explicitly disable wide as well. The original geometry
is available with both alternate flags set to zero.

The normal EXE/ADF was rebuilt in `.tmp/wide-ring-default-release-build.log`.
This is a verified performance improvement within stock A500 memory, not
completion of the smooth-50-Hz goal. Continuous visual motion, actual damage/
crash paths and the remaining combat overruns still need work.

## Real damage coverage and sparse failure-smoke erase

The earlier cruise/stress drivers suppressed actual player damage. A new
test-only `HAR_HEADLESS_DAMAGE_EXERCISE=1` driver calls the real flak damage
handler, enables normal fuel/life rules and waits for failure, crash and its
terminal state. It does not change release gameplay. The parity runner now
rejects this exercise unless armour reaches zero and those transitions occur;
Enhanced must also actually draw smoke. The log records injected hits and
logical transition frames separately from ordinary combat counters.

Stock A500 Enhanced `real_damage_enhanced` exposes a severe previously hidden
overrun: smoke/failure windows reach 8 FPS minimum, 13 average and a six-field
gap. `real_damage_stages` confirms expensive smoke background reconstruction
in stage 2 (its raster counter can wrap over multiple fields); smoke drawing
also costs about 90–110 raster lines late in the descent. This is not evidence
that ordinary flight is smooth merely because an invulnerable cruise is 50 FPS.

The old smoke eraser restored the complete rectangle around all six particles,
including large empty spaces between puffs. Each footprint now records up to
12 unique columns with a touched-row bit mask. It reconstructs only occupied
cells from current world data, including direct objects and mirrored ring
placements. Conservative bounds still govern streaming invalidation. No stale
saved background or reduced particle cadence is introduced.

Matched A500 `real_damage_sparse` improves the failure minimum to 25 FPS and
maximum gap to two fields; the lowest sample average is 31 FPS. Both runs log
exactly `22,352,100,0,520,559,624,36` for injected hits, first hit, armour before/
after, failure/crash/terminal frames and smoke draws. This is a substantial
improvement, but not yet 50 Hz during damage.

The full A500 contract passed (`.tmp/a500-sparse-failure-smoke-contract.log`),
including a new whole-bitmap before/after check for scattered particles,
vertical clipping, negative X, ring seams and town scenery in both modes.
This uses freshly rendered whole world columns as its background reference,
not the sparse eraser's own bookkeeping. A500 Classic's real damage exercise
also passed: immediate failure/crash at logical frame 520, terminal 584 and
zero failure-smoke draws as expected. Its final sample averages are 48–50 FPS,
with isolated two-field gaps. Stock A1200 `A1200_real_damage_sparse` reproduces
the Enhanced A500 damage-event record exactly and holds 50 FPS through descent
and crash. The short A1200 run finished before a GUI snapshot was obtained;
these results establish automated timing and event checks, not visual motion.

Final A500 `sparse_smoke_full_visible` passed the longer Enhanced combat route
with parity identical to `wide_ring_full`. Across the same 20 selected combat
windows (scroll 1000–6500, speed 15), mean average FPS remains 45.40, range
37–49 versus 38–49 before, and max gap remains two fields. Ordinary combat is
therefore still below the 50-Hz goal. Three GUI snapshots during this run show
coherent land and town scenery, objects and ring display; they are point-in-time
checks, not continuous-video evidence of smooth motion or opening timing.
The runner restored the normal build successfully: EXE 513660 bytes, ADF 901120.

## Failure-smoke row writes and world-column reuse

The next A500 trial, `real_damage_smoke_rows`, combines each puff's row into
one or two byte masks before updating the four display planes. Ring placement
is resolved per puff, preserving the original dither and clipped pixels. By
itself this produces the same frame samples as `real_damage_sparse`: the low
failure window remains 31 average FPS. The instrumented follow-up
`real_damage_smoke_rows_stages` still shows background reconstruction dominating
late failure frames (stage 2 roughly 236–299 raster lines, versus stage 9
roughly 77–96). Those are instrumented timings, not release frame budgets.

A separate 16-slot cache now reuses world-derived columns beneath the slowly
drifting puffs. Full column tags, presentation mode and the global world-object
revision guard every hit; direct masked objects are still painted from live
state. This does not use the experimental local BOB invalidation or retain
framebuffer backgrounds.

A500 `real_damage_smoke_cache` raises the low sample average from 31 to 40 FPS,
and the preceding sample from 44 to 48. The minimum remains 25 FPS/max gap two
fields. The entire parity log and damage-exercise record match the sparse-only
reference. No claim of a steady 50-Hz failure sequence is justified yet.

New contracts compare byte-batched drawing with the original pixel routine
over ages 0–29, all pixel shifts, all pens, clipping and negative/wrapped X;
cache checks cover reuse, same-slot aliases, revision/mode changes and actual
flak insertion/replacement plus persistent wreck smoke. The first contract
build stopped because this freestanding project has no declared `memcmp`;
the tests now use its existing `referenceBuffersEqual`. No emulator check
ran in that failed build. The corrected full A500 contract passed in
`.tmp/a500-smoke-row-cache-contract-v2.log`, including all three new smoke
checks and the extended real-mutation test. The cache and batched writer are
retained together; the measured improvement belongs to column reuse, since
row batching alone did not improve the release FPS samples.

Stock A1200 `A1200_real_damage_smoke_cache` also passed, with 50 FPS and
one-field gaps throughout the failure/crash windows and a parity log identical
to A500. Its runner restored the normal EXE (513872 bytes) and ADF (901120 bytes).
This round improves failure rendering; it does not establish smooth 50-Hz
ordinary A500 combat, which remains a separate measured overrun.

## Current wide-ring combat profile and powerup-row trial

After the failure-smoke changes, stock A500 `current_wide_combat_stages`
profiles the first 128 logged hitches after scroll 3500. Means in raster lines
are stage 0 29.0, all logic 141.1, erase/sprites 28.2, streaming 44.1, missile
draw 31.7, impact 21.3, powerup 20.1 and bomb 14.0. Powerup peaks at 120 lines.
Nested stages overlap their parent totals: collisions 36.6, weapons/flak 35.7,
wingman 19.7, engine 15.0 and HUD 8.5 must not be added to all logic again.
These are instrumented hitch samples, not release frame budgets.

The powerup transition trial resolves its two ring positions once and composes
each row's four plane bytes once, then writes the same values to the optional
mirror. It retains row-by-row reconstruction/drawing to avoid the old blank
erase interval. A500 `powerup_shared_rows_full` has identical gameplay parity
to `sparse_smoke_full_visible`. The same 20 selected combat windows average
45.55 versus 45.40 FPS, with range 38–49 versus 37–49 and max gap two fields.
That small change does not establish an overall scrolling improvement. A
matched instrumented comparison now exists in `powerup_shared_rows_stages`.
For the 24 common logical hitch frames where the reference powerup stage was
active (>2 raster lines), stage 8 falls from 72.42 to 30.42 lines on average.
This establishes a local rendering-cost reduction, not a comparable whole-route
FPS improvement. A new full-bitmap reference compares the transition against
fresh base columns plus the unchanged standalone powerup draw, covering both
modes, all types, tile boundaries, vertical clipping, optional mirrors and the
ring seam. Its full A500 contract passed in
`.tmp/a500-powerup-shared-rows-contract.log`. The optimized compositor is
retained for its measured local cost reduction, with no new persistent cache
or change to update cadence.

Stock A1200 `A1200_powerup_shared_rows_full` matches the A500 gameplay parity
exactly and reports 50 FPS/max gap one field in all 18 selected combat windows.
The runner restored the normal release EXE/ADF. A500's overall combat overrun
remains open; the powerup improvement alone does not resolve it.

The generated 68000 assembly also identifies a separate next candidate:
`objectCellForWorldColumnTile` computes a 108-byte column stride through multiple
address operations, makes a variable long shift for its row mask, spills
temporaries and copies a hit through four byte moves. `ObjectCacheColumn` is
currently 100 bytes of cells plus two 4-byte fields. Investigate representation
and alignment against the generated code before proposing another resolver
cache or claiming a speedup; no layout change has been made in this trial.

## Object-cache layout and call-path experiments

`HAR_OBJECT_CACHE_POWER2_STRIDE=1` pads each cached column from 108 to 128
bytes, adding 1280 bytes across 64 slots. The generated 68000 code confirms
one shift by seven for the slot address instead of the former address chain.
A500 `object_stride128_full` matches `powerup_shared_rows_full` gameplay
parity, but averages 45.50 versus 45.55 FPS over the same 20 selected combat
windows (range 39–49 versus 38–49). There is no demonstrated speed benefit
worth its memory cost, so the layout flag remains off by default.

The next isolated trial, `HAR_OBJECT_CACHE_SPLIT_MISS`, keeps the existing
compact storage but separates the miss resolver into a non-inlined helper
and inlines the hit path. A500 `object_split_miss_full` matches gameplay parity
but averages 45.40 FPS (20 selected windows, range 38–49). ELF symbols confirm
the standalone hit function disappeared while `fillObjectCacheCell` remains.
This did not improve the whole-route result, so its default remains off.

`HAR_ALIGNED_OBJECT_CELLS=1` gives the four-byte local ObjectCell type two-byte
alignment, allowing wider copies without assuming alignment of arbitrary byte
buffers. A500 `aligned_object_cells_full` also matches gameplay parity and
averages 45.45 FPS (20 windows, range 38–49). It likewise remains off. These
three trials do not demonstrate an FPS benefit; do not promote them based only
on a shorter-looking instruction sequence. All runners restored the normal
513364-byte EXE and 901120-byte ADF with all three flags disabled.

The late projectile group explicitly waits for the lowest old/new missile row
before erasing and drawing. Existing profiles separate that raster wait from
erase and draw time. Further investigations should distinguish time saved in
logic from time absorbed by display synchronization, and consider repeated
world-column construction and the remaining rendering work. Do not remove the
wait merely to improve FPS: it protects the single-buffered projectile image.

## Combined primary/mirror terrain writes

`HAR_MIRRORED_TILE_BATCH=1` replaces two independent base-tile passes for
mirrored columns with one writer. It resolves each source tile once and loads
each row's four colour bytes once, then stores them to both placements. It
adds no persistent cache memory. Non-mirrored columns, the 25-row gameplay
map and stream row-credit accounting are unchanged; the paired writer clips
only its pixel output at the actual 168-line playfield. Direct masked objects
still get their normal separate placement passes.

A500 `mirrored_tiles_full` matches `powerup_shared_rows_full` gameplay parity
and improves the same 20 selected combat windows from mean 45.55 FPS/range
38–49 to mean 46.20/range 40–50. The full A500 contract passed with an
independent byte-for-byte comparison against two calls to the old single-column
writer, covering all 256 tile IDs, both modes, partial row ranges and the last
valid mirrored byte (`a500-mirrored-tiles-stack-fixture`). The paired writer is
now enabled by default.

A500 Classic `mirrored_tiles_classic_full` also preserves gameplay parity:
19 selected combat windows average 47.58 FPS, range 43–49, versus the earlier
`wide_ring_classic_full` reference's 20 windows at 46.90, range 42–50. That
reference predates the powerup compositor change, so it is not an isolated
measurement of this writer. Both A500 modes still have two-field gaps.
Stock A1200 `A1200_mirrored_tiles_full` preserves Enhanced gameplay parity
and runs all 18 selected combat windows at 50 FPS with maximum field gap 1.
These instrumented runs do not establish continuous visual smoothness on A500.

The first full trial failed the visible-ring check (`a500-mirrored-tiles-contract`);
gameplay parity alone was not sufficient. A diagnostic run was manually stopped
without a result and must not be counted as passed. Investigation found that the
contract runner's generated function reserved 3540 stack bytes before nested
calls, while the CLI startup does not issue a larger Stack command. Its six
large GameState fixtures and weapon fixture are now static (still reset at entry),
reducing the generated local stack reservation from 3540 to 744 bytes.
An initial task-bound diagnostic returned capacity 1600 and apparent headroom
281740: those bounds do not describe the active DOS command stack and are not
evidence of its available space. Result metadata now reports the CLI's configured
default stack size in bytes, without claiming to measure active stack headroom.
Visible-ring failures also record allocation/bounds classification or first
mode/scene/step/column/Y/plane/actual/expected/last-streamed values. A fresh full
validation with these fixture changes passed; the original failure has not
been conclusively attributed to stack pressure.

The suite now includes several whole-bitmap sweeps and exceeds its old default
six-minute window. The runner defaults to 900 seconds (maximum 1200), prints
the complete result before throwing, and still restores the normal build.
Only the host timeout changed; emulated CPU speed and memory did not.

## Consecutive-frame profile after paired terrain writes

`HAR_DEBUG_PERF_CAPTURE_ALL_FRAMES=1` optionally makes the existing bounded
128-row stage capture include every frame after the selected scroll position,
rather than only hitches. It defaults to zero, uses the existing storage and
post-exit writer, and has no effect without the stage/hitch instrumentation.
The `!` record layout is unchanged. The CIA frame gap is sampled by WaitVbl
at loop start, so each record retains the preceding workload's stage values.

A500 Enhanced `mirrored_all_frames_stages` records 128 consecutive workloads
at scroll 3500–3881 (logical frames 1369–1496) under the full weapon/enemy
stress scenario. Gameplay parity is identical to `mirrored_tiles_full`.
121 workloads have a one-field gap; seven have a two-field gap. Mean stage
costs in raster lines, grouped by that following gap:

| Work | One field | Two fields |
| --- | ---: | ---: |
| Gameplay logic (stage 1) | 97.98 | 154.57 |
| Whole terrain stream (3) | 36.69 | 47.71 |
| Column construction (23, included in 3) | 7.57 | 18.43 |
| Tile writes (24, included in 3) | 20.60 | 22.14 |
| Collisions (15, included in 1) | 24.20 | 37.86 |
| Missile raster wait (21) | 1.74 | 10.29 |

Summing disjoint stages 0–10 plus 21 and 22 gives 239.05 lines on one-field
workloads (180–299) and 326.86 on two-field workloads (300–410). Do not add
nested stages 11–18 or 23–28 again. The difference from a nominal 312-line
deadline includes instrumentation and unmarked loop work; these sums are not
exact available CPU budgets. Individual beam-sampled sections must remain
shorter than one PAL field to avoid wrap ambiguity.

Six of seven hitches construct a column, versus 42 of 121 one-field workloads.
There is therefore evidence for testing workload redistribution using the
offscreen ring reserve, not merely an assumption that quiet frames have room.
However, the worst workload (frame 1373) reaches 410 measured lines with only
46 in streaming and 84 in collision work: removing all streaming would still
leave too much work. Scheduling alone cannot solve every observed hitch.
Any scheduling trial must retain guaranteed fetch lead, eventual stream
progress, the conservative BOB overwrite prediction, and authoritative damage
restoration. No adaptive scheduling change has been enabled from this profile.
The runner completed and restored the ordinary paired-writer build.

## Land-first baseline experiment (not retained)

Closer inspection of workload 1373 shows stage 16 (player object-map search)
at 22 lines, stage 17 at 4, and stage 18 (fatal/flak handling, including live
flak background restoration) at 41, within the 84-line collision parent.
It also has 61 lines of missile raster waiting and 60 of wingman work. Thus
the spike must not be described as 84 lines of repeated collision searches.
The raster wait is required for the single-buffer missile erase/draw interval;
it is not freely removable CPU overhead.

An A500 full combat trial, `terrain_first_base_full`, reordered the base-column
loop so land cells did not first resolve and overwrite the sea baseline. It
was tested via temporary `HAR_TERRAIN_FIRST_BASE=1`, leaving coast priority,
craters and subsequent object passes intact. It preserved gameplay parity
against `mirrored_tiles_full`. The same 20 selected windows averaged 46.30 FPS
(41–50), versus 46.20 (40–50), with maximum field gap 2 in both runs. This
single-run difference is too small to establish a worthwhile gain. The code
and temporary flag were removed, and the runner restored the ordinary build.
No full graphics contract or A1200 promotion test was needed for this rejected
candidate. Background reconstruction after real hits remains a useful target,
but this baseline reordering does not provide an established solution.

## Paired BOB base-tile restoration experiment (not retained)

`paired_bob_erase_full` reused the validated paired terrain writer inside
`bobCompositorErase` for mirrored cells, loading each base tile once before
the two direct-overlay passes. It retained full authoritative column resolution
and unchanged radar/passability publication. Full A500 Enhanced combat parity
matched `mirrored_tiles_full`; all 20 selected performance windows aggregate
to the same 46.20 FPS mean, 40–50 range and maximum field gap 2. This does not
establish a useful end-to-end improvement. The experimental branch was removed
and the normal build rebuilt. A future single-cell resolver must account for
the full column builder's visual precedence and metadata side effects; merely
reducing duplicate writes is insufficient in this measured path.

## Background-cache miss classification

Optional `HAR_DEBUG_BOB_CACHE_STATS=1` records session-wide lookups, hits,
invalid/empty entries, tag misses, revision misses and mode misses, in that
order, as a `#bob_cache` CSV comment after gameplay has stopped. Miss reasons
are exclusive and follow lookup order: a tag mismatch may also have an old
revision, but is counted as a tag miss. A tag miss does not by itself prove
capacity pressure; the requested column may never have been cached before.
Normal builds compile out the counters and their lookup overhead.

A500 `bob_cache_causes_full` preserves gameplay parity with
`mirrored_tiles_full`. It records `775,398,2,186,189,0`: 398 hits and 377
misses. Its 20 selected combat windows average 46.15 FPS (40–50), compared
with 46.20 without these counters. The counts cover the whole test session,
not only those selected combat windows. A controlled 16-slot capacity trial
uses the same instrumentation and global revision checks; the default remains
two slots until there is evidence to justify a change.

`bob_cache16_causes_full` uses `HAR_BOB_ERASE_CACHE_SLOTS=16` with otherwise
identical flags. It preserves gameplay parity and records
`775,409,16,132,218,0`: only 11 additional hits (1.42% of lookups), despite
eight times as many entries. Selected combat windows average 46.20 FPS,
range 40–50, too close to the instrumented two-slot reference's 46.15 to
claim a useful pacing improvement. Some former tag misses are now classified
as revision misses; keeping their column resident does not make its data
current. The default remains two slots and the runner restores that build.
The bounded power-of-two slot option (2–128) remains available for diagnostic
comparisons, alongside the existing disabled local-invalidation experiment.

## Notify caches only for successful flak mutations

`addRuntimeFlak` and `removeRuntimeFlakAt` previously notified world and
powerup caches before checking whether they could change the runtime list.
The notifications now occur after rejection checks, immediately before the
actual mutation. Successful operations retain their existing notifications,
including displaced lookup-slot handling. Rejected operations no longer
invalidate unchanged world data. This correction is enabled normally.

The focused A500 contract `a500-flak-mutation-contract` passes invalid input,
absent-cell removal, duplicate insertion, wrong-row removal, full capacity,
and successful insertion/removal checks. It verifies return values, world
revision, powerup cache validity, count and preserved/removed tile data. These
checks are also included in the full contract; optional
`HAR_HEADLESS_FLAK_MUTATION_TEST_ONLY=1` runs just this new block. This run is
a focused pass, not a new full graphics-suite pass.

A500 `flak_success_notify_full` preserves gameplay parity with the instrumented
`bob_cache_causes_full` baseline. Cache counts change from
`775,398,2,186,189,0` to `775,401,2,186,186,0`: three fewer rebuilds caused by
revision mismatch. Selected combat windows average 46.20 FPS (40–50), versus
46.15 before, which is not evidence of a meaningful pacing improvement. The
change is retained for avoiding demonstrated unnecessary invalidation, not
as a solution to the remaining scrolling stalls.

Stock A1200 `A1200_flak_success_notify_full` also preserves the A500 gameplay
parity. All 18 selected combat windows run at 50 FPS with maximum field gap 1.
The runner completed and rebuilt the ordinary executable and ADF.

## Splitting the former Wingman timing bucket

Stage 14 includes enemy missile updates as well as Wingman control, AI and
weapons; it is not solely formation AI. The profiler now has 39 stages, with
new child timings: 29 enemy missile, 30 formation/takeoff/P2 control, 31 AI
intercept/bombing/landing and visual Y, 32 Wingman rocket, 33 P2 bomb update.
These children are included in stage 14 and must not be added to parent
totals. All additions compile out of normal gameplay builds.

The first split run, `wingman_substages_full`, used 33 stages and included P2
bomb handling in stage 30. Its 128 consecutive workloads at scroll 3500
preserve gameplay parity against `flak_success_notify_full`. Stage 14 averages
14.78 lines, max 63. Of that, enemy missile averages 0.75, the combined P2
group 11.93 (max 60), AI 1.01 and Wingman rocket 1.09. At frame 1373/scroll
3512, the parent takes 61 lines and the P2 group 58. Similar peaks occur at
frames 1402, 1435 and 1464. This directs further investigation toward P2
control/bomb work rather than missile movement or formation AI. The subsequent
34-stage capture separates P2 bomb handling into stage 33.

`p2_bomb_substage_full` confirms the specific function: across the same 128
workloads, stage 30 (control) averages 2.84 lines, max 11, while stage 33
(`updateWingmanPlayer2Bomb`) averages 9.59, max 59. At frames 1373, 1402,
1435 and 1464, bomb work costs 56, 59, 57 and 59 lines respectively; control
costs only 3, 2, 2 and 2. The third peak still fits one field, demonstrating
that simultaneous work determines whether the expensive bomb update hitches.
Gameplay parity matches `flak_success_notify_full`; normal EXE/ADF were rebuilt.
This is diagnostic evidence, not a gameplay optimization or proof that every
bomb cost is drawing: the measured function also queries collisions, retires
the old footprint, mutates world state and starts impacts. Further work can
target that function without changing formation AI or enemy missile behavior.

The 39-stage `p2_bomb_hit_cost_full` capture further splits stage 33 into
34 collision queries, 35 old-footprint retirement, 36 world mutation/redraw,
and 37 impact/sound. Stage 38 is the hit object ID, NOT a duration. These
children must not be added again to their parent. Gameplay parity is unchanged.
The four large peaks at frames 1373, 1402, 1435 and 1464 are all object ID 3
(land). Queries cost 10–12 lines, footprint retirement 2–3, world mutation and
redraw 34–36, and impact/sound 9–10. This rules out formation AI and locates
the largest component in the land-hit mutation/redraw path.

The now-removed `HAR_DIRECT_CRATER_TILE=1` candidate called `redrawNewLandCrater`
after a successful crater insertion. It writes the known crater tile directly
only for solid rows in CPC procedural land without overriding persistent
smoke, then reapplies direct masked objects and the mirror. Other scenery,
smoke and forced-stage debug configurations use `bobCompositorErase` as before.
No persistent cache was added. It was kept off by default during performance
testing; the independent crater/full-column bitmap comparison was temporarily
routed through the helper, but no full graphics contract was run for it.

The first A500 `direct_crater_full` run preserves gameplay parity against
`flak_success_notify_full`. Its 20 selected combat windows average 46.25 FPS,
range 41–50, with maximum field gap 2. The previous ordinary baseline was
46.20; this difference does not establish a meaningful overall improvement.
The subsequent matched `direct_crater_stages_full` capture preserves gameplay
parity but does not improve pacing. At the four land-hit peaks, mutation/redraw
falls from 34–36 to 15–17 raster lines. Total measured work at frames 1373,
1402, 1435 and 1464 changes from 410/322/284/381 to 338/305/265/322 lines;
their one/two-field outcomes remain 2/2/1/2. Across all 128 consecutive
workloads, mean disjoint stage work is 247.22 versus 246.63 lines, while
two-field gaps increase from 8 to 10. This instrumented sample does not prove
a universal regression, but it gives no evidence of smoother scrolling to
justify the added path. Local work savings cannot be equated with improved
presentation timing. The candidate flag/helper and call-site changes were
removed, and the ordinary EXE/ADF rebuilt. Full graphics validation of this
rejected path is unnecessary; scheduling/raster-phase interactions remain
the next issue to investigate.

## Missile wait follows draw eligibility

`rocketPixelBobVisible` now shares the original active/crash and X/Y clipping
rules between friendly/enemy missile drawing and wait-target selection.
`rocketLastScreenYForUpdate` always includes valid old footprints, but adds
new positions only when that projectile is drawable. Friendly rockets are
suppressed during player crash; enemy missiles retain their original behavior.
The raster polling loop itself is unchanged. Previously it included any active
weapon's Y even if the following draw routine would reject that weapon.

The focused A500 `a500-rocket-wait-contract` passes empty/invalid-buffer cases,
left/right exclusion, partial visibility, the last legal bottom position,
vertical rejection, friendly crash suppression, enemy visibility during crash,
and protection of an old footprint after its new position is suppressed.
These checks are included in the full contract and can be selected alone with
`HAR_HEADLESS_ROCKET_WAIT_TEST_ONLY=1`. This is a wait-target contract, not a
new full graphics-suite pass or a visual tearing test.

A500 `visible_rocket_wait_full` preserves gameplay parity against
`flak_success_notify_full`, with identical BOB-stat instrumentation. Its 20
selected combat windows average 46.10 FPS (39–50), maximum field gap 2,
versus 46.20 (40–50) before. This is not evidence of improved combat pacing;
the change is retained to match wait eligibility to actual writes, including
crash and offscreen cases, rather than as a claimed FPS optimization.

Stock A1200 `A1200_visible_rocket_wait_full` matches the A500 gameplay parity;
all 18 selected combat windows run at 50 FPS with maximum field gap 1.
The runner completed and restored the ordinary executable and ADF.

## Adaptive offscreen stream budget (enabled)

`HAR_ADAPTIVE_STREAM_BUDGET=1` adjusts the existing stream row budget from
the beam phase and completed column lead. With a phase above line 170 and
lead at least `GAME_FETCH_BYTES + 6`, it defers that update's streaming.
Below line 120 with lead under `GAME_FETCH_BYTES + 12`, it allows four extra
rows to replenish the reserve. Otherwise the original speed-derived budget
applies. Fixed takeoff and the scripted landing slide retain their existing
paths. The original maximum lead remains the ring reuse limit, and fractional
row credit is unchanged; reserve recovery is driven by lead, not stored debt.
Beam phase is a scheduling heuristic, not an absolute elapsed-time deadline
if a workload has already crossed a field boundary.

BOB overwrite prediction includes the largest possible four-row boost rather
than sampling the earlier beam phase. Thus its predicted range conservatively
contains any actual streaming range even while the raster advances between
preflight and drawing. The policy is now enabled by default after validation.
Optional perf output `#adaptive_stream` gives minimum completed lead, deferred
updates, boosted updates and eligible samples, covering the whole test session.

A500 `adaptive_stream_full` preserves gameplay parity with
`visible_rocket_wait_full`, using identical BOB-stat instrumentation. The same
20 selected combat windows improve from 46.10 FPS (39–50) to 47.10 (43–50).
Maximum field gap remains 2, so steady 50 FPS has not been achieved.
Counters are `46,180,385,2431`; the measured minimum completed lead exceeds
the 42-byte fetch width. That sampled lead alone does not prove every visual
invariant. The full A500 contract passes with the adaptive flag
(`a500-adaptive-stream-contract`), including visible-ring comparison through
two periods in both modes, existing whole-bitmap mutation/BOB checks and game
rules. Machine/mode gameplay checks also pass as recorded below.

A500 Classic `adaptive_stream_classic_full` also preserves gameplay parity
against `mirrored_tiles_classic_full`. Its 19 selected combat windows average
48.47 FPS, range 46–50, maximum field gap 2. Adaptive counters are
`46,207,464,2431`. The earlier Classic reference averaged 47.58, but predates
the small flak-notification and missile-wait corrections, so that Classic
comparison is not an isolated adaptive-only A/B.

Stock A1200 `A1200_adaptive_stream_full` preserves Enhanced gameplay parity
and holds 50 FPS in all 18 selected combat windows, maximum field gap 1.
Counters are `53,4,4,2431`. In the 20 A500 Enhanced combat windows, logged
hitches fall from 142 to 107 while completed loops rise from 1858 to 1893.
These windows cover approximately the same region and hardware-field duration;
they are not exactly aligned logical-frame intervals. This is a measured
reduction in missed updates, not a claim of perfectly smooth 50 Hz gameplay
or completed continuous visual validation. Normal EXE/ADF are rebuilt with
the policy enabled; earlier experimental/rejected paths remain unchanged.

Interpolating hardware-field samples at scroll 1000 and 6500 gives about
39.42 seconds for the reference versus 38.70 for the adaptive trial over the
same distance. This is an estimate between logged samples, not an exact
per-frame timestamp, but supports the direction of the windowed FPS result.

## Real damage after adaptive streaming

A500 `adaptive_real_damage` preserves the earlier `real_damage_smoke_cache`
gameplay parity and exact damage record:
`22,352,100,0,520,559,624,36` (hits, first-hit frame, armour before/after,
failure, crash, terminal frame, smoke draws). In the three late windows at
scroll >= 1000, averages are now 50/46/49 FPS versus the earlier 48/40/49;
the minimum instantaneous rate remains 25 FPS and maximum field gap 2.
Adaptive counters are `48,39,36,479`. Window boundaries differ slightly, and
the old baseline predates several other corrections, so a same-code run with
only adaptive streaming disabled is used to isolate the scheduling effect.

The matched `adaptive_disabled_real_damage` run confirms the effect on the
current source: off gives 48/40/49 FPS and 2/10/1 hitches in the three late
windows; on gives 50/46/49 and 0/4/1. Both runs log these windows at hardware
fields 751/801/851, although logical progress within a window differs. Their
gameplay parity and exact damage records are identical. The instantaneous
minimum still reaches 25 FPS (two fields), so this reduces the frequency of
slow updates rather than eliminating them. The runner completed and restored
the normal adaptive-enabled executable (515728 bytes) and ADF (901120 bytes).

## Longer skill-5 A500 validation

`adaptive_skill5_full` completed with Enhanced mode, speed 15, wingman control
2, enemy rate 3, weapon stress, enemy-plane exercise and landing exercise on
the A500 profile (68000, 512 KB Chip + 512 KB Slow RAM). The normal executable
and ADF were restored afterwards. The gameplay aggregate records one landing
start and one landing completion; final-carrier summary fields are zero, so
this result is not evidence for those separate summary assertions.

The 21 speed-15 windows at scroll 1000–6500 average 45.05 FPS (37–49), with
197 hitches and maximum field gap 3. Across the longer scroll 1000–15000
region, 54 speed-15 windows average 43.06 FPS (32–49), with 719 hitches and
maximum field gap 3. These figures exclude startup/loading and landing.
Adaptive counters are `45,253,569,5163`. This is a wider workload validation,
not an isolated adaptive-on/off comparison and not continuous visual proof.
Unlike the earlier skill-1 case, this route still has three-field updates;
smooth 50 Hz A500 scrolling therefore remains unresolved.

## Skill-5 late-flight attribution

`skill5_late_stages` captures 128 consecutive workloads at logical frames
4203–4330, scroll 12002–12383. Gameplay parity matches `adaptive_skill5_full`.
60 updates take one field, 67 take two and one takes three. Mean raster-line
costs are: logic 128.17, stream 45.75, missile drawing 30.49, early BOB work
26.55, impact BOB 23.37 and missile wait 6.14. Within the overlapping logic
bucket, weapons cost 28.11 and collisions 35.26 (player object query 16.35).
Stream column building costs 13.30 averaged across all frames, peaking at 45.
The measurements implicate computation and restoration as well as drawing;
the missile wait alone does not explain the late-flight slowdown.

`HAR_CRATER_COLUMN_BOUND` is enabled by default after validation. A two-byte maximum of
stored crater columns rejects lookups ahead of all recorded craters before
the historical list scan. Column comparisons retain UWORD alias semantics;
reset and rescue restoration also maintain the bound. The original list,
mutation behavior and lookup within its extent remain unchanged.

`crater_bound_skill5` preserves the complete gameplay parity row. In the same
scroll 1000–15000/speed-15 selection, 52 windows average 45.21 FPS (38–49)
versus 54 windows at 43.06 (32–49); hitches fall from 719 to 468, but these
are different window counts rather than matched logical-frame intervals.
Maximum field gap remains 3. Interpolated travel time between scroll 1000
and 15000 improves from 107.73 to 102.60 seconds (sample interpolation, not
exact per-frame timestamps). The focused A500 contract passes
(`a500-crater-bound-contract`): independent scans of the original arrays
agree for empty, unordered/full and reset histories, rejected insertions,
UWORD aliases and row/column boundaries. The full A500 contract with the
trial enabled passes in `a500-crater-bound-full-contract`, including the
visible ring across two periods in both modes and the complete gameplay
contract (`fuelFrames=9558`, CPC-table collision and exact nine-direction
missiles). The policy is promoted. Stock A1200 `A1200_crater_bound_skill5`
preserves the complete A500 Enhanced gameplay parity row and holds 50 FPS
in all 47 speed-15 windows at scroll 1000–15000, with zero hitches and maximum
field gap 1. The runner restored the normal executable (515968 bytes) and
ADF (901120 bytes). A500 Classic `crater_bound_classic_skill5` completes the
long route and records one landing start/completion. Its 49 speed-15 windows
at scroll 1000–15000 average 47.73 FPS (37–50), with 199 hitches and maximum
field gap 3. No matched skill-5 Classic baseline exists here, so this is an
absolute pacing check, not an isolated speedup claim. Classic gameplay
counters differ from Enhanced as expected for the distinct game rules;
the full contract, rather than cross-mode row equality, checks those rules.
The runner again restored the normal 515968-byte executable and 901120-byte
ADF. Three-field updates remain on A500, and continuous visual assessment
is still outstanding.

## Follow-up: rejected crater mutations

`HAR_CRATER_SUCCESS_NOTIFY` is enabled after a separate experiment prepared
alongside the full bound-only contract. It moves world/cache notification
after invalid, duplicate and full-history rejection, just as the retained
flak path already does. Actual insertion still invalidates the affected cell
before updating history. A focused contract checks revision, background
validity and unchanged terrain on rejection, and invalidation on success.
The focused A500 run `a500-crater-success-contract` passes
`crater-bound-and-mutation-contract`. It confirms the independent history
scan equivalence and successful/invalid/duplicate/full-history notification
behavior. A500 Enhanced `crater_success_skill5` preserves the complete
`crater_bound_skill5` gameplay parity row. Over scroll 1000–15000 at speed 15,
51 windows average 45.61 FPS versus 52 windows at 45.21. Minimum window FPS
remains 38 and maximum field gap remains 3. Hitches are 428 versus 468, with
different window counts; interpolated travel time for the same distance is
101.83 versus 102.60 seconds. This is a small improvement (under one percent),
not a solution to the longest updates. It is retained as removal of
unnecessary invalidation on rejected mutations. Actual mutations still notify
before changing the history. The focused contract and full-route gameplay
check cover this change; the earlier full graphics contract was bound-only.
Fresh consecutive-frame attribution `crater_success_late_stages` preserves
gameplay parity. At the same 128 logical frames 4203–4330, one-field updates
increase from 60 to 69 and two-field updates fall from 67 to 58; one
three-field update remains. This compares both crater improvements against
`skill5_late_stages`, not success notification alone. Mean column-building
cost falls from 13.30 to 9.92 raster lines across all samples; impact BOB work
falls from 23.37 to 19.87. Large weapon-update spikes remain.

`weapon_parts_late_stages` splits that bucket without changing gameplay:
stage 39 is the player's rocket update, 40 the player's bomb update, and
41/42 are their hit object IDs (not durations). PERF_STAGE_COUNT is now 43;
these buckets overlap stage 26 and the parent logic bucket. In frames 4249
and 4263, bomb work takes 182/181 lines and hits object 8 (ground target).
Frame 4287 takes 291 lines in the rocket path, also hitting a ground target;
the concurrent bomb hits land. The complete gameplay parity row matches.
Extra instrumentation shifts hardware timing, so the changed field gap at
4287 is not a speedup result. The next attribution must split ground-target
damage/persistent-smoke updates from repaint and impact/sound. Inspection
confirms procedural target identity survives destruction, so destruction
does not itself disable the existing compact two-row repaint path.
The runner restored the normal executable (515844 bytes) and ADF (901120
bytes), with both crater options enabled and perf instrumentation excluded.

## Ground-target hit attribution and row batching trial

`target_hit_late_stages` preserves gameplay parity and separates the P1
ground-target path: stage 43 is destruction bookkeeping (including bomb lock
cleanup), 44 persistent smoke, 45 target repaint. Stage 46 is impact/sound
startup for all callers of `startGroundTargetHitImpact`, including land and
P2; it is not exclusive to the P1 target hit. PERF_STAGE_COUNT is now 47.
At frames 4249/4263, destruction costs 9/9 raster lines, smoke 44/44, repaint
103/104 and impact/sound 8/7. At 4287, these are 9/45/208/15; the impact
bucket also contains that frame's simultaneous land hit. Repaint dominates.

`HAR_DESTROYED_TARGET_ROWS` is enabled after validation. Within the existing
compact procedural-target path, it confirms destruction and conservatively
rejects horizontal overlap with any native carrier or promoted gunship.
Otherwise it obtains each current world-derived BOB erase column once and
draws both affected rows together, using paired primary/mirror writes when
available. It skips repeated direct-overlay scans for this proven empty
overlay region. The old path handles all rejected cases. Source inspection
confirms the direct row overlays are carrier, gunship and Enhanced target;
the destroyed target contributes no pixels.

`destroyed_target_rows_skill5` preserves the complete gameplay parity row
against `crater_success_skill5`. Both have 51 selected speed-15 windows at
scroll 1000–15000: mean FPS is effectively unchanged (45.59 versus 45.61),
minimum remains 38, and hitches are 426 versus 428. Maximum field gap drops
from 3 to 2. No speed-15 window anywhere in the trial log exceeds gap 2.
This suggests better worst-case pacing, not an average throughput gain;
matched detailed repaint timing is still needed to attribute the effect.

The full A500 contract passes as `a500-destroyed-target-rows-contract`.
The existing complete-bitmap comparison covers all target kinds, both tank
halves, both presentation modes and mirrored/nonmirrored placements. Added
checks ensure intact targets take the fallback without changing their pixels,
and the new repair branch actually executes in each mode. Full visible-ring
and gameplay checks also pass (`fuelFrames=9558`, exact nine-direction
missiles, CPC-table collision).

Matched detailed timing `destroyed_target_rows_late_stages` preserves gameplay
parity. Repaint at frames 4249/4263 drops from 103/104 to 57/57 raster lines;
at 4287 it drops from 208 to 101. Total rocket work at 4287 falls from 293
to 186 and its field gap from 3 to 2. Across the same 128 logical workloads,
one-field updates stay at 70; two-field updates become 58 instead of 57,
and three-field updates become zero instead of one. This supports the
worst-case pacing effect independently of the near-unchanged window average.
The policy is promoted. Stock A1200 `A1200_destroyed_target_rows_skill5`
preserves the A500 gameplay parity row and holds 50 FPS in all 47 selected
windows, with zero hitches and maximum field gap 1. The runner restored
the normal executable (517204 bytes) and ADF (901120 bytes). Full A500
graphics/gameplay validation and A1200 timing are complete for this change;
steady 50 Hz A500 gameplay and continuous visual review remain unresolved.

## Continuous visual review preparation

Frame-rate logs and bitmap contracts do not establish perceived motion
quality. A separate capture remains required. Upstream WinUAE exposes AVI
capture through its Output dialog (`IDC_AVIOUTPUT_ACTIVATED` invokes
`AVIOutput_Toggle` in
[win32gui.cpp](https://github.com/tonioni/WinUAE/blob/master/od-win32/win32gui.cpp)).
The recording implementation is
[avioutput.cpp](https://github.com/tonioni/WinUAE/blob/master/od-win32/avioutput.cpp).
This is preparation only: no continuous capture has been made or inspected,
and the installed debugger build's capture controls have not been verified.
Keep capture separate from the performance runs.

The visible A500 `visual_capture_skill5` attempt reached gameplay and the
installed WinUAE Output dialog, but no recording was started. Setting the
output filename through UI automation failed; the file chooser and settings
were cancelled. The 480-second host deadline expired after time spent paused
in these dialogs. This attempt supplies neither valid timing results nor
continuous visual evidence; use the completed `destroyed_target_rows_skill5`
run for timing. The runner entered its normal build-restoration path.

## Earlier crater-column mask validation

The crater-column mask change passed the full Classic
contract (`.tmp/a500-crater-mask-contract.log`). It scans crater history once
per generated column instead of once per solid row, with no persistent new
cache. That contract alone does not establish its speed benefit. It fits the cold column
generation path above, but is not a substitute for the active-map migration.

## Persistent hit-smoke attribution

The next profiling build extends the debug-only stage record to 50 stages.
Stages 47, 48 and 49 divide `addCpcHitSmokeAtColumnRow` into lower smoke
registration, the above-cell collision query, and conditional upper smoke
registration. These apply to every caller of this helper and overlap their
parent weapon/logic buckets; they are not three additional disjoint frame
costs. The original order (lower mutation before above-cell query) remains
unchanged. Normal builds exclude the timers. Compare matching logical frames
and gameplay parity; instrumentation may shift hardware-field phase.

`smoke_parts_late_stages` completed on the cycle-exact A500 profile with an
identical complete gameplay parity CSV to `destroyed_target_rows_late_stages`.
At logical frames 4249/4263/4287, stages 47/48/49 are respectively
3/26/15, 3/26/16 and 2/27/16 raster lines. Total target-smoke cost is
45/46/47 lines. Thus the above-cell query and upper registration dominate,
not the first insertion. The first insertion sets the column-presence filter
and invalidates the collision column; the subsequent above-cell query and
duplicate check both revisit the sparse history. A candidate is to resolve
the above cell before lower insertion, then avoid a redundant smoke lookup
only when its absence has been proven. This is not implemented or promoted:
it needs independent row-dependency, capacity, alias and priority validation
plus a matched timing comparison. The normal EXE (517204 bytes) and ADF
(901120 bytes) were restored after the successful profiling run.

The first bounded trial instead preserves that ordering entirely:
`HAR_SMOKE_KNOWN_ABSENT` (default off) lets only the upper insertion skip its
duplicate smoke lookup, because the immediately preceding collision query
returned SKY. Persistent smoke takes priority over sky in the resolver.
Bounds, capacity, notifications and insertion order remain shared with the
ordinary insertion path. This targets stage 49 alone, without moving a
collision query across a mutation.

Matched `smoke_known_absent_late_stages` preserves the entire gameplay
parity CSV against `smoke_parts_late_stages`. At frames 4249/4263/4287,
stage 49 falls from 15/16/16 to 3/3/3 raster lines; total target smoke
falls from 45/46/47 to 33/33/33. The above-cell query remains 26/26/27.
All three frames still take two fields: this is a local cost reduction,
not evidence of steady A500 50 Hz. The successful runner restored the
normal build with the trial disabled.

Across all 128 matching workloads both 50-stage builds have 66 single-field
and 62 two-field updates. Mean disjoint measured work (stages 0-10 plus
21-22) drops from 301.30 to 298.55 raster lines. Do not compare this field
distribution with the older 47-stage build to attribute the smoke change:
the extra instrumentation itself changes field phase. Long-route pacing
without those probes is still needed.

Full A500 contract validation is started in
`.tmp/a500-smoke-known-absent-contract.log`. In the contract build every
known-absent insertion additionally performs the reference history search
and records any disagreement. Tests require that the branch was exercised
and all such searches agreed, and check invalid coordinates and a truly
filled history list. The normal build excludes these reference searches.
Full-route timing without the detailed probes and A1200 validation remain
required before promotion.

The full A500 contract completed successfully for this trial:
`PASS fuelFrames=9558 cpcX=8..15 amigaX=96..186 scroll=1..3 engine=1..4
bombMomentumFrames=13 bombDescentFrames=3 maverick=exact-9dir
collision=cpc-table carrierTower=2x4`. This includes the reference-history
checks on known-absent insertions and the bounds/capacity cases added above,
as well as the full existing graphics/gameplay suite. The runner restored
the normal, trial-disabled EXE (517208 bytes) and ADF (901120 bytes).
The non-stage A500 `smoke_known_absent_skill5` timing run is the next gate.

That gate completed with identical gameplay parity, but no meaningful
frame-pacing improvement against `destroyed_target_rows_skill5`: both have
51 speed-15 windows at scroll 1000-15000, mean 45.5882 FPS, minimum 38 FPS
and maximum field gap 2. Hitches are 427 versus 426. Consequently the
known-absent special case, its flag and its trial-only tests were removed;
it is not promoted. The matching detailed probes remain useful attribution
tools. An A1200 run is unnecessary for this rejected implementation.

The next broader candidate is collision-cache row retention: currently
`worldObjectCellChanged` invalidates every cached row in a column even for
a single-row smoke, crater or flak mutation. Audit each caller and preserve
unaffected rows only where row independence is established. Keep other
render/sea/BOB invalidations intact, handle wrapped history column identities
and invalid row arguments conservatively, and verify retained hits against
the uncached resolver. This is a candidate, not yet an implementation.

## Bounded 68000 assembly trial

`HAR_ASM_BOMB_ERASE` defaults off. For aligned two-byte bomb footprints,
it restores twelve words with postincrement source reads and fixed plane
offsets. It retains the unused fifth storage plane and falls back to the
original C byte writes for single-byte or unaligned footprints. Compile-time
layout guards prevent use with a different height/plane arrangement; inline
assembly declares an early-clobbered source pointer plus condition-code and
memory clobbers. Placement, clipping and footprint lifecycle stay in C.

`HAR_HEADLESS_BOMB_ERASE_TEST_ONLY` runs the existing pixel-reference test
covering bomb phases, offsets, saved background and full-buffer restoration,
including clipped edge placements. This is a focused correctness gate, not
the full gameplay suite. Timing is still pending. Any gain must also be
compared with a C word-copy variant: switching byte accesses to aligned words
is not intrinsically an assembly-only optimization.

The focused A500 run `.tmp/a500-asm-bomb-erase-reference.log` completed:
`PASS bomb-draw-and-erase-reference`. The runner restored the normal EXE
(517204 bytes), with assembly disabled. The full-route A500 timing trial
`asm_bomb_erase_skill5` is now running; no speed benefit is established yet.

The route completed with identical gameplay parity. Against
`destroyed_target_rows_skill5`, both have 51 speed-15 windows over scroll
1000-15000. Mean FPS is 45.5098 versus 45.5882, minimum 38 in both, hitches
432 versus 426, and maximum field gap 2 in both. This does not justify
promotion. The assembly kernel and flag were removed; the focused existing
bomb-reference test entry point remains useful for future work. A C word-copy
comparison is not required to reject this particular assembly candidate,
and no claim of assembly outperforming C is made. This result applies only
to aligned bomb-background restoration, not to assembly in general.

## Collision rows retained across single-cell mutations

`HAR_OBJECT_CACHE_ROW_INVALIDATION` defaults off. `worldObjectCellChanged`
retains the collision column's known-row mask except the changed row, while
preserving all existing global, BOB, render, aircraft and sea notifications.
Whole-column mutations and resets retain their original full invalidation.
Invalid row arguments conservatively discard the full collision column to
avoid undefined shifts and stored UBYTE row aliasing. Low-bit indexing also
invalidates the corresponding UWORD column-history aliases.

Audit: smoke/town-smoke insertion, crater mutation and ship-cell destruction
change their addressed resolver row. Flak pruning/removal addresses its old
row, but insertion can additionally evict a different old row from the
128-slot lookup. The trial explicitly clears that old collision row before
overwriting the lookup slot. This matters because the 128-slot lookup's
colliding columns also share a 64-slot collision-cache entry.

The A500 `object_rows_verify_skill5` run enables both the trial and
`HAR_OBJECT_CELL_CACHE_VERIFY`, comparing every retained cache hit against
the uncached resolver throughout the long stress route. It is a correctness
probe, not a representative speed measurement. Full contract coverage of
retention, invalid rows, history aliases and displaced flak plus uninstrumented
pacing comparisons are required before promotion.

`object_rows_verify_skill5` completed with `#object_cache,15845,12596,0`
(verified hits, misses, incorrect hits). The complete gameplay parity CSV
matches `destroyed_target_rows_skill5`. This establishes no stale hits along
that stress route, not exhaustive coverage or a speed gain. The normal
517204-byte executable was restored. Non-verifying `object_rows_skill5`
is now running as the pacing comparison.

The pacing run completed with identical gameplay parity, but regressed
slightly: 51 matching windows, mean45.4902 vs45.5882 FPS, minimum37 vs38,
hitches430 vs426, maximum field gap2 in both. The row-retention implementation
and flag were removed rather than promoted. The normal EXE is517204 bytes.

Reassessment after repeated neutral micro-optimizations: the detailed
`smoke_parts_late_stages` sample has only4.01 mean raster lines in explicit
missile raster waiting (stage21, maximum60), compared with10.09 in missile
erase (stage22, maximum17). Eliminating that wait alone cannot explain the
whole throughput shortfall, and risks single-buffer artifacts. Broader
experiments should target recurring logic/BOB work and terrain streaming,
with matched instrumentation and actual visual verification. These elapsed
beam buckets include DMA contention and instrumentation, so do not equate
them with pure CPU instruction cost or infer all-frame worst-case coverage.

## Projectile rendering cost ceiling diagnostic

`HAR_DEBUG_OMIT_PROJECTILE_BOBS` is a default-zero diagnostic bit mask:
bit1 omits both player/wingman rockets and enemy missile drawing, plus their
late raster wait; bit2 omits player/wingman bomb drawing. It is compile-time
restricted to headless autoplay performance builds. Weapon updates,
collisions, persistent world edits and impact effects still execute.
Since no projectile footprints are created, their later erasure and ring
stream deferral also disappear. This measures the combined presentation
pipeline's possible headroom, not just inner-loop drawing instructions.

`projectile_cost_ceiling_skill5` runs with mask3. Compare complete gameplay
parity before interpreting timing. The diagnostic deliberately lacks visible
projectiles and is not a candidate release or evidence of visual quality;
it tests whether a substantial projectile renderer redesign could address
the measured pacing deficit. The runner restores the normal mask-zero build.

Mask3 completed with identical complete gameplay parity. For speed15 and
scroll1000-15000, mean FPS rises from45.5882 (51 windows) to48.7917
(48 windows), minimum38->47, hitches426->93, maximum field gap remains2.
This is substantial combined headroom, but even complete projectile
presentation omission does not produce steady50Hz. The normal executable
was restored before sequential mask1 (missiles only) and mask2 (bombs only)
diagnostics were started. Do not treat their gains as additive: saved
footprints, terrainstream deferral and PAL field phase interact.

Hardware sprite investigation must account for palette sharing. Channels4/5
share COLOR25-27 with attached aircraft pens9-11; channel7 shares COLOR29-31
with Wingman. Channel5 also hosts crash debris. Existing comments about
channel7/ejection conflict, so actual writes must be audited rather than
assuming a spare channel is universally usable. Recolouring aircraft to
make a projectile palette fit is not an acceptable shortcut.

Mask1 (omit rockets/enemy missiles, keep bombs) completed: 48 matching
speed15 windows, mean48.2708 FPS, minimum45, hitches141, maximum field gap2.
Mask2 is still running. This prioritizes the missile presentation pipeline.

Palette feasibility finding, not implemented: keep the existing Wingman
COLOR29/30/31=333/777/BBB. For attached sprites remap original pens
4->14,9->12,10->4,12->6, and leave all other pens unchanged. Set
COLOR20=old COLOR26 (485), COLOR28=old COLOR25 (263), COLOR25=000 and
COLOR26=FF0. A direct comparison across all16 pen values has zero RGB
mismatches with the effective existing palette (including Wingman overrides).
This exploits duplicate white at22/28 and duplicate grey at20/30. It leaves
normal sprite palettes17-19 (seat/parachute),21-23 and29-31 unchanged.

Channel5 crash fragments DO use25-27 and would be recoloured. A real trial
must relinquish projectile channels and restore25/26 during crash, then
restore projectile colours on return to ordinary play. Attached sprites no
longer address25/26 under this mapping, so that switch need not recolour
them. Also preserve layer priority against Wingman (sprite6) and retain a
BOB fallback for conflicts. Palette arithmetic alone does not validate DMA
timing, sprite attachment, transitions, silhouettes or priority.

Mask2 (omit bombs, keep missiles) completed: 50 matching windows,
mean47.14 FPS, minimum42, hitches264, maximum field gap2. Both mask1 and
mask2 complete gameplay parity CSVs exactly match the normal reference.
The sequential runner finished successfully and restored normal artifacts.
Missiles are the first offload candidate, with bomb rendering also material.

## Hardware projectile payload and palette reference gate

Implemented `projectileAttachedPenMap`, `projectileSpritePaletteWord` and
`buildProjectileHardwareSprite` as currently unconnected candidate helpers.
The payload stores black/yellow in normal sprite pens1/2, using the same
source transparency and monochrome enemy-missile rule as the BOB renderer.
It leaves the sprite's right8 pixels transparent and terminates DMA data.

The focused A500 `.tmp/a500-projectile-sprite-reference.log` passes:
`PASS projectile-sprite-pixels-and-palette`. An independent per-pixel tile
decoder checks all256 tile IDs, including invalid IDs, in both colour modes;
it checks all16 pixels per row, buffer guards, terminators and attach-bit
state. All16 original attached pen RGBs match in both projectile and crash
palette states; untouched normal palettes and restored crash colours are
also checked. Normal517204-byte EXE was restored.

This gate proves payload/palette conversion, not live sprite DMA, overlap
priority, clipping, palette-switch timing or gameplay integration. No active
renderer selects these helpers yet. Live integration needs a feature flag,
resident projectile payloads, explicit ownership of channels4/5, aircraft
palette remapping, crash palette handoff and BOB fallback for conflicts.

## Integrated player hardware rocket trial (default off)

`HAR_HARDWARE_PLAYER_ROCKET=1` now selects channel 4 for eligible player
rockets. It remaps attached artwork pens without changing their RGB values,
restores crash-piece palette entries during a crash, and falls back to the
existing BOB for clipped positions and conflicting Wingman/projectile bounds.
The late missile raster wait and old BOB restoration remain before handoff.
Payload data is rebuilt only when the tile or destination buffer changes.
The normal build keeps this feature disabled.

Focused A500 test `.tmp/a500-hardware-projectile-eligibility.log` passes
`projectile-sprite-pixels-and-palette`, including the eligibility cases.
This does not yet test the updater's complete live ownership transitions.

Full A500 route `hardware_player_rocket_skill5` completed successfully with
identical gameplay parity CSV to `destroyed_target_rows_skill5`. Over speed-15
windows at scroll 1000-15000, the trial has 50 windows, mean 46.90 FPS,
weakest window 42 FPS, 284 hitches, and maximum field gap 2. The reference
has 51 windows, mean 45.5882 FPS, weakest window 38 FPS, 426 hitches, and
maximum field gap 2. Window counts differ because route duration changes;
hitch totals are not measurements over exactly equal wall-clock durations.
Runner log: `.tmp/a500-hardware-player-rocket.log`. Normal 517204-byte EXE
was restored. The attempted additional selection-counter logging was removed
before restoration; this run does not report selection counts.

This is a promising rendering-offload result, not proof of steady 50 FPS
or live visual correctness. Before promotion, exercise actual handoffs,
run the full graphics contract and A1200 route, and visually check sprite
DMA, priority, clipping, crash palette transitions and continuous scrolling.
The rejected bomb-erase assembly trial above remains evidence against that
specific kernel, not against assembly in other measured hot paths.

## Vertical reuse of projectile sprite channel 4

`HAR_HARDWARE_PROJECTILE_CHAIN=1`, together with
`HAR_HARDWARE_PLAYER_ROCKET=1`, enables an experimental DMA display list
for player rocket, Wingman rocket and enemy missile. Both flags remain off
by default. Three eight-row objects share one channel when vertically
separated. The next object's control words replace the preceding object's
two terminator words; only the last object has a zero terminator. This uses
normal sprite DMA rather than a CPU interrupt/Copper rewrite per object.

The [Commodore hardware manual, Reusing Sprite DMA Channels](https://www.amigarealm.com/computing/knowledge/hardref/ch4.htm)
requires a blank scanline for fetching the next control words. This trial
conservatively leaves two blank lines (start-to-start distance at least 10
for eight-row images). Same-height objects cannot share this channel merely
because their X coordinates differ. Attached 16-colour sprites consume two
channels; palette and priority restrictions still apply. Sprite DMA saves
CPU/blitter drawing work but still consumes Chip-memory bus slots.

The selector keeps BOB fallback for clipping, crash/eject/game-over,
Wingman overlap and projectile overlap. It gives the player rocket first
choice and sorts accepted objects vertically. It is a bounded three-object
allocator, not an optimal general-purpose sprite scheduler. The final old
sprite's Y participates in the existing late raster wait before modifying
the list. Old BOB backgrounds are restored before handoff. Channel 4's
allocation grows from 40 to 112 bytes, an extra 72 bytes of Chip RAM.
Payloads are cached by tile and monochrome mode per list slot; movement
normally updates the control words only.

Focused A500 `.tmp/a500-projectile-chain-contract.log` passes the existing
independent pixel/palette gate plus list ordering, the 9/10-line boundary,
reordering, shrinking, regrowing, crash removal, changed enemy direction,
terminators, buffer guards and final-Y tracking. List payload comparisons
reuse the separately pixel-tested sprite builder. This is not a full
graphics contract or evidence of correct live DMA presentation.

Full A500 `projectile_chain_skill5` completes with identical gameplay parity
to `destroyed_target_rows_skill5`. Matching speed-15 scroll 1000-15000
windows: 50 windows, mean 47.22 FPS, weakest window 43 FPS, 250 hitches,
maximum field gap 2. The single-player-sprite trial was 46.90 / 42 / 284;
the normal reference was 45.5882 / 38 / 426. These are descriptive route
results, not a statistically established difference from repeated trials.

`#projectile_chain,1073,3863,836,77` records updates selecting 0, 1, 2 and
3 objects, respectively, across the full session (not just the timing
interval). Thus vertical channel reuse is exercised in the real route,
not only in the synthetic test. Selection counts do not prove visible
output. A500 runner log: `.tmp/a500-projectile-chain-route.log`.

Visual DMA/priority verification and the full integrated graphics contract
remain promotion requirements. The headless emulator has no targetable
window in computer-use, so no visual correctness claim is made from this
run. Normal builds retain the BOB implementation.

A1200 `A1200_projectile_chain_skill5` also completed: 47 matching windows,
all 50 FPS, zero hitches, maximum field gap 1, identical gameplay parity to
the A500 reference. The selection histogram is identical to A500. Log:
`.tmp/a1200-projectile-chain-route.log`. Both runners restored the normal
517204-byte EXE and 901120-byte ADF. No release default was changed.

The full A500 contract with both projectile flags enabled subsequently
passed: `.tmp/a500-projectile-chain-full-contract.log`, result beginning
`PASS fuelFrames=9558 ... maverick=exact-9dir collision=cpc-table`.
The runner exited 0 and restored the normal 517204-byte EXE. Together with
the focused chain test this covers the existing software graphics/gameplay
references, but still does not observe actual sprite DMA output.

Crash-palette review: channel-5 debris and projectile colours share entries
25/26. Copper palette operands and sprite payloads are updated at different
points in the game loop. A proposed extra raster wait before publishing
debris was removed on review: waiting alone cannot establish an atomic
handoff across a field boundary, and dormant sprite controls are latched by
DMA rather than continuously reread from memory. No runtime wait was
retained. Verify the actual transition before promoting the feature; do
not treat the hypothesised one-field colour mismatch as an observed bug.

Matched A500 skill-5 damage routes `projectile_damage_baseline` and
`projectile_damage_chain` both completed and restored normal artifacts.
Their complete gameplay parity CSVs match exactly. Both report
`#damage_exercise,13,352,100,0,448,491,556,40`: identical injected damage,
armour exhaustion, failure/crash/terminal logical frames and smoke draw count.
The chain route selects 0/1/2/3 objects in 180/151/112/17 updates. Both have
maximum post-startup field gap 2. Different route-window counts and phases
make individual window comparisons inappropriate. This verifies the logical
crash path, not its on-screen palette timing.

User steering: crash debris may be BOBs; prioritize normal gameplay over
retaining dedicated debris sprites. Investigate moving the three small
crash tiles to the existing masked playfield compositor with exact movement,
clipping and background restoration. In particular, removing channel-5
debris removes its need to restore projectile palette entries 25/26.
This is the next implementation direction, not an implemented change yet.

## Crash debris BOB implementation (experimental)

`HAR_CRASH_DEBRIS_BOBS=1` now renders all three crash parts as masked
playfield BOBs and suppresses their hardware sprites. It retains the old
eight-pixel artwork at x+4, all 16 source rows used by the former sprite
builder, and unchanged crash state/movement. Colour classes now use fixed
dark/mid/light grey playfield pens 4/3/2 instead of channel-specific sprite
palettes. With hardware projectiles enabled, Copper starts and stays with
the projectile palette; crash no longer restores entries 25/26.

Each part saves the underlying four display planes for up to two ring
placements; the fifth storage plane is untouched. Parts are restored in
reverse order before terrain/object changes and other BOB erases. They are
drawn after the other effects. Pause retains the displayed footprint, and
session initialization clears footprint state. Shapes and plane masks are
cached once per immutable source tile rather than decoded per frame.

`WaitVbl()` returns on PAL line 311. A newly drawn, post-raster BOB must be
allowed to appear in the following field before it is erased. The early
debris eraser therefore waits out the end-of-field region and then waits
for the displayed debris rows. The first trial omitted the explicit wrap
guard; its timing does not establish valid visible output. Continuous visual
validation of the final scheduling, pause/resume and clipping remains needed.

Focused A500 tests `.tmp/a500-crash-bob-reference.log` and
`.tmp/a500-crash-bob-cached-reference.log` both pass
`crash-bob-pixels-and-restoration`. They compare with decoded original
sprite silhouettes across pixel alignments, mirrored ring positions,
screen clipping and all three overlapping parts, then check complete
background restoration including untouched storage. These are software
pixel tests, not a physical display or sprite-DMA capture.

The integrated A500 skill-5 damage test
`projectile_damage_crash_bobs_cached` completes with identical full gameplay
parity to `projectile_damage_baseline`, and identical damage statistics
`13,352,100,0,448,491,556,40`. The final cached/wrap-corrected trial has
post-startup windows 46,46,46,41,29 updates/s and maximum field gap 3.
The earlier unoptimized BOB trial reached a weakest window of 18. Window
boundaries differ; these are indicative route results, not matched per-frame
speedups. Crash rendering remains slower than the previous sprite debris;
this is an explicit tradeoff favouring projectile channel/palette ownership.

Log: `.tmp/a500-projectile-damage-crash-bobs-cached.log`. The runner exited
0 and restored the normal 517204-byte EXE. The new flag remains off by
default. Full normal-flight timing with all three flags, A1200 coverage,
the complete integrated graphics contract, and visual checks remain before
promotion. The full chain contract recorded above predates debris BOBs.

## Combined normal-flight and A1200 checks

A500 `chain_crash_bobs_skill5` completes the normal stress/landing route
with all three experimental flags enabled. Over speed-15 scroll 1000-15000:
50 windows, mean 47.24 FPS, weakest window 44, 253 hitches, maximum field
gap 2. Full gameplay parity matches `destroyed_target_rows_skill5` exactly.
The chain-only trial was 47.22 / 43 / 250; this tiny difference is not
evidence of a further speedup. It supports retaining the existing flight
gain when the debris BOB option is included.

A1200 `A1200_chain_crash_bobs_damage` passes the real-damage driver with
identical complete gameplay parity to `projectile_damage_baseline` and
identical damage transition statistics. All four completed post-startup
windows report 50 FPS, zero hitches, maximum field gap 1. This covers the
damage/crash route, not all A1200 game situations. The sequential runner
finished with exit 0 and restored normal artifacts. Logs:
`.tmp/a500-chain-crash-bobs-skill5.log` and
`.tmp/a1200-chain-crash-bobs-damage.log`.

Added `tools/winuae-ipc.ps1` for WinUAE's native named-pipe API, based on
[upstream uaeipc.cpp](https://github.com/tonioni/WinUAE/blob/master/uaeipc.cpp).
It uses a bounded UTF-8 request/reply with disposal on failure. A separate
menu run successfully returned `200` and `68000` for `CFG cpu_model`.
Earlier connection timeouts occurred around short test-process shutdowns;
they do not establish that IPC is unavailable. Sandbox access requires
the same escalation used for emulator runs.

The installed executable contains AKS_VIDEORECORDFILE and AKS_VIDEORECORD.
A separate menu experiment sent filename/start/stop events but returned
`404` with no verified file under `.tmp/capture`. Event replies alone do
not prove recording success (the upstream handler can return no text even
for applied events). No video verification has been achieved. Inspect the
installed recorder's path/codec setup before relying on this capture route;
do not mix recording runs with benchmark measurements.

### A500 user feedback: falling bombs flicker (2026-09-08)

The user tested the manual sprite-chain + crash-BOB build on A500 and
reported promising speed, possibly excessive gameplay pace, but flickering
falling bombs. Bombs were erased before streaming and redrawn after world
overlays, leaving a visible empty interval in the single-buffered playfield.
Added `waitUntilBombRowsPassed` before that erase group, covering both
players' retained and new bomb rows and the line-311 field boundary. This
preserves world-overlay ordering and bomb motion; it does not change pace.
It is a candidate visual fix, not yet visually confirmed by the user.

A500 `bomb_row_wait_skill5` completed with exact whole-file gameplay parity
against `chain_crash_bobs_skill5`. For speed 15 and scroll 1000..15000,
50 windows: mean window FPS 46.76 versus 47.24, weakest window 42 versus 44,
304 hitches versus 253, maximum field gap 2 for both. This is a small but
measurable performance cost, not an optimization gain. Source log:
`.tmp/a500-bomb-row-wait-skill5.log`. No new A1200 visual claim is made.
Manual build retains all three experimental feature flags enabled; a normal
default build still leaves them disabled. Await visual comparison before
accepting the timing tradeoff or changing gameplay cadence.

### Enhanced power-up fall and drift (2026-09-08)

Requested slower, varied parachute drops. Enhanced now selects 6..9 eighths
of a pixel per update (previously 8/5 pixels) and left/right drift of one
pixel per eight updates. A seed/position/type hash chooses the motion once
at spawn without consuming the terrain/enemy random stream. Collision Y
tracks visible Y. Classic keeps its original five-update character-row fall.

The power-up compositor now shifts its two source tiles across two or three
columns for exact pixel X. Shifted rows are cached by type and phase, and
background invalidation covers the third column. No floating-point motion.
Focused emulator contract `PASS powerup-drift-and-pixels` checks both drift
directions and all fall rates over 80 updates, each shifted source/mask bit,
and background transitions across tile/ring boundaries and clipped heights.
Log: `.tmp/powerup-drift-contract-v3.log`. Earlier focused attempts lacked
runtime-route fixture initialization; the final test initializes that route.
Visual feel and resulting pickup availability still require playtesting;
the new gameplay intentionally changes Enhanced power-up timing.

### Enemy planes follow the scenery (2026-09-10)

Chris Perver's playtest clarified that intact enemy planes in the Amstrad
game have no horizontal motion independent of the scenery. The Amiga port
had subtracted another tile from targetWorldX at each logical update and
interpolated worldX toward it, adding roughly one pixel per frame to their
closing speed in the default profile. Removed that extra movement in both
Classic and Enhanced. Screen X remains worldX minus scrollX; vertical motion,
spawn admission and the firing-distance threshold remain in place.

Retreat now uses an independent alternating logical-tick phase for the
existing half-rate climb. The old world-column parity test would leave a
plane at an even fixed world column unable to climb. Damage/broken-aircraft
animation remains separate from intact flight. No global pace adjustment.

Focused A500 contract: `PASS enemy-scenery-motion-and-retreat`, logged in
`.tmp/enemy-scenery-contract.log`. It covers both modes, camera steps 0..3,
even/odd anchors, descent, entry into firing range with an occupied missile
slot, and completed retreat with a stationary camera. Earlier parity logs
encode the old extra horizontal movement and are not an authoritative
reference for this intentional correction. Manual visual testing remains
necessary to assess the new approach timing.

Full A500 heavy route `enemy_scenery_skill5` completed with exit 0. At speed
15, scroll 1000..15000: 49 windows, mean window FPS 47.33, weakest 43,
242 hitches, maximum field gap 2. Log: `.tmp/a500-enemy-scenery-skill5.log`.
This does not show an obvious performance regression, but changed combat
timing and the newer power-up motion prevent a controlled speedup claim
against the earlier bomb-row-wait run. Manual build uses the same three
sprite-chain/crash-BOB flags as Public Beta 2.


### Independent menu tempo (2026-09-11)

The menu now separates Skill (1 Easiest through 5 Hardest) from Tempo
(80%, 90%, 100%; default 100%). The extra row fits within the existing PAL
menu at 14-pixel row spacing. Gameplay rules and collision coordinates
remain integer simulation state in both Classic and Enhanced.

Reduced tempo samples CIA-A's field clock after the existing WaitVbl raster
synchronization. An integer accumulator schedules logical updates; short
input edges from either player survive fields without an update. The
renderer interpolates poses between logical updates and restores every
changed coordinate before telemetry or the next update. Pause freezes the
presentation phase and clears pending gameplay input. Mission rebases snap.
Audio and menu handling retain display cadence. Overload debt is bounded,
so the code never enters an unbounded catch-up loop or appends a fixed delay.

On a stock 68000 the first general 32-bit interpolation implementation was
unacceptable (28.80 mean window FPS in the heavy A500 route). Native word
arithmetic improved this to 35.69, still too slow. The current implementation
uses a 3300-byte integer lookup table initialized once, records only the
previous pose, and saves/restores only coordinates that actually change.
Profiling stage 50 records logic before presentation; stage 1 now isolates
the tempo presentation setup. Stage timing requires the existing hitch/all
frame capture flags when deferred performance logging is enabled.

The 100% baseline retained the previous complete parity CSV and measured
47.14 mean window FPS, weakest window 43, maximum gap 2 fields, at speed 15
and scroll 1000..15000. Evidence: `.tmp/tempo-isolated-A500-100/`.
The A500 configuration is cycle-exact PAL, 512 KiB Chip plus 512 KiB Slow,
without Fast RAM. Hardware projectile chain and crash-debris BOB flags match
Public Beta 2. This workload includes Player 2 and continuous weapon stress.

Focused contract: `HAR_HEADLESS_TEMPO_TEST_ONLY=1`, invoked through
`run-amiga-classic-contract.ps1 -ExtraCcFlags`. It verifies accumulator
rates, bounded debt, short input edges, every signed interpolation delta
-32..32 and alpha 0..99, pose restoration, teleports and menu row bounds.
Manual visual testing remains necessary for perceived smoothness and feel.

Final A500 90% heavy route (including landing and transition to mission 2):
56 windows at speed 15 / scroll 1000..15000, mean window FPS 43.48, weakest
33, maximum field gap 2. Evidence: `.tmp/tempo-isolated-A500-90-input/`.
Across active gameplay it executed 5810 steps in 6881 observed PAL fields
(42.22 steps/s versus the ideal 45). CPU overload still loses bounded work
credit; selected tempo is a target, not a claim of exact pacing under load.
Combat events differ from 100% because the automated pilot/fire input runs
at display cadence, so these whole-route numbers do not isolate rendering
cost. Radar cadence now follows logical steps; audio and purely cosmetic
sea-wave animation remain on the display clock. Input copying occurs only
when a deferred press actually needs overriding.

Final A500 80% short smoke (1200 outer-loop frames, not the full route):
6 cruise windows after scroll 1000, mean window FPS 45.83, weakest 37,
maximum gap 2. It executed 855 logical steps in 1090 active PAL fields
(39.22 steps/s versus ideal 40), with pause/resume exercised.
Evidence: `.tmp/tempo-isolated-A500-80-input-short/`.

Final stock A1200 90% short smoke (1200 outer-loop frames): 6 cruise
windows after scroll 1000, all 50 FPS, maximum field gap 1. It executed
930 steps in 1034 active PAL fields (44.97 steps/s versus ideal 45),
including pause/resume. Configuration: 68020, 24-bit addressing, real
speed, AGA, 2 MiB Chip, no Fast/Slow RAM, no JIT, A1200 Kickstart 3.0.
Evidence: `.tmp/tempo-isolated-A1200-90-input-short/`. This is a short
machine smoke, not proof of 50 FPS throughout the entire campaign.

The expanded final focused contract passed in `.tmp/tempo-final-contract.log`.
It includes simultaneous active weapons/crash pieces, exact whole-state
restoration, enemy/world alignment, and a press that begins and ends on a
non-simulation field. Normal input must avoid the temporary-override path.
The interactive build restores Tempo 100% and excludes all headless/perf
flags; the three Public Beta 2 projectile/debris flags remain enabled.


## 2026-09-12: Skill/Tempo scoring and mode-specific records

Awards now apply the starting Skill factor (1.00..1.20) and selected Tempo
factor (1.00/1.05/1.10). Fractional points carry across awards and mission
transitions. Arithmetic runs only when awarding points, not each frame.
The unscaled mission tally retains the extra-aircraft threshold. Landing
awards 2000 base points (CPC internal units converted to displayed points).

Classic and Enhanced use independent version-2, checksummed A/B files with
Skill/Tempo metadata and distinct mode signatures. Prior records remain a
read-only Legacy archive; fixture tests verify their bytes are unchanged.
The HUD follows the active game mode, independently of the archive view.

Focused score contract passed in `.tmp/score-rules-contract-capture-v5.log`:
all 15 multipliers, fractional accumulation, saturation, actual filesystem
round trips, mode isolation, metadata, legacy decoding and corrupted newest
slot fallback. Tests use a newly created scratch directory, not player saves.
The full Classic gameplay contract passed in
`.tmp/score-classic-full-contract.log` (fuel, movement, weapons and collision).
Its emulation was accelerated during the run; this is functional evidence,
not a new stock-machine FPS benchmark. Earlier tempo measurements above
remain the performance evidence; scoring changes have not been benchmarked.

Both Current and Legacy menu layouts were inspected together at native
proportions in `.tmp/score-menu-combined.png`; all labels, metadata and
multiplier text fit. Apparent missing glyphs in individual tool previews
were not missing from the underlying image pixels.


## 2026-09-12: Editable weapons and Missile Tank

Enhanced now reads 13 8x8 projectile masters and two 4x3 bomb masters from a
550-byte masked bank. Original black/yellow projectiles retain hardware
multiplexing; other palette indices select four-plane BOB drawing from the
same source. Sprite and shifted-shape caches include presentation mode.
Unedited bombs retain the original specialised plot; changed bombs use the
new colour-preserving masked path. No collision footprints were enlarged.
The editor adds a scrollbar and successfully loads/previews all 28 masters.

The separate 16x8 Missile Tank master starts as an exact Tank copy. Its
selection bitset is generated after CPC target placement, without consuming
the CPC random stream: a 15..20-tank countdown with minimum 42-column spacing.
Enhanced consumes an exit crossing once; destroyed tanks, active enemy
planes and occupied missile slots suppress the shot. Aircraft admission is
blocked while the truck shot is active. The shared missile slot retains the
existing weapon/aircraft collisions. Rise uses one screen pixel horizontally
and vertically per two logical steps; at the lower aircraft's height it
locks altitude and accelerates to 7 world pixels/step. Classic is unchanged.

Validation:
- `.tmp/editable-weapons-final-bomb-contract.log`: PASS all palette colours,
  all eight shifts, end-of-buffer clipping, saved background bytes, untouched
  fifth plane, bank-to-hardware/BOB equivalence and both bomb phases.
- `.tmp/missile-tank-contract-v4.log`: PASS rarity/minimum spacing, Classic
  exclusion, destroyed rear/front identity, aircraft/missile exclusion,
  one-shot exit, lowest-aircraft selection, rise and fixed-height cruise.
- Python isolated-master round trips: PASS edited indices retained exactly,
  correct 550-byte bank, hardware-incompatible colours detected, Tank copy
  independent. Tk loads and previews all 28 entries without saving them.
- A500 full default autoplay weapon stress completed in
  `.tmp/tempo-isolated-A500-100-editable-weapons-short/` (despite the directory
  name, the default frame limit was used). 53 cruise windows: mean 44.55 FPS,
  weakest 33, max field gap 3. This run preceded restoring the original bomb
  fast path; it is completion evidence, not a claim of locked 50 FPS or a
  controlled before/after comparison. New enemy behaviour changes workload.


Missile Tank frequency adjustment after playtesting: the initial 15..20-tank
wait restarted in every mission and made the enemy too rare. The first now
appears after 4..6 tank encounters, then every 8..12. The 42-column minimum
spacing and all firing/exclusion rules remain in effect; spacing may postpone
an otherwise eligible tank. Placement remains independent of the CPC RNG.

The updated focused emulator contract passed in
`.tmp/missile-tank-frequency-contract.log`, including minimum spacing,
exclusion, destruction and flight checks.


Field Guide update: Enhanced lists Missile Tank with its current editable
16x8 master enlarged 2x, base points and FIRES ON EXIT. Classic omits the row.
The Enhanced ticker explains the launch and aircraft-exclusion rules. The
six enemy rows and five powerups fit in 320x256; both modes were visually
checked in `.tmp/field-guide-combined.png`. The updated user weapon art
passed `.tmp/new-graphics-guide-contract.log`, including both hardware-
compatible and full-colour BOB paths and the edited bomb phases.

The new-art A500 smoke completed at the intended 1200-frame limit:
`.tmp/new-art-guide-a500-smoke.log`. This checks startup/gameplay with the
new ticker allocation and weapon colours, not full-campaign performance.


## Public Beta 3 RC1 release gate

Version `v0.9.0-beta.3-rc.1`, build label `BETA 3 RC1`.
Latest saved PNG masters validated and runtime banks matched. Focused weapon
contract passed: `.tmp/beta3rc1-weapon-contract.log`. Short 1200-frame machine
smokes passed for A500 at 100% and stock A1200 at 90%:
`.tmp/beta3rc1-a500-smoke.log` and `.tmp/beta3rc1-a1200-smoke.log`.
The A1200 executed 930 logical steps over 1034 active PAL fields. These are
short release smoke tests, not full-campaign performance guarantees.

Normal interactive build passed in `.tmp/beta3rc1-release-build.log`, with
only the three Public Beta projectile/debris flags enabled. Release source
and graphics hashes were checked against the tested inputs before packaging.
The package uses `package-amiga.ps1 -Version 0.9.0-beta.3-rc.1 -NoBuild` and
contains ADF, HD ZIP and SHA-256 checksums; no ROMs or debug dumps.
