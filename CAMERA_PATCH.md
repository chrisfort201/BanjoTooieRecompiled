# Third-person camera inversion and manual rotation speed

This experimental feature modifies original camera behavior using N64Recomp's
instruction hooks rather than controller remapping or hand-maintained generated C.
It targets the USA v1.0 ROM and builds on the fork's native-hook implementation
(commit `7079aff` on `camera-controls`).

## Source of truth and reproducibility

- `banjotooie.us.toml` defines ten native `[[patches.hook]]` entries.
- `src/camera_options.cpp` implements camera inversion, transition scoping,
  and the actual motion parameters; `src/camera_diagnostics.cpp` logs optional
  measurements.
- `RecompiledFuncs/funcs_37.c` and `funcs_38.c` are generated build artifacts,
  produced from that TOML file by Vidanox's patched N64Recomp.
- `py tools/tests/test_camera_hooks.py` checks every hook's address, text and
  position against the committed generated files.

The generated files were independently regenerated from the SHA-1-matched
Banjo-Tooie USA 1.0 ROM with the patched N64Recomp and checked for unrelated
output differences. The regeneration only altered the expected camera hooks.

## Camera path and findings

`func_80110E00` requests a manual left/right turn through `func_801111D0`.
Its two original callsites make it safe to invert the turn angle without
swapping C-buttons, including their use in first-person egg aiming.

Previously, `rotation_speed` scaled the original 45.0f parameter passed to
`func_80112D90`. The runtime trace confirmed this shortened the transition
*duration* (1.2 s at 1x versus 0.3 s at 4x), but did **not** change the
per-frame yaw. The trace showed 2.5 degrees per 30 Hz update at both settings.

The underlying cause is the distinct per-frame motion controller:

- `func_80112A84` handles a type-1 yaw transition, calling `func_801127C8`
  at the `0x80112B90` instruction.
- `func_801127C8` uses `func_800FF060` to approach the requested angle, with
  original float constants 75.0 (angular-velocity limit), 1000.0
  (acceleration) and -560.0 (braking).
- At a 30 Hz update, a 75-deg/s cap corresponds to the observed 2.5 degrees
  per update.

The new speed option leaves the original 45.0f request parameter unchanged.
Instead, it scales all three motion-controller parameters by `rotation_speed`.
For example at 4x the limits are 300.0, 4000.0 and -2240.0. At 1x they remain
exactly the original values.

### Scoping safeguards

The manual request hook records the camera and target. Its completion hook only
tags a transition after confirming the original game accepted the request
(type=1, priority=2, exact target and state pointer). Every entry to the
original transition setup invalidates an earlier manual tag for the same
camera; only a new, accepted manual request can re-enable it.

The per-frame controller is bracketed by hooks at `0x80112B90` and
`0x80112B98`. It scales the original limit and acceleration constants only if
the active camera/state/target still match the accepted manual transition.
Other transition types, overwritten transitions and unrelated camera pointers
use the original parameters. No memory fields are patched in place.

## Configuration

Put `camera.ini` in the game's working directory (normally alongside the
executable and `assets/`). The settings load once per process:

```ini
invert_horizontal = true
rotation_speed = 2.5
debug_trace = false
```

`rotation_speed` accepts 1.0–4.0. Setting speed to 1.0 and inversion to false
preserves original camera parameters. The game still uses discrete 45-degree
manual yaw targets; this does **not** add unrestricted camera orbit or pitch.

For verification set `debug_trace = true` and compare 1.0 versus 4.0 with
separate launches. The append-only `camera-debug.csv` distinguishes sessions
with `# camera-transition trace session` and now also records the configured
multiplier (`# configured rotation_speed=...`). When the speed hooks activate,
`# applied manual motion: ...` records their actual velocity/acceleration/
braking parameters. For an apples-to-apples comparison, use similar manual
rotations. The CSV's `current` column exposes game-engine yaw per tick.

The trace captures at most 80 requests and 100 frames following each request.
Disable it in normal gameplay.

## Validation status

- Patch architecture: native TOML hooks, not manual generated-code edits.
- ROM-based regeneration: passed, with no unrelated generated code differences.
- Static hook consistency: passed.
- Native helpers: compiled and exercised against synthetic guest camera memory
  at 1x and 4x, including an override request, changed priority and target.
- Full Windows packaging and actual gameplay at the new speed: **not yet tested**.

The speed option remains experimental until verified in-game. In particular,
other camera modes, scripted sequences, transitions at very short targets and
switching between camera states should be checked before proposing an upstream
pull request. Diagnostics and the ad-hoc `camera.ini` UI may require polish.
