# CPC music export

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

Regenerate it from the assembler source at the repository root:

```powershell
python tools/cpc_music_to_mod.py
```

Generate the separately arranged four-voice version:

```powershell
python tools/cpc_music_to_mod.py --four-channel-arrangement `
  --output amiga/assets/music/harrier_menu_fixed.mod
```

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

The converter reads the active `HARRIERATTACK` score from
`CPSoundEffectGenerator2.asm`, maps its three note columns to the first three
MOD channels, preserves normal/half-beat timing, and validates the resulting
module structure.
