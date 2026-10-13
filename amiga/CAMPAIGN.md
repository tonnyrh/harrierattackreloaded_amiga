# Enhanced campaign and scene testing

The regular ADF now contains the terrain game and all three special encounters.
Classic retains its terrain/landing loop.

## Campaign order

1. Sortie 1: take off, terrain and city, carrier landing, carrier defence.
2. Sortie 2: terrain and city, open-sea approach with gradual braking to VTOL
   as the oil platform enters from the right, defence, then automatic departure
   over open sea to the carrier approach, landing and carrier defence.
   Helipad service during combat is optional; victory requires no touchdown
   from either player. Surviving P2 follows the departure; CPU Wingman still
   returns only via a later red pickup.
3. Sortie 3: terrain and city, open sea and battleship encounter, final carrier landing.
   Harrier and surviving Wingman retain their screen positions on entry.
   The boss camera starts at the terrain scrolling speed and eases down to
   the ship-relative speed. The existing scene preparation screen remains.
4. The next three-sortie cycle starts automatically after the landing fanfare.
   Mission number continues increasing; existing skill progression is retained.
   Carrier wave count and aircraft quota also increase with progress, with caps.

Oil-platform health is separate from carrier hull. Carrier hull, gun damage,
repair cargo, player lives, ammunition, armour and score survive scene changes.
A lost Wingman stays lost. Oil-platform CPU Wingman follows its existing rule:
return through a later red pickup. P2 uses the platform's shared-lives rules.

Encounter balance: the non-repairable oil platform takes 80% of normal hull
damage (rounded up) and waits 25% longer between aircraft spawns. Carrier
damage and spawn timing are unchanged. Battleship cannon intervals are
60/51/42 ticks across its three damage stages, and missile checks use 100 ticks;
the existing two-shell/four-flak-cloud limits still apply.

## Debug scene jump

- At the main menu press Shift+D (Ctrl+D also works).
- Set SCENE JUMP to ON, return to the main menu and start Enhanced.
- During play press Shift+L to pause and open the scene selector. Shift+D
  (or Ctrl+D) always opens the framerate/telemetry overview, even when scene
  jumping is enabled. Esc or Shift+L closes the selector without leaving play.
- The scene selector is redrawn only on opening or a changed selection, not
  every paused frame, to avoid repeated clear/redraw flicker.
- Left/right selects Takeoff, Land + City, Carrier Attack, Oil Platform,
  Battleship or Landing. Fire/Return activates; Esc closes without jumping.
- The current run keeps its resources. Using a jump disqualifies that run from
  high-score entry. A fresh game clears the jump marker.

## Hard landing

Enhanced Carrier and platform landings accept up to 1 pixel per logical update
of downward velocity (256 in 8.8 fixed point). A faster deck contact removes
30 armour percentage points and bounces the aircraft upward. Gentle landings
are unchanged. This is collision damage, independent of the friendly-fire option.
Helicopters derive their facing from actual horizontal motion and briefly use
the middle pose during turns; the hovering pose is front-facing.

## Verification (2026-10-09)

`test-campaign.ps1` executes Amiga-native contract tests in WinUAE:
scene order, automatic special-scene requests, no repeats, Classic exclusion,
state preservation, hard/soft landing and helicopter facing.

Disk-only boot and manual scene-jump smoke test used cycle-exact A500,
Kickstart 1.2, 512 KB Chip + 512 KB Slow RAM. Menu and jumps between terrain,
Carrier, oil platform, battleship and landing were inspected. Around 17 KB
Chip RAM remained in the initial combined build. This is a tight memory budget;
existing optional ambient audio allocation remains conditional on free memory.
A complete three-sortie playthrough and real-hardware timing still need testing.
Additional native regressions passed: battleship camera reversal, persistent
targets, victory and background restoration; oil platform solo/CPU/P2 combat,
health, landing, respawn and VTOL restoration.

## Standalone oil test and boot verification

Use `build-oil-platform.ps1` to build a direct-entry oil ADF from the SAME
sources as the campaign. Primary output: `amiga/out/harrier_oil_platform.adf`.
The two legacy oil ADF names in `.tmp/oil-platform-prototype/amiga/out` are
updated as aliases; they must not be rebuilt from stale prototype sources.
Run `amiga-build.ps1` afterwards to leave normal campaign objects as default.
The game executable on each disk is named `harrier_amiga.exe` deliberately.

A500 / KS1.3 / 512 KB Chip + 512 KB Slow, disk-only: cold boot, menu and
normal takeoff passed. The same setup with development DH0/DH1 mounted
reproduced a return to DOS: HUD and engine allocations failed.
The menu ticker arena now holds HUD + engine while gameplay runs, saving
19,648 bytes of Chip allocation. Their lifetimes do not overlap; session
entry rebuilds both and menu entry rebuilds the ticker. The arena is freed
only once. A compile-time size check guards against overlap. The previously
failing mounted-drive setup now boots and passes menu -> play -> menu -> play.
Optional sea/gull audio is omitted if there is insufficient free Chip RAM.
Allocation failures now print a DOS explanation instead of silently exiting.

Clean WinUAE disk profiles: `amiga/harrier_disk_a500.uae` (campaign) and
`amiga/harrier_oil_disk_a500.uae` (oil). They exclude mounted hard disks.
Boot fresh after replacing an ADF: a saved state retains the old executable
and graphics, independently of the current disk image.
# Music residency follow-up, 2026-10-09

All three MODs now use lossless packed masters and one ordinary-RAM decode
buffer. Only menu instruments reserve dedicated Chip RAM (128 bytes); the
other tracks reuse the ticker arena tail while it is not displayed. See
`assets/music/README.md` for accounting and bounds. This reduces dedicated
music Chip storage by 20,876 bytes, but reduces total data storage by only
1,624 bytes. Do not report the former as total free-memory growth.

Verified in WinUAE A500/KS1.3, 512K Chip + 512K Slow: full ADF boot to menu,
flight and menu return; all three tracks through the music browser with valid
Paula Chip sample pointers; then return to the menu and start another game.
The instrument-sharing build also boots with DH0/DH1 mounted. In that run,
after optional allocations, 4,352 Chip bytes and 15,272 Slow bytes remained;
sea ambience was available, the optional 44,100-byte gull decode buffer was not.
This is still a tight 1MB target, not a claim of abundant memory.

Run automated WinUAE tests muted (`sound_volume=100`, WinUAE attenuation).
Keep audio emulation running for meaningful Paula/DMA timing verification.
