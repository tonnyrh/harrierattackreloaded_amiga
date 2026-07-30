# Harrier Amiga SFX

See `../AUDIO.md` for the canonical source-to-runtime mapping and a description
of every sound's gameplay use.

These runtime samples are WAV masters converted to Paula-friendly assets.
Short gameplay effects use direct PCM; long carrier ambience uses IMA ADPCM
and is decoded before Paula playback:

- mono
- signed 8-bit PCM
- 11025 Hz
- even byte length
- direct PCM is embedded into chip RAM with `#embed`
- ADPCM is embedded in ordinary memory and decoded to a shared chip buffer

The gameplay WAV masters live under `amiga/assets/sfx-sourcefiles`. They are
versioned in Git and are the user-editable source of truth used by every Amiga
build. Replace a WAV there with a preferred version, press F5, and the build
converts it automatically. The `.raw` files in this directory are derived
runtime assets.

Run the pipeline directly with:

```powershell
.\prepare-amiga-sfx.ps1
```

It generates only missing WAV masters through AudioGen. It never passes
AudioGen's `--overwrite` switch, so an existing user-selected WAV cannot be
silently replaced. Missing, changed, or stale runtime samples are converted
with:

```powershell
python .\tools\wav_to_amiga_sfx.py <input.wav> amiga\assets\sfx\<name>.raw --rate 11025 --max-ms <duration>
```

The converter removes DC offset, applies a Blackman-windowed low-pass filter
before downsampling, normalizes the result and adds short edge fades. The
low-pass stage is important: simple interpolation without anti-alias filtering
turns high-frequency AudioGen content into rasp/noise at Paula's lower sample
rate.

Use `--preview-wav <path>` to write a PC-playable WAV reconstructed from the
exact signed 8-bit bytes sent to Paula. If that preview is clean but WinUAE is
not, the remaining fault is in DMA playback rather than conversion.

Prompts used for Sprint 10.2:

- `fire.raw`: single close rocket launch, short dry ignition pop followed by a smooth airy exhaust hiss fading out, no explosion, laser, music or voice
- `bomb.raw`: short falling bomb whistle then soft thump, retro arcade game sound, no music, no voice
- `impact.raw`: short small explosion blast, crunchy retro arcade impact, no music, no voice
- `hit.raw`: short metallic hit alarm zap, player damage, retro arcade game sound, no music, no voice

Additional user-created variants integrated in Sprint 15.23:

- `flak_gun_1.raw` and `flak_gun_2.raw`: selected when a runtime flak
  emplacement fires, with distance-based volume and subtle pitch variation.
- `ground_target_hit_1.raw` ... `ground_target_hit_4.raw`: randomly selected
  for destructive hits on ground targets, buildings, land and enemy ships.

Their source WAV files are kept in the repository-local `.tmp` working folder.
Runtime copies use 11025 Hz and at most 700 ms each to limit chip-RAM use on a
1 MiB Amiga 500.

Sprint 15.25 regenerated the gameplay bank under
`.tmp/audiogen_sprint` using short, dry, isolated prompts designed to survive
8-bit/11025 Hz conversion cleanly:

- rocket: ignition snap plus compact exhaust
- bomb: mechanical release plus short descending air whistle
- impact: compact explosion and debris thump
- player hit: metallic crack and warning zap
- four anti-aircraft cannon shots
- four metal/concrete ground-target impacts

Exact-data WAV auditions produced from the final signed Paula bytes are under
`.tmp/amiga-sfx-preview`. They are disposable build output and do not need to
be versioned. Audition these to hear the exact data that Paula receives,
rather than the higher-quality source WAV.

Game Over uses `assets/music/raf_game_over.mod`, not a one-shot RAW sample.
Menu navigation is deliberately silent so the four-channel menu MOD remains
uninterrupted.

`carrier_idle_1.adpcm` and `carrier_idle_2.adpcm` are derived from the versioned
`idle_0.wav` and `idle_1.wav` masters. They alternate roughly every
15-20 seconds while the Harrier is on its carrier and no MOD is playing. Paula
volume provides a short fade at both ends; these low-priority sounds yield to
important gameplay effects. The four-bit IMA ADPCM data lives outside chip
RAM and is decoded on demand into one shared Paula DMA buffer.
