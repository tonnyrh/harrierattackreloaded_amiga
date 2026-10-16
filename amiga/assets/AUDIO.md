# Amiga audio overview

This is the canonical overview of audio assets and their use in the Amiga
port. Update it whenever a sound is added, renamed, removed, regenerated, or
assigned to another gameplay event.

## Build pipeline

Gameplay effects use this path:

```text
amiga/assets/sfx-sourcefiles/<source>.wav
    -> prepare-amiga-sfx.ps1
    -> tools/wav_to_amiga_sfx.py
    -> amiga/assets/sfx/<runtime>.raw or <runtime>.adpcm
    -> embedded in chip RAM by amiga/main.c
```

F5 runs the pipeline automatically. Existing WAV masters are never replaced
by AudioGen. AudioGen is used only when a manifest entry's source WAV is
missing. Exact-data PC previews are generated under
`.tmp/amiga-sfx-preview`. For ADPCM assets, the preview is reconstructed after
compression and therefore represents the bytes produced by the Amiga decoder.

### Memory profile (October 2026)

`sfx/profile.json` selects the recorded effects' sample rate: currently **8820
Hz**, previously 11025 Hz. The converter filters before resampling and generates
`sfx/sample_config.h` with the matching Paula period (402) and ambience decode
capacity (35280 bytes). Change the profile back to 11025 and rebuild to restore
the old rate. Original WAVs, generated motor/alarm waveforms and MOD music are
unchanged. Ground scrape (`sfx/ground_impact_source.wav`, maximum 450 ms) now
uses this same pipeline.

The ground-target bank has three variants. The fourth master, `ground_hit_3.wav`,
is retained in Git but no longer converted or embedded. The two flak-gun sounds
and separate flak-hit cue remain.

Fixed gameplay PCM falls from 107784 to 80160 bytes: **27624 Chip bytes saved**.
The two packed ambience assets save another 8820 stored bytes, and their optional
decode buffer shrinks by 8820 Chip bytes. The latter is not an additional saving
on machines where that buffer was already omitted. All 18 WAV masters were
hash-verified unchanged; playback-duration differences are under 1.3 ms.
Reduced high-frequency bandwidth is the intentional quality tradeoff.

Validation: the native audio contract passes all three depot half/full pairs,
Paula period, reload-to-silence and bomber loop checks. Both ADF variants boot
to the menu, and the standalone oil-platform disk enters gameplay, on a muted
A500 KS1.3 512K Chip + 512K Slow test. In the full build
with DH0/DH1 mounted, both optional ambience buffers now allocate successfully;
after the carrier blitter allocation, 5552 Chip bytes and 15080 Slow bytes remain.
Tests were muted as requested; tonal preference still needs the user's audition.

## Gameplay sound effects

| WAV master | Paula runtime asset | Used for |
|---|---|---|
| `rocket.wav` | `fire.raw` | Rocket launch and enemy missile launch. |
| `bomb.wav` | `bomb.raw` | Bomb release/fall-start cue for the player and Wingman. It is not the impact sound. |
| `impact.wav` | `impact.raw` | General explosions created by `startImpact()`: aircraft destruction, missile destruction, crashes and impacts without a specific ground-target sound. |
| `player_hit.wav` | `hit.raw` | Harrier damage, collision/ejection crash initiation and other direct player-hit cues. |
| `flak_hit.wav` | `flak_hit.raw` | Used only when the player's Harrier is struck by flak. A fatal flak hit does not additionally play `player_hit.wav`. |
| `eject.wav` | `eject.raw` | Ejector-seat launch cue when the player presses E during an aircraft-failure descent; healthy-flight E is ignored. |
| `pickpuppowerup.wav` | `pickup_powerup.raw` | Played once when the player collects any health, rocket, bomb or Wingman powerup. |
| `flak_0.wav` | `flak_gun_1.raw` | One of two flak-gun firing variants. Volume falls with vertical distance from the player and pitch occasionally varies slightly. |
| `flak_1.wav` | `flak_gun_2.raw` | One of two flak-gun firing variants. Volume falls with vertical distance from the player and pitch occasionally varies slightly. |
| `ground_hit_0.wav` | `ground_target_hit_1.raw` | Random destructive hit variant for an actual ground target, enemy ship or town block. |
| `ground_hit_1.wav` | `ground_target_hit_2.raw` | Random destructive hit variant for an actual ground target, enemy ship or town block. |
| `ground_hit_2.wav` | `ground_target_hit_3.raw` | Random destructive hit variant for an actual ground target, enemy ship or town block. |
| `ground_hit_3.wav` | Archived master only | Fourth variant retained for restoration; no runtime allocation. |
| `ground_miss_0.wav` | `ground_miss_1.raw` | Bomb/rocket hit on bare land that is not a real target - a "miss". Previously reused a random `ground_target_hit_*` variant; now its own dedicated sound. |
| `WaterSplash.wav` | `water_splash.raw` | Played when a player or Wingman bomb reaches open water; accompanies the temporary white Smoke 1/Smoke 2 spray animation. |
| `idle_0.wav` | `carrier_idle_1.adpcm` | First idle cue, shortened to 800 ms and triggered once per arriving gull flock; shares the MOD instrument arena. |
| `idle_1.wav` | Archived second cue | Retained for restoration; not embedded. Legacy browser slot uses cue 1. |

