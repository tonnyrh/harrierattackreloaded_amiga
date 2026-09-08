# Enhanced graphics scrolling audit — 2026-09-06

## Incremental wave window — 2026-09-08

### Landing stall diagnosis and memory fill

`HAR_DEBUG_PERF_HITCH_MIN_SCROLL` limits detailed records to a selected route
section. Stage count is now 21: stages 19/20 are raw CIA-A field deltas inside
engine update/HUD drawing, rather than scanline durations. This distinguishes
long calls that alias in the relative beam profiler.
`tod_landing_field_stages` proves the five-field gap at logical frame 2703,
scroll 7208, includes **four fields inside drawHudValues** and zero inside
engine update. The proposed post-landing engine guard was redundant (the
existing outer branch already excludes completed missions); its control run
`tod_landed_engine_quiet` was unchanged and the guard was removed.

The support `memset` now uses even-aligned 32-bit stores for larger spans,
with byte handling for short spans, odd leading addresses and tails. A
may-alias word type preserves its ability to write any object representation.
Tests cover offsets 0–3, lengths 0–65 and 65537, truncated fill values, guard
bytes and return values. Contract passes; A500 `tod_wide_memset_landing`
reduces bootstrap cost slightly but retains the five-field landing stall.

HUD status panels now draw only their visible border strips on the already
cleared band, instead of overpainting three full rectangles. Full-buffer
comparison against the independent pixel reference passes for both red and
green panels, including surrounding pixels (`a500-hud-border-contract`).

The first border-strip version (`tod_hud_border_landing`) reduces A500's
stationary landing gap from five fields to two. The final side-border version
writes each shared edge byte directly after the band clear; its independent
full-buffer reference also passes (`a500-hud-side-contract`). The full A500
`tod_hud_side_landing` still has one two-field landing gap, with flight windows
at 46–50 updates/s and subsequent slide windows at 50. Final gameplay parity
matches the previous full-route runs. This does not establish fully smooth
A500 flight or remove the last landing miss.

Stock A1200 `A1200_tod_hud_side_landing` completes all logged flight,
landing and slide windows at 50 updates/s, including the previous stationary
landing miss. Its final gameplay record matches A500 exactly. Both runners
restore the normal EXE and ADF successfully. These results cover the skill-1
autoplay route, not ordinary damage, all skills or a visual motion review.

The support `memcpy` now copies larger equally aligned spans using even
32-bit accesses. Opposite-parity pointers stay bytewise; one leading byte
aligns two odd pointers. Independent checks exercise every source/destination
offset 0–3 with lengths 0–65, plus seven 65537-byte cases covering every
destination alignment and both parity paths. They verify return values,
unchanged sources and destination guard bytes. All pixel/gameplay contracts
pass (`a500-wide-memcpy-contract-bounded`). The first 16-long-copy sweep
exceeded the 120-second runner limit; repeated equivalent long cases were
removed while keeping the full short-copy sweep. The runner now defaults
to 120 seconds for the expanded reference suite. Flight timing is evaluated
separately before accepting this as a scrolling improvement.

A500 `tod_wide_memcpy_landing` matches the previous final gameplay record.
Logged moving windows (scroll >96 and <7208) improve modestly from 46–50
to 47–50 updates/s, with 41 hitch records versus 43. These hardware-clock
windows shift slightly between builds, so this is a small aggregate gain,
not proof that every corresponding frame is faster. The stationary landing
gap remains two fields; subsequent slide windows remain 50 Hz.

Stock A1200 `A1200_tod_wide_memcpy_landing` preserves 50 Hz without hitches
in all 30 completed windows after the two startup windows. Final gameplay
matches A500 exactly; normal EXE/ADF restoration succeeds on both machines.

### Remaining stress limitation

Persistent hit smoke now has a 32-byte conservative column-membership filter.
Successful inserts set a bit and reset clears all bits; exact list lookups
still resolve possible hits. Hash collisions can only cause extra scans.
Both per-cell lookup and bulk render-column composition skip the sparse
list when the queried column cannot contain smoke. Independent raw-list
tests cover filled capacity, modulo collisions, coordinate boundaries and
reset, alongside the full pixel contracts (`a500-smoke-filter-contract`, PASS).

A500 `tod_smoke_filter_landing` preserves the prior full combat/landing
gameplay record. The comparable combat interval (scroll 1000–6500, speed 15)
improves from 26–40 to 28–40 updates/s. Landing is reached earlier; completed
windows end near hardware second 89 versus 91 previously, but partial final
windows are not flushed, so this is not an exact duration saving. Four-field
gaps still occur in flight; the landing window retains a two-field gap and
subsequent slides hold 50 Hz. The wider A500 deficit remains unresolved.
Stock A1200 `A1200_tod_smoke_filter_landing` retains 49–50 Hz in all 30
completed post-startup windows and the same six two-field hitches. Its final
gameplay matches A500. Both runners restore normal EXE/ADF. The retained
filter uses 32 bytes of additional state; the measured benefit is on A500.

A500 `tod_town_transition_stages` collects 128 hitch records from scroll
5200 onward in the complete combat/landing exercise. Mean raster costs are
166.1 for game logic, 101.2 for sprite/ambience preparation, 94.9 for streaming,
31.9 for missile drawing and 33.9 for impacts. Column construction alone
averages 64.2 (maximum 101), versus 23.1 for tile copying. Player collision
work averages 51.3; its object-map portion averages 20. These instrumented
samples localize the late-route cost, not an uninstrumented FPS estimate.

`HAR_HEADLESS_GAME_MODE` defaults to 1 (Enhanced): the recent TOD runs cover
the Enhanced graphics path. The first full A500 skill-1, two-player, enemy-3x
weapon/landing combination (`tod_enhanced_full_stress`) reaches the end of
the map but does not land: final scroll 7400, speed 15, no landing state or
completed mission. Moving full-speed windows span 26–43 updates/s, with
four-field gaps late in the route. The final stationary windows at the map
limit must not be presented as scrolling performance.

The stress pilot fires over friendly carriers, while the normal game refuses
automatic landing approach after a friendly ship has been hit. The landing
exercise now suppresses stress fire in the opening two screens and from one
screen before final approach, allowing existing projectiles to clear. This
changes only the combined debug landing/stress pilot; ordinary short stress
probes and release gameplay remain unchanged. The first run is useful flight
coverage but is not a passed landing test.

