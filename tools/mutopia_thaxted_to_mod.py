#!/usr/bin/env python3
"""Convert Mutopia's public-domain Thaxted MIDI to a four-voice Amiga MOD."""

from __future__ import annotations

import argparse
import math
import struct
from dataclasses import dataclass
from pathlib import Path


PAL_CLOCK = 7_093_789.2
ROWS_PER_PATTERN = 64
CHANNELS = 4
MOD_SPEED = 2
MOD_BPM = 47  # ~111 ms/row; 63 was "almost there", nudged down 25% from there per direct feedback.


@dataclass(frozen=True)
class Note:
    start: int
    end: int
    pitch: int


def read_vlq(data: bytes, pos: int) -> tuple[int, int]:
    value = 0
    while True:
        byte = data[pos]
        pos += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, pos


def parse_midi(path: Path) -> tuple[int, list[list[Note]], int]:
    data = path.read_bytes()
    if data[:4] != b"MThd":
        raise ValueError("Input is not a Standard MIDI file")
    header_size = struct.unpack(">I", data[4:8])[0]
    midi_format, track_count, division = struct.unpack(">HHH", data[8:14])
    if midi_format != 1 or division & 0x8000:
        raise ValueError("Expected a format-1 MIDI with PPQN timing")

    pos = 8 + header_size
    tracks: list[list[Note]] = []
    song_end = 0
    for _ in range(track_count):
        if data[pos : pos + 4] != b"MTrk":
            raise ValueError("Malformed MIDI track chunk")
        size = struct.unpack(">I", data[pos + 4 : pos + 8])[0]
        chunk = data[pos + 8 : pos + 8 + size]
        pos += 8 + size

        tick = 0
        cursor = 0
        running_status: int | None = None
        active: dict[tuple[int, int], list[int]] = {}
        notes: list[Note] = []
        while cursor < len(chunk):
            delta, cursor = read_vlq(chunk, cursor)
            tick += delta
            status = chunk[cursor]
            if status & 0x80:
                cursor += 1
                if status < 0xF0:
                    running_status = status
            elif running_status is not None:
                status = running_status
            else:
                raise ValueError("MIDI running status without prior status")

            if status == 0xFF:
                cursor += 1
                length, cursor = read_vlq(chunk, cursor)
                cursor += length
            elif status in (0xF0, 0xF7):
                length, cursor = read_vlq(chunk, cursor)
                cursor += length
            else:
                event = status & 0xF0
                channel = status & 0x0F
                pitch = chunk[cursor]
                cursor += 1
                if event in (0xC0, 0xD0):
                    continue
                value = chunk[cursor]
                cursor += 1
                key = (channel, pitch)
                if event == 0x90 and value:
                    active.setdefault(key, []).append(tick)
                elif event in (0x80, 0x90):
                    starts = active.get(key)
                    if starts:
                        notes.append(Note(starts.pop(0), tick, pitch))
        song_end = max(song_end, tick)
        tracks.append(notes)
    return division, tracks, song_end


def voice_at(notes: list[Note], tick: int, highest: bool) -> int | None:
    sounding = [note.pitch for note in notes if note.start <= tick < note.end]
    if not sounding:
        return None
    return max(sounding) if highest else min(sounding)


def split_four_voices(
    division: int, tracks: list[list[Note]], song_end: int
) -> list[tuple[int | None, ...]]:
    if len(tracks) < 3:
        raise ValueError("Mutopia Thaxted MIDI is missing its upper/lower staves")
    upper = tracks[1]
    lower = tracks[2]
    step = division // 4  # Sixteenth-note grid.
    if not step or division % 4:
        raise ValueError("MIDI PPQN cannot represent a clean sixteenth-note grid")

    rows = []
    for tick in range(0, song_end, step):
        rows.append(
            (
                voice_at(upper, tick, True),
                voice_at(upper, tick, False),
                voice_at(lower, tick, True),
                voice_at(lower, tick, False),
            )
        )
    return rows


def midi_pitch_to_mod_period(pitch: int, waveform_samples: int = 32) -> int:
    frequency = 440.0 * (2.0 ** ((pitch - 69) / 12.0))
    period = round(PAL_CLOCK / (2.0 * waveform_samples * frequency))
    return max(1, min(4095, period))


