# Measuring visible scrolling, not just average FPS

Use three separate observations. A passing result in one does not prove the
others are smooth:

1. `HAR_SCROLL_PRESENT_TRACE` / `check-scroll-presentation.py`: emulated CIA
   fields, scroll coordinate, Copper pointer/fine-scroll consistency and raster
   deadline. This catches missed Amiga fields and wrong coarse/fine positions.
2. `check-scroll-frames.py`: displacement between consecutive WinUAE screenshots.
   This catches repeated game images and wrong pixel steps, but PNG numbers and
   file timestamps are **not** actual display times. Disk recording adds load.
3. PresentMon / `check-host-presentation.py`: host QPC timestamps and Windows
   presentation/display durations. This catches uneven display cadence despite
   steady game FPS. It cannot tell whether two submissions contain the same image.

## Host-clock capture

Download the standalone, Intel-signed x64 console executable from the official
[PresentMon releases](https://github.com/GameTechDev/PresentMon/releases).
Version 2.6.0 is used by the default command below; no installer is needed.
Place it in `.tools/PresentMon/PresentMon-2.6.0-x64.exe` or pass `-PresentMon`.

ETW requires an elevated PowerShell or membership of Performance Log Users.
Do not change group membership just to run this test. Target a dedicated muted
WinUAE process, with a known Enhanced land/VTOL scenario. Keep the window visible
and do not move/minimize it or change focus during the measurement. First capture
without screenshots or overlays, then compare with an instrumented capture.

```powershell
./tools/measure-winuae-presentation.ps1 -EmulatorProcessId 12345 -OutputCsv .tmp/host-run/present.csv -Seconds 20
python tools/check-host-presentation.py .tmp/host-run/present.csv
python tools/test-host-presentation.py
```

Outputs: raw CSV, clock/process metadata, `.summary.json`, and `.timeline.csv`.
The timeline preserves every event and marks presentation/display durations
over 30 ms (1.5 PAL periods). Missing display metrics are reported as unavailable,
not silently treated as smooth. Different swap chains are analyzed separately.
`relative_ms` is the CPU frame-start QPC timestamp relative to the first row,
not the exact beginning of physical screen illumination.

Look for regular ~20 ms submissions but ~16.7/33.3 ms displayed durations, or
bursts of late submissions at the same real-time intervals as the reported jump.
Do not claim physical panel verification: ETW tracks Windows presentation,
not photons. A high-frame-rate camera remains an independent final check.

For exact frame-content correlation, an optical frame ID in a diagnostic build
plus capture timestamps on the same QPC clock is required. Do not pair separate
PNG and ETW captures by row number. At zero scroll in VTOL, track a moving actor
or such a marker; stationary terrain alone cannot reveal duplicate game frames.

## Initial attempt, 2026-10-10

The standalone binary's Intel signature was valid. Initial capture failed with
ETW access denied. After user-approved UAC elevation, two muted isolated A500
Enhanced autoplay captures succeeded (20 seconds each, previous high-speed
baseline executable, not a freshly built release). Raw evidence:

- `.tmp/enhanced-qpc-admin-oct10/present.csv`: 982 submissions, median submission
  interval 20.031 ms; 624 available display-change intervals, median 33.356 ms,
  p95 50.054 ms, maximum 150.143 ms. 358 unavailable/zero display entries.
- `.tmp/enhanced-qpc-repeat-oct10/present.csv`: 899 submissions, median submission
  interval 20.032 ms; 605 available display-change intervals, median 33.353 ms,
  p95 66.720 ms, maximum 391.636 ms. Maximum submission interval 342.157 ms.

Both used Composed: Flip with SyncInterval=0. These are evidence of host
presentation irregularity, not proof that all game-side stalls are resolved.
No pixel/frame ID was correlated to these QPC events; no exact game scroll
coordinate can yet be assigned to a particular host stall. Hidden-launched test
window visibility/focus was not controlled, which limits equivalence to normal
interactive play. Repeat with a controlled visible window for final attribution.

PresentMon 2.6 emits CPUStartQPCTimeInMs and MsBetweenDisplayChange; the analyzer
also supports the earlier CPUStartQPCTime / DisplayedTime schema. The source
metric is identified in JSON. MsBetweenDisplayChange describes the interval from
the previous display change, whereas DisplayedTime describes this frame's tenure.
The game and its timing logic were not changed by adding these tools.

## Verified detector checks

Run `python tools/test-scroll-measurement.py` and
`python tools/test-host-presentation.py`. These inject repeated images, catch-up
jumps, backward motion, ambiguous textures, unrelated image corruption, missing
capture files, display cadence irregularity and inconsistent clock reconstruction.
A clean sequence must pass; uncertain evidence must not become a false pass.

The content checker now writes a JSON summary and CSV containing the exact pair
of filenames for every transition. Exit codes: 0 clean, 1 detected movement
fault, 2 inconclusive/invalid capture. A repeated source image is an observation,
not proof of a game bug: recording overhead can also cause it.

```powershell
python tools/check-scroll-frames.py .tmp/enhanced-fast-frame-capture-oct10/001 --expected-step 6 --output .tmp/scroll-verified-fast
python tools/check-scroll-frames.py .tmp/enhanced-frame-capture-oct10/001 --expected-step 4 --output .tmp/scroll-verified-cruise
python tools/build-scroll-report.py --host .tmp/enhanced-qpc-admin-oct10/present.csv --host .tmp/enhanced-qpc-repeat-oct10/present.csv --frames .tmp/enhanced-fast-frame-capture-oct10/001 --expected-step 6 --output .tmp/scroll-diagnostics/report.html
```

Verified on real captures: the fast sequence detects the repeated image
`00172.png` to `00173.png` (246 normal steps and one repeat). The cruise sequence
passes all 90 transitions. No uncertain matches were reported in either.

Host CSV now contains `present_qpc_ms` and `display_qpc_ms`, reconstructed as
CPUStartQPCTimeInMs + MsCPUBusy [+ MsUntilDisplayed]. Differences between
successive reconstructed display timestamps are independently checked against
MsBetweenDisplayChange. Missing fields stay unavailable. An error above 0.1 ms
is flagged `display_clock_inconsistent`. This verifies the clock calculation,
not correspondence with an Amiga image or scroll coordinate.

The HTML report plots host timing and shows the source-frame fault images in
separate sections. Host timelines keep every event in CSV; the report lists the
20 longest display intervals. The measurements are never silently aligned by
row number. These tools do not alter gameplay, video settings or the release ADF.