With that debug-pilot correction, A500 `tod_enhanced_combat_landing`
completes landing, slides and next-mission preparation (mission 2, one
landing start and completion). Its moving full-speed windows between scroll
640 and 6900 span 26–47 Hz, with gaps up to four fields. The first/last such
windows may straddle the ceasefire boundaries; this is a full-route result,
not a replacement baseline for the unchanged short combat probe. The
stationary landing window has one two-field gap and later slide windows
hold 50 Hz. This verifies the extended route but confirms a significant
A500 combat deficit late in the map.

For the less boundary-sensitive combat interval (scroll 1000–6500, speed
15), A500 measures 26–40 Hz. Its four-field gaps occur at scroll 5381 and
5552, around the town transition. Stock A1200
`A1200_tod_enhanced_combat_landing` completes the same landing/next-mission
route with an identical final gameplay record. All 30 completed windows
after startup are 49–50 Hz, with six two-field hitches (scroll 4238, 5435,
5729 and 6026); landing/slides have no missed fields. Both runners restore
normal EXE/ADF. The next detailed A500 profile should target the town
transition instead of only the first 1000 logical frames. No additional
release optimization is attributed to this expanded validation pass.

Procedural ground-target destruction now restores the target row and the
smoke row above it, including both columns for a tank. It reuses the existing
world-derived Bob compositor. Authored targets with geometry outside those
rows retain the full-column fallback. The independent full-bitmap reference
checks all five procedural target IDs (including front/rear tank hits), both
presentation modes and primary-only/wrap-duplicate placements against the old
full-column repaint (`a500-target-rows-contract`, PASS, 240-second allowance).

A500 `tod_target_rows_stress` retains 31–39 updates/s and the same final
gameplay. Its last completed window reaches the same scroll position one
hardware field earlier; no material general FPS gain is claimed. Stock A1200
`A1200_tod_target_rows_stress` reduces post-startup two-field hitches from
four to one: the first flight window is 49 Hz and every later completed
window is 50 Hz. Final gameplay matches the previous baseline. Both runners
restore normal EXE/ADF. Heavy A500 load and continuous visual/damage testing
remain open; this is not completion of the scrolling goal.

Player rockets, player bombs and the second player's bombs now repaint only
the changed crater row through the world-derived Bob compositor. Previously
they repainted all 25 rows and the full object overlays. An independent
comparison applies real crater mutations to eight sampled land columns per
presentation mode, covering primary-only and wrap-duplicate placements, and
compares the entire bitmap against the former full-column renderer. The
expanded contract passes (`a500-crater-row-contract`, 240-second allowance).

A500 `tod_crater_rows_stress` preserves final gameplay and modestly improves
completed full-speed windows from 30–39 to 31–39 updates/s. Near 32 hardware
seconds the last completed window reaches scroll 2360 versus 2342, with
slightly different window boundaries. This is a small gain around impacts,
not proof of uniformly smooth A500 scrolling or an exact runtime saving.
Stock A1200 `A1200_tod_crater_rows_stress` keeps the same 49–50 Hz windows,
four two-field hitches after startup and identical final gameplay. Both
runners restore normal EXE/ADF. The ordinary full landing route was not
repeated for this crater-only change; its last evidence is the preceding
rocket-word run. Continuous visual motion and real player-damage coverage
remain outstanding.

The profiler now exposes 28 stages. Stages 26/27 split stage 12 into weapon
updates and flak spawning. A500 `tod_weapons_split_stages` records means of
35.8 and 6.3 raster lines, with maxima 255 and 26 in the 128 late-opening
hitch samples. This localizes the large combined-stage spikes to weapon
updates rather than flak spawning. As before, instrumentation affects timing
and relative raster durations can alias long uninterrupted calls.

Missile BOB drawing/erasure now use 16-bit transfers for two-byte footprints
when both framebuffer and saved-background addresses are even. Odd addresses
and one-byte/clipped placements retain the byte path. The alias-safe word
type respects 68000 alignment and the existing big-endian row masks; the
fifth storage plane is untouched. The pixel reference now covers all shifts
at even and odd byte addresses, both destination footprint slots, clipped
edges and erasure for every tested alignment (`a500-rocket-word-contract`,
PASS with the expanded 240-second allowance).

A500 `tod_rocket_words_stress` preserves final gameplay. Completed full-speed
windows improve from 30–37 to 30–39 updates/s. Near 32 hardware seconds the
latest completed window reaches scroll 2342 versus 2273; this is a partial
window comparison, not exact total runtime. The weakest periods remain near
30 Hz, so heavy-load scrolling is not yet smooth.

Stock A1200 `A1200_tod_rocket_words_stress` retains 49–50 Hz and four
two-field hitches after startup, with identical final gameplay. Full A500
`tod_rocket_words_landing` remains mostly 49–50 Hz, weakest late window 47,
but saves one missed field in that late window (five versus six hitches).
The landing gap remains two fields and later slide windows remain 50 Hz.
Final gameplay is unchanged. These are modest gains, not completion of the
A500 scrolling objective.

The refreshed A500 `tod_bob_cache_stages` profile (128 late-opening hitch
records) measures mean raster-line costs of 156.4 for game logic, 62.1 for
streaming, 39.6 for missile drawing, 20.4 for impacts, 17.3 for bomb drawing,
7.6 for missile raster waiting and 11.1 for missile erasure. Nested streaming
stages average 28.8 for column construction, 23.0 for tile copies and 4.7 for
direct overlays. These instrumented hitch samples identify remaining work;
they are not uninstrumented FPS or an exact frame-for-frame comparison to
earlier profiles.

The generic Bob eraser now reuses two world-derived render columns, keyed by
absolute column, world revision and presentation mode. It never caches screen
bytes. Direct masked objects still redraw from current state on every erase.
Existing revisions cover route/seed changes, crater/smoke damage and runtime
flak changes. The contract compares fresh columns around every route segment
boundary in both modes, asserts reuse, and forces revision/mode invalidation
(`a500-bob-column-cache-contract`, timeout 240 seconds, PASS).

