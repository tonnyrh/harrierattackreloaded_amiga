# Harrier Amiga SFX

These runtime samples are AudioGen-generated WAV candidates converted to Amiga
Paula-friendly raw assets:

- mono
- signed 8-bit PCM
- 11025 Hz
- even byte length
- embedded into chip RAM with `#embed`

The selected WAV candidates were generated outside this repository under the
user temp directory, then converted with:

```powershell
python .\tools\wav_to_amiga_sfx.py <input.wav> amiga\assets\sfx\<name>.raw --rate 11025 --max-ms <duration>
```

Prompts used for Sprint 10.2:

- `menu.raw`: very short soft retro arcade menu click, clean, no music, no voice
- `fire.raw`: single close rocket launch, short dry ignition pop followed by a smooth airy exhaust hiss fading out, no explosion, laser, music or voice
- `bomb.raw`: short falling bomb whistle then soft thump, retro arcade game sound, no music, no voice
- `impact.raw`: short small explosion blast, crunchy retro arcade impact, no music, no voice
- `hit.raw`: short metallic hit alarm zap, player damage, retro arcade game sound, no music, no voice
- `gameover.raw`: short retro arcade game over failure sting, low dramatic synth hit, no voice
