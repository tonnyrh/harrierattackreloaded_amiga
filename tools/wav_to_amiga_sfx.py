#!/usr/bin/env python3
from __future__ import annotations

import argparse
import audioop
import wave
from pathlib import Path


PAL_COLOR_CLOCK_HZ = 3_546_895


def read_wav(path: Path) -> tuple[bytes, int, int]:
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        sample_width = wav.getsampwidth()
        sample_rate = wav.getframerate()
        frames = wav.readframes(wav.getnframes())

    if channels > 1:
        frames = audioop.tomono(frames, sample_width, 0.5, 0.5)

    if sample_width != 2:
        frames = audioop.lin2lin(frames, sample_width, 2)

    return frames, sample_rate, 2


def resample(frames: bytes, source_rate: int, target_rate: int, sample_width: int) -> bytes:
    if source_rate == target_rate:
        return frames
    converted, _ = audioop.ratecv(frames, sample_width, 1, source_rate, target_rate, None)
    return converted


def trim_silence(frames: bytes, threshold: int, sample_width: int) -> bytes:
    sample_count = len(frames) // sample_width
    if sample_count == 0:
        return frames

    first = 0
    last = sample_count - 1
    while first < sample_count and abs(audioop.getsample(frames, sample_width, first)) < threshold:
        first += 1
    while last > first and abs(audioop.getsample(frames, sample_width, last)) < threshold:
        last -= 1

    start = max(0, first * sample_width)
    end = min(len(frames), (last + 1) * sample_width)
    return frames[start:end]


def normalize(frames: bytes, target_peak: int, sample_width: int) -> bytes:
    peak = audioop.max(frames, sample_width)
    if peak <= 0:
        return frames
    return audioop.mul(frames, sample_width, target_peak / peak)


def signed_16_to_signed_8(frames: bytes) -> bytes:
    converted = audioop.lin2lin(frames, 2, 1)
    if len(converted) & 1:
        converted += b"\x00"
    return converted


def convert(input_path: Path, output_path: Path, target_rate: int, max_ms: int) -> int:
    frames, source_rate, sample_width = read_wav(input_path)
    frames = resample(frames, source_rate, target_rate, sample_width)
    frames = trim_silence(frames, threshold=384, sample_width=sample_width)
    frames = normalize(frames, target_peak=26_000, sample_width=sample_width)

    max_bytes = (target_rate * max_ms // 1000) * sample_width
    frames = frames[:max_bytes]
    raw = signed_16_to_signed_8(frames)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(raw)
    return len(raw)


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert WAV to Amiga Paula signed 8-bit mono raw sample.")
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--rate", type=int, default=11025)
    parser.add_argument("--max-ms", type=int, default=700)
    args = parser.parse_args()

    length = convert(args.input, args.output, args.rate, args.max_ms)
    period = round(PAL_COLOR_CLOCK_HZ / args.rate)
    print(f"Wrote {args.output} ({length} bytes, {args.rate} Hz, Paula period {period})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