A500 `tod_bob_column_cache_stress` preserves final gameplay and improves
completed full-speed windows to 30–37 updates/s from 26–36 in the bomb-plane
baseline. At roughly 32 hardware seconds the last completed window reaches
scroll 2273 versus 2180 previously. The final partial window is not flushed,
so these are completed-window comparisons, not exact end-to-end durations.
The remaining heavy-load deficit is still substantial.

Stock A1200 `A1200_tod_bob_column_cache_stress` preserves the previous
49–50 Hz windows and four two-field hitches after startup. Full A500
`tod_bob_column_cache_landing` likewise preserves the prior route timing
(mostly 49–50 Hz, weakest late window 47, two-field landing gap). Final
gameplay matches the corresponding baseline on both machines. All runners
restore normal EXE/ADF; release EXE is 506540 bytes, up 288 bytes. This
optimization improves the weapon stress workload, not ordinary flight.

Bomb drawing now advances fixed-stride pointers and writes the four visible
planes directly; erasure walks saved bytes with the same fixed strides.
The fifth storage plane remains untouched. An independent pixel reference
checks both silhouettes, all bit shifts at even and odd byte addresses,
both simultaneous placements, clipped right edges, saved backgrounds and
complete restoration, including guard rows (`a500-bomb-planes-contract`).
The full contract passes and normal EXE/ADF restoration succeeds.

A500 `tod_bomb_planes_stress` preserves the previous final gameplay record.
Full-speed windows range from 26–36 updates/s versus 27–34 previously;
several early windows improve, but the weakest window does not. The last
completed window reaches scroll 2348 versus 2270 at approximately the same
hardware time; this is not an exact total-duration measurement because
partial final windows are not flushed. A visible emulator screenshot showed
no obvious residue, but does not establish smooth motion or absence of
transient tearing. Heavy A500 stress remains below the target cadence.

Stock A1200 `A1200_tod_bomb_planes_stress` retains 49–50 updates/s and
the same four two-field hitches after startup, with identical final gameplay.
Both stress runners restore the normal EXE/ADF. The release EXE is now
506252 bytes, 332 bytes larger than the resident-sprite version.

The full A500 `tod_bomb_planes_landing` route retains the previous timing:
most flight windows 49–50 Hz, weakest late window 47 Hz, a two-field landing
gap and subsequent slide windows at 50 Hz. Its final gameplay record matches
`tod_sprite_resident_landing`. The bomb change therefore has no measured
benefit on this ordinary route; its benefit is limited to parts of the weapon
stress probe. Real damage/audio and continuous visual motion review remain
unverified, and the overall scrolling goal is still open.

Attached aircraft now track which immutable artwork resides in each sprite
pair. A matching pair only rewrites its control words. Every pixel writer
(mapped attached, CPC halves, game tile, eject and row-based sprites)
invalidates overlapping ownership; hiding preserves the payload. Initial
sprite allocation resets residency. Tests exercise movement/hiding plus all
five writer paths, compare the entire pair with fresh source conversion,
and assert that the no-copy path was actually reached
(`a500-sprite-resident-contract`).

A500 `tod_sprite_resident_stress` preserves final gameplay and the overall
27–34 Hz window range. Several intermediate windows improve modestly; the
last completed window reaches scroll 2270 versus 2243 previously. This is a
small gain, not an assertion of smooth stress performance.

A500 `tod_sprite_resident_landing` completes the full skill-1 route with
the same final gameplay record as `tod_rocket_shapes_landing`. Most completed
flight windows are now 49–50 Hz, with the weakest late window still 47 Hz.
The stationary landing gap remains two fields and slide windows stay 50 Hz.
This also covers the accumulated cloud/impact changes on the full route.

Stock A1200 `A1200_tod_sprite_resident_stress` retains the previous timing
exactly (49–50 Hz, four two-field hitches after startup), with final gameplay
matching A500. All runners restore normal EXE/ADF. The current release EXE
is 505920 bytes: residency adds code as well as metadata, so its modest speed
gain should be weighed against this footprint in subsequent review. A500
stress performance, real damage/audio and visible motion checks remain open.

The profiler now has 26 stages. Stages 23/24/25 are subsets of streaming
stage 3: column-content construction, tile copying, and direct object overlays.
`tod_stream_split_stages` measures means of 21.8, 22.9 and 4.3 scanlines
respectively in the 128 late-opening hitch records. Current missile drawing
averages 42.2 and erasure 11.9 scanlines. Relative raster measurements and
profiler overhead retain the limitations described above.

Cloud insertion now resolves the authored cloud column once, applying at
most three rows after the terrain baseline. Claimed land/coast cells remain
protected. An independent comparison checks every generated column against
the original per-row query, with both open sky and alternating terrain claims.
The complete contract passes (`a500-cloud-column-contract-extended`). The
larger suite exceeded 120 seconds; the runner now defaults to 180 seconds
(maximum 240), retaining all checks. A500 `tod_cloud_column_stress` improves
to 27–34 updates/s from 26–33, with identical final gameplay.

Bomb/splash source masks now OR the five tile bitplanes directly; falling
art copies its four visible planes, while water spray keeps its white mapping.
The independent per-pixel conversion matches all four changed kinds
(`a500-impact-planar-contract`). A500 `tod_impact_planar_stress` retains
27–34 updates/s and identical gameplay, saving only one field in the logged
probe: no material FPS gain is attributed to this simpler conversion. Both
the cloud-only and combined probes have maximum three-field logged gaps,
versus four in `tod_rocket_shapes_stress`; unlogged tails remain unproven.

Stock A1200 `A1200_tod_impact_planar_stress` retains the previous 49–50 Hz
timing exactly, with four two-field hitches after startup and final gameplay
identical to A500. Normal EXE/ADF restoration succeeds. The latest cloud/impact
changes still need a full-route check; real damage and visible motion review
remain outstanding, alongside the A500 stress performance deficit.

