# Experimental Breegull Blaster input translation

Branch: `modern-breegull-controls` (based on the fork's manual-build main).

- Optional **Modern Breegull Blaster controls** under a new RecompFrontend
  settings tab, default **off**, live and persisted in `breegull.json`.
- When enabled **and** the observed first-person player has one of the
  designated `bsfirstp` states (C8–CC, D1, or D9–DC) **and** a controller is open:
  left analog X produces original C-left/right strafing; right analog X
  produces original N64 stick-X turning (continuous analog); left analog Y,
  C-up/down pitching, other buttons and the original game camera are unchanged.
- Right-stick sampling prefers the SDL controller assigned to player zero by
  RecompFrontend. When its single-player mode has no explicit assignment, it
  uses a controller only if exactly one is already open; it does not choose an
  arbitrary first device when DS4Windows exposes multiple devices. This is
  read-only and does not open, close or take ownership of controllers.
  **Remaining limitation:** the right-X analog axis is still read directly
  from SDL on that selected device; arbitrary RecompFrontend axis rebindings
  are not yet honored. Left-stick input and all N64 buttons still come through
  RecompFrontend's configured profile. Keyboard-only play remains original.
- Native N64Recomp TOML hooks mark the first-person player and rewrite only the
  N64 controller packet immediately before `func_8001608C` consumes it. The
  initial producer-side hook after `osContGetReadData` could race the game's
  input-consumption thread, so the remap is now performed at the consumer.
  Subsequent CSV traces identified a separate, confirmed defect: `0xD1`
  was incorrectly excluded while the game frequently transitions between
  `0xCA` and `0xD1` during movement. This revision accepts `0xD1` explicitly.
  The observed player pointer
  is shared atomically between the game-state and controller-polling threads,
  even while the option is off, so enabling mid-mode works without re-entry.
  C-left/right are OSContPad bits 0x0002/0x0001; C-up/down stay untouched.
  Their corresponding generated-C call sites are checked by
  `tools/tests/test_breegull_hooks.py`.
  **Do not edit generated C without a matching TOML hook.**
- When the toggle is off, the pad hook returns without reading/writing guest
  memory or altering MIPS registers. The overlay hook only remembers the actor
  pointer in host memory; neither hook modifies guest state while disabled.
  Any other player state also passes the packet through unchanged.

## Risks and necessary manual tests

Earlier builds compiled and demonstrated live switching and working right-X
turning, but left-stick strafing was intermittent/stuttering. Input traces
showed `0xCA` translated followed by `0xD1` bypassed, repeatedly. The narrow
state-filter correction needs fresh in-game validation. Verify in Breegull
Blaster that movement, strafing and yaw are correctly oriented; that
right-stick Y still controls pitch; and that leaving the mode immediately
restores ordinary third-person and Egg Aim controls. In particular check the
first few frames at mode entry and exit, and reload a level while the toggle
remains enabled. If the state-family assumptions fail, do not widen the
allowlist without investigating it.

N64 pad layout: 0x80079B40 (`osContGetReadData` in `func_800165F0`,
consumed by `func_8001608C`), 6-byte `OSContPad` entries. State accessor: `func_8009E6F8` (+0x120 to state object,
+0x4 to state value). The observer is `bsfirstp_entrypoint_1` in overlay 108.
This keeps the original first-person movement/camera engine authoritative.

The F9 developer recorder can be activated regardless of the Modern Breegull
setting. It records the original, translated and game-copied N64 packets, plus
SDL stick samples. Windows displays a click-through recording/saved/error
notice over the game; other platforms retain the window-title indicator. The
CSVs are saved in `input-traces/` beside the executable. See
`BREEGULL_INPUT_TRACE.md`. It does **not** claim to record player position or
camera orientation.

Tests (no ROM):
```
py tools/tests/test_breegull_hooks.py
```
The pure remap test can run with a C++20 compiler:
```
c++ -std=c++20 -Iinclude tools/tests/breegull_remap_test.cpp -o breegull_remap_test
```
