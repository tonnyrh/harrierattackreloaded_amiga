#!/usr/bin/env python3
"""Convert Harrier Attack Reloaded's CPC AY score to a ProTracker MOD."""

from __future__ import annotations

import argparse
import math
import re
import struct
from dataclasses import dataclass
from pathlib import Path


NORMAL_TIMING = (4, 38)  # 4 * 2.5 / 38 = 263 ms; CPC is 13 / 50 = 260 ms
HALF_TIMING = (2, 42)    # 2 * 2.5 / 42 = 119 ms; CPC is 6 / 50 = 120 ms
ROWS_PER_PATTERN = 64
CHANNELS = 4


@dataclass(frozen=True)
class ScoreRow:
    notes: tuple[str, str, str]
    timing: tuple[int, int] | None = None


def strip_comment(line: str) -> str:
    return line.split(";", 1)[0].strip()


def parse_equ_periods(source: str) -> dict[str, int]:
    periods: dict[str, int] = {}
    for raw in source.splitlines():
        line = strip_comment(raw)
        match = re.fullmatch(r"([A-Za-z][A-Za-z0-9]*)\s+equ\s+(\d+)", line, re.I)
        if match:
            periods[match.group(1)] = int(match.group(2))
    if not periods:
        raise ValueError("No note EQU definitions found")
    return periods


def active_harrier_score(source: str) -> str:
    """Return the final uncommented musicscore block (the HARRIERATTACK score)."""
    starts = list(re.finditer(r"(?im)^\s*musicscore:\s*(?:;.*)?$", source))
    if not starts:
        raise ValueError("No active musicscore label found")
    start = starts[-1].end()
    end_match = re.search(r"(?im)^\s*endif\s*(?:;.*)?$", source[start:])
    if not end_match:
        raise ValueError("Could not find end of HARRIERATTACK musicscore")
    return source[start : start + end_match.start()]


def parse_score(block: str, known_notes: dict[str, int]) -> list[ScoreRow]:
    rows: list[ScoreRow] = []
    pending_timing: tuple[int, int] | None = NORMAL_TIMING

    for line_number, raw in enumerate(block.splitlines(), 1):
        line = strip_comment(raw)
        if not line:
            continue

        words = re.fullmatch(r"defw\s+(.+)", line, re.I)
        if words:
            values = tuple(part.strip() for part in words.group(1).split(","))
            if len(values) != 3:
                raise ValueError(f"Score line {line_number}: expected three channels")
            for value in values:
                if value != "0" and value not in known_notes:
                    raise ValueError(f"Score line {line_number}: unknown note {value!r}")
            rows.append(ScoreRow(values, pending_timing))
            pending_timing = None
            continue

        bytes_match = re.fullmatch(r"defb\s+(.+)", line, re.I)
        if bytes_match:
            values = [
                int(part.strip(), 0)
                for part in bytes_match.group(1).split(",")
                if part.strip()
            ]
            command = values[0]
            if command == 1:
                break
            if command == 2:
                rows.append(ScoreRow(("0", "0", "0"), pending_timing))
                pending_timing = None
            elif command == 3:
                pending_timing = HALF_TIMING
            elif command == 4:
                pending_timing = NORMAL_TIMING
            else:
                raise ValueError(f"Score line {line_number}: unsupported command {command}")
            continue

        raise ValueError(f"Score line {line_number}: unsupported syntax: {line!r}")

    if not rows:
        raise ValueError("The selected score contains no rows")
    return rows


def ay_period_to_mod_period(ay_period: int, waveform_samples: int = 32) -> int:
    """Convert AY pitch to Paula period for a waveform of waveform_samples samples."""
    # CPC AY: 1 MHz / (16 * period). PAL Paula: 7,093,789.2 / (2 * period).
    period = round(7_093_789.2 * 16 * ay_period / (2 * 1_000_000 * waveform_samples))
    return max(1, min(4095, period))


def make_chip_sample(pulse_width: float, harmonics: float) -> bytes:
    """Make a signed 8-bit, non-looped chip waveform with a short decay."""
    cycle = 32
    length = 4096
    output = bytearray()
    for index in range(length):
        phase = (index % cycle) / cycle
        square = 1.0 if phase < pulse_width else -1.0
        overtone = math.sin(phase * math.tau * 2.0) * harmonics
        envelope = math.exp(-5.2 * index / length)
        value = round(max(-1.0, min(1.0, square + overtone)) * 112 * envelope)
        output.append(value & 0xFF)
    return bytes(output)


