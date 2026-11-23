# Breegull input and camera trace (optional developer diagnostics)

The earlier movement-state and strafing defects have been fixed. This document describes the **current F9 recorder**, not a pending diagnostic build. It exists to investigate regressions in the game's input-consumption path and first-person camera, including unwanted pitch leveling.

## Recording

- Focus the game window and press **F9** to start capture. Press **F9** again to save and stop.
- Windows displays a non-activating on-screen recording/saved/error notice; on other platforms the window title is used instead. Failure to display an indicator does not itself prove that the CSV failed to save.
- Files are written to `input-traces/` beside the executable, with a bounded maximum of **18,000 N64 input polls** (roughly five minutes at 60 polls/second).
- The recorder does not alter controller mappings, game movement or camera angles. Recording is independent of the Modern Breegull setting.

The current CSV header has **23 columns**:

```text
elapsed_ms,packet,stage,reason,SDL_LX,SDL_LY,SDL_RX,SDL_RY,player_state,N64_X,N64_Y,N64_buttons,camera_object,camera_valid,camera_mode,angle_20c,angle_210,angle_214,bound_1e8,bound_1ec,reg_f4,reg_f6,reg_f0
```

## Reading the rows

Input stages include `producer_read`, snapshots before/after input translation and `copied_by_game`. A shared packet number aligns input-consumer stages. Producer snapshots are asynchronous and should be correlated by elapsed time, **not** treated as the exact same controller poll. `reason` describes whether translation ran or was skipped. N64 analog fields are **signed decimal integers**, and buttons are represented as numeric bitmasks.

`SDL_LX`, `SDL_LY`, `SDL_RX`, `SDL_RY` are **raw diagnostic snapshots**, not authoritative logical game inputs after rebinding. Gameplay translation uses RecompFrontend's configured C-input bindings and their analog strengths. These SDL samples may be absent if the correct device cannot be unambiguously selected; the recorder does not open or close controllers.

First-person camera rows are produced by four **read-only native hooks** in the `bsfirstp` overlay:

| Stage | Meaning |
|---|---|
| `camera_before_update` | Candidate angle before the normalizer; registers `f4` (prior angle) and `f6` (pending adjustment) are recorded. |
| `camera_after_update` | Camera fields after the original normalization/range-limit path. |
| `camera_before_reset` | Candidate angle just before the original reset/initialization path writes `f0`. |
| `camera_after_reset` | Candidate camera fields after the original reset/initialization path. |

`camera_object` is a guest object address. `angle_20c`, `angle_210`, `angle_214`, `bound_1e8`, `bound_1ec`, and `camera_mode` are **measured guest fields at those offsets**, not guaranteed final GPU view angles. `reg_f0`, `reg_f4` and `reg_f6` are meaningful only for the corresponding camera hook. Empty fields indicate that an item was irrelevant or could not be safely read.

Camera rows include the **most recently observed** N64 packet, not necessarily the exact packet used by the camera on that frame. Use `elapsed_ms`, event order and on-screen observations together; world position, yaw and the final rendered view matrix are **not** directly measured.

## What the trace established

A Windows recording made on 2026-11-22 contained **5,734 before/after first-person camera updates**. The value at offset `0x20C` correlated with upward/downward looking and was repeatedly adjusted toward zero following ordinary forward/backward movement changes, approximately **`−pitch/6`** each update. No explicit `camera_before_reset` events appeared in that recording. This identified the original target-correction branch responsible for the unwanted leveling and helped isolate it from ordinary manual pitch smoothing.

The feature patch separately scales the manual pitch increment and suppresses only zero-target leveling in Modern, non-R Breegull context. The F9 trace hooks remain in the completed fork for regression investigation. **A correlation between the recorded field and visible camera behavior does not prove the field is the final rendered pitch.**

Native hook declarations and generated call-site checks live in `banjotooie.us.toml` and `tools/tests/test_breegull_hooks.py`. Optional diagnostics do not justify changing generated C without changing the TOML source.
