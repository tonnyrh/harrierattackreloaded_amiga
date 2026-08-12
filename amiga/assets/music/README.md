# CPC music export

`raf_game_over.mod` is the four-channel Game Over song. The shared MOD
replayer runs it once at 120% tempo when the last life is lost. It does not
loop, and also stops before Retry or return to the main menu.

`carrier_landing_fanfare.mod` is the successful carrier-landing fanfare. The
shared MOD replayer starts it once, at normal tempo, when landing is confirmed
on the carrier deck. It does not loop.

Both one-shot fanfares use all four Paula channels: lead brass, low brass,
snare/final harmony, and cymbal/final harmony. Their percussion samples are
one-shot MOD instruments, while the brass instruments use their declared
sustain loops; the shared replayer preserves that distinction.

`harrier_menu.mod` is a standard 4-channel, 31-instrument ProTracker MOD
(`M.K.`). It contains the CPC menu tune, **I Vow to Thee, My Country**.

- MOD channels 1-3 correspond to AY channels A-C.
- MOD channel 4 contains occasional speed commands and is otherwise free for
  arranging. BPM commands share channel 3 rows because ProTracker needs separate
  effects for speed and BPM when the CPC score changes cadence.
- The three generated samples approximate the original AY chip voices.
- The module loops from the beginning.

The file can be opened in common Amiga music tools such as ProTracker,
OctaMED, MilkyTracker and OpenMPT. Keeping it as a normal MOD also makes it
suitable for common Amiga replay routines such as ptplayer.

`harrier_menu_fixed.mod` adds a fourth, octave-down supporting voice derived
from AY channel A and gives it a separate bass chip sample.

## Mutopia four-voice arrangement

The current `harrier_menu_fixed.mod` is generated from Mutopia's public-domain
`Thaxted.mid` instead of the CPC three-channel score. The upper staff is split
into soprano/alto and the lower staff into tenor/bass. Its 3/4 metre and
sixteenth-note rhythm are retained, while playback is scaled to approximately
the 38.9-second cadence of the CPC menu version.

```powershell
python tools/mutopia_thaxted_to_mod.py
```

Source: Mutopia project, `Thaxted`, G. T. Holst, typeset by Peter Chubb,
Mutopia-2005/01/18-527, public domain.

The historical three-channel score is retained only as provenance. Original
CPC assembler and its converter are deliberately not part of this standalone
Amiga repository.
