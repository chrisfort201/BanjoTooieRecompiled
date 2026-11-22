// Modern Breegull Blaster input translation, outside generated game code.
// One overlay-scoped pointer hook observes the actual first-person player;
// one core input hook translates the N64 controller packet immediately before
// the game copies it into its processed input state (func_8001608C). Hooking
// the async osContGetReadData producer risks tearing/racing with that copy.
// Both hook declarations live in banjotooie.us.toml
// so a subsequent N64Recomp run recreates the calls automatically.
#include "tooie_breegull_controls.h"
#include "tooie_breegull_remap.hpp"
#include "tooie_breegull_settings.hpp"
#include "tooie_breegull_trace.hpp"
#include "recompinput/players.h"
#include "recompinput/profiles.h"
#include "recompinput/input_state.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

namespace {
std::atomic<bool> enabled{false};
// One atomic snapshot of four RecompFrontend-mapped logical C directions.
// No physical SDL axis is used for gameplay translation.
std::atomic<uint64_t> mapped_look{0};
std::atomic<bool> invert_vertical{true};
// Shared between the first-person game-state thread and the N64 controller
// polling thread. A thread_local here made the pad hook see zero forever.
// Observe the actor even with the toggle off, so turning it on while already
// in Breegull Blaster does not depend on the overlay entrypoint running again.
// This host-side bookkeeping never changes guest state when disabled.
std::atomic<uint32_t> firstperson_player{0};

bool valid_guest_pointer(uint32_t ptr, uint32_t bytes) {
    // Tooie uses KSEG0 guest pointers for objects in the 8 MiB N64 RDRAM.
    return ptr >= 0x80000000u && ptr <= 0x80800000u - bytes;
}

gpr guest_address(uint32_t ptr) {
    return static_cast<gpr>(static_cast<int32_t>(ptr));
}

} // namespace

namespace tooie::breegull {
void set_modern_controls(bool state) {
    enabled.store(state, std::memory_order_release);
}
bool modern_controls_enabled() {
    return enabled.load(std::memory_order_acquire);
}
void set_invert_vertical_look(bool state) {
    invert_vertical.store(state, std::memory_order_release);
}
// Read the SAME profile slots and per-player input accessor as
// recompinput::profiles::get_n64_input in the pinned RecompFrontend. This
// preserves analog magnitude for rebound axes and digital inputs for keys.
float mapped_strength(int controller_profile, int keyboard_profile,
                      recompinput::GameInput input) {
    const auto from_profile = [input](int profile) -> float {
        if (profile < 0 || profile >= recompinput::profiles::get_input_profile_count())
            return 0.0f;
        float strength = 0.0f;
        for (size_t slot = 0; slot < recompinput::num_bindings_per_input; ++slot) {
            const recompinput::InputField field =
                recompinput::profiles::get_input_binding(profile, input, slot);
            strength += recompinput::get_input_analog(0, field);
        }
        return std::clamp(strength, 0.0f, 1.0f);
    };
    return std::clamp(from_profile(controller_profile) +
                      from_profile(keyboard_profile), 0.0f, 1.0f);
}

void sample_mapped_look() {
    const bool active = enabled.load(std::memory_order_relaxed);
    LookInput look{};
    // This runs on the SDL/UI event thread, after handle_events(). Read-only
    // frontend profile queries never run in the recompiled game thread.
    if (active && !recompinput::game_input_disabled() &&
                  !recompinput::all_input_disabled()) {
        const bool single = recompinput::players::is_single_player_mode();
        const int controller_profile = single
            ? recompinput::profiles::get_sp_controller_profile_index()
            : recompinput::profiles::get_input_profile_for_player(
                  0, recompinput::InputDevice::Controller);
        const int keyboard_profile = single
            ? recompinput::profiles::get_sp_keyboard_profile_index()
            : recompinput::profiles::get_input_profile_for_player(
                  0, recompinput::InputDevice::Keyboard);
        look.left = mapped_strength(controller_profile, keyboard_profile,
                                    recompinput::GameInput::C_LEFT);
        look.right = mapped_strength(controller_profile, keyboard_profile,
                                     recompinput::GameInput::C_RIGHT);
        look.up = mapped_strength(controller_profile, keyboard_profile,
                                  recompinput::GameInput::C_UP);
        look.down = mapped_strength(controller_profile, keyboard_profile,
                                    recompinput::GameInput::C_DOWN);
    }
    mapped_look.store(pack_look(look), std::memory_order_release);

    // The optional F9 recorder still logs raw SDL axes as diagnostic context.
    // These values have NO influence on gameplay or active input bindings.
    if (!trace_active()) return;
    SDL_GameController* controller = recompinput::players::get_player(0).controller;
    if (controller && !SDL_GameControllerGetAttached(controller)) controller = nullptr;
    if (!controller && recompinput::players::is_single_player_mode()) {
        SDL_GameController* only = nullptr;
        bool ambiguous = false;
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (!SDL_IsGameController(i)) continue;
            SDL_GameController* opened = SDL_GameControllerFromInstanceID(
                SDL_JoystickGetDeviceInstanceID(i));
            if (!opened || !SDL_GameControllerGetAttached(opened)) continue;
            if (only && only != opened) { ambiguous = true; break; }
            only = opened;
        }
        if (!ambiguous) controller = only;
    }
    const int16_t raw_lx = controller ? SDL_GameControllerGetAxis(
        controller, SDL_CONTROLLER_AXIS_LEFTX) : 0;
    const int16_t raw_ly = controller ? SDL_GameControllerGetAxis(
        controller, SDL_CONTROLLER_AXIS_LEFTY) : 0;
    const int16_t raw_rx = controller ? SDL_GameControllerGetAxis(
        controller, SDL_CONTROLLER_AXIS_RIGHTX) : 0;
    const int16_t raw_ry = controller ? SDL_GameControllerGetAxis(
        controller, SDL_CONTROLLER_AXIS_RIGHTY) : 0;
    trace_raw_axes(raw_lx, raw_ly, raw_rx, raw_ry);
}
} // namespace tooie::breegull

