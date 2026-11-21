# Experimental camera-only controls for Banjo-Tooie: Recompiled

This patch targets Vidanox/BanjoTooieRecompiled, commit
`9e5852541ba1436417e2c4468960e8c6de6deda5`.

The port's original controller bindings are unchanged. In particular, C-left
and C-right keep their normal meanings while aiming eggs in first person and
using contextual actions.

## Configure

Edit `camera.ini` next to the executable (the working directory used for
`assets/`) and restart the game:

```ini
invert_horizontal = true
rotation_speed = 2.5
```

* `invert_horizontal`: `true` or `false` (original = `false`).
* `rotation_speed`: a number between `1.0` and `4.0` (original = `1.0`).
* Removing `camera.ini` restores original behavior for both settings.
* **Restore normal right-stick/C-button mappings in the game's controller
  settings.** This is required to avoid reversing first-person aiming twice.

This patch does not add a graphical settings menu. Settings load once per
process; edit the file while the game is closed.

## Scope of the change

The game input dispatcher at `func_800A4878` calls
`bainput_should_rotate_camera_left/right` (functions `func_80097B70` and
`func_80097B94`). Its manual camera requests are forwarded to
`func_80110EFC` with action values 0 (left) and 1 (right).

`func_80110EFC` calls `func_80110E00`, which passes **-45.0f or +45.0f**
to `func_801111D0`. Two specific additions to the generated C in
`RecompiledFuncs/funcs_37.c` optionally flip the sign of these target-angle
deltas. They do not swap C-buttons or alter first-person input.

`func_801111D0` passes **45.0f** as the turn-rate parameter to
`func_80112D90`. Another specific addition scales this parameter by the
requested multiplier. `func_80112D90` divides the angular difference by
this value to set its transition duration; larger values should yield faster
transitions while preserving the original +/-45-degree target turns.

Caller search in the supplied generated C found that `func_801111D0` is
called only twice, both from `func_80110E00`'s manual left/right branches.
Thus this change is scoped to manual third-person yaw, rather than scripted
cameras or movement.

These modifications are in *committed generated C*. Regenerating
`RecompiledFuncs/` from a ROM and TOML will overwrite them. If the source is
regenerated later, reapply the generated-C hunks.

## Build from the public fork

A separate workflow `.github/workflows/camera-windows.yml` performs only the
Windows x64 build and uploads a ZIP artifact. It runs on pushes to a branch
called **`camera-controls`**; it does not publish a release. Keep the branch
separate from `main` so the upstream `build.yml` workflow does not run its
Windows/Linux/Flatpak/release jobs.

Example PowerShell commands from your existing local clone, assuming `main`
is at the target commit and your working tree has no unrelated modifications:

```powershell
git switch -c camera-controls
git apply .\BanjoTooie-camera-controls.patch
git add .
git commit -m "Add configurable camera-only inversion and faster yaw"
git push -u origin camera-controls
```

Then open your fork's **Actions** tab -> **Camera controls - Windows x64** ->
the push run -> **Artifacts** -> `BanjoTooieRecompiled-camera-windows-x64`.
The ZIP contains the Windows executable, runtime DLLs, assets, and `camera.ini`.

## Verification and limitations

* The code changes and standalone camera-option helpers have been examined;
  they still require a Windows build and gameplay verification.
* This environment does not contain the four required Git submodules, so a
  complete Windows build cannot be validated locally.
* Without running the game, we cannot guarantee the sign convention, actual
  perceived speed increase, or compatibility with every camera mode.
* If the manual camera stops rotating correctly, first try
  `rotation_speed = 1.0` and `invert_horizontal = false`; this is an
  experimental patch, **not a tested binary**.
* Gameplay mechanics, scripted cameras, global controller bindings and
  first-person aim have no direct source changes in this patch.