Missile drawing now uses the exact pen-6/pen-10 bit structure directly:
plane 0 clears the mask, plane 1 sets it, and planes 2/3 select yellow/black.
Eight cached immutable shifted shapes hold opaque/black row masks and are
shared by primary/seam placements (288 bytes on the target ABI). All covered
background bytes are still saved, including transparent pixels. The pixel
and background contract passes (`a500-rocket-shapes-contract`). A500
`tod_rocket_shapes_stress` improves the completed moving windows to 26–33
updates/s from 25–32, with identical final gameplay. Four-field worst gaps
remain; this is not a smoothness pass.

Stock A1200 `A1200_tod_rocket_shapes_stress` improves to 49–50 updates/s
after startup, with four two-field hitches instead of eight. Final gameplay
matches A500 exactly. Both stress runners restore normal EXE/ADF successfully.

A500 `tod_rocket_shapes_landing` preserves the previous full-route final
gameplay record. The first two moving windows improve to 49 Hz and several
later windows reach 50, but late flight still has 47 Hz windows. The stationary
landing transition retains one two-field gap, with subsequent slide windows
at 50 Hz. This verifies the current build beyond the opening weapon probe.

Stock A1200 `A1200_tod_rocket_shapes_landing` completes all 30 logged
post-startup flight/landing/slide windows at 50 Hz without hitches. Full-route
final gameplay matches A500. Both full-route runners restore normal EXE/ADF.
The remaining A500 stress deficit, real damage/audio load and visible motion
review still prevent declaring the overall scrolling goal complete.

The missile compositor now has an independent pixel-wise contract covering
all eight bit offsets, both footprint placements, one-byte clipping at the
ring edge, transparent/directional/arbitrary tiles and monochrome mapping.
The full ten-row band and entire saved footprint must match, including the
untouched fifth plane and surrounding rows (`a500-rocket-bytes-contract`).
An explicit one/two-byte inner loop passed this test but did not materially
improve `tod_rocket_bytes_stress`; that production change was reverted.

Stage count is now 23: stage 21 measures missile raster waiting, stage 22
measures old missile erasure, and stage 5 measures new missile drawing only.
Earlier logs use stage 5 for all three combined. The hitch output buffer
size now derives from the stage count.

`tod_rocket_split_stages` shows mean late-opening costs of 57.6 scanlines
for missile drawing, 30.1 for erasure and 7.4 for waiting (128 hitch records).
The wait is therefore not the main cost. Weapon/flak logic falls to 33.6
scanlines after the ship-column filter, versus 86.2 in the previous profile.
Missile erasure now walks the saved placement bytes and destination planes
with fixed strides, replacing repeated indexed byte loops. The independent
contract restores the complete ten-row test band exactly and verifies retired
footprint state (`a500-rocket-erase-contract`).

A500 `tod_rocket_erase_stress` improves completed post-acceleration windows
to 25–32 updates/s from 24–29, with unchanged final gameplay. Worst gaps
remain four fields. The last completed hardware window is now 1849 versus
1949; because the trailing partial window is not emitted, this is not an
exact total-run duration comparison. This improvement is retained.

Stock A1200 `A1200_tod_rocket_erase_stress` matches its previous timing
exactly (48–50 updates/s, eight two-field hitches after startup). Its final
gameplay record matches A500. Both runners finish normal EXE/ADF restoration.
Drawing remains the larger missile cost; full-route current-build checks,
real damage/audio and visible motion verification are still outstanding.

`tod_stress_stages` profiles the same 1000-frame stress workload. In the
128 late-opening hitch records (scroll >=1000), weapon/flak update averages
86 scanlines, versus about 9 for enemy-plane/target logic. Root logic averages
186 scanlines and the combined missile wait/erase/draw stage averages 97.
Nested stages are not additive with their parent; relative beam samples can
alias calls longer than a field and profiling adds overhead.

Enemy-ship neighbourhood probes now reject columns without an authored ship
through the existing object index before resolving terrain/cloud/smoke cells.
Candidate columns still use the full resolver, preserving destruction and
terrain/smoke precedence and the original row/column hit order. The candidate
predicate matches an independent full scan across the complete column range;
the expanded contract passes (`a500-ship-column-contract`).

A500 `tod_ship_column_stress` raises the post-acceleration window range
from 22–26 to 24–29 updates/s with identical final gameplay. Total logged
hardware fields at the last completed window fall from 2049 to 1949, although
the final partial window is not flushed. Four-field worst gaps still occur;
this is a useful reduction in work, not a smoothness pass.

Stock A1200 `A1200_tod_ship_column_stress` has 48–50 updates/s after
startup, with eight two-field hitches in the completed moving windows.
Final gameplay matches A500 exactly. Thus even A1200's skill-1 50 Hz result
does not cover this heavier workload. Both runners restore normal EXE/ADF.

A500 `tod_wide_memcpy_stress` tests skill 5, wingman control 2, enemy rate
3x, the enemy-plane exercise and continuous two-player weapon stress for
1000 logical frames. After acceleration, the completed moving windows fall
to **22–26 updates/s**, with gaps up to four PAL fields. The synthetic pilot
remains invulnerable, so this does not cover real damage/failure audio load.
All four logs are captured and the ordinary EXE/ADF is restored successfully.
The final scroll is 2393; this is an opening stress probe, not a full route
or landing check. The skill-1 47–50 Hz result must not be generalized to this
load. Next performance work should profile this weapon/enemy workload,
followed by ordinary damage and visible motion checks on both machines.

### Retaining waves after the outgoing head

The retained range may now start after outgoing old waves. Erase the old
head and tail, shift only the retained footprint metadata, then draw the new
tail. The unchanged-phase/world-revision/stream checks remain conservative;
potentially shared bytes at either boundary force redraw. The existing pixel
reference passes and explicitly exercises shifted retention
(`a500-wave-range-contract`). A500 `tod_wave_range` improves the weakest
opening window from 45 to 47 updates/s (nine missed fields down to five).
The first moving window improves 47 to 48; final gameplay parity is unchanged.

A500 `tod_wave_range_landing` completes landing with identical final parity
to `tod_frigate_bounds_landing`. Completed flight windows span 46–50 updates/s;
the weakest is now the late approach, not the opening sea. The stationary
landing transition still contains a five-field gap, while the slide windows
are 50 Hz. This is further progress, not a completed smoothness audit.

