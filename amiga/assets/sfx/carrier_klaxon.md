# Carrier klaxon

Generated locally with AudioGen medium, 2026-09-16. Selected candidate 0.
Prompt: "Naval ship general quarters alarm, a loud vintage mechanical klaxon siren, urgent rising and falling awooga tone, dry isolated clean recording, no voices no music no sea noise"

Generation: `generate_sfx.py <prompt> --duration 2.0 --n 2`.
The selected unmodified 16 kHz mono source is `carrier_klaxon_source.wav`.

Conversion: `python tools/wav_to_amiga_sfx.py amiga/assets/sfx/carrier_klaxon_source.wav <new-output.raw> --rate 6000 --max-ms 1400 --preview-wav <new-preview.wav>`.
The converter removes DC, anti-alias filters, resamples, normalizes and fades
the edges. The runtime is 8,400 bytes of even-length signed 8-bit mono PCM,
Paula period 591, 211-frame channel lifetime (about three passes / 4.2 seconds). No runtime synthesis or decode.
The alarm loops at the start of each 150-tick scramble/lull, stopped by its
physical-frame timer even after the first attackers arrive.
Normal radar alarms keep their existing waveform.

Playback correction: normalize the converted signed PCM to peak 120/127
(the initial converter output peaked at only 19). Runtime volume 64 before
the normal mix attenuation, player priority, and retry until accepted.
