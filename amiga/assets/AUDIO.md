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

## Gameplay sound effects

| WAV master | Paula runtime asset | Used for |
|---|---|---|
| `rocket.wav` | `fire.raw` | Rocket launch and enemy missile launch. |
| `bomb.wav` | `bomb.raw` | Bomb release/fall-start cue for the player and Wingman. It is not the impact sound. |
| `impact.wav` | `impact.raw` | General explosions created by `startImpact()`: aircraft destruction, missile destruction, crashes and impacts without a specific ground-target sound. |
| `player_hit.wav` | `hit.raw` | Harrier damage, collision/ejection crash initiation and other direct player-hit cues. |
| `eject.wav` | `eject.raw` | Ejector-seat launch cue when the player presses E during flight. |
| `pickpuppowerup.wav` | `pickup_powerup.raw` | Played once when the player collects any health, rocket, bomb or Wingman powerup. |
| `flak_0.wav` | `flak_gun_1.raw` | One of two flak-gun firing variants. Volume falls with vertical distance from the player and pitch occasionally varies slightly. |
| `flak_1.wav` | `flak_gun_2.raw` | One of two flak-gun firing variants. Volume falls with vertical distance from the player and pitch occasionally varies slightly. |
| `ground_hit_0.wav` | `ground_target_hit_1.raw` | Random destructive hit variant for ground targets, enemy ships, land and town blocks. |
| `ground_hit_1.wav` | `ground_target_hit_2.raw` | Random destructive hit variant for ground targets, enemy ships, land and town blocks. |
| `ground_hit_2.wav` | `ground_target_hit_3.raw` | Random destructive hit variant for ground targets, enemy ships, land and town blocks. |
| `ground_hit_3.wav` | `ground_target_hit_4.raw` | Random destructive hit variant for ground targets, enemy ships, land and town blocks. |
| `idle_0.wav` | `carrier_idle_1.adpcm` | Random carrier-deck ambience while the Harrier has not taken off and no MOD music is playing. |
| `idle_1.wav` | `carrier_idle_2.adpcm` | Random carrier-deck ambience under the same conditions. |

Ground-target hits use `startWorldImpactQuiet()` for the visual explosion and
play one `ground_target_hit_*` sample. This deliberately avoids also playing
`impact.raw` for the same hit.

These four masters receive a mild low-shelf boost during Paula conversion.
That preserves their bass body after peak normalization to signed 8-bit,
11025 Hz data without changing playback pitch.

Carrier ambience alternates the two idle variants. A new cue starts after a
random 15-20 second delay, producing
approximately 3-4 cues per minute while the aircraft remains on the carrier.
Each cue fades in and out over about half a second. Taking off, pausing, leaving
gameplay, reaching Game Over, or starting MOD music triggers an early fade-out.

The two long idle masters use four-bit IMA ADPCM in the executable. They are
decoded on demand into one shared 44,100-byte chip-RAM buffer while the Harrier
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
