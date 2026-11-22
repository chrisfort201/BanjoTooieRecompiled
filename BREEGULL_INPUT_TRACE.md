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

Controller samples use the SDL controller bound to player 0 in RecompFrontend.
In single-player mode without an explicit assignment, an implicit pad is used
only when exactly one opened controller is present. With multiple ambiguous
controllers no raw right-stick axes are sampled: the recorder does **not**
claim that the first device is the frontend's selected input.

The recorder does not open or close controllers. Outside an active capture, it
performs only a keyboard-state check and leaves all existing guest behavior
unchanged. Temporary traces may contain button/stick activity and should be
shared only intentionally.
