"""Exhaustively compare the stored batch tables with scalar engine synthesis."""
from pathlib import Path
import re


def main():
    source = (Path(__file__).resolve().parents[1] / "amiga/assets/engine_noise_tables.h").read_text()
    tables = {}
    for name, size in (("engineWhite4", 256), ("engineRumble4", 128), ("engineFeedback4", 512)):
        body = re.search(rf"\b{name}\[{size}\]\s*=\s*\{{(.*?)\}};", source, re.S)
        if body is None:
            raise AssertionError(f"Missing table: {name}")
        tables[name] = [int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]+)", body[1])]
        assert len(tables[name]) == size, name

    for initial in range(65536):
        for speed in range(5):
            state, expected = initial, 0
            for _ in range(4):
                bit = (state ^ (state >> 2) ^ (state >> 3) ^ (state >> 5)) & 1
                state = (state >> 1) | (bit << 15)
                sample = (((state & 31) - 16 + ((state >> 8) & 15) - 8 + speed * 2) * 2) & 255
                expected = (expected << 8) | sample

            packed = tables["engineWhite4"][(initial >> 1) & 255] + tables["engineRumble4"][initial >> 9]
            actual = (((packed | 0x80808080) - (48 - speed * 4) * 0x01010101) ^ 0x80808080) & 0xffffffff
            end = (initial >> 4) | tables["engineFeedback4"][initial & 511]
            assert actual == expected and end == state, (initial, speed, actual, expected, end, state)
    print("PASS: 65536 states x 5 speeds; four samples and final LFSR state match")


if __name__ == "__main__":
    main()