Stock A1200 `A1200_tod_wave_range_landing` retains 50 Hz in the completed
flight and slide windows. Its stationary landing window at scroll 7208 still
has one two-field gap (49 updates/s). The final gameplay record matches A500,
and both runners completed normal EXE/ADF restoration.

An exact 3872-byte engine-amplitude lookup table passed the exhaustive state
and audio tests (`a500-engine-amplitude-contract`) but saved only one field
in the opening probe (`tod_engine_amplitude`). It was reverted rather than
adding that permanent memory cost for an insignificant pacing improvement.

### Flak restoration and deck probes

Player/runtime-flak contact now calls the existing one-row world compositor
instead of redrawing the complete column. This restores the authoritative
base plus masked ship/Enhanced overlays in both ring placements. Sixteen
Classic/Enhanced full-buffer comparisons pass (`a500-flak-cell-contract`).
A500 `tod_flak_cell_landing` completes the landing route; moving windows
remain 44–50 Hz, so the overall pacing improvement is not yet clear.

The hitch profiler now has 19 stages. Additional stages 16/17/18 measure
player object-map collision, deck replenishment checks, and fatal/flak
handling, respectively; all are subsets of stage 15. In
`tod_collision_stages`, the collision workloads at scroll 1682/1697 fall
from the earlier 120/117 scanlines to 74/82, despite the additional probes.
Deck replenishment still takes 15–16 scanlines even away from deck height.
The next change derives minimum/maximum eligible probe rows from the actual
friendly-frigate index, retaining its existing +/-1-row contact tolerance.

Autoplay skips real flak damage/audio via `applyPlayerFlakDamage`'s existing
headless return. These routes therefore do not validate normal damage/audio
headroom; an interactive or dedicated damage exercise remains required.

The derived frigate height bounds pass comparison with an independent full
object scan at every pixel height and each ship's horizontal edges
(`a500-frigate-bounds-contract`). A500 `tod_frigate_bounds_landing` completes
mission 1 and reaches mission 2 with identical final parity. Flight windows
are 45–50 updates/s; several later windows improve from 45–48 to 48–49.
The stationary landing transition still has a five-field gap. This remains
work in progress, with misses during flight and ordinary damage/audio plus
broader skill/weapon validation outstanding.

Stock A1200 `A1200_tod_frigate_bounds_landing` completes the full route with
identical final parity. All completed flight and landing-slide windows are
50 Hz. The stationary landing window at scroll 7208 still has one two-field
gap (49 updates/s). Both runners completed and restored the normal EXE/ADF.

Follow-up: local (per-column) invalidation of this window passed the contract
but did not improve the opening probe (`tod_local_wave`), so it was reverted.

`HAR_DEBUG_PERF_HITCH_STAGES` adds deferred `!` records containing the
previous logical frame, scroll, measured TOD gap and 16 stage scanline
counts. The previous workload is intentional: the TOD sample at the next
loop's start measures the field(s) consumed by that workload. Stage indices
11–15 are subsets of logic (1), not additional time. Beam-based stage counts
are only meaningful for sections shorter than a PAL field; bootstrap cannot
be reconstructed by summing them. The profiler adds overhead and must not be
used as the uninstrumented performance result.

A500 `tod_hitch_stages` identifies full wave drawing as a large opening-sea
burst: 90–109 scanlines in several missed-field workloads. Later misses at
scroll 1682/1697 instead show collision work at 120/117 scanlines (within
logic at 184/193) and powerup work at 54/53. Both remain optimization targets;
the earlier average stage totals obscured these bursts.

A conservative partial wave redraw now retains only the matching prefix
whose phase, coordinates and world revision remain unchanged and whose cells
are outside planned streaming. It retreats across any potentially shared-byte
boundary before erasing/drawing the tail. A phase change or a departing first
wave still falls back to full redraw. The contract compares wave pixels after
each of 96 camera steps, and the complete buffers after each sequence, with
full erase/redraw; it covers animation phases and the physical page seam.
`a500-wave-prefix-contract-static.log` passes. The large test footprint arrays
must remain static: automatic arrays expanded the test stack frame to 4992
bytes and two attempts timed out; moving those arrays to static storage
allowed the unchanged comparison sequence to complete.

A500 `tod_wave_prefix` (no detailed stage profiler) improves the weakest
opening window from 44 to 45 updates/s, with 9 instead of 11 missed fields;
other completed moving windows are 47–50. Final parity is identical to
`tod_wave_candidates`. This is a small gain, not the final smoothness target.
An identified next target is `updateGameCollisions`: consuming runtime flak
calls `dirtyRedrawWorldColumn` for a one-cell mutation. A replacement must
restore the exact cached base row plus any direct wide-object overlay, in
both physical ring placements, and compare against the complete redraw.

Stock A1200 `A1200_tod_wave_prefix` retains 50 Hz in every completed moving
window of the 1000-logical-frame opening probe. This latest partial-redraw
change has not yet had the longer landing/weapon/skill sweep; those remain
required before the optimization work can be considered complete.

Wave selection and drawing now share an ordered candidate window. Forward
scrolling retains overlapping candidates and evaluates only incoming columns;
backward movement, large jumps and world revisions rebuild the window. It
retains all candidates before applying the existing 16-wave drawing limit.
The Classic contract passes an independent full scan across route boundaries,
forward/backward movement, flak slot replacement and smoke mutations.

The A500 `tod_wave_candidates` opening probe improves completed moving
windows from 41–50 to 44–50 updates/s. The first moving window improves
46 to 47; later windows include 49 instead of 45 and 48 instead of 46.
Final gameplay parity is byte-for-byte identical to `tod_carrier_patch`.
These are hardware-field measurements with deferred logging; they still
show missed fields and do not establish universally smooth scrolling.

A500 `tod_wave_candidates_landing` completes landing and reaches mission 2
with the same final parity record as `tod_local_powerup_landing`. Completed
flight windows span 44–50 updates/s. The landing slide windows at scroll
7266 and 7288 are 50 Hz. One five-field gap occurs in a stationary window
at scroll 7208; this transition still needs investigation. Neither a
single screenshot nor these counters alone can establish absence of tearing.

