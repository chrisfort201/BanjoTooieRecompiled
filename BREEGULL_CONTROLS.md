# Modern Breegull Blaster controls

This optional, game-specific input scheme is included in the fork's `main`. It is intended primarily for SDL-compatible **dual-stick controllers**, but consumes RecompFrontend's *active logical input bindings*, not hard-coded PlayStation or Xbox buttons.

## Player controls and menu

Open **Settings → Breegull Blaster**:

| Option | Default | Behavior |
|---|---|---|
| **Modern Breegull Blaster controls** | **Off** | Enables the optional layout only in the validated Breegull player states. Off preserves the original input/camera path. |
| **[Modern only] Invert Vertical Look** | **On** | On preserves the original inverted vertical direction; Off makes pushing the right stick up look up. Applies in ordinary Breegull and R-held aiming. |
| **[Modern only] Vertical Look Speed (Outside R-Aim)** | **1.00×** | Adjustable from **0.50× to 3.00×** in 0.25× steps. Multiplies ordinary **vertical pitch speed** only. Horizontal yaw and R-held precision pitch retain their original speeds. |

The dependent options are disabled in the UI when Modern is Off, and their saved values are preserved for the next time Modern is enabled. Settings are live and persist through RecompFrontend's `breegull.json`.

With Modern enabled:

- The original N64 movement stick moves forward/backward. Its X axis is translated into **C-left/right strafing** in ordinary Breegull mode, including when firing eggs.
- RecompFrontend's configured **C-left/right bindings** control horizontal look; **C-up/down** control ordinary vertical looking.
- Holding **R** enters the original precision-aim state. Its horizontal and vertical aiming are assigned to the configured C-direction bindings. Left-stick Y no longer controls vertical precision pitch. The original restriction on locomotion during R-held aiming is **not** changed.
- The optional Modern scheme prevents the unwanted automatic zero-target pitch leveling observed on forward/backward movement transitions. Intentional camera corrections toward **nonzero** targets remain enabled, including the cutscene adjustment observed in Windows testing.

**1.00× means the original vertical speed**, not equality between horizontal and vertical angular speeds. The original game turns vertically much more slowly than horizontally. The speed option scales the actual pitch increment in the original first-person look calculation—not the digital C-button bits.

## Bindings and compatibility

The remapper reads the active player-zero controller and keyboard bindings through `recompinput::profiles::get_input_binding` and `recompinput::get_input_analog`. It therefore honors custom N64 C-direction assignments, including analog stick axes (proportional strength) and keyboard/button inputs (digital strength). The host does not take ownership of an SDL controller or open additional devices.

The *intended* layout maps the N64 movement stick to the physical left stick and the four N64 C-directions to the physical right stick. If you assign those controls to face buttons or a different axis, the Modern interpretation follows the **logical N64 bindings**; it may no longer resemble the described twin-stick FPS layout. No extra contextual binding editor or mouse-look is implemented.

To safely share logical C-axis readings between RecompFrontend's event/UI thread and the game's input-consumer thread, the four strengths are atomically packed. Game-side remapping happens **immediately before `func_8001608C` consumes the controller packet**, rather than asynchronously in the SDL event producer.

## Native hook architecture

- `banjotooie.us.toml` is the authoritative list of native N64Recomp hooks; its generated calls are committed in `RecompiledFuncs/`.
- The first-person overlay hook observes the current player and camera object. The validated state predicate permits `0xC8`–`0xCC`, `0xD1`, and `0xD8`–`0xDC`. State `0xD1` was found during movement; `0xD8` occurs during firing. Do not widen this predicate without evidence.
- Host-side C++ in `src/breegull_controls.cpp`, `src/breegull_config.cpp` and associated `include/` headers owns input translation, live settings and the pitch policy.
- Ordinary C-up/down pitch is calculated in the original first-person function `func_808069FC_bsfirstp`; the native output hook scales its manual angular result by the selected vertical-speed multiplier.
- The separate automatic target-correction function `func_808069BC_bsfirstp` was observed leveling camera pitch by approximately `−pitch/6` following movement transitions. The native output hook suppresses only **zero-target** corrections in validated, Modern-enabled, non-R Breegull context; other adjustments remain intact.
- The default/opted-out game path does not rewrite N64 input or pitch values. The optional F9 recorder can run independently of the Modern setting.

There are **10 Breegull native hooks**: four input/observation hooks, four read-only camera telemetry hooks and two pitch behavior hooks. The preexisting third-person camera feature has **10 separate native hooks**. Source-generated call sites must match TOML declarations; never patch generated C alone.

## Verification and limits

Windows gameplay testing has confirmed: movement and diagonal strafing, strafing while firing eggs, live enable/disable, R-held two-axis aiming, inversion, scaling of ordinary vertical look at several values, prevention of unwanted walking-transition leveling, preservation of an intentional cutscene camera correction, unchanged third-person and Egg Aim behavior, and the dependent settings becoming inactive. The `[Modern only]` wording was also visually checked after the final label update.

The **complete Breegull Blaster section has not yet been comprehensively playtested**; other controllers and Linux gameplay have not been tested. Some inherited pitch acceleration/deceleration remains, so this is an optional reinterpretation of the N64 input layout rather than an entirely rewritten FPS engine.

Validation commands:

```sh
python tools/tests/test_breegull_hooks.py
python tools/tests/test_camera_hooks.py
c++ -std=c++20 -Iinclude tools/tests/breegull_remap_test.cpp -o breegull_remap_test
./breegull_remap_test
```

The manual Windows workflow additionally builds and runs `TooieBreegullControlsTests`. Hook consistency and unit tests do not replace whole-game or platform testing. See [BREEGULL_INPUT_TRACE.md](BREEGULL_INPUT_TRACE.md) for the optional developer recorder.
