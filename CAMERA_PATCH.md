# Third-person manual camera controls — native hook implementation

This feature starts from Vidanox/BanjoTooieRecompiled at
`9e5852541ba1436417e2c4468960e8c6de6deda5`. The fork's `camera-controls`
branch currently has the original experimental camera implementation and a
Windows-only workflow. This revision **moves the behavior to N64Recomp's native
hook configuration**; it is not a new input remapping layer.

## Authoritative source and generated counterpart

- `banjotooie.us.toml` contains five `[[patches.hook]]` entries targeting the
  original game's **instruction addresses**.
- `src/camera_options.cpp` implements inversion and the speed option in host
  C++. `src/camera_diagnostics.cpp` implements optional, passive logging.
- `RecompiledFuncs/funcs_37.c` and `funcs_38.c` contain the **committed generated
  C counterpart** of those TOML hooks. Vidanox's repository commits generated
  functions so normal builds do not require a ROM or a working recompiler.
  These generated copies are build artifacts, **not feature source**.
- `tools/tests/test_camera_hooks.py` verifies that the TOML hook declarations
  and their committed C counterparts agree at the documented instruction sites.

A real re-generation via Vidanox's patched N64Recomp and the correct ROM still
needs to be checked before upstream submission; that cannot be performed in a
ROM-free sandbox. Do not hand-edit generated C to change camera behavior.

## Camera call chain

`func_800A4878` dispatches manual third-person camera input;
`func_80110E00` selects -45° or +45° and calls `func_801111D0` (only two
static callsites in the committed code). The `0x801111D0` hook can therefore
invert that **manual yaw argument** without changing the actual C-buttons.

At `0x80111294`, after the game's `lui $a2, 0x4234` sets 45.0f and before
calling `func_80112D90`, a second hook applies the configured rotation-rate
multiplier. `func_80112D90` is the transition setup and contains priority
checks. `func_80112A84` advances the transition per frame. We have **not yet
verified** that this rate parameter controls perceived camera yaw linearly:
user testing of the previous implementation found little or no change.
The new hook organization does not pretend that problem is fixed.

## Configure / test

`camera.ini` in the game's working directory (alongside `assets/`):

```ini
invert_horizontal = true
rotation_speed = 2.5
debug_trace = false
```

- `invert_horizontal = false`, `rotation_speed = 1.0` restores the original
  camera parameters. Controller bindings should remain unmodified.
- `rotation_speed` accepts `1.0` through `4.0`. This is currently an experimental
  **rate parameter**, not yet a validated multiplier of actual degrees/second.
- Enable `debug_trace = true` temporarily to append `camera-debug.csv` in the
  game's working directory. Run at 1.0, then at 4.0, with one short left/right
  camera turn each. Fully exit between runs. Send both logs for comparison.
- The trace records the transition request, state before/after setup and up to
  100 before/after samples from its original per-frame update. It is bounded to
  80 requests per session. It does not change camera data.

The camera is still limited to the game's original manual yaw target/behavior.
This patch does not implement full right-stick free-look, vertical movement,
new camera interpolation, or a graphical settings menu.

## Build and PR readiness

The existing `camera-windows.yml` workflow is unchanged by this revision; it
already uses `windows-2022`. Push to `camera-controls` to run the Windows build.

The core architecture is suitable for review because the hooks live in TOML,
are implemented in dedicated native source, and the committed C mirrors them.
This is **not yet a finished PR**: the speed behavior requires runtime traces,
the graphical configuration may merit integration into the existing launcher,
and the generated output needs an upstream-style regeneration check with ROM.