def sample_header(name: str, sample: bytes, volume: int = 64) -> bytes:
    encoded_name = name.encode("ascii")[:22].ljust(22, b"\0")
    return encoded_name + struct.pack(">HBBHH", len(sample) // 2, 0, volume, 0, 1)


def encode_event(sample_number: int, period: int, effect: int = 0, param: int = 0) -> bytes:
    return bytes(
        (
            (sample_number & 0xF0) | ((period >> 8) & 0x0F),
            period & 0xFF,
            ((sample_number & 0x0F) << 4) | (effect & 0x0F),
            param & 0xFF,
        )
    )


def lower_octave(note: str, note_periods: dict[str, int]) -> str:
    if note == "0":
        return "0"
    octave_names = "mnopqrst"
    octave = octave_names.find(note[0])
    if octave <= 0:
        return note
    lowered = octave_names[octave - 1] + note[1:]
    return lowered if lowered in note_periods else note


def add_fourth_voice(rows: list[ScoreRow], note_periods: dict[str, int]) -> list[ScoreRow]:
    """Create a restrained bass voice from channel A, retriggered on its notes."""
    arranged: list[ScoreRow] = []
    for row in rows:
        bass = lower_octave(row.notes[0], note_periods)
        arranged.append(ScoreRow(row.notes + (bass,), row.timing))
    return arranged


def build_mod(
    rows: list[ScoreRow],
    note_periods: dict[str, int],
    title: str,
    four_channel_arrangement: bool = False,
) -> bytes:
    samples = (
        make_chip_sample(0.50, 0.00),
        make_chip_sample(0.25, 0.18),
        make_chip_sample(0.375, -0.12),
        make_chip_sample(0.50, -0.28),
    )
    if not four_channel_arrangement:
        samples = samples[:3]
    pattern_count = math.ceil(len(rows) / ROWS_PER_PATTERN)
    if pattern_count > 128:
        raise ValueError("Score exceeds the 128-position MOD order table")

    data = bytearray(title.encode("ascii", "replace")[:20].ljust(20, b"\0"))
    for index in range(31):
        if index < len(samples):
            data.extend(sample_header(f"CPC AY channel {chr(65 + index)}", samples[index]))
        else:
            data.extend(sample_header("", b""))

    data.extend(bytes((pattern_count, 0)))
    data.extend(bytes(range(pattern_count)).ljust(128, b"\0"))
    data.extend(b"M.K.")

    for pattern_index in range(pattern_count):
        for pattern_row in range(ROWS_PER_PATTERN):
            row_index = pattern_index * ROWS_PER_PATTERN + pattern_row
            empty_notes = ("0", "0", "0", "0") if four_channel_arrangement else ("0", "0", "0")
            row = rows[row_index] if row_index < len(rows) else ScoreRow(empty_notes)
            music_channels = 4 if four_channel_arrangement else 3
            for channel in range(music_channels):
                note = row.notes[channel]
                period = 0 if note == "0" else ay_period_to_mod_period(note_periods[note])
                sample_number = 0 if note == "0" else channel + 1
                # A timing change needs both ProTracker speed and BPM effects.
                # Put BPM on channel 3; an event may carry a note and an effect.
                if channel == 2 and row.timing is not None:
                    data.extend(encode_event(sample_number, period, 0xF, row.timing[1]))
                elif channel == 3 and row_index == len(rows):
                    data.extend(encode_event(0, 0, 0xD, 0))
                elif channel == 3 and row.timing is not None:
                    data.extend(encode_event(sample_number, period, 0xF, row.timing[0]))
                else:
                    data.extend(encode_event(sample_number, period))
            if four_channel_arrangement:
                continue
            if row_index == len(rows):
                # End the partially filled final pattern instead of playing its
                # padding rows. Advancing past the order list loops to restart 0.
                data.extend(encode_event(0, 0, 0xD, 0))
            elif row.timing is not None:
                data.extend(encode_event(0, 0, 0xF, row.timing[0]))
            else:
                data.extend(encode_event(0, 0))

    for sample in samples:
        data.extend(sample)
    return bytes(data)


def validate_mod(data: bytes, expected_patterns: int) -> None:
    if data[1080:1084] != b"M.K.":
        raise ValueError("Output is missing the M.K. ProTracker signature")
    song_length = data[950]
    if song_length != expected_patterns:
        raise ValueError("MOD order length does not match generated pattern count")
    pattern_bytes = expected_patterns * ROWS_PER_PATTERN * CHANNELS * 4
    sample_start = 1084 + pattern_bytes
    declared_sample_bytes = sum(
        struct.unpack(">H", data[20 + index * 30 + 22 : 20 + index * 30 + 24])[0] * 2
        for index in range(31)
    )
    if len(data) != sample_start + declared_sample_bytes:
        raise ValueError("MOD size does not match its pattern and sample headers")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("CPSoundEffectGenerator2.asm"),
        help="CPC sound/music assembler source",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("amiga/assets/music/harrier_menu.mod"),
        help="ProTracker MOD output",
    )
    parser.add_argument(
        "--four-channel-arrangement",
        action="store_true",
        help="Add an octave-down fourth voice derived from AY channel A",
    )
    args = parser.parse_args()

    source = args.input.read_text(encoding="utf-8-sig")
    periods = parse_equ_periods(source)
    rows = parse_score(active_harrier_score(source), periods)
    if args.four_channel_arrangement:
        rows = add_fourth_voice(rows, periods)
    mod = build_mod(
        rows,
        periods,
        "Harrier Menu Fixed" if args.four_channel_arrangement else "Harrier Attack Menu",
        args.four_channel_arrangement,
    )
    pattern_count = math.ceil(len(rows) / ROWS_PER_PATTERN)
    validate_mod(mod, pattern_count)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(mod)
    current_timing = NORMAL_TIMING
    duration = 0.0
    for row in rows:
        if row.timing is not None:
            current_timing = row.timing
        duration += current_timing[0] * 2.5 / current_timing[1]
    print(
        f"Wrote {args.output}: {len(rows)} rows, {pattern_count} patterns, "
        f"{duration:.1f}s/loop, "
        f"{'4 arranged channels' if args.four_channel_arrangement else '3 used channels + 1 free'}"
    )


if __name__ == "__main__":
    main()