extern "C" void tooie_breegull_hook_player(uint8_t*, recomp_context* ctx) {
    // bsfirstp_entrypoint_1 receives the player actor as a0. Keep its guest
    // pointer across host threads; the pad hook still checks the *live* player
    // state before changing any N64 input. No guest memory is read here.
    const uint32_t player = static_cast<uint32_t>(ctx->r4);
    firstperson_player.store(valid_guest_pointer(player, 0x124) ? player : 0,
                             std::memory_order_release);
}

extern "C" void tooie_breegull_hook_pad(uint8_t* rdram, recomp_context*) {
    // The normal off path has zero guest reads/writes, as before. Recording is
    // explicitly opt-in; while recording we read input even with the feature off.
    const bool active = enabled.load(std::memory_order_acquire);
    const bool recording = tooie::breegull::trace_active();
    if (!active && !recording) return;
    const gpr pad = guest_address(0x80079B40u);
    tooie::breegull::Pad original{
        static_cast<uint16_t>(MEM_H(0, pad)),
        static_cast<int8_t>(MEM_B(2, pad)),
        static_cast<int8_t>(MEM_B(3, pad)),
    };
    auto changed = original;
    const char* reason = "toggle_off";
    uint32_t state = 0;
    if (active) {
        const uint32_t player = firstperson_player.load(std::memory_order_acquire);
        if (!valid_guest_pointer(player, 0x124)) {
            reason = "player_not_observed";
        } else {
            const uint32_t state_ptr = static_cast<uint32_t>(
                MEM_W(0x120, guest_address(player)));
            if (!valid_guest_pointer(state_ptr, 8)) {
                reason = "state_ptr_invalid";
            } else {
                state = static_cast<uint32_t>(MEM_W(4, guest_address(state_ptr)));
                if (!tooie::breegull::is_blaster_state(state)) {
                    reason = "not_blaster_state";
                } else {
                    reason = "translated";
                    const uint64_t sampled = mapped_look.load(std::memory_order_acquire);
                    changed = tooie::breegull::remap(original,
                        tooie::breegull::unpack_look(sampled),
                        invert_vertical.load(std::memory_order_acquire));
                    MEM_H(0, pad) = changed.buttons;
                    MEM_B(2, pad) = changed.stick_x;
                    MEM_B(3, pad) = changed.stick_y;
                }
            }
        }
    }
    if (recording) tooie::breegull::trace_packet(original, changed, state, reason);
}

