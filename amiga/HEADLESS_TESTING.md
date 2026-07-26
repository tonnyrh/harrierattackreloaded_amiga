# Headless WinUAE Testing (Self-Verifying Scroll/Perf Changes)

This documents how Claude (or anyone else) can run the Amiga build headlessly in
WinUAE, without a human watching the screen or pressing keys, and get objective
frame-timing numbers back as a CSV file. It exists because verifying a scrolling
fix normally requires a human to eyeball WinUAE and judge "does it still hitch" -
this harness replaces that judgement call with numbers.

## Why not just use the VS Code "Amiga 500 debug" launch config?

The project's `.vscode/launch.json` config starts WinUAE via the
`bartmanabyss.amiga-debug` extension, which drives it through GDB
(`winuae-gdb.exe`, `target remote localhost:2345`) so VS Code can set
breakpoints and show the debug console. Its generated `default.uae` sets:

```
debugging_features=gdbserver
debugging_trigger=:harrier_amiga.exe
```

This pauses the emulated CPU the moment `harrier_amiga.exe` is loaded, waiting
for a GDB client to attach and `continue`. That's fine when VS Code is driving
it, but useless for a non-interactive test run - nothing ever attaches, so the
game never actually starts.

For headless perf runs we don't need GDB at all: the game writes its own
result file (see below), so we just need WinUAE to boot and run freely.

## The headless `.uae` config

Copy the extension's `bin/win32/default.uae` and delete the two
`debugging_*` lines. Keep everything else (kickstart path, `filesystem=` /
`filesystem2=` mounts, chip/bogo mem sizes) identical, since the mounts are
what let the game write its result file straight into the host filesystem:

- `filesystem=rw,dh0:...\bartmanabyss.amiga-debug-1.8.2\bin\dh0` - boot volume,
  contains `s/startup-sequence` which just does `cd dh1: :harrier_amiga.exe`.
- `filesystem2=rw,dh1:dh1:<repo>/amiga/out,-128` - this is the important one:
  `dh1:` maps directly to the real `amiga/out/` folder on Windows. Anything the
  game writes to `DH1:` lands there and can be read straight from the host.

`use_gui=no` and `win32.start_not_captured=yes` keep it from grabbing
keyboard/mouse focus or popping the settings GUI, which matters when it's
launched from a script rather than by hand.

This exact config is checked in at `amiga/harrier_headless.uae` (paths inside
it are absolute to this machine/user - regenerate from `default.uae` if the
extension version, repo location, or Windows username ever changes). Run it
directly (no VS Code needed):

```powershell
$ext = "$env:USERPROFILE\.vscode\extensions\bartmanabyss.amiga-debug-1.8.2"
$exe = Join-Path $ext "bin\win32\winuae-gdb.exe"
Start-Process -FilePath $exe -ArgumentList @("-f", "C:\vscode\_EXT\harrierattackreloaded\amiga\harrier_headless.uae") -PassThru
```

It still opens a visible emulator window (WinUAE has no true off-screen mode),
but nothing needs to interact with it - just let it run and poll for the
output file, then kill the process (`Stop-Process`) once done.

## Build flags (`amiga/main.c`)

Three `#define`s near the top of `main.c` control the headless harness. All
default to `0`/off for normal/release builds:

| Flag | Purpose |
|---|---|
| `HAR_DEBUG_PERF_LOG` | Turns on the perf sampler (`perfLogOpen`/`perfLogFrame`) and the CSV writer described below. |
| `HAR_HEADLESS_AUTOPLAY` | Injects synthetic input so the game plays itself (see below), and auto-quits after a fixed frame budget. |
| `HAR_USE_RING_WORLD_SCROLL` | The actual scrolling strategy being tested - not test-only, but this is the flag these tests exist to evaluate. |

Set both `HAR_DEBUG_PERF_LOG` and `HAR_HEADLESS_AUTOPLAY` to `1`, pick the
`HAR_USE_RING_WORLD_SCROLL` value you want to measure, rebuild
(`.\amiga-build.ps1`), and run as above. **Remember to set them back to `0`
before shipping** - they add a busy per-frame CPU cost and, more importantly,
autoplay overrides real input.

## The critical gotcha: DOS file I/O deadlocks mid-game

The first version of this harness wrote each CSV row straight to disk (`Open`
once at startup, `Write` + `Flush` every 10 seconds) and it reliably froze the
emulated machine with:

```
Software error - task held
Finish ALL disk activity
Select CANCEL to reset/debug
```

Root cause: `main()` calls `TakeSystem()` early on, which does `Forbid()` and
`Disable()` and only undoes them in `FreeSystem()` at the very end of the
program - i.e. for the *entire* game session, AmigaOS task switching and CPU
interrupts are off. `dos.library` calls like `Open()`/`Write()` are not simple
register pokes - they send a message to the filesystem handler's task and wait
for a reply, which requires the scheduler to run that other task. With
`Forbid()`/`Disable()` in effect, that handler task can never be scheduled, so
the call hangs forever. AmigaOS's own watchdog eventually surfaces that hang as
the "task held" alert. `KPrintF` never had this problem because it goes
through `RawPutChar` (`exec.library`), a synchronous hardware/serial write with
no task hand-off involved.

