"""Measure host presentation cadence from PresentMon 2.x CSV, not game FPS.

Capture with --qpc_time_ms; do not use --exclude_dropped. Each swap chain is
analyzed separately. No pixel movement is inferred from a Present() event:
WinUAE may submit the same Amiga image twice. Combine with scroll traces and
pixel comparisons to diagnose the layer responsible for visible judder.
"""
import argparse
import csv
import json
import math
from collections import Counter, defaultdict
from pathlib import Path
from statistics import median


def number(row, key):
    try:
        value = float(row.get(key, ""))
        return value if math.isfinite(value) else None
    except (ValueError, TypeError):
        return None


def stats(values):
    if not values:
        return None
    ordered = sorted(values)
    return {"count": len(values), "min_ms": min(values),
            "median_ms": median(values),
            "p95_ms": ordered[math.ceil(len(values) * .95) - 1],
            "max_ms": max(values),
            "histogram_rounded_ms": dict(sorted(Counter(round(v) for v in values).items()))}


def analyze(rows, expected_ms=20.0):
    groups = defaultdict(list)
    for row in rows:
        groups[(row.get("ProcessID"), row.get("SwapChainAddress"))].append(row)
    result = []
    events = []
    for (pid, chain), frames in groups.items():
        starts = [number(r, "CPUStartQPCTimeInMs") if "CPUStartQPCTimeInMs" in r else number(r, "CPUStartQPCTime") for r in frames]
        if any(t is None for t in starts):
            raise ValueError("Capture needs --qpc_time_ms (CPUStartQPCTime column)")
        if any(b < a for a, b in zip(starts, starts[1:])):
            raise ValueError("Non-monotonic QPC timestamps within swap chain")
        origin = starts[0]
        present_intervals = []
        display_intervals = []
        unavailable = 0
        long_times = []
        previous_display_qpc = None
        clock_residuals = []
        for index, (row, qpc) in enumerate(zip(frames, starts)):
            delivered = number(row, "MsBetweenPresents")
            displayed = number(row, "DisplayedTime") if "DisplayedTime" in row else number(row, "MsBetweenDisplayChange")
            busy = number(row, "MsCPUBusy")
            until_displayed = number(row, "MsUntilDisplayed")
            present_qpc = qpc + busy if busy is not None else None
            display_qpc = (present_qpc + until_displayed
                           if present_qpc is not None and until_displayed is not None else None)
            residual = None
            if display_qpc is not None:
                # Independently check the reconstructed QPC timeline against
                # ETW's display-change intervals. Do not assign image IDs here.
                interval = number(row, "MsBetweenDisplayChange")
                if previous_display_qpc is not None and interval is not None:
                    residual = abs(display_qpc - previous_display_qpc - interval)
                    clock_residuals.append(residual)
                previous_display_qpc = display_qpc
            if delivered is not None and delivered > 0:
                present_intervals.append(delivered)
            if displayed is not None and displayed > 0:
                display_intervals.append(displayed)
            else:
                unavailable += 1
            flags = []
            if delivered is not None and delivered > expected_ms * 1.5:
                flags.append("late_present")
            if displayed is not None and displayed > expected_ms * 1.5:
                flags.append("long_display")
                if display_qpc is not None:
                    long_times.append(display_qpc)
            if residual is not None and residual > .1:
                flags.append("display_clock_inconsistent")
            if displayed is None:
                flags.append("not_displayed_or_unavailable")
            events.append({"process_id": pid, "swap_chain": chain,
                           "row": index, "qpc_ms": qpc,
                           "relative_ms": qpc - origin,
                           "present_qpc_ms": present_qpc,
                           "display_qpc_ms": display_qpc,
                           "display_relative_ms": display_qpc-origin if display_qpc is not None else None,
                           "display_clock_residual_ms": residual,
                           "between_presents_ms": delivered,
                           "displayed_ms": displayed,
                           "flags": ";".join(flags)})
        result.append({"process_id": pid, "swap_chain": chain, "frames": len(frames),
                       "duration_ms": starts[-1] - origin,
                       "present_modes": dict(Counter(r.get("PresentMode") for r in frames)),
                       "sync_intervals": dict(Counter(r.get("SyncInterval") for r in frames)),
                       "between_presents": stats(present_intervals),
                       "display_interval_metric": "DisplayedTime" if "DisplayedTime" in frames[0] else "MsBetweenDisplayChange",
                       "display_intervals": stats(display_intervals),
                       "display_clock_max_residual_ms": max(clock_residuals) if clock_residuals else None,
                       "not_displayed_or_zero_or_unavailable": unavailable,
                       "long_display_recurrence": stats([b-a for a, b in zip(long_times, long_times[1:])])})
    return result, events


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--expected-ms", type=float, default=20.0)
    args = parser.parse_args()
    if args.expected_ms <= 0:
        parser.error("--expected-ms must be positive")
    with args.csv.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        parser.error("No presentation events captured")
    summary, events = analyze(rows, args.expected_ms)
    output = args.csv.with_suffix(".summary.json")
    output.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    with args.csv.with_suffix(".timeline.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=events[0].keys())
        writer.writeheader()
        writer.writerows(events)
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
