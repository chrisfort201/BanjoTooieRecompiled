# Third-person camera inversion and manual rotation speed

This optional feature modifies original camera behavior using N64Recomp's
instruction hooks rather than controller remapping or hand-maintained generated C.
It targets the USA v1.0 ROM and builds on the fork's native-hook implementation
(first introduced at commit `7079aff`, now incorporated in `main`).

## Source of truth and reproducibility

- `banjotooie.us.toml` defines ten native `[[patches.hook]]` entries.
- `src/camera_options.cpp` implements camera inversion, transition scoping,
  and the actual motion parameters; `src/camera_config.cpp` registers live UI
  settings and `src/camera_diagnostics.cpp` logs optional measurements.
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

## Live configuration (frontend)

Open the game's **Settings > Camera** tab. All three options can be changed
while the game is running, without restarting:

- **Invert horizontal camera** — reverses the original C-left/C-right manual
  third-person turn requests, without remapping the first-person C-buttons.
- **Manual camera rotation speed** — 1.00x (original) to 4.00x, in 0.25x steps.
  Newly issued manual turns use the selected multiplier; switching settings
  invalidates old tagged camera transitions rather than changing scripted ones.
- **Camera diagnostic trace** — enables the bounded `camera-debug.csv` logger.
  Keep this off during ordinary gameplay.

RecompFrontend persists these settings as `camera.json` in its existing
application config directory, together with the game's other JSON settings.
There is no restart and no need to edit an INI file. **On first launch only**, if
there is a legacy `camera.ini` in the working directory and no saved camera
configuration, its values become the initial UI defaults. Subsequent launches
use `camera.json` and ignore changes to `camera.ini`. The new Windows package
does not contain `camera.ini`. A fresh installation defaults to the original
camera behavior (inversion off, speed 1.00x, diagnostics off).

### Neutral-settings guarantee

With inversion off, 1.00x speed and trace off, the native camera hooks still
exist and are called by the generated game code, but they do not change any
guest registers, guest memory or original motion parameters. They also do not
read guest camera transition state. Disabled hooks return after checking the
host-side atomic settings or resetting a host-only marker. This preserves the
original game's numeric camera inputs; it is **not** instruction-for-instruction
identical to an unmodified build, and whole-game equivalence still needs
regression testing.

When speed changes, an atomic generation counter is changed along with the
multiplier. A pending request from an earlier speed setting cannot be reused
when the player changes 4x -> 1x -> 4x. Each motion-controller update captures
one multiplier so its velocity and acceleration hooks are consistent.

For diagnostics, compare the CSV `current` values at different speeds. The
append-only CSV records session boundaries, the chosen multiplier and
motion-controller constants when they are modified. Tracing is limited to at
most 80 camera requests and 100 frames following each request per session.

## Validation status

- Native TOML-hook consistency: **10 camera hooks** verified by `tools/tests/test_camera_hooks.py`.
- Original USA v1.0 ROM regeneration: previously reproduced, with no unrelated generated C output differences after normalizing line endings.
- Synthetic native-helper tests and live/neutral settings tests: passed. The Windows manual build workflow runs `TooieCameraSettingsTests`.
- **Windows gameplay:** the live settings, horizontal inversion and manual-turn speed have been tested. Settings do not require a restart. This does **not** imply exhaustive coverage of every camera transition or another platform.
- With inversion Off, speed 1.00× and diagnostics Off, the code retains its neutral fast path. A hook still executes, so this is not an instruction-for-instruction guarantee of equivalence to an unmodified binary.

This feature has been merged into the fork's `main`. The original upstream code remains a separate project; see the fork README for its status and relationship to upstream.
