# Ground bounce sound
Generated locally with AudioGen audiogen-medium, 2026-09-19.
Prompt: A single short cartoon spring boing, elastic metallic twang bouncing upward then dropping in pitch, clean isolated arcade sound effect, no voices, no music
Source: ground_boing_source.wav, mono 16-bit 16000 Hz, 1 second.
Runtime: ground_boing.raw, signed 8-bit mono 11025 Hz, capped at 800 ms,
DC removed, antialias filtered, normalized and edge faded by tools/wav_to_amiga_sfx.py.
Embedded in chip RAM; Paula DMA playback, no runtime audio generation.