Ground-target hits and land misses both use `startWorldImpactQuiet()` for the
visual explosion and play one sample (`ground_target_hit_*` for an actual
target/ship/town block, `ground_miss_1.raw` for bare land) - never both. This
deliberately avoids also playing `impact.raw` for the same hit.

The runtime mix attenuates MOD music, the synthesized engine, carrier ambience
and all one-shot effects by approximately 15 percent. The three
`ground_target_hit_*` effects are the sole exception and retain their authored
Paula volume so their bass-heavy impact remains prominent. WAV masters and
converted sample bytes are not altered by this mix adjustment.

The two flak-gun firing variants have no global retrigger delay. A new flak
shot preferentially cuts and restarts an existing `flak_gun_1/2` Paula voice,
creating a dense rhythmic burst under heavy fire without consuming additional
voices. It may interrupt only those two firing sounds; `flak_hit` and every
other gameplay or music voice remain protected.

The three target-hit masters and bare-earth miss receive a mild low-shelf boost during Paula conversion.
That preserves their bass body after peak normalization to signed 8-bit,
8820 Hz data without changing playback pitch.

Carrier ambience alternates the two idle variants. A new cue starts after a
random 15-20 second delay, producing
approximately 3-4 cues per minute while the aircraft remains on the carrier.
Each cue fades in and out over about half a second. Taking off, pausing, leaving
gameplay, reaching Game Over, or starting MOD music triggers an early fade-out.

The two long idle masters use four-bit IMA ADPCM in the executable. They are
decoded on demand into one shared 35,280-byte chip-RAM buffer while the Harrier
is stationary on the carrier. This approximately halves their stored size and
removes four permanent PCM copies from chip RAM. Short, latency-sensitive and
overlapping gameplay effects remain direct PCM until decode cost has been
measured on a stock 68000.

## Legacy and generated audio

| Asset | Used for |
|---|---|
| `amiga/assets/music/harrier_menu_fixed.mod` | Four-channel ProTracker music on the main menu. Gameplay stops the MOD so all Paula channels can be used for effects and engine audio. |
| `amiga/assets/music/raf_game_over.mod` | Four-channel game-over song. Played once at 120% replay tempo when the final life is lost; also stops immediately on Retry or return to the menu. |
| Runtime engine buffers in `amiga/main.c` | Continuous Harrier engine sound. Generated in chip RAM and adjusted according to speed; it has no WAV or RAW asset. |
| Runtime radar waveform in `amiga/main.c` | Two-tone `beep-beep` warning from 70% detection. Paula loops a 32-byte waveform and varies `AUDxPER`, volume and repetition rate; it has no WAV or RAW asset. |

## Paula channel policy

- Gameplay reserves channel 3 for the continuous engine while it is active.
- One-shot effects dynamically use channels 0-2, with channel 3 also available
  while the engine is stopped.
- Effects have priority and left/right selection based on screen position.
- Carrier ambience has the lowest priority. It can share gameplay with the
  synthesized engine and may be pre-empted by weapons, impacts, or player cues.
- Main-menu MOD playback owns all four channels exclusively. Menu navigation
  is deliberately silent so input cannot interrupt or restart a music voice.

## Maintenance checklist

When audio behavior changes:

1. Update the `$Sounds` manifest in `prepare-amiga-sfx.ps1` for source name,
   runtime name, AudioGen fallback prompt, duration, and Paula maximum length.
2. Update the `SFX_*` table and event call sites in `amiga/main.c`.
3. Update the relevant row in this document.
4. Run `.\prepare-amiga-sfx.ps1 -NoGenerateMissing` and audition the WAV under
   `.tmp/amiga-sfx-preview`.
5. Build with F5 and test the sound in the Shift+D sound browser and in its
   real gameplay event.

## October 2026 asset cleanup

Removed the obsolete recorded carrier klaxon, rejected ground-boing source/runtime
and unused flak_pop.raw. Carrier alarms use short inline Paula waveforms;
ground bounce uses ground_impact.raw. Active WAV masters and MOD files are retained.

Depot double hit: no dedicated recording. Paula plays 50% (word-aligned) of
one existing ground_target_hit variant, followed immediately by the next
variant in full, at volume 64. Three pairs rotate; the old one-third ground-miss
replay is removed. tools/generate-bomber-drone.py writes a listening preview
only; no concatenated audio buffer is loaded by the game.

## Gull cue and shared Chip audio (2026-10-16)
Only the first idle recording is embedded for carrier gulls. The second legacy
sound-browser slot aliases it; both source WAVs remain available. See
`sfx/README.md` for the 800 ms edit and shared buffer lifetime. All attack
alarms still use the 32-byte inline Paula waveform; no recorded klaxon is linked.
