# Experimental Breegull Blaster input translation

Branch: `modern-breegull-controls` (fork-only manual Windows build).

## Controls

- **Modern Breegull Blaster controls** is an opt-in setting in the
  RecompFrontend Breegull Blaster tab. Default **off**; persisted in
  `breegull.json`. With the option off, original N64 inputs are untouched.
- With Modern enabled in observed Breegull states C8–CC, D1, or D8–DC:
  - N64 movement-stick X produces C-left/right strafing, while movement-stick
    Y retains forward/backward movement in ordinary Breegull play.
  - RecompFrontend's **configured C-left/right bindings** drive analog turn
    strength. **C-up/down bindings** control normal vertical looking.
  - While holding **R** for precision aim, those same C-up/down bindings now
    control the original analog vertical pitch. The original no-walk R-aim
    gameplay remains unchanged. Left-stick Y no longer controls precision
    pitch in Modern mode.
  - **Invert Vertical Look** is a separate live option, default **on**, that
    preserves Tooie's original inverted direction. Off switches to the
    conventional FPS vertical direction. It affects both ordinary Breegull
    vertical look and R-held precision pitch, but only in Modern mode.

This layout is intended for two-stick controllers with the N64 movement inputs
on the left stick and N64 C-direction inputs on the right stick. The gamepad
can be a DualShock, Xbox pad or another SDL-compatible controller. Custom
RecompFrontend C-button bindings also work: they are read through the
**active configured input profiles**, not directly from an assumed physical
right-stick axis. Analog rebindings retain proportional strength; keyboard
keys/buttons provide full-strength directions. For unusual bindings (e.g.,
all C directions assigned to face buttons), the same logical scheme still
applies, but the result may not resemble an FPS layout. No separate button
binding editor is added to Breegull's menu.

## Implementation

The frontend samples `recompinput::profiles::get_input_binding` and
`recompinput::get_input_analog` for player 0's current controller and keyboard
profiles on the SDL/UI thread after `handle_events()`. These are the same
profile/binding accessors used by RecompFrontend's N64 packet producer. Four
16-bit input strengths (C-left/right/up/down) are packed into one atomic word
for the game's separate input-consumer thread. The host does not open, close,
or own any controller. With Modern enabled, the remap also works for keyboard
bindings and does not require an SDL controller to be connected. If a frontend
menu blocks gameplay input, the sampled look strengths are zero.

The native game hooks in `banjotooie.us.toml` remain authoritative. The
first-person overlay hook observes the player actor; the game-consumer hook
modifies only the N64 controller packet just before `func_8001608C` copies it.
Neither the TOML hooks nor `RecompiledFuncs/*.c` are changed here. The
read-only producer/consumer trace hooks remain in place. Never modify
generated C without a corresponding TOML hook definition.

Trace recordings established two state-filter defects: `0xD1` occurs during
movement and `0xD8` while firing eggs (and for some frames afterward). Both
have since been corrected and confirmed through gameplay in earlier builds.
No additional game states should be included without trace evidence.

**Opt-out guarantee:** when Modern is disabled, the packet hook does not
read or modify guest input. The overlay hook can still update its host-only
player pointer. Tracing remains independently opt-in with **F9**.

## Testing status

- Build `5b765e6`: verified continuous movement/strafing, diagonals, live
  enable/disable, ordinary third-person and normal first-person Egg Aim.
- Commit `bc71914`: confirmed egg firing no longer disables strafing.
- **This mapped-profile/R-held pitch/inversion revision has not yet been
  verified by a complete Windows build or gameplay.** Test both inversion
  directions with R held/released, shooting while strafing, re-entering the
  mode, original-controls master toggle, and custom rebinding of a C direction
  (a different controller axis or a keyboard/button binding).

N64 packet: `0x80079B40`, copied by `func_8001608C`; game-side copy
`0x80079B58`. The observed first-person player comes from overlay 108,
`bsfirstp_entrypoint_1`. See `BREEGULL_INPUT_TRACE.md` for optional F9 CSV
recordings. The recorder confirms input values copied by the game, **not**
actual world movement or camera orientation.

Run tests with Python and a C++20 compiler:

```sh
python tools/tests/test_breegull_hooks.py
python tools/tests/test_camera_hooks.py
c++ -std=c++20 -Iinclude tools/tests/breegull_remap_test.cpp -o breegull_remap_test
./breegull_remap_test
```
