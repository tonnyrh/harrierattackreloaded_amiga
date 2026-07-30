#!/usr/bin/env python3
from __future__ import annotations

import argparse
import audioop
import math
import struct
import wave
from array import array
from pathlib import Path


PAL_COLOR_CLOCK_HZ = 3_546_895
DEFAULT_FILTER_TAPS = 63


def read_wav(path: Path) -> tuple[bytes, int, int, int]:
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        sample_width = wav.getsampwidth()
        sample_rate = wav.getframerate()
        frames = wav.readframes(wav.getnframes())

    # WAV stores 8-bit PCM as unsigned, whereas audioop treats width=1 as
    # signed. Centre it before any conversion or mono mixing.
    if sample_width == 1:
        frames = audioop.bias(frames, 1, -128)

    if channels > 1:
        if channels == 2:
            frames = audioop.tomono(frames, sample_width, 0.5, 0.5)
        else:
            raise ValueError(f"{path}: only mono or stereo WAV is supported, got {channels} channels")

    if sample_width != 2:
        frames = audioop.lin2lin(frames, sample_width, 2)

    return frames, sample_rate, 2, channels


def remove_dc(frames: bytes) -> bytes:
    samples = array("h")
    samples.frombytes(frames)
    if not samples:
        return frames
    dc = round(sum(samples) / len(samples))
    if dc == 0:
        return frames
    for index, value in enumerate(samples):
        samples[index] = max(-32768, min(32767, value - dc))
    return samples.tobytes()


def low_pass_for_downsampling(
    frames: bytes,
    source_rate: int,
    target_rate: int,
    taps: int = DEFAULT_FILTER_TAPS,
) -> bytes:
    """Apply a windowed-sinc anti-alias filter before reducing sample rate.

    audioop.ratecv performs interpolation but no sufficiently steep low-pass
    filter. AudioGen effects often contain lots of energy above Paula's new
    Nyquist limit; without this stage it folds down into audible rasp/noise.
    """
    if target_rate >= source_rate:
        return frames
    if taps < 7:
        return frames
    if taps % 2 == 0:
        taps += 1

    samples = array("h")
    samples.frombytes(frames)
    if len(samples) < taps:
        return frames

    # Leave transition-band headroom below target Nyquist.
    cutoff_hz = target_rate * 0.45
    cutoff = cutoff_hz / source_rate
    middle = taps // 2
    kernel: list[float] = []
    for index in range(taps):
        offset = index - middle
        sinc = 2.0 * cutoff if offset == 0 else math.sin(
            2.0 * math.pi * cutoff * offset
        ) / (math.pi * offset)
        # Blackman window gives strong rejection without external packages.
        window = (
            0.42
            - 0.5 * math.cos(2.0 * math.pi * index / (taps - 1))
            + 0.08 * math.cos(4.0 * math.pi * index / (taps - 1))
        )
        kernel.append(sinc * window)
    scale = sum(kernel)
    kernel = [coefficient / scale for coefficient in kernel]

    padded = [samples[0]] * middle + list(samples) + [samples[-1]] * middle
    filtered = array("h")
    for position in range(len(samples)):
        value = sum(
            padded[position + tap] * kernel[tap] for tap in range(taps)
        )
        filtered.append(max(-32768, min(32767, round(value))))
    return filtered.tobytes()


def resample(frames: bytes, source_rate: int, target_rate: int, sample_width: int) -> bytes:
    if source_rate == target_rate:
        return frames
    converted, _ = audioop.ratecv(frames, sample_width, 1, source_rate, target_rate, None)
    return converted


def boost_bass(
    frames: bytes,
    sample_rate: int,
    amount: float,
    cutoff_hz: float = 520.0,
) -> bytes:
    """Restore low-frequency body before peak-normalizing to Paula's 8 bits."""
    if amount <= 0.0 or not frames:
        return frames

    samples = array("h")
    samples.frombytes(frames)
    alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff_hz / sample_rate)
    low = float(samples[0])
    values: list[float] = []
    for sample in samples:
        low += alpha * (sample - low)
        values.append(sample + amount * low)
    peak = max(abs(value) for value in values)
    scale = 32760.0 / peak if peak > 32760.0 else 1.0
    boosted = array("h", (round(value * scale) for value in values))
    return boosted.tobytes()


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


