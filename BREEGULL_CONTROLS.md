# Experimental Breegull Blaster input translation

Branch: `modern-breegull-controls` (based on the fork's manual-build main).

- Optional **Modern Breegull Blaster controls** under a new RecompFrontend
  settings tab, default **off**, live and persisted in `breegull.json`.
- When enabled **and** the observed first-person player has one of the
  designated `bsfirstp` states (C8–CC or D9–DC) **and** a controller is open:
  left analog X produces original C-left/right strafing; right analog X
  produces original N64 stick-X turning (continuous analog); left analog Y,
  C-up/down pitching, other buttons and the original game camera are unchanged.
- A single-player SDL controller is selected without modifying RecompFrontend
  controller profiles or opening/closing SDL devices. This is an initial
  controller-only implementation; keyboard controls remain original.
- Native N64Recomp TOML hooks mark the first-person player and rewrite only the
  N64 controller packet after Tooie's original read. The observed player pointer
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

This is *not yet gameplay-tested or Windows-built*. The first-person player
pointer and state allowlist were inferred from static generated code. Verify in
Breegull Blaster that movement, strafing and yaw are correctly oriented; that
right-stick Y still controls pitch; and that leaving the mode immediately
restores ordinary third-person and Egg Aim controls. In particular check the
first few frames at mode entry and exit, and reload a level while the toggle
remains enabled. If the state-family assumptions fail, do not widen the
allowlist without investigating it.

N64 pad layout: 0x80079B40 (`osContGetReadData` in `func_800165F0`), 6-byte
`OSContPad` entries. State accessor: `func_8009E6F8` (+0x120 to state object,
+0x4 to state value). The observer is `bsfirstp_entrypoint_1` in overlay 108.
This keeps the original first-person movement/camera engine authoritative.

Tests (no ROM):
```
py tools/tests/test_breegull_hooks.py
```
The pure remap test can run with a C++20 compiler:
```
c++ -std=c++20 -Iinclude tools/tests/breegull_remap_test.cpp -o breegull_remap_test
```