Stock A1200 `A1200_tod_wave_candidates_landing` completes the same route
with identical final parity. Every completed flight and landing-slide window
records 50 Hz; the stationary landing window at scroll 7208 records one
two-field gap (49 updates/s). Startup is separately visible as 185- and
30-field gaps. A live window capture showed terrain and objects rendered
normally during flight, without the previous ROM warning. This was a still
capture, not video evidence of smoothness. Both runners restored the normal
executable and ADF after testing.

## Correction — 2026-09-07

### Active optimization: hardware-clock measurements

`HAR_DEBUG_PERF_LOG` now samples CIA-A TOD (high/mid/low, read-only) at
the end of `WaitVbl()`, at a consistent raster phase. Its modulo-24-bit
increments accumulate real PAL fields independently of the logical gameplay
counter. Gameplay timing is unchanged. `frame`/`seconds` in newly tagged
`tod_*` logs refer to this hardware clock, not the old logical counter.
The clock mechanism is documented in the
[Amiga Hardware Reference Manual, CIA appendix](https://www.theflatnet.de/pub/cbm/amiga/AmigaDevDocs/hard_f.html).

An A1200 self-test intentionally busy-waits for two fields at logical frame
450 without calling `WaitVbl`. Relative to the uninjected baseline, the
corresponding 100-field window falls from 100 updates to 98, with exactly
one 3-field gap. This validates detection of missed fields.

The new baseline **contradicts the old smoothness claims**:

- Stock A1200: six single-field misses later in the opening flight; there
  are also longer startup/transition gaps. Several windows are steady 50 Hz.
- A500 + 512 KiB slow RAM: opening flight windows range around 18–33
  updates/s, with frequent multi-field gaps. This is not smooth scrolling.
- A control run using the former compulsory-FAST hunk packaging reproduces
  every A500 timing sample exactly. The A1200-compatible packaging change
  did not introduce this slowdown.

Evidence tags: `tod_selftest`, `tod_baseline`, `tod_fastcontrol` in
`.tmp/amiga-parity-results`; logs `.tmp/a1200-tod-*.log` and
`.tmp/a500-tod-*.log`. These are instrumented autoplay builds, not proof
of exact release-build headroom or every visual artifact.

Work in progress: retained changes include the fixed-stride world tile
copier, four cached aircraft sprite variants, recent level-segment lookup,
revision-invalidated sea-cell and powerup-background caches, carrier-height
rejection, indexed friendly-frigate probes and a table-based engine LFSR
loop. The Classic contract suite passes byte-for-byte tile/sprite checks,
world mutation/cache checks and original gameplay checks. The engine loop
also passes comparison with the original generator over 128 batches per
speed, including buffer wrap and final generator state. The attempted
unrolled sea-wave compositor did not improve timings and was reverted.

The A500 `tod_optimized_clean` opening-flight run (no stage profiler) reaches
scroll 2366 at hardware field 1682, compared with the original baseline's
later progress. Midflight windows reach 39–40 updates/s; early moving
windows still fall to 23–26 and later windows to 32–38. Frequent missed
fields remain: this is a measurable improvement, **not smooth 50 Hz yet**.
Evidence: `.tmp/a500-tod-optimized-clean.log` and the correspondingly tagged
performance CSV. All probes use the same 1000-logical-frame autoplay route.

The corresponding stock A1200 `A1200_tod_optimized_clean` run records 50 Hz
in all completed moving-flight windows except one 49 Hz window containing
a single two-field gap. Startup/scene transitions remain separate long
gaps. This improves on the baseline's six later missed fields, but does not
yet establish flawless pacing or validate the full mission.

Further measurements (`tod_collision_cache`, `tod_compiler_o3`) retain a
64-entry aircraft cell cache and move the standard build from O2 to O3.
Aircraft-cache tests compare the facade/base resolver over sampled columns
across the entire route, both cold and warm, and exercise flak/smoke changes.
Route reconfiguration now also invalidates the world revision. O3 passes
the full Classic contract suite and produces exactly the same short-route
gameplay parity record as `tod_optimized_clean`. Its A500 flight windows
reach 44–48 updates/s after the early sea section (35–38). Last complete
window: hardware field 1280, scroll 2213. This compiler change makes a larger
difference than the small collision-cache gain. The A500 `tod_o3_full` run
reaches the final carrier with later windows around 35–48 updates/s, still
insufficient for smooth PAL scrolling. The former "full route" harness
stops 50 logical frames after entering hover; it does **not** test landing.

`HAR_HEADLESS_LANDING_EXERCISE=1` now drives normal left/right/down input to
the carrier's clear left pad, then observes the landing, carrier slide and
next-mission ready state. Stock A1200 `A1200_tod_o3_landing` completes this
sequence (mission 2, landingCompletes=1). Completed flight/slide windows
are 50 Hz except one 49 Hz window at scroll 7208. Startup transition gaps
are reported separately. This build also omits writes to the unused fifth
world tile plane: the world's Copper already fetches four planes. Contract
checks compare every visible byte to the original renderer and verify the
unused plane stays untouched. The initial division-per-byte verifier hit
its 45-second limit; a row/plane traversal passed the same full-buffer check.

`HAR_DEBUG_PERF_DEFERRED=1` is the next, lower-overhead pacing probe. It
stores numeric snapshots during play and formats CSV only at shutdown,
avoiding the original interval console/CSV formatting in the gameplay loop.
Its CSV retains the first ten timing/scroll columns; detailed gameplay
parity remains in the separate parity log. A500 `tod_deferred_landing`
completes landing and reaches mission 2 with exactly the same parity record
as `A1200_tod_o3_landing`. Moving-flight windows still range 35–48 Hz;
late-route windows are 36–42 Hz. Removing formatting only modestly improves
these results, confirming the remaining misses are not just logger overhead.
The carrier slide records 47–48 Hz and the following stationary hold 50 Hz;
one landing window includes a five-field gap. Ordinary EXE/ADF builds were
restored after all runners completed. O3 alone is not evidence of smoothness, and completed timing
windows still need visual corroboration and broader skill/weapon coverage.

Further retained work (`tod_bulk_columns`, `tod_rocket_masks`): resolve
packed town smoke and ship-wreck entries once per rendered column, and
consult the runtime-flak slot once instead of probing all 25 rows. Reference
tests preserve per-cell smoke/claim precedence across segment boundaries.
Town facade/smoke lookups also reject impossible rows before world lookup,
using the same named constant as the town terrain height. Missile row masks
now operate directly on planar bytes, retaining the original transparent,
black and yellow mapping. The Classic contract compares every tile ID,
row, offset and monochrome mode with the original pixel decoder and passes.
A powerup position-hoisting experiment had identical pacing samples and
was reverted, including its temporary reference helper/test.

Latest full A500 evidence `tod_rocket_landing`: mission 2 is reached;
moving-flight windows range 41–50 Hz, with late-route windows mostly
42–46 Hz. Landing/slide still includes missed fields. These are worthwhile
gains over the initial 18–33 Hz baseline but remain short of smooth PAL.
`HAR_DEBUG_PERF_BEAM_STAGES=1` is an optional lightweight relative profiler:
it measures steady sections shorter than one PAL field using beam position.
It cannot measure startup or sections lasting a full field, and is not a
replacement for the independent CIA-A pacing clock.

`tod_beam_stages` shows steady stage totals without the prior CIA-B wrap
artifacts. At scroll 2204 the approximate scanlines/update are: audio/input
21, logic 81 (engine 26 and collisions 25 are subsets), sprites/erase 51,
stream 36, powerup 66, missiles/wait 12. At scroll 878 powerup is inactive
and its cost is about 1 line. This identifies powerup background restoration
as a remaining expensive variable component. Its two-column cache currently
invalidates on any world mutation, including distant flak; a more precise
invalidation scheme must preserve all local terrain/smoke edits.

That targeted invalidation is now implemented: map/mission resets dirty
the whole two-column powerup cache; point edits dirty it only when either
cached column is affected. Tank destruction also covers its second column.
Replacing a direct-mapped flak slot invalidates the old column as well as
the new one. Contract tests pass for local left/right changes, distant
changes that must not rebuild the cache, and slot replacement. The short
`tod_local_powerup` run gains another complete 50 Hz window. Full A500
`tod_local_powerup_landing` reaches mission 2 with identical gameplay parity:
most flight windows are 46–50 Hz, with slower sections at 41–44 Hz and
late-route windows of 42–48 Hz. The final carrier slide still misses fields.
The preceding A1200 `A1200_tod_rocket_landing`
run completed at 50 Hz in measured flight/slide windows with one two-field
gap at landing, confirming no regression from bulk smoke/missile rendering.

Takeoff investigation: the 2048-byte engine buffer is now prepared at
session setup, before the new scene is displayed. Preparation leaves the
LFSR unchanged; start consumes the prepared buffer only with matching speed
and seed, then commits the original final state. Stale/mismatched/reused
preparation falls back to the original fill. Contract tests pass byte-for-byte
audio, state, speed mismatch, seed mismatch and single-consumption checks.
The `tod_prepared_engine` probe improved takeoff but retained a six-field gap.

The remaining large gap was traced to refreshing every full-height carrier
column when the parked Wingman launched. `dirtyRedrawNativeCarrierAt()` now
compares the two carrier artwork variants and rebuilds only changed tiles.
A full-buffer reference test matches the previous complete redraw. In
`tod_carrier_patch`, the largest post-startup gap in the takeoff window falls
from six fields to two. Normal-flight single-field misses remain.
Stock A1200 `A1200_tod_carrier_patch` records 50 Hz without missed fields
in every completed moving-flight window of the 1000-logical-frame probe.
The initial scene-construction gap remains separate. Standard EXE/ADF were
restored after this run. This short probe does not replace full-route and
visual validation of the final optimized version.

Two further wave experiments were measured and reverted: hash caching
(`tod_cached_ambience`) and retaining an unchanged empty wave window
(`tod_empty_wave_window`). Neither materially improved field pacing, so
their implementation and temporary tests were removed. The expensive
boundary frames still rescan the complete column window. An incremental
ordered candidate window, shared by wave selection and drawing, is a
possible next optimization; it would need exact ordering, capacity, world
mutation invalidation and pixel-output checks against the current scan.

Optional `HAR_DEBUG_PERF_STAGES` records
comment lines beginning `#` with CIA-B scanline stage totals; ignore these
lines when parsing the main CSV. Occasional implausible CIA-B stage deltas
are not used as quantitative proof. The stable-phase CIA-A pacing samples
are the primary timing evidence. Full-route and visual validation remain
required before this scrolling objective is complete.

The FPS/hitch figures below are **not independent hardware frame-pacing
measurements**. `WaitVbl()` increments `frameCounter` once per call, and the
performance logger derives its timing from that counter. Frames missed while
rendering may therefore go uncounted. The recorded routes remain useful for
functional coverage and streamed-column readiness, but the earlier 50 FPS
figures cannot establish smooth scrolling on A500 or A1200.

The parity runner now accepts `-Machine A1200` (A500 remains the default).
It generates a PAL, cycle-exact 68020/24-bit, AGA, 2 MiB chip RAM configuration
with no fast/slow expansion RAM or JIT. Use the model-specific
`amiga-os-300-a1200.rom` with its existing local ROM key: the generically named
`Kickstart3.0.rom` in this checkout is not an A1200 ROM and produces WinUAE's
32-bit-addressing warning. Result names include `A1200` to prevent replacing
A500 measurements. Host filesystem mounts are retained for the test harness;
this is not a floppy-boot validation.

### Stock A1200 startup result

The initial executable failed before `main`: its ordinary text/data/BSS hunks
were forced to `MEMF_FAST`, unavailable on an unexpanded A1200. The Makefile
now converts the original ELF directly, leaving ordinary hunks unrestricted
and retaining the explicit chip-memory DMA section. The executable also
depends on the Makefile so this packaging change takes effect without a
source-code edit.

After the fix, the visible A1200 autoplay completed its 1,000-counter-frame
startup probe, wrote all four CSVs and reached scroll 2393. A screenshot
confirmed gameplay beyond the first shoreline. This verifies loading and
route progress, not the absence of the user's reported scrolling artifact.
No scrolling logic was changed. The release executable and ADF are restored
by the runner after each test.

```powershell
.\run-amiga-parity.ps1 -Machine A1200 -Visible -Skills 1 -ResultTag startup_anyram -ExtraCcFlags '-DPERF_LOG_INTERVAL_FRAMES=100 -DHAR_HEADLESS_MAX_FRAMES=1000 -DHAR_HIGHSCORE_DISK_IO=0'
```

Evidence: `.tmp/a1200-startup-anyram.log` and
`.tmp/amiga-parity-results/*_A1200_startup_anyram.csv`.

The matching A500 startup probe also completed after the packaging fix;
every field of its parity CSV matches the A1200 result (including scroll
2393). Evidence: `.tmp/a500-startup-anyram.log` and the same result names
without `_A1200`. The restored release HUNK allocation flags are
`0, 0, CHIP, 0, 0`: no compulsory FAST hunks remain.

The compact ground graphics and eight editable city blocks were checked on
the project's cycle-exact PAL A500 configuration: 68000, OCS, Kickstart 1.3,
512 KiB chip RAM and 512 KiB expansion RAM. All runs use seed 12040,
cruise 15 and Enhanced mode. High-score disk writes are disabled.

## Measurements

Samples cover 100 VBlanks (two PAL seconds). The first sample includes
loading/world setup and is excluded from gameplay timing. Stationary
final-carrier samples are also excluded from the moving-window figures.

| Route | Moving samples | Minimum FPS | Hitches | Maximum VBlank gap | Final carrier |
|---|---:|---:|---:|---:|---|
| Skill 1, CPU Wingman | 24 | 50 | 0 | 1 | Reached |
| Skill 5, CPU Wingman | 51 | 50 | 0 | 1 | Reached |
| Skill 1, two-player continuous weapons, final code | 25 | 50 | 0 | 1 | Reached |
| Skill 5, two-player weapon probe (1,000-frame cap) | 8 | 50 | 0 | 1 | Intentionally stopped early |

Each CPU route contains six samples while the city is visible. All have
50 FPS minimum and zero hitches. The smallest sampled distance from the
right screen edge to the last completed streamed column is 117 pixels,
using a conservative 320-pixel screen width. This is a sampled readiness
check, not a per-frame assertion.

The complete two-player route also has six city samples at 50 FPS with zero
hitches and the same 117-pixel minimum sampled stream margin. Logged launches
are P1: 197 rockets / 82 bombs; P2: 124 rockets / 87 bombs. It reaches the final
carrier with final scroll 7358. Thus the final code is exercised through the
city with both projectile streams active.

CPU runs establish the baseline before the two lookup optimizations below.
The final code passes the Classic contract suite, including the added
city-bank boundary checks. Its short two-player probe also passes timing
checks: both players launch rockets and bombs, and the run stops normally
at its deliberately shortened frame budget (final scroll 2393).

**Unresolved stress-test limit:** the full skill-5 two-player weapon run
exceeded its 480-second host timeout without flushing results. It must not
be counted as a pass, nor interpreted as measured FPS loss. The shorter
probe rules out an immediate startup failure, but does not explain why the
long run exceeded its deadline. The follow-up emulator was observed running
at Windows BelowNormal priority; host scheduling is a possible timing factor,
not a demonstrated explanation. The complete skill-1 weapon run passes,
including its city section.

## Code review and changes

- City graphics use the existing cached render-column path and row budget.
  They add 2,760 bytes of source tile data, with no extra city overlay pass
  and no additional per-frame pixel conversion or allocation.
- The compiler already strength-reduces the innermost plane addressing:
  disassembly shows pointer increments, not a multiply per plane. This loop
  was left unchanged.
- Original tile IDs now resolve before checking the presentation mode.
  Disassembly confirms the common path skips the mode read. Both ordinary
  drawing and powerup-background restoration use this resolver.
- Classic exits the Enhanced ground-overlay helper before segment/target
  lookups. No camera speed, gameplay or collision rule changed.
- The debug-only performance buffer scales with the logging frequency.
  Previously 100-frame samples could exhaust 4 KiB before the long route
  reached the city. The skill-5 log now contains 7,200 bytes and covers the
  city and final approach. Release builds allocate none of this buffer.
- The Classic contract checks city-bank isolation, first/last valid city
  tiles, and invalid-ID fallback. Test emulators launch with a hidden-window
  request. The runners restore the normal executable and ADF on completion.

## Reproduction and evidence

Run from the repository root:

```powershell
.\run-amiga-parity.ps1 -Skills 1,5 -WingmanControl 1 -ResultTag city_editor_audit -ExtraCcFlags '-DPERF_LOG_INTERVAL_FRAMES=100 -DHAR_HIGHSCORE_DISK_IO=0'
.\run-amiga-parity.ps1 -Skills 5 -WingmanControl 2 -WeaponStress -ResultTag city_editor_stress_final -ExtraCcFlags '-DPERF_LOG_INTERVAL_FRAMES=100 -DHAR_HIGHSCORE_DISK_IO=0'
.\run-amiga-parity.ps1 -Skills 1 -WingmanControl 2 -WeaponStress -ResultTag city_editor_stress_city -ExtraCcFlags '-DPERF_LOG_INTERVAL_FRAMES=100 -DHAR_HIGHSCORE_DISK_IO=0'
.\run-amiga-classic-contract.ps1
```

Raw perf/parity CSVs are under `.tmp/amiga-parity-results`, tagged
`city_editor_audit` and `city_editor_stress_final`. These local artifacts are
ignored by Git. Build/run logs are `.tmp/city-scroll-audit.log`,
`.tmp/city-scroll-stress.log` and `.tmp/city-scroll-contract.log`.
The timeout run produced no CSV; short-probe CSVs use
`city_editor_stress_probe`, and the complete skill-1 weapon run uses
`city_editor_stress_city` (`.tmp/city-scroll-stress-city.log`).

## Limits

These results verify frame pacing in the measured emulator routes, not
universal headroom or every gameplay state. Bootstrap takes multiple VBlanks.
The autoplay reaches the final carrier's landing hover; it does not prove a
complete landing/relaunch loop. Stable 50 FPS does not by itself rule out
tearing or a visible ring-wrap artifact. A visual pass on real A500 hardware
is still needed for those aspects. At full throttle the existing camera
moves three low-resolution pixels per PAL field; this audit does not change
that visual cadence.