extern "C" void tooie_breegull_hook_produced(uint8_t* rdram, recomp_context*) {
    if (!tooie::breegull::trace_active()) return;
    // A read-only snapshot immediately after osContGetReadData updates the
    // producer buffer. This is not the same thread as the game input consumer.
    const gpr producer = guest_address(0x80079B40u);
    tooie::breegull::trace_produced({
        static_cast<uint16_t>(MEM_H(0, producer)),
        static_cast<int8_t>(MEM_B(2, producer)),
        static_cast<int8_t>(MEM_B(3, producer)),
    });
}

extern "C" void tooie_breegull_hook_consumed(uint8_t* rdram, recomp_context*) {
    if (!tooie::breegull::trace_active()) return;
    // 0x800160D8..0x800160E4 copied OSContPad[0] into this game-side buffer.
    // This is the packet actually copied by the game, not a claim that the
    // movement state machine subsequently executed a particular action.
    const gpr copied = guest_address(0x80079B58u);
    tooie::breegull::trace_consumed({
        static_cast<uint16_t>(MEM_H(0, copied)),
        static_cast<int8_t>(MEM_B(2, copied)),
        static_cast<int8_t>(MEM_B(3, copied)),
    });
}


// Diagnostic-only observations of the *candidate* first-person pitch state.
// The first-person overlay's funcs_45/44 hooks supply live ctx registers at
// precise instruction boundaries. These offsets came from the actual guest
// instructions, NOT from a guessed global camera pointer. The CSV is needed
// to establish whether 0x20C corresponds to rendered vertical viewing angle.
extern "C" void tooie_breegull_hook_camera_trace(uint8_t* rdram,
                                                    recomp_context* ctx,
                                                    uint32_t stage) {
    if (!tooie::breegull::trace_active() || stage > 3) return;

    const uint32_t object = static_cast<uint32_t>(
        stage < 2 ? ctx->r16 : ctx->r2);
    tooie::breegull::CameraSample sample{};
    sample.object = object;
    sample.valid = valid_guest_pointer(object, 0x218);
    if (sample.valid) {
        const gpr guest = guest_address(object);
        const auto load_float = [rdram, guest](uint32_t offset) -> float {
            const uint32_t bits = static_cast<uint32_t>(MEM_W(offset, guest));
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        };
        sample.mode = static_cast<uint32_t>(MEM_W(0xC, guest));
        sample.angle_20c = load_float(0x20C);
        sample.angle_210 = load_float(0x210);
        sample.angle_214 = load_float(0x214);
        sample.bound_1e8 = load_float(0x1E8);
        sample.bound_1ec = load_float(0x1EC);
        // These registers are meaningful only at their corresponding sites.
        if (stage == 0) {
            sample.reg_f4 = ctx->f4.fl;  // previous angle at 0x80806884
            sample.reg_f6 = ctx->f6.fl;  // candidate angular increment
        } else if (stage == 2) {
            sample.reg_f0 = ctx->f0.fl;  // reset replacement value
        }
    }

    // Capture the same observed player-state family as the pad hook. A
    // player pointer can outlive an overlay; validate before every read.
    uint32_t player_state = 0;
    const uint32_t player = firstperson_player.load(std::memory_order_acquire);
    if (valid_guest_pointer(player, 0x124)) {
        const uint32_t state_ptr = static_cast<uint32_t>(
            MEM_W(0x120, guest_address(player)));
        if (valid_guest_pointer(state_ptr, 8)) {
            player_state = static_cast<uint32_t>(
                MEM_W(4, guest_address(state_ptr)));
        }
    }
    tooie::breegull::trace_camera(stage, player_state, sample);
}
