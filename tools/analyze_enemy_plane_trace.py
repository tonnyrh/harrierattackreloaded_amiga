#!/usr/bin/env python3
"""Summarise diagnostic enemy_plane_log.csv files from headless A500 runs."""

from __future__ import annotations

import argparse
import csv
import glob
import statistics
from pathlib import Path


EVENT_NAMES = {1: "spawn", 2: "step", 3: "fire", 4: "blocked", 5: "despawn"}


def as_int(row: dict[str, str], key: str) -> int:
    return int(row.get(key, "0") or 0)


def summarise(path: Path) -> dict[str, object]:
    with path.open(newline="", encoding="ascii") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or not {"rate", "event", "visualX"}.issubset(reader.fieldnames):
            raise ValueError(f"not an enemy-plane trace: {path}")
        rows = list(reader)
    if not rows:
        raise ValueError(f"empty trace: {path}")

    events = [as_int(row, "event") for row in rows]
    decision_rows = [row for row in rows if as_int(row, "event") in (2, 3, 4)]
    intervals = [as_int(row, "framesSinceTick") for row in decision_rows
                 if as_int(row, "framesSinceTick") > 0]
    lag_x = [abs(as_int(row, "lagX")) for row in rows]
    lag_y = [abs(as_int(row, "lagY")) for row in rows]
    return {
        "file": path.name,
        "rate": as_int(rows[0], "rate"),
        "rows": len(rows),
        "spawns": events.count(1),
        "steps": events.count(2),
        "fires": events.count(3),
        "blocked": events.count(4),
        "despawns": events.count(5),
        "mean_tick": statistics.fmean(intervals) if intervals else 0.0,
        "max_lag_x": max(lag_x),
        "max_lag_y": max(lag_y),
        "mean_lag_x": statistics.fmean(lag_x),
        "mean_lag_y": statistics.fmean(lag_y),
        "min_y": min(as_int(row, "visualY") for row in rows),
        "max_y": max(as_int(row, "visualY") for row in rows),
        "dropped": as_int(rows[-1], "dropped"),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "patterns", nargs="+",
        help="CSV paths or glob patterns (quote wildcards in PowerShell)")
    args = parser.parse_args()

    paths: list[Path] = []
    for pattern in args.patterns:
        matches = [Path(item) for item in glob.glob(pattern)]
        paths.extend(matches or [Path(pattern)])

    print("rate rows spawn step fire block exit tick  lagX lagY  y-range drop file")
    results = []
    for path in paths:
        try:
            results.append(summarise(path))
        except ValueError as error:
            if "not an enemy-plane trace" not in str(error):
                raise
    for result in sorted(results,
                         key=lambda item: (item["rate"], item["file"])):
        print(
            f"{result['rate']:>4} {result['rows']:>4} {result['spawns']:>5} "
            f"{result['steps']:>4} {result['fires']:>4} {result['blocked']:>5} "
            f"{result['despawns']:>4} {result['mean_tick']:>4.1f} "
            f"{result['max_lag_x']:>5} {result['max_lag_y']:>4} "
            f"{result['min_y']:>3}..{result['max_y']:<3} "
            f"{result['dropped']:>4} {result['file']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
