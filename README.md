# Banjo-Tooie: Recompiled — camera and Breegull controls fork

An unofficial feature fork of [Vidanox's Banjo-Tooie: Recompiled](https://github.com/Vidanox/BanjoTooieRecompiled), adding optional **third-person camera settings** and **modernized Breegull Blaster controls** to the native PC port.

This fork is based on an earlier upstream revision and may not include newer upstream fixes. For the latest development of the original port, visit [Vidanox/BanjoTooieRecompiled](https://github.com/Vidanox/BanjoTooieRecompiled). The enhancements here may be proposed for upstream integration.

## Contents

- [What's changed](#whats-changed)
- [Downloads and running](#downloads-and-running)
- [Testing and compatibility](#testing-and-compatibility)
- [Building and implementation](#building-and-implementation)
- [Credits and licensing](#credits-and-licensing)

## What's changed

These features are optional; default settings preserve the original controls.

### Third-person camera

In **Settings → Camera**:

- **Invert horizontal camera** reverses manual third-person camera rotation.
- **Manual camera rotation speed** changes the speed of manual camera turns (**1.00×–4.00×**, where 1.00× is the original speed).

### Breegull Blaster

In **Settings → Breegull Blaster**, turn on **Modern Breegull Blaster controls** to use a twin-stick layout:

- **Left stick:** move forward/backward and strafe left/right.
- **Right stick:** look horizontally and vertically, including when holding **R** to aim.
- **Invert Vertical Look:** On retains the original inverted look direction; Off makes pushing the right stick up look up. Default: On.
- **Vertical Look Speed (Outside R-Aim):** adjusts ordinary vertical looking (**0.50×–3.00×**). It does not change horizontal turning or R-held aiming speed. **1.00×** is the original vertical speed.

Modern Breegull also prevents unwanted vertical camera leveling when movement starts or changes direction. Intentional camera adjustments, such as scripted transitions, remain intact. Disabling Modern Breegull restores the original control behavior.

For controller-binding details and technical limitations, see [BREEGULL_CONTROLS.md](BREEGULL_CONTROLS.md).

## Downloads and running

Available builds will be listed under [Releases](https://github.com/chrisfort201/BanjoTooieRecompiled/releases). The [original project's releases](https://github.com/Vidanox/BanjoTooieRecompiled/releases) do not necessarily include this fork's changes.

**Windows x64:** extract the distribution ZIP and keep the executable, DLLs, and `assets/` together. Run `BanjoTooieRecompiled.exe`, then use **Select ROM** to provide your own Banjo-Tooie (USA v1.0) ROM.

**Linux x64 (experimental):** if a Linux archive is available, extract it and run `./run.sh` from the extracted directory. Consult the [Linux dependencies in the base-version README](https://github.com/chrisfort201/BanjoTooieRecompiled/blob/9e5852541ba1436417e2c4468960e8c6de6deda5/README.md). The Linux build of this fork has not been launched or playtested.

**No ROM is included.** You need your own  copy of Banjo-Tooie (USA v1.0).

## Testing and compatibility

The features have been exercised in gameplay on **Windows x64** with a DualShock 4, including normal movement, Breegull aiming, inversion, speed adjustment, and preservation of a scripted camera adjustment. The entire Breegull Blaster sequence has **not** been comprehensively playtested.

Modern Breegull is intended for conventional dual-stick controllers. It uses RecompFrontend's configured N64 bindings rather than requiring PlayStation-specific inputs. Other supported gamepads and custom bindings have not been comprehensively playtested; mouse-look is not included.

Linux x64 compilation is supported by a separate **manual build workflow**, but its resulting game has not been run or playtested for this fork. The fork does not currently provide independently maintained Flatpak or macOS releases.

## Building and implementation

For build requirements, ROM/code-generation procedures, required platform libraries, and troubleshooting, see the **[README from the exact upstream revision on which this fork was based](https://github.com/chrisfort201/BanjoTooieRecompiled/blob/9e5852541ba1436417e2c4468960e8c6de6deda5/README.md)**. This frozen reference matches the original source base; [upstream's current README](https://github.com/Vidanox/BanjoTooieRecompiled/blob/main/README.md) may describe newer changes.

The modifications use native hooks declared in `banjotooie.us.toml`, with host-side C++ implementations; corresponding `RecompiledFuncs/` call sites are generated output. The changes are documented in [CAMERA_PATCH.md](CAMERA_PATCH.md) and [BREEGULL_CONTROLS.md](BREEGULL_CONTROLS.md). For optional developer traces, see [BREEGULL_INPUT_TRACE.md](BREEGULL_INPUT_TRACE.md).

Builds are started manually through [GitHub Actions](https://github.com/chrisfort201/BanjoTooieRecompiled/actions); commits do not automatically produce releases. For maintainers preparing builds and releases, see [RELEASING.md](RELEASING.md).

## Credits and licensing

The native port and its supporting technology originate with [Vidanox/BanjoTooieRecompiled](https://github.com/Vidanox/BanjoTooieRecompiled), [N64Recomp](https://github.com/N64Recomp/N64Recomp), [RT64](https://github.com/rt64/rt64), [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) and their respective contributors. This fork is not affiliated with the original game's owners. The game and trademarks remain the property of their respective rights holders.

The repository includes [GPL-3.0 license text](COPYING); dependencies and assets may carry other terms. See [RELEASING.md](RELEASING.md) for packaging and redistribution checks before publishing binaries.