**Fix**: never call `Open`/`Write`/`Close`/`Flush` from inside the game loop.
Instead:

1. `perfLogAppend()` appends each CSV line to a static in-RAM buffer
   (`perfLogBuffer`, `PERF_LOG_BUFFER_BYTES`) - no OS calls, safe under
   `Forbid()`/`Disable()`.
2. `HAR_HEADLESS_AUTOPLAY` breaks out of the main `while (1)` loop after a
   fixed frame budget, so the program actually reaches its normal shutdown
   path (`FreeSystem()` -> `Enable()`/`Permit()` restore multitasking).
3. `perfLogFlushToDisk()` runs *after* `FreeSystem()` and does the one-shot
   `Open(MODE_NEWFILE)` + `Write()` + `Close()` of the whole buffer. By this
   point the OS is fully back, so the filesystem handler task can run and the
   call completes normally.

If you ever add more headless instrumentation, keep this rule: **no blocking
DOS calls between `TakeSystem()` and `FreeSystem()`.** RAM-buffer everything
and flush once at the end.

## Headless autoplay

Real input requires a human wiggling a joystick, which defeats the point of a
headless test. `HAR_HEADLESS_AUTOPLAY` injects synthetic `InputState` bits
right after the real `ReadInput()` call each frame:

1. Once `frameCounter > 100` (~2s), force a one-shot `input.select` press to
   start the game from the menu (menu already defaults to `MENU_ITEM_START`).
2. Once `game.takeoffState == TAKEOFF_STATE_READY`, force a one-shot
   `input.up` press to go airborne (takeoff roll-in is automatic, no input
   needed for that part).
3. From then on, hold `input.up` every frame. This isn't a takeoff detail -
   forward scroll speed is automatic regardless of input, but height is not,
   and without deliberately climbing the plane reliably flies into terrain and
   dies almost immediately. Holding "up" keeps it climbed and clear of most
   terrain for long enough to get a useful sample. (Earlier test runs without
   this repeatedly hit "Game over" within the first ~60-70 seconds - visible in
   the CSV as `scrollX` going flat and `armour` cycling 100/0 as the plane
   respawned and died over and over.)
4. Once airborne and past a fixed frame count (currently `frameCounter > 4700`,
   ~94s), `break` out of the main loop so the program reaches
   `FreeSystem()`/`perfLogFlushToDisk()` and exits cleanly back to AmigaDOS.

This is a blunt instrument (no obstacle/enemy avoidance beyond "fly high") -
good enough to get several tens of seconds of mostly-continuous forward
scrolling for a perf comparison, not a real playtest.

## Output: `amiga/out/perf_log.csv`

Written once, at the very end of the run (see above), via the `DH1:` mount.
One header row plus one row per completed 10-second interval
(`PERF_LOG_INTERVAL_FRAMES = 500` frames @ 50 Hz PAL):

```
frame,seconds,loops,minFps,maxFps,avgFps,hitches,maxVblDelta,scroll,speed,origin,job,stage,tileX,tileCols,objCols,pages,fuel,armour,rockets,bombs
```

The two fields that matter most for scrolling smoothness:

- **`hitches`** - count of frames in that 10s window where more than 1 VBlank
  passed between two consecutive main-loop iterations (i.e. a real frame was
  dropped). Measured via `frameCounter` deltas, independent of game logic
  state (crash/respawn pauses don't inflate this - it's wall-clock frame
  timing, not gameplay pacing).
- **`maxVblDelta`** - the single worst VBlank gap seen in that window. At 50 Hz,
  a value of e.g. 208 means one single freeze of ~4.2 real seconds - this is
  the number that corresponds to a human saying "it hitches."

`scroll` (world scrollX), `origin` (which world-buffer page is active) and
`job`/`stage`/`tileX` (background world-render job progress) let you correlate
a hitch with "was this at a page turnover."

## Known limitations

- Single continuous flight per run, not a loop - once it hits the frame budget
  or truly dies (out of lives, `scrollX` stops advancing, `armour` stays 0),
  remaining rows are dead time, not signal. Sanity-check the CSV for a flat
  `scroll` column before trusting later rows.
- `cycle_exact=true` in the `.uae` config matters - without it WinUAE's timing
  doesn't correspond to real A500 hardware and hitch numbers aren't meaningful.
- This harness only measures frame-timing/backbuffer-readiness. It says
  nothing about visual correctness (e.g. a wrap-seam glitch in a ring-buffer
  scroller) - that still needs a human or a screenshot diff.
