# Breegull input trace (optional developer diagnostics)

The current remapper still causes slow/intermittent strafing and unexpected
turning on diagonals. The next build records the inputs **without changing the
remapping logic**. Native N64Recomp hook declarations remain in
`banjotooie.us.toml`; do not add hand-written-only changes to generated C.

- Press **F9** while the game window has focus to start recording. The window
  game's recording indicator appears while capturing. Press **F9** again to
  stop and save. An on-screen notice confirms saving or reports an error.
  No capture takes place until F9.
- Find the CSV files in `input-traces\` next to the game executable.
  Each session is limited to 18,000 N64 input polls (~5 minutes at 60 polls/s).
- Record with the modern Breegull toggle **on**, attempting pure left/right
  strafing, then diagonals, then transitions between them. Optionally repeat
  with the toggle off in a separate session.
- The CSV includes host SDL left/right stick axes, a separate read-only
  snapshot after the original N64 controller read (`producer_read`), N64
  inputs before and after remapping, and the copy the original game takes from
  the pad (`copied_by_game`).
  It also records why remapping was skipped (if applicable). Consumer-stage samples are aligned by a unique input-poll number;
  producer snapshots are asynchronous and correlated by elapsed milliseconds; raw SDL axes are the latest observed values
  at rendering-event time, not necessarily sampled at the same instant as
  each N64 input poll. All N64 analog values are written as signed decimal
  integers (not raw byte characters), so recordings are standard CSV.

**Important limitation:** `copied_by_game` confirms only that the game received
those button and stick values. It does **not** independently establish what
movement or rotation actually occurred. Player-world position and camera
orientation have not yet been tied to validated game-state fields. A second,
separately verified state hook would be required to measure those accurately;
please don't interpret raw input values as observed physical movement.

Raw SDL stick samples are **diagnostics only**. They prefer the frontend's
explicitly assigned player-0 controller, with an unambiguous one-controller
fallback for display in single-player mode. The game input remapper no longer
uses these raw samples: it reads active RecompFrontend C-input bindings and
analog strengths, which support remapped sticks, buttons and keyboard keys.
The trace's `raw_sdl` values should not be mistaken for the logical control
strength after rebinding, and they may be unavailable when several controllers
are open without an explicit assignment.

The recorder does not open or close controllers. Outside an active capture, it
performs only a keyboard-state check and leaves all existing guest behavior
unchanged. Temporary traces may contain button/stick activity and should be
shared only intentionally.


## Stage-1 first-person camera diagnostics (F9; experimental)

This build adds **four read-only native hooks** in the existing `bsfirstp`
first-person overlay. All events go into the **same F9 CSV** as the input
recording, with the same `elapsed_ms` clock. It does not implement a speed
multiplier or remove automatic leveling. No menu setting changes are made.

The added `stage` values are:

- `camera_before_update`: before `func_8080682C_bsfirstp` passes the sum of
  registers `f4` (previous angle) and `f6` (candidate adjustment) to its angle
  normalization routine (`0x80806884`).
- `camera_after_update`: after the same function's normalization and range
  limiting, just before return (`0x808068D8`).
- `camera_before_reset`: just before `func_808067B0_bsfirstp` writes another
  returned angular value (`f0`) to the same guest field (`0x808067CC`).
- `camera_after_reset`: after that function writes `0x20C`, `0x210`, and
  `0x214` (`0x808067D8`).

For these rows, `camera_object` is the guest address of the *first-person state
object*, and `camera_valid` says whether reading its fields was safe. The
columns `angle_20c`, `angle_210`, `angle_214`, `bound_1e8`, `bound_1ec`, and
`camera_mode` correspond to **guest offsets, not conclusively identified game
semantics**. The `reg_f4`/`reg_f6` values are only meaningful for the before-
update stage, and `reg_f0` only for before-reset. Blank cells mean a value is
not relevant or could not be safely read. These are candidate camera values;
**0x20C has not yet been verified as the actual rendered pitch**. The purpose
of this trace is to test that hypothesis, not to assert it.

Input rows retain their original meaning and have empty camera columns. Camera
rows contain the **most recently observed** N64 input and poll number, not an
exactly synchronized controller packet; use `elapsed_ms` and event order when
correlating them. `player_state` comes from the most recently observed
first-person player pointer, independently sampled at the camera hook.

Recording is still activated/stopped with F9 and remains bounded to 18,000
input polls. The first-person hooks immediately return when recording is off;
none changes guest memory, camera registers, or the original game algorithms.

### Requested single recording

With **Modern Breegull controls enabled** and preferably inversion Off:

1. Start F9 while standing still. Look up, release right-stick Y, and hold
   still for ~2 seconds (does the angle remain?).
2. Without touching right-stick Y, start walking forward, stop, then walk
   again. Pause ~1 second at each transition.
3. Walk continuously, look up, release right-stick Y *while still walking*,
   then stop and restart walking.
4. Repeat a brief look-down version (with another pause and walk transition).
5. Do one sideways strafe and one egg-firing transition while holding a
   nonlevel view.
6. Briefly hold and release R; test looking up in normal mode before and
   after R. Stop F9.

The CSV should be under `input-traces/` beside the game executable, just as
before. If some action unexpectedly levels the view, a short note about which
step triggered it is useful but not required. The first diagnostic question is
whether the candidate angle field changes at the corresponding transition.

## Stage-2 interpretation and validation

The 2026-11-22 Stage-1 recording (`breegull-input-20261122-191922-745-2.csv`)
contained 5,734 before/after first-person pitch-update pairs, showing that
field `0x20C` follows manual up/down looking and, during movement transitions,
undergoes a systematic approximately `-current_pitch/6` adjustment toward
zero. No `camera_before_reset`/`camera_after_reset` events were observed.
This supports, but does not mathematically prove, that `0x20C` is the rendered
pitch rather than a closely related internal camera angle.

Stage 2 adds two **gameplay** hooks at the outputs of the separate normal
manual-look and automatic-target-correction functions. When Modern Breegull
is on, R is not held and the camera actor/state are validated, the former
applies the configured `0.50x`–`3.00x` vertical multiplier; the latter cancels
only corrections toward a zero-pitch target. The existing F9 camera rows
continue to record the resultant pitch increments; they do not gain new
columns, and Stage 2 remains subject to runtime verification.