def make_loop_sample(pulse_width: float, overtone: float) -> bytes:
    output = bytearray()
    for index in range(32):
        phase = index / 32
        pulse = 1.0 if phase < pulse_width else -1.0
        value = pulse * 82 + math.sin(phase * math.tau * 2) * overtone
        output.append(round(max(-127, min(127, value))) & 0xFF)
    return bytes(output)


def sample_header(name: str, sample: bytes, volume: int) -> bytes:
    return (
        name.encode("ascii")[:22].ljust(22, b"\0")
        + struct.pack(">HBBHH", len(sample) // 2, 0, volume, 0, len(sample) // 2)
    )


def event(sample: int, period: int, effect: int = 0, param: int = 0) -> bytes:
    return bytes(
        (
            (sample & 0xF0) | ((period >> 8) & 0x0F),
            period & 0xFF,
            ((sample & 0x0F) << 4) | effect,
            param,
        )
    )


def build_mod(rows: list[tuple[int | None, ...]]) -> bytes:
    samples = (
        ("Soprano pulse", make_loop_sample(0.50, 8), 48),
        ("Alto soft pulse", make_loop_sample(0.375, -10), 38),
        ("Tenor hollow", make_loop_sample(0.25, 12), 40),
        ("Bass square", make_loop_sample(0.50, -18), 46),
    )
    pattern_count = math.ceil(len(rows) / ROWS_PER_PATTERN)
    if pattern_count > 128:
        raise ValueError("Arrangement is too long for the MOD order table")

    output = bytearray(b"Thaxted - Harrier".ljust(20, b"\0"))
    for index in range(31):
        if index < 4:
            name, sample, volume = samples[index]
            output.extend(sample_header(name, sample, volume))
        else:
            output.extend(sample_header("", b"", 0))
    output.extend(bytes((pattern_count, 0)))
    output.extend(bytes(range(pattern_count)).ljust(128, b"\0"))
    output.extend(b"M.K.")

    previous: list[int | None] = [None] * CHANNELS
    for pattern in range(pattern_count):
        for pattern_row in range(ROWS_PER_PATTERN):
            row_index = pattern * ROWS_PER_PATTERN + pattern_row
            pitches = rows[row_index] if row_index < len(rows) else (None,) * CHANNELS
            for channel, pitch in enumerate(pitches):
                sample = 0
                period = 0
                effect = 0
                param = 0
                if pitch != previous[channel]:
                    if pitch is None:
                        effect, param = 0xC, 0
                    else:
                        sample = channel + 1
                        period = midi_pitch_to_mod_period(pitch)
                    previous[channel] = pitch
                if row_index == 0 and channel == 2:
                    effect, param = 0xF, MOD_BPM
                elif row_index == 0 and channel == 3:
                    effect, param = 0xF, MOD_SPEED
                elif row_index == len(rows) and channel == 3:
                    effect, param = 0xD, 0
                output.extend(event(sample, period, effect, param))

    for _, sample, _ in samples:
        output.extend(sample)
    return bytes(output)


def validate_mod(data: bytes, pattern_count: int) -> None:
    if data[1080:1084] != b"M.K." or data[950] != pattern_count:
        raise ValueError("Invalid ProTracker header or order table")
    pattern_bytes = pattern_count * ROWS_PER_PATTERN * CHANNELS * 4
    sample_bytes = sum(
        struct.unpack(">H", data[42 + index * 30 : 44 + index * 30])[0] * 2
        for index in range(31)
    )
    if len(data) != 1084 + pattern_bytes + sample_bytes:
        raise ValueError("MOD length does not match declared patterns and samples")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=Path("tools/Thaxted.mid"))
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("amiga/assets/music/harrier_menu_fixed.mod"),
    )
    args = parser.parse_args()

    division, tracks, song_end = parse_midi(args.input)
    rows = split_four_voices(division, tracks, song_end)
    mod = build_mod(rows)
    patterns = math.ceil(len(rows) / ROWS_PER_PATTERN)
    validate_mod(mod, patterns)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(mod)
    duration = len(rows) * MOD_SPEED * 2.5 / MOD_BPM
    print(
        f"Wrote {args.output}: {len(rows)} sixteenth-note rows, {patterns} patterns, "
        f"4 voices, {duration:.2f}s/loop"
    )


if __name__ == "__main__":
    main()