def apply_edge_fades(frames: bytes, sample_rate: int, fade_ms: float = 3.0) -> bytes:
    samples = array("h")
    samples.frombytes(frames)
    fade_samples = min(len(samples) // 2, round(sample_rate * fade_ms / 1000.0))
    if fade_samples <= 0:
        return frames
    for index in range(fade_samples):
        gain = index / fade_samples
        samples[index] = round(samples[index] * gain)
        samples[-1 - index] = round(samples[-1 - index] * gain)
    return samples.tobytes()


def signed_16_to_signed_8(frames: bytes) -> bytes:
    converted = audioop.lin2lin(frames, 2, 1)
    if len(converted) & 1:
        converted += b"\x00"
    return converted


def write_preview_wav(path: Path, signed_raw: bytes, sample_rate: int) -> None:
    # WAV 8-bit PCM is unsigned; Paula raw samples are signed.
    unsigned_raw = bytes((value + 128) & 0xFF for value in signed_raw)
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as preview:
        preview.setnchannels(1)
        preview.setsampwidth(1)
        preview.setframerate(sample_rate)
        preview.writeframes(unsigned_raw)


def encode_ima_adpcm(signed_raw: bytes) -> bytes:
    """Pack Paula PCM as IMA ADPCM with a tiny platform-neutral header.

    Header: four-byte big-endian decoded length. The stream starts with the
    standard IMA state (predictor=0, step-index=0), so the Amiga decoder needs
    no per-file state fields.
    """
    pcm16 = audioop.lin2lin(signed_raw, 1, 2)
    packed, _ = audioop.lin2adpcm(pcm16, 2, (0, 0))
    return struct.pack(">I", len(signed_raw)) + packed


def convert(
    input_path: Path,
    output_path: Path,
    target_rate: int,
    max_ms: int,
    bass_boost: float = 0.0,
    preview_wav: Path | None = None,
    adpcm: bool = False,
) -> tuple[int, int, int]:
    frames, source_rate, sample_width, source_channels = read_wav(input_path)
    frames = remove_dc(frames)
    frames = low_pass_for_downsampling(frames, source_rate, target_rate)
    frames = resample(frames, source_rate, target_rate, sample_width)
    frames = trim_silence(frames, threshold=384, sample_width=sample_width)
    frames = boost_bass(frames, target_rate, bass_boost)
    frames = normalize(frames, target_peak=26_000, sample_width=sample_width)

    max_bytes = (target_rate * max_ms // 1000) * sample_width
    frames = frames[:max_bytes]
    frames = apply_edge_fades(frames, target_rate)
    raw = signed_16_to_signed_8(frames)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    runtime_data = encode_ima_adpcm(raw) if adpcm else raw
    output_path.write_bytes(runtime_data)
    if preview_wav:
        preview_raw = raw
        if adpcm:
            decoded_16, _ = audioop.adpcm2lin(
                runtime_data[4:], 2, (0, 0)
            )
            preview_raw = audioop.lin2lin(decoded_16, 2, 1)[:len(raw)]
        write_preview_wav(preview_wav, preview_raw, target_rate)
    return len(raw), source_rate, source_channels


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert WAV to Amiga Paula signed 8-bit mono raw sample.")
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--rate", type=int, default=11025)
    parser.add_argument("--max-ms", type=int, default=700)
    parser.add_argument(
        "--bass-boost",
        type=float,
        default=0.0,
        help="Low-shelf amount before normalization (1.0 is about +6 dB at low frequencies).",
    )
    parser.add_argument(
        "--preview-wav",
        type=Path,
        help="Write an audible WAV containing the exact signed 8-bit Paula data.",
    )
    parser.add_argument(
        "--adpcm",
        action="store_true",
        help="Store four-bit IMA ADPCM with a decoded-length header.",
    )
    args = parser.parse_args()

    length, source_rate, source_channels = convert(
        args.input,
        args.output,
        args.rate,
        args.max_ms,
        bass_boost=args.bass_boost,
        preview_wav=args.preview_wav,
        adpcm=args.adpcm,
    )
    period = round(PAL_COLOR_CLOCK_HZ / args.rate)
    duration_ms = round(length * 1000 / args.rate)
    print(
        f"Wrote {args.output} ({args.output.stat().st_size} stored bytes, "
        f"{length} decoded bytes, {duration_ms} ms, "
        f"{args.rate} Hz, Paula period {period}; source "
        f"{source_rate} Hz/{source_channels}ch)"
    )
    if args.preview_wav:
        print(f"Wrote exact-data preview {args.preview_wav}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
